// SPDX-License-Identifier: GPL-3.0-or-later
// Carry the existing Moonlight sockets through ICE/DTLS. No media codec,
// controller injection, application launch or arbitrary destination forwarding.
#include <rtc/rtc.hpp>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QFile>
#include <QDir>
#include <QPointer>
#include <QRegularExpression>
#include <QSocketNotifier>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUdpSocket>
#include <openssl/rand.h>
#include <array>
#include <atomic>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace {
constexpr int FirstPort=49010, MaxDatagram=4096, MaxRecord=65536;
constexpr qsizetype QueueLimit=256*1024;
volatile std::sig_atomic_t interrupted=0;
bool hex(QString text,int length) {return text.size()==length&&QRegularExpression("^[a-f0-9]+$").match(text).hasMatch();}
QByteArray random(int size) {QByteArray data(size,0);if(RAND_bytes(reinterpret_cast<unsigned char*>(data.data()),size)!=1)throw std::runtime_error("random");return data;}

class Bridge final:public QObject {
public:
    Bridge():input(STDIN_FILENO,QSocketNotifier::Read,this),output(STDOUT_FILENO,QSocketNotifier::Write,this) {
        fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK);
        fcntl(STDOUT_FILENO,F_SETFL,fcntl(STDOUT_FILENO,F_GETFL)|O_NONBLOCK);
        output.setEnabled(false);
        connect(&input,&QSocketNotifier::activated,this,[this]{readInput();});
        connect(&output,&QSocketNotifier::activated,this,[this]{flush();});
        deadline.setSingleShot(true);deadline.start(30000);
        connect(&deadline,&QTimer::timeout,this,[this]{fail();});
        connect(&tcpServer,&QTcpServer::newConnection,this,[this]{acceptRtsp();});
        candidateTimer.setSingleShot(true);candidateTimer.setInterval(100);
        connect(&candidateTimer,&QTimer::timeout,this,[this]{
            if(!localDescription){candidateTimer.start();return;}
            if(!localCandidates.isEmpty()){auto batch=localCandidates;localCandidates={};event({{"event","candidates"},{"identity",identity},{"candidates",batch}});}
        });
        shutdownTimer.start(50);connect(&shutdownTimer,&QTimer::timeout,this,[this]{if(interrupted)QCoreApplication::quit();});
    }
    ~Bridge() override {
        // All callbacks enqueue through this QObject. Its lifetime extends until
        // library workers have closed; queued callbacks are then removed by Qt.
        stopping=true;
        for(auto& c:channels)if(c)c->resetCallbacks();
        if(pc){pc->resetCallbacks();pc->close();}
        for(auto& c:channels)c.reset();pc.reset();
        rtc::Cleanup().wait();
    }
