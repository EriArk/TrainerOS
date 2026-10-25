#include "LocalLobbyTransport.h"
#include <QCryptographicHash>
#include <QNetworkInterface>
#include <QNetworkDatagram>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>
#include <QDateTime>
#include <QUuid>
#include <QSslKey>
#include <utility>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

namespace trainer {
namespace {
QString certificateId(const QSslCertificate& cert) {
    return cert.isNull()?QString():"local-"+QString::fromLatin1(cert.digest(QCryptographicHash::Sha256).toHex());
}
QByteArray makeIdentity() {
    auto* key=EVP_PKEY_Q_keygen(nullptr,nullptr,"RSA",2048);
    auto* cert=X509_new();auto* bio=BIO_new(BIO_s_mem());QByteArray result;
    if(key&&cert&&bio) {
        X509_set_version(cert,2);ASN1_INTEGER_set(X509_get_serialNumber(cert),1);
        X509_gmtime_adj(X509_getm_notBefore(cert),-300);X509_gmtime_adj(X509_getm_notAfter(cert),10L*365*86400);
        auto* subject=X509_get_subject_name(cert);
        X509_NAME_add_entry_by_txt(subject,"CN",MBSTRING_ASC,reinterpret_cast<const unsigned char*>("TrainerOS Local Lobby"),-1,-1,0);
        X509_set_issuer_name(cert,subject);
        if(X509_set_pubkey(cert,key)==1&&X509_sign(cert,key,EVP_sha256())>0
           &&PEM_write_bio_PrivateKey(bio,key,nullptr,nullptr,0,nullptr,nullptr)==1&&PEM_write_bio_X509(bio,cert)==1) {
            char* data=nullptr;const auto size=BIO_get_mem_data(bio,&data);result=QByteArray(data,int(size));
        }
    }
    BIO_free(bio);X509_free(cert);EVP_PKEY_free(key);return result;
}
bool privateAddress(const QHostAddress& ip) {
    return ip.isInSubnet(QHostAddress("10.0.0.0"),8)||ip.isInSubnet(QHostAddress("172.16.0.0"),12)
        ||ip.isInSubnet(QHostAddress("192.168.0.0"),16)||ip.isInSubnet(QHostAddress("169.254.0.0"),16);
}
}
bool LocalLobbyTransport::validIdentity(const QString& id) {
    if(!id.startsWith("local-")||id.size()!=70)return false;
    for(const auto c:id.mid(6))if(!QString("0123456789abcdef").contains(c))return false;
    return true;
}
LocalLobbyTransport::LocalLobbyTransport(QObject* parent,quint16 port,bool loopback):QObject(parent),discoveryPort_(port),loopbackOnly_(loopback) {
    timer_.setInterval(1500);connect(&timer_,&QTimer::timeout,this,&LocalLobbyTransport::refresh);
    connect(&discovery_,&QUdpSocket::readyRead,this,&LocalLobbyTransport::discover);
}
LocalLobbyTransport::~LocalLobbyTransport(){close();}
bool LocalLobbyTransport::configure(QString directory,QString name) {
    close();identity_.clear();name_=name.trimmed().left(48);if(name_.isEmpty())name_="Local player";
    const auto fail=[this]{error_="Local identity could not be opened. Your existing identity was kept.";emit changed();return false;};
    if(directory.isEmpty()||!QSslSocket::supportsSsl()||!QDir().mkpath(directory))return fail();
    const auto path=directory+"/identity.pem";QByteArray pem;
    if(QFileInfo::exists(path)) {
        QFile file(path);if(QFileInfo(file).isSymLink()||file.size()>16384||!file.open(QIODevice::ReadOnly))return fail();pem=file.readAll();
    } else {
        pem=makeIdentity();QSaveFile file(path);
        if(pem.isEmpty()||!file.open(QIODevice::WriteOnly))return fail();
        if(!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)||file.write(pem)!=pem.size()||!file.commit())return fail();
    }
    const QSslCertificate cert(pem);const QSslKey key(pem,QSsl::Rsa,QSsl::Pem,QSsl::PrivateKey);
    if(cert.isNull()||key.isNull()||cert.expiryDate()<=QDateTime::currentDateTimeUtc())return fail();
    tls_=QSslConfiguration::defaultConfiguration();tls_.setProtocol(QSsl::TlsV1_2OrLater);
    tls_.setLocalCertificate(cert);tls_.setPrivateKey(key);tls_.setPeerVerifyMode(QSslSocket::VerifyPeer);
    identity_=certificateId(cert);error_.clear();return true;
}
QHash<QHostAddress,QHostAddress> LocalLobbyTransport::interfaces() const {
    if(loopbackOnly_)return {{QHostAddress::LocalHost,QHostAddress::LocalHost}};
    QHash<QHostAddress,QHostAddress> result;
    for(const auto& iface:QNetworkInterface::allInterfaces()) {
        if(!iface.flags().testFlag(QNetworkInterface::IsUp)||!iface.flags().testFlag(QNetworkInterface::IsRunning)
           ||iface.flags().testFlag(QNetworkInterface::IsLoopBack)
           ||(iface.type()!=QNetworkInterface::Ethernet&&iface.type()!=QNetworkInterface::Wifi))continue;
        for(const auto& entry:iface.addressEntries())if(entry.ip().protocol()==QAbstractSocket::IPv4Protocol
            &&privateAddress(entry.ip())&&entry.prefixLength()>0&&!entry.broadcast().isNull())result.insert(entry.ip(),entry.broadcast());
    }
    return result;
}
bool LocalLobbyTransport::local(const QHostAddress& address) const {
    if(loopbackOnly_)return address.isLoopback();
    if(!privateAddress(address))return false;
    const auto allowed=interfaces();
    for(const auto& iface:QNetworkInterface::allInterfaces())for(const auto& entry:iface.addressEntries())
        if(allowed.contains(entry.ip())&&address.isInSubnet(entry.ip(),entry.prefixLength()))return true;
    return false;
}
bool LocalLobbyTransport::open() {
    if(running())return true;
    if(identity_.isEmpty())return false;
    if(!discovery_.bind(loopbackOnly_?QHostAddress::LocalHost:QHostAddress::AnyIPv4,discoveryPort_,QUdpSocket::ShareAddress|QUdpSocket::ReuseAddressHint)) {
        error_="Local discovery could not open. Retry Local mode.";emit changed();return false;
    }
    timer_.start();refresh();return true;
}
void LocalLobbyTransport::close(bool graceful) {
    timer_.stop();discovery_.close();
    for(auto* listener:listeners_)delete listener;listeners_.clear();
    for(auto* socket:links_.keys()) {
        if(graceful&&socket->isEncrypted()) {
            socket->disconnectFromHost();
            QTimer::singleShot(1000,socket,[this,socket]{drop(socket);});
        } else drop(socket);
    }
    discovered_.clear();rooms_={};emit changed();
}
void LocalLobbyTransport::setRooms(QJsonArray rooms){if(rooms_==rooms)return;rooms_=rooms;if(running())refresh();}
void LocalLobbyTransport::refresh() {
    const auto available=interfaces();bool changed=false;
    for(const auto& ip:listeners_.keys())if(!available.contains(ip)){delete listeners_.take(ip);changed=true;}
    for(auto* socket:links_.keys())if(!local(socket->peerAddress())||!available.contains(socket->localAddress()))drop(socket);
    for(auto it=available.cbegin();it!=available.cend();++it)if(!listeners_.contains(it.key())) {
        auto* server=new LocalLobbyListener(this);server->setMaxPendingConnections(16);
        server->accept=[this](qintptr fd){auto* socket=new QSslSocket(this);
            if(!socket->setSocketDescriptor(fd)||links_.size()>=32||!local(socket->peerAddress())){socket->abort();socket->deleteLater();return;}
            attach(socket);socket->startServerEncryption();};
        if(server->listen(it.key(),0)){listeners_.insert(it.key(),server);changed=true;}else delete server;
    }
    const auto message=listeners_.isEmpty()?QString("Connect to Wi-Fi or Ethernet to find local lobbies."):QString();
    if(error_!=message){error_=message;changed=true;}
    const auto now=QDateTime::currentSecsSinceEpoch();
    for(auto it=discovered_.begin();it!=discovered_.end();)if(it->value("expires").toLongLong()<=now){it=discovered_.erase(it);changed=true;}else ++it;
    for(auto it=listeners_.cbegin();it!=listeners_.cend();++it) {
        QUdpSocket sender;if(!sender.bind(it.key(),0))continue;
        // No content digests, runtime configuration, membership or private data.
        for(const auto& value:rooms_) {
            auto room=value.toObject();room["protocol"]="traineros.local-lobby.1";room["owner"]=identity_;room["name"]=name_;room["port"]=it.value()->serverPort();
            const auto bytes=QJsonDocument(room).toJson(QJsonDocument::Compact);
            if(bytes.size()<=2048)sender.writeDatagram(bytes,available.value(it.key()),discoveryPort_);
        }
    }
    if(changed)emit this->changed();
}
void LocalLobbyTransport::discover() {
    int count=0;bool changed=false;
    while(discovery_.hasPendingDatagrams()&&count++<64) {
        const auto datagram=discovery_.receiveDatagram(2049);if(datagram.data().size()>2048||!local(datagram.senderAddress()))continue;
        const auto obj=QJsonDocument::fromJson(datagram.data()).object();const auto peer=obj["owner"].toString(),room=obj["lobby"].toString();
        const int port=obj["port"].toInt();
        if(obj["protocol"]!="traineros.local-lobby.1"||!validIdentity(peer)||peer==identity_||QUuid(room).isNull()
           ||port<1||port>65535||obj["title"].toString().size()>80||obj["name"].toString().isEmpty()||obj["name"].toString().size()>48)continue;
        const auto key=peer+"/"+room;if(discovered_.size()>=128&&!discovered_.contains(key))continue;
        // Whitelist presentation fields; never treat discovery as session state.
        QVariantMap row{{"lobby",room},{"owner",peer},{"title",obj["title"].toString()},{"name",obj["name"].toString()},
            {"topic",obj["topic"].toString().left(32)},{"status",obj["status"].toString().left(16)},
            {"access","approval"},{"type","local"},{"members",qBound(0,obj["members"].toInt(),16)},
            {"seats",qBound(0,obj["seats"].toInt(),16)},{"available",qBound(0,obj["available"].toInt(),16)},
            {"game",QVariantMap{{"title",obj["game"].toObject()["title"].toString().left(160)},{"platform",obj["game"].toObject()["platform"].toString().left(32)},
                {"mode",obj["game"].toObject()["mode"].toString().left(64)}}},
            {"address",datagram.senderAddress().toString()},{"port",port}};
        auto previous=discovered_.value(key);previous.remove("expires");changed|=previous!=row;
        row["expires"]=QDateTime::currentSecsSinceEpoch()+6;discovered_[key]=row;
    }
    if(changed)emit this->changed();
    if(discovery_.hasPendingDatagrams())QTimer::singleShot(0,this,&LocalLobbyTransport::discover);
}
QVariantList LocalLobbyTransport::rooms() const {QVariantList rows;for(const auto& row:discovered_)rows.append(row);return rows;}
QSslSocket* LocalLobbyTransport::connection(QString peer) const {
    for(auto it=links_.cbegin();it!=links_.cend();++it)if(it.key()->state()!=QAbstractSocket::ClosingState&&(it->peer==peer||it->expected==peer))return it.key();
    return nullptr;
}
QString LocalLobbyTransport::addressOf(QString peer) const {auto* s=connection(peer);return s&&s->isEncrypted()?s->peerAddress().toString():QString();}
QString LocalLobbyTransport::localAddressFor(QString peer) const {auto* s=connection(peer);return s&&s->isEncrypted()?s->localAddress().toString():QString();}
bool LocalLobbyTransport::send(QString peer,QJsonObject packet) {
    if(!running()||!validIdentity(peer)||peer==identity_||QJsonDocument(packet).toJson(QJsonDocument::Compact).size()>65536)return false;
    auto* socket=connection(peer);
    if(!socket) {
        QVariantMap remote;for(const auto& row:discovered_)if(row["owner"]==peer&&row["expires"].toLongLong()>QDateTime::currentSecsSinceEpoch()){remote=row;break;}
        if(remote.isEmpty()||links_.size()>=32||!local(QHostAddress(remote["address"].toString())))return false;
        socket=new QSslSocket(this);attach(socket,peer);
        socket->connectToHostEncrypted(remote["address"].toString(),quint16(remote["port"].toInt()));
        if(!links_.contains(socket))return false;
    }
    auto& link=links_[socket];
    if(!link.hello){if(link.pending.size()>=32){drop(socket);return false;}link.pending.append(packet);return true;}
    return write(socket,{{"packet",packet}});
}
void LocalLobbyTransport::attach(QSslSocket* socket,QString expected) {
    socket->setSslConfiguration(tls_);socket->setReadBufferSize(131072);links_.insert(socket,Link{expected});
    connect(socket,&QSslSocket::sslErrors,this,[this,socket](const QList<QSslError>& errors){
        // Self-signed certificate identity is the trust model; never bypass other
        // TLS validation or accept a different advertised host certificate.
        if(!links_.contains(socket))return;
        const auto id=certificateId(socket->peerCertificate());
        if(id.isEmpty()||(!links_[socket].expected.isEmpty()&&id!=links_[socket].expected)){drop(socket);return;}
        for(const auto& e:errors)if(e.error()!=QSslError::SelfSignedCertificate&&e.error()!=QSslError::HostNameMismatch){drop(socket);return;}
        socket->ignoreSslErrors(errors);
    });
    connect(socket,&QSslSocket::encrypted,this,[this,socket]{
        if(!links_.contains(socket))return;const auto id=certificateId(socket->peerCertificate());auto& link=links_[socket];
        if(!validIdentity(id)||id==identity_||(!link.expected.isEmpty()&&link.expected!=id)){drop(socket);return;}
        for(auto* other:links_.keys())if(other!=socket&&links_[other].peer==id){drop(socket);return;}
        link.peer=id;write(socket,{{"hello",1},{"name",name_}});
    });
    connect(socket,&QSslSocket::readyRead,this,[this,socket]{read(socket);});
    connect(socket,&QSslSocket::disconnected,this,[this,socket]{drop(socket);});
    connect(socket,&QSslSocket::errorOccurred,this,[this,socket](auto){drop(socket);});
    QTimer::singleShot(10000,socket,[this,socket]{if(links_.contains(socket)&&!links_[socket].hello)drop(socket);});
}
bool LocalLobbyTransport::write(QSslSocket* socket,const QJsonObject& packet) {
    const auto bytes=QJsonDocument(packet).toJson(QJsonDocument::Compact)+'\n';
    if(bytes.size()>66000||socket->bytesToWrite()>131072||socket->write(bytes)!=bytes.size()){drop(socket);return false;}return true;
}
void LocalLobbyTransport::read(QSslSocket* socket) {
    if(!links_.contains(socket)||!socket->isEncrypted())return;
    links_[socket].buffer+=socket->readAll();if(links_[socket].buffer.size()>131072){drop(socket);return;}
    for(int count=0;links_.contains(socket)&&count<32;++count) {
        auto& link=links_[socket];const auto at=link.buffer.indexOf('\n');if(at<0)return;
        if(at>66000){drop(socket);return;}
        QJsonParseError error;const auto obj=QJsonDocument::fromJson(link.buffer.left(at),&error).object();link.buffer.remove(0,at+1);
        if(error.error!=QJsonParseError::NoError||obj.isEmpty()){drop(socket);return;}
        if(!link.hello) {
            if(obj["hello"]!=1||obj["name"].toString().trimmed().isEmpty()||obj["name"].toString().size()>48){drop(socket);return;}
            link.name=obj["name"].toString();link.hello=true;const auto pending=std::exchange(link.pending,{});
            for(const auto& packet:pending)if(!write(socket,{{"packet",packet}}))return;
        } else {
            if(obj.size()!=1||!obj["packet"].isObject()){drop(socket);return;}
            const auto peer=link.peer,name=link.name;emit received(peer,name,obj["packet"].toObject());
        }
    }
    if(links_.contains(socket)&&links_[socket].buffer.contains('\n'))QTimer::singleShot(0,socket,[this,socket]{read(socket);});
}
void LocalLobbyTransport::drop(QSslSocket* socket) {
    if(!links_.contains(socket))return;const auto link=links_.take(socket);socket->disconnect(this);socket->abort();socket->deleteLater();
    const auto peer=link.peer.isEmpty()?link.expected:link.peer;
    // A rejected duplicate socket must not disconnect the admitted session.
    if(!peer.isEmpty()) {
        for(auto it=links_.cbegin();it!=links_.cend();++it)
            if(it->peer==peer&&it->hello&&it.key()->isEncrypted()&&it.key()->state()==QAbstractSocket::ConnectedState)return;
        emit disconnected(peer);
    }
}
}
