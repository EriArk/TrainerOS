#include "LocalLinkPeer.h"
#include <QNetworkInterface>
#include <QNetworkDatagram>
#include <QJsonDocument>
#include <QDateTime>
#include <QUuid>
namespace trainer {
LocalLinkPeer::~LocalLinkPeer(){close();}
LocalLinkPeer::LocalLinkPeer(QObject* parent,quint16 port,quint16 discoveryPort,QString discoveryKey):QObject(parent),port_(port),discoveryPort_(discoveryPort),discoveryKey_(std::move(discoveryKey)) {
    connectTimer_.setSingleShot(true);connectTimer_.setInterval(12000);
    connect(&connectTimer_,&QTimer::timeout,this,[this]{
        emit error("Could not reach your friend. Try inviting them again.");disconnectPeer();
    });
    timer_.setInterval(1500);connect(&timer_,&QTimer::timeout,this,&LocalLinkPeer::announce);
    connect(&discovery_,&QUdpSocket::readyRead,this,&LocalLinkPeer::readDiscovery);
    connect(&server_,&QTcpServer::newConnection,this,[this]{
        while(server_.hasPendingConnections()) {auto* socket=server_.nextPendingConnection();
            if(socket_ || !localAddress(socket->peerAddress())){socket->abort();socket->deleteLater();}else {outgoing_=false;attach(socket);}}
    });
}
void LocalLinkPeer::configure(const QString& id,const QString& name){id_=id;name_=name.left(48);if(timer_.isActive())announce();}
bool LocalLinkPeer::localAddress(const QHostAddress& address) const {
    if(address.isLoopback())return true;
    for(const auto& iface:QNetworkInterface::allInterfaces())for(const auto& entry:iface.addressEntries())
        if(entry.ip().protocol()==QAbstractSocket::IPv4Protocol && entry.prefixLength()>0 && address.isInSubnet(entry.ip(),entry.prefixLength()))return true;
    return false;
}
bool LocalLinkPeer::open() {
    if(timer_.isActive())return true;
    if(QUuid(id_).isNull() || !server_.listen(QHostAddress::AnyIPv4,port_)
        || !discovery_.bind(QHostAddress::AnyIPv4,discoveryPort_,QUdpSocket::ShareAddress|QUdpSocket::ReuseAddressHint)) {
        close();emit error("Local Link could not open. Check the Wi-Fi connection.");return false;
    }
    server_.setMaxPendingConnections(2);timer_.start();announce();return true;
}
void LocalLinkPeer::close(){timer_.stop();server_.close();discovery_.close();disconnectPeer();peers_.clear();emit changed();}
void LocalLinkPeer::disconnectPeer(){connectTimer_.stop();if(!socket_)return;auto* old=socket_;socket_=nullptr;old->disconnect(this);old->abort();old->deleteLater();buffer_.clear();emit disconnectedFromPeer();}
QVariantList LocalLinkPeer::peers() const {QVariantList result;for(const auto& row:peers_)result.append(row);return result;}
void LocalLinkPeer::announce() {
    const auto now=QDateTime::currentMSecsSinceEpoch();bool changed=false;
    for(auto it=peers_.begin();it!=peers_.end();)if(now-it.value()["seen"].toLongLong()>6000){it=peers_.erase(it);changed=true;}else ++it;
    if(changed)emit this->changed();
    if(!advertising_)return;
    const auto packet=QJsonDocument(QJsonObject{{discoveryKey_,2},{"id",id_},{"name",name_},{"bluetooth",bluetoothId_}}).toJson(QJsonDocument::Compact);
    for(const auto& iface:QNetworkInterface::allInterfaces())if(iface.flags().testFlag(QNetworkInterface::IsUp))for(const auto& address:iface.addressEntries())
        if(address.ip().protocol()==QAbstractSocket::IPv4Protocol && !address.broadcast().isNull())discovery_.writeDatagram(packet,address.broadcast(),discoveryPort_);
}
void LocalLinkPeer::readDiscovery() {
    int processed=0;
    while(discovery_.hasPendingDatagrams() && processed++<32) {
        const auto packet=discovery_.receiveDatagram(1025);if(packet.data().size()>1024 || !localAddress(packet.senderAddress()))continue;
        const auto j=QJsonDocument::fromJson(packet.data()).object();const auto id=j["id"].toString();
        if(j[discoveryKey_].toInt()!=2 || QUuid(id).isNull() || id==id_ || j["name"].toString().size()>48)continue;
        if(peers_.size()>=16 && !peers_.contains(id))continue;
        auto addresses=peers_.value(id).value("addresses").toStringList();const auto address=packet.senderAddress().toString();
        if(!addresses.contains(address))addresses.append(address);while(addresses.size()>8)addresses.removeFirst();
        const auto bluetooth=j["bluetooth"].toString();
        peers_[id]={{"id",id},{"name",j["name"].toString()},{"address",address},{"addresses",addresses},
                    {"bluetooth",QUuid(bluetooth).isNull()?QString{}:bluetooth},{"seen",QDateTime::currentMSecsSinceEpoch()}};
        emit changed();
    }
}
void LocalLinkPeer::connectPeer(int index) {
    const auto list=peers();if(socket_ || index<0 || index>=list.size())return;
    connectId(list[index].toMap()["id"].toString());
}
void LocalLinkPeer::connectId(const QString& id,const QString& interface) {
    if(socket_ || !peers_.contains(id))return;
    auto address=peers_[id]["address"].toString();
    if(!interface.isEmpty()) {
        address.clear();const auto iface=QNetworkInterface::interfaceFromName(interface);
        for(const auto& entry:iface.addressEntries())for(const auto& ip:peers_[id]["addresses"].toStringList())
            if(entry.ip().protocol()==QAbstractSocket::IPv4Protocol && QHostAddress(ip).isInSubnet(entry.ip(),entry.prefixLength()))address=ip;
        if(address.isEmpty())return;
    }
    outgoing_=true;auto* socket=new QTcpSocket(this);attach(socket);connectTimer_.start();socket->connectToHost(address,port_);
}
void LocalLinkPeer::attach(QTcpSocket* socket) {
    socket_=socket;buffer_.clear();socket->setReadBufferSize(65536);
    connect(socket,&QTcpSocket::readyRead,this,&LocalLinkPeer::receive);
    connect(socket,&QTcpSocket::connected,this,[this]{connectTimer_.stop();emit connectedToPeer();});
    connect(socket,&QTcpSocket::disconnected,this,[this]{disconnectPeer();});
    connect(socket,&QTcpSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){
        if(connectTimer_.isActive())emit error("Could not reach your friend. Try inviting them again.");
        disconnectPeer();
    });
    if(socket->state()==QAbstractSocket::ConnectedState)emit connectedToPeer();
}
void LocalLinkPeer::connectBridge(quint16 port) {
    if(socket_ || !port || port==port_)return;
    outgoing_=true;auto* socket=new QTcpSocket(this);attach(socket);
    connectTimer_.start();socket->connectToHost(QHostAddress::LocalHost,port);
}
void LocalLinkPeer::send(const QJsonObject& object) {
    if(!connected())return;const auto bytes=QJsonDocument(object).toJson(QJsonDocument::Compact)+'\n';
    if(bytes.size()>32768 || socket_->bytesToWrite()>65536 || socket_->write(bytes)!=bytes.size())disconnectPeer();
}
void LocalLinkPeer::receive() {
    if(!socket_)return;buffer_+=socket_->readAll();if(buffer_.size()>65536){disconnectPeer();return;}
    int at=0,count=0;
    while(socket_ && (at=buffer_.indexOf('\n'))>=0) {
        if(at>32768 || ++count>32){disconnectPeer();return;}
        const auto bytes=buffer_.left(at);buffer_.remove(0,at+1);QJsonParseError error;
        const auto doc=QJsonDocument::fromJson(bytes,&error);
        if(error.error!=QJsonParseError::NoError || !doc.isObject()){disconnectPeer();return;}
        emit received(doc.object());
    }
}
}