private:
    void event(QJsonObject value) {
        auto bytes=QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n';
        if(out.size()+bytes.size()>MaxRecord){fail();return;}
        out+=bytes;flush();
    }
    void flush() {
        if(out.isEmpty()){output.setEnabled(false);return;}
        auto n=::write(STDOUT_FILENO,out.constData(),out.size());
        if(n>0)out.remove(0,n);else if(n<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR){fail();return;}
        output.setEnabled(!out.isEmpty());
    }
    void fail() {if(stopping.exchange(true))return;QCoreApplication::exit(1);}
    template<class F> void post(F fn,qsizetype size=0) {
        if(stopping)return;
        if(pending.fetch_add(size)+size>QueueLimit){pending-=size;QMetaObject::invokeMethod(this,[this]{fail();},Qt::QueuedConnection);return;}
        QMetaObject::invokeMethod(this,[this,fn=std::move(fn),size]{pending-=size;if(!stopping)try{fn();}catch(...){fail();}},Qt::QueuedConnection);
    }
    void readInput() {
        char bytes[4096];auto n=::read(STDIN_FILENO,bytes,sizeof(bytes));
        if(n==0){QCoreApplication::quit();return;}
        if(n<0){if(errno!=EAGAIN&&errno!=EINTR)fail();return;}
        in.append(bytes,n);if(in.size()>MaxRecord){fail();return;}
        while(in.contains('\n')) {
            auto at=in.indexOf('\n');auto value=QJsonDocument::fromJson(in.left(at)).object();in.remove(0,at+1);
            try{command(value);}catch(...){fail();return;}
        }
    }
    void command(const QJsonObject& value) {
        if(!pc){configure(value);return;}
        if(value==QJsonObject{{"close",true}}){close();return;}
        if(value.size()==2&&value["identity"]==identity&&value["candidates"].isArray()) {
            const auto batch=value["candidates"].toArray();
            if(batch.isEmpty()||batch.size()+remoteCandidateCount>32)throw std::runtime_error("candidate bounds");
            for(const auto& entry:batch) {
                const auto c=entry.toObject();const auto text=c["candidate"].toString(),mid=c["mid"].toString();
                if(c.size()!=2||text.isEmpty()||text.size()>512||text.contains('\n')||text.contains('\r')||mid.size()>32||mid.isEmpty())throw std::runtime_error("candidate");
                ++remoteCandidateCount;
                if(remoteDescription)pc->addRemoteCandidate(rtc::Candidate(text.toStdString(),mid.toStdString()));
                else remoteCandidates.append(c);
            }
            return;
        }
        if(value.size()!=2||value["identity"]!=identity||!value["description"].isString()||remoteDescription)throw std::runtime_error("description");
        const auto text=value["description"].toString();if(text.size()>16384||text.isEmpty())throw std::runtime_error("description bounds");
        remoteDescription=true;
        pc->setRemoteDescription(rtc::Description(text.toStdString(),host?"answer":"offer"));
        for(const auto& entry:remoteCandidates){const auto c=entry.toObject();pc->addRemoteCandidate(rtc::Candidate(c["candidate"].toString().toStdString(),c["mid"].toString().toStdString()));}
        remoteCandidates={};
        if(!host)pc->setLocalDescription(rtc::Description::Type::Answer);
    }
    void configure(const QJsonObject& value) {
        game=value.size()==5&&value["mode"]=="game";
        const int lifetime=value["seconds"].toInt(-1);
        if((value.size()!=4&&!game)||!value["host"].isBool()||!hex(value["identity"].toString(),64)
           ||!value["ice"].isArray()||!value["seconds"].isDouble()||value["seconds"].toDouble()!=lifetime
           ||!((lifetime>=5&&lifetime<=120)||(game&&lifetime==0)))throw std::runtime_error("config");
        host=value["host"].toBool();identity=value["identity"].toString();seconds=value["seconds"].toInt();
        rtc::Configuration config;config.maxMessageSize=MaxRecord;config.mtu=1200;config.disableAutoNegotiation=true;
        const auto servers=value["ice"].toArray();if(servers.size()>4)throw std::runtime_error("ice bounds");
        for(const auto& entry:servers){const auto url=entry.toString();if(url.size()>2048||(!url.startsWith("stun:")&&!url.startsWith("turn:")))throw std::runtime_error("ice server");config.iceServers.emplace_back(url.toStdString());}
        pc=std::make_shared<rtc::PeerConnection>(config);
        // Trickle ICE starts connectivity checks without waiting for a silent
        // public STUN server. Later reflexive candidates use the same bound
        // connection, never a replacement SDP or a second game launch.
        pc->onLocalDescription([this](auto description){post([this,description]{
            if(localDescription){fail();return;}
            const auto sdp=QString::fromStdString(std::string(description));
            if(sdp.size()>16384){fail();return;}
            localDescription=true;
            event({{"event","description"},{"identity",identity},{"description",sdp}});
        });});
        pc->onLocalCandidate([this](auto candidate){post([this,candidate]{
            if(++localCandidateCount>32){fail();return;}
            const auto text=QString::fromStdString(std::string(candidate)),mid=QString::fromStdString(candidate.mid());
            if(text.isEmpty()||text.size()>512||mid.isEmpty()||mid.size()>32){fail();return;}
            localCandidates.append(QJsonObject{{"candidate",text},{"mid",mid}});
            if(!candidateTimer.isActive())candidateTimer.start();
        });});
        pc->onStateChange([this](auto state){if(state==rtc::PeerConnection::State::Failed||state==rtc::PeerConnection::State::Closed||state==rtc::PeerConnection::State::Disconnected)post([this]{if(closing)QCoreApplication::quit();else fail();});});
        for(int i=0;i<3;++i) {
            rtc::DataChannelInit init;init.negotiated=true;init.id=i;init.protocol="traineros-moonlight-sockets-v1";
            if(i==0){init.reliability.unordered=true;init.reliability.maxRetransmits=0;}
            auto c=pc->createDataChannel(i==0?"datagrams":i==1?"admission":"rtsp",init);channels[i]=c;
            c->onOpen([this,i]{post([this,i]{opened[i]=true;open();});});
            c->onClosed([this]{post([this]{if(closing)QCoreApplication::quit();else fail();});});
            c->onError([this](std::string){post([this]{fail();});});
            c->onMessage([this,i](rtc::binary data){
                if(data.size()>size_t(i==0?MaxDatagram+1:MaxRecord)){post([this]{fail();});return;}
                QByteArray bytes(reinterpret_cast<const char*>(data.data()),data.size());
                post([this,i,bytes]{receive(i,bytes);},bytes.size());
            },[this](std::string){post([this]{fail();});});
        }
        if(host)pc->setLocalDescription(rtc::Description::Type::Offer);
    }
    bool send(int channel,const QByteArray& bytes) {
        try {
            const auto c=channels[channel];if(!c||!c->isOpen()){fail();return false;}
            if(c->bufferedAmount()+bytes.size()>size_t(channel==0?32768:MaxRecord)) {
                if(channel==0){++dropped;return false;}fail();return false;
            }
            c->send(reinterpret_cast<const std::byte*>(bytes.constData()),bytes.size());return true;
        }catch(...){fail();return false;}
    }
    void control(QJsonObject value) {send(1,QJsonDocument(value).toJson(QJsonDocument::Compact));}
    void close() {
        if(closing)return;closing=true;
        if(ready)control({{"close",true}});
        QTimer::singleShot(100,this,[]{QCoreApplication::quit();});
    }
    void open() {
        if(!opened[0]||!opened[1]||!opened[2]||greeted)return;
        greeted=true;control({{"identity",identity},{"host",host}});
        material();
    }
    void material() {
        if(!host||!verified||!greeted||materialSent)return;
        materialSent=true;
        key=random(16).toHex();ping=random(8).toHex();auto number=random(4);std::memcpy(&token,number.data(),4);if(!token)token=1;
        control({{"key",QString::fromLatin1(key)},{"ping",QString::fromLatin1(ping)},{"token",qint64(token)}});
    }
    void receive(int channel,const QByteArray& bytes) {
        if(channel==1){admission(QJsonDocument::fromJson(bytes).object());return;}
        if(!ready){fail();return;}
        if(channel==2){rtsp(bytes);return;}
        if(bytes.size()<2){fail();return;}
        const int index=static_cast<unsigned char>(bytes[0]);if(index<1||index>3){fail();return;}
        auto& socket=udp[index-1];
        if(host)socket.writeDatagram(bytes.mid(1),mediaAddress,FirstPort+index);
        else if(peerPorts[index-1])socket.writeDatagram(bytes.mid(1),QHostAddress::LocalHost,peerPorts[index-1]);
        ++received;
    }
    void admission(const QJsonObject& value) {
        if(ready&&value==QJsonObject{{"close",true}}){closing=true;QCoreApplication::quit();return;}
        if(!verified) {
            if(value.size()!=2||value["identity"]!=identity||!value["host"].isBool()||value["host"].toBool()==host)throw std::runtime_error("identity");
            verified=true;
            material();
            return;
        }
        if(ready)throw std::runtime_error("repeated admission");
        if(host) {if(value!=QJsonObject{{"ready",true}})throw std::runtime_error("ack");}
        else {
            if(value.size()!=3||!hex(value["key"].toString(),32)||!hex(value["ping"].toString(),16)
               ||value["token"].toInteger()<1||value["token"].toInteger()>0xffffffffLL)throw std::runtime_error("material");
            key=value["key"].toString().toLatin1();ping=value["ping"].toString().toLatin1();token=uint32_t(value["token"].toInteger());
        }
        startSockets();
        if(!host)control({{"ready",true}});
        ready=true;if(seconds)deadline.start(seconds*1000);else deadline.stop();
        event({{"event","ready"},{"config",directory.path()+"/session.conf"}});
    }
    void startSockets() {
        // A reserved loopback pair isolates each local guest/session. The port
        // protocol stays unchanged; no public listener or peer-chosen address.
        bool reserved=false;
        for(int attempt=0;attempt<16&&!reserved;++attempt) {
            auto bytes=random(4);uint32_t value=0;std::memcpy(&value,bytes.data(),4);
            value=0x7f000002u|(value&0x00fffffcu);
            mediaAddress=QHostAddress(value);proxyAddress=QHostAddress(value+1);
            reserved=tcpServer.listen(proxyAddress,FirstPort);
        }
        if(!reserved)throw std::runtime_error("local session reservation");
        for(int i=0;i<3;++i) {
            auto& socket=udp[i];
            if(!socket.bind(proxyAddress,host?0:FirstPort+i+1,QUdpSocket::DontShareAddress))throw std::runtime_error("udp bind");
            connect(&socket,&QUdpSocket::readyRead,this,[this,i]{
                auto& socket=udp[i];int budget=64;
                while(socket.hasPendingDatagrams()&&budget--) {
                    if(socket.pendingDatagramSize()>MaxDatagram){socket.receiveDatagram();continue;}
                    QByteArray data(socket.pendingDatagramSize(),0);QHostAddress address;quint16 port=0;
                    socket.readDatagram(data.data(),data.size(),&address,&port);
                    if(address!=(host?mediaAddress:QHostAddress(QHostAddress::LocalHost))||data.isEmpty()||(host&&port!=FirstPort+i+1))continue;
                    if(!host){if(peerPorts[i]&&peerPorts[i]!=port)continue;peerPorts[i]=port;}
                    if(!ready)continue;
                    data.prepend(char(i+1));if(send(0,data))++sent;
                }
                if(socket.hasPendingDatagrams())QTimer::singleShot(0,&socket,[&socket]{emit socket.readyRead();});
            });
        }
        if(!directory.isValid())throw std::runtime_error("private directory");
        QFile file(directory.path()+"/session.conf");
        const QByteArray config="address="+(host?mediaAddress:proxyAddress).toString().toLatin1()+"\npeer="+proxyAddress.toString().toLatin1()+"\nkey="+key+"\nping="+ping+"\nconnect="+QByteArray::number(token)+"\nseconds="+QByteArray::number(seconds)
            +(game?"\nmode=game\ndisplay=1\nsound=1\n":"\nmode=synthetic\ndisplay=0\nsound=0\n");
        if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)||!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)
           ||file.write(config)!=config.size()||!file.flush())throw std::runtime_error("private material");
        key.fill(0);key.clear();
    }
    void acceptRtsp() {
        auto socket=tcpServer.nextPendingConnection();
        if(host||!ready||tcp||socket->peerAddress()!=QHostAddress::LocalHost){socket->abort();socket->deleteLater();return;}
        tcp=socket;tcpBytes.clear();requestSent=false;socket->setReadBufferSize(MaxRecord+1);
        connect(socket,&QTcpSocket::readyRead,this,[this,socket]{
            tcpBytes+=socket->readAll();if(tcpBytes.size()>MaxRecord){fail();return;}
            const int split=tcpBytes.indexOf("\r\n\r\n");if(split<0)return;
            int length=0;
            for(const auto& line:tcpBytes.left(split).split('\n'))if(line.toLower().startsWith("content-length:")) {
                bool ok=false;length=line.mid(15).trimmed().toInt(&ok);if(!ok||length<0||length>MaxRecord){fail();return;}
            }
            if(tcpBytes.size()<split+4+length)return;
            if(requestSent||tcpBytes.size()!=split+4+length){fail();return;}
            requestSent=true;send(2,tcpBytes);tcpBytes.clear();
        });
        connect(socket,&QTcpSocket::disconnected,this,[this,socket]{if(tcp==socket)tcp=nullptr;socket->deleteLater();});
        QTimer::singleShot(5000,socket,[this,socket]{if(tcp==socket)fail();});
    }
    void rtsp(const QByteArray& bytes) {
        if(bytes.isEmpty()||bytes.size()>MaxRecord)throw std::runtime_error("rtsp bounds");
        if(!host) {
            if(!tcp||!requestSent)throw std::runtime_error("rtsp response");
            tcp->write(bytes);tcp->disconnectFromHost();return;
        }
        if(tcp)throw std::runtime_error("rtsp busy");
        auto socket=new QTcpSocket(this);tcp=socket;tcpBytes.clear();socket->setReadBufferSize(MaxRecord+1);
        connect(socket,&QTcpSocket::connected,this,[socket,bytes]{socket->write(bytes);});
        connect(socket,&QTcpSocket::readyRead,this,[this,socket]{tcpBytes+=socket->readAll();if(tcpBytes.size()>MaxRecord)fail();});
        connect(socket,&QTcpSocket::disconnected,this,[this,socket]{
            tcpBytes+=socket->readAll();if(tcpBytes.isEmpty()||tcpBytes.size()>MaxRecord){fail();return;}
            auto response=tcpBytes;tcpBytes.clear();tcp=nullptr;socket->deleteLater();send(2,response);
        });
        connect(socket,&QTcpSocket::errorOccurred,this,[this](auto error){if(error!=QAbstractSocket::RemoteHostClosedError)fail();});
        if(!socket->bind(proxyAddress,0))throw std::runtime_error("rtsp local bind");
        socket->connectToHost(mediaAddress,FirstPort);
        QTimer::singleShot(5000,socket,[this,socket]{if(tcp==socket)fail();});
    }
    QSocketNotifier input,output;QTimer deadline,shutdownTimer,candidateTimer;QByteArray in,out;
    QJsonArray localCandidates,remoteCandidates;int localCandidateCount=0,remoteCandidateCount=0;bool localDescription=false;
    QTemporaryDir directory{QDir::tempPath()+"/traineros-stream-XXXXXX"};
    std::shared_ptr<rtc::PeerConnection> pc;
    std::array<std::shared_ptr<rtc::DataChannel>,3> channels;
    std::atomic<bool> stopping{false};std::atomic<qsizetype> pending{0};
    std::array<bool,3> opened{};std::array<QUdpSocket,3> udp;std::array<quint16,3> peerPorts{};
    QHostAddress mediaAddress,proxyAddress;
    QTcpServer tcpServer;QPointer<QTcpSocket> tcp;QByteArray tcpBytes,key,ping;uint32_t token=0;
    bool host=false,remoteDescription=false,greeted=false,verified=false,ready=false,requestSent=false,materialSent=false,closing=false;
    QString identity;int seconds=0;bool game=false;quint64 sent=0,received=0,dropped=0;
};
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    signal(SIGPIPE,SIG_IGN);
    signal(SIGTERM,[](int){interrupted=1;});signal(SIGINT,[](int){interrupted=1;});
    rtc::InitLogger(rtc::LogLevel::None); // No endpoint/SDP/credential logging.
    Bridge bridge;return app.exec();
}
