#include "RmgNetplay.h"
#include "core/model/GamePlayers.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace trainer::rmg {
namespace {
QString digest(const QString& path,const std::atomic_bool& cancel,qint64 limit) {
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()<=0||f.size()>limit)return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while(!f.atEnd()){if(cancel)return {};const auto bytes=f.read(1024*1024);if(bytes.isEmpty())return {};hash.addData(bytes);}
    return QString::fromLatin1(hash.result().toHex());
}
bool write(const QString& path,const QByteArray& bytes) {
    QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();
}
bool hex(const QString& s,int length) {
    return s.size()==length&&QRegularExpression("^[a-f0-9]+$").match(s).hasMatch();
}
QString roomId(const QString& token) {
    // Room/player names are published by the upstream lobby. Never put the
    // invitation password in those names or in ordinary server log fields.
    return QString::fromLatin1(QCryptographicHash::hash(token.toLatin1(),QCryptographicHash::Sha256).toHex().left(24));
}
bool serverUrl(const QUrl& url) {
    return url.isValid()&&!url.host().isEmpty()&&url.userInfo().isEmpty()&&url.fragment().isEmpty()
        &&url.query().isEmpty()&&url.port(45000)>0&&url.port(45000)<=65535
        &&(url.scheme()=="wss"||(url.scheme()=="ws"&&!QHostAddress(url.host()).isNull()));
}
}
QString bundleRoot(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/TrainerOS/emulators/rmg-netplay";}
QJsonObject bundle(const std::atomic_bool& cancel) {
    const auto root=bundleRoot();QFile file(root+"/manifest.json");
    if(!file.open(QIODevice::ReadOnly)||file.size()>65536)return {};
    auto m=QJsonDocument::fromJson(file.readAll()).object();
    if(m["protocol"]!=1||m["source"]!=sourceRevision||m["serverSource"]!=serverRevision)return {};
    // Nearby owns its local server. An operator endpoint enables Online
    // separately; its absence must not disable an installed Nearby runtime.
    const QUrl online(m.value("onlineServer").toString());
    if(m.contains("onlineServer")&&!m.value("onlineServer").isString())return {};
    if(!online.isEmpty()&&(!serverUrl(online)||online.scheme()!="wss"))return {};
    const auto files=m["files"].toObject();
    if(files.size()<7||files.size()>512)return {};
    for(const auto& required:{"bin/RMG","bin/rmg-netplay-server","lib/libRMG-Core.so",
        "lib/RMG/Core/libmupen64plus.so","lib/RMG/Plugin/Input/RMG-Input.so","lib/RMG/Plugin/Audio/RMG-Audio.so",
        "lib/RMG/Plugin/RSP/mupen64plus-rsp-hle.so","lib/RMG/Plugin/GFX/mupen64plus-video-GLideN64.so",
        "lib/RMG/Plugin/RSP/mupen64plus-rsp-parallel.so","lib/RMG/Plugin/GFX/mupen64plus-video-parallel.so",
        "share/RMG/mupen64plus.ini","share/RMG/gamecontrollerdb.txt","share/RMG/InputProfileDB.json"})
        if(!files.contains(required))return {};
    if(m["renderer"]!="vulkan"&&m["renderer"]!="opengl")return {};
    QCryptographicHash aggregate(QCryptographicHash::Sha256);
    const auto canonical=QFileInfo(root).canonicalFilePath()+"/";
    for(auto it=files.constBegin();it!=files.constEnd();++it) {
        const auto relative=it.key();const QFileInfo f(root+"/"+relative);
        if(relative.contains('\\')||relative.contains("..")||QDir::isAbsolutePath(relative)
           ||!f.canonicalFilePath().startsWith(canonical)||!hex(it.value().toString(),64)
           ||digest(f.filePath(),cancel,256*1024*1024)!=it.value().toString())return {};
        aggregate.addData(relative.toUtf8());aggregate.addData(QByteArray(1,'\0'));aggregate.addData(it.value().toString().toLatin1());
    }
#ifdef Q_OS_UNIX
    // The shipped bundle is Linux-only. Windows source checks can fingerprint
    // it but cannot interpret its Unix execute bits (extensionless ELF files).
    if(!QFileInfo(root+"/bin/RMG").isExecutable()||!QFileInfo(root+"/bin/rmg-netplay-server").isExecutable())return {};
#endif
    m["digest"]=QString::fromLatin1(aggregate.result().toHex());return m;
}
QJsonObject netplayIdentity(const AdventureRegistration& r,const std::atomic_bool& cancel,int players) {
    if(r.adventure.platformId!="n64"||r.adventure.adapterId!="retroarch"||players<2||players>4)return {};
    QFile rom(r.contentPath);if(!rom.open(QIODevice::ReadOnly)||rom.size()<4096||rom.size()>64*1024*1024||rom.size()%4)return {};
    // All three cartridge byte orders are accepted, but exact bytes must match.
    const auto magic=rom.read(4).toHex();
    if(magic!="80371240"&&magic!="37804012"&&magic!="40123780")return {};
    const auto m=bundle(cancel);if(m.isEmpty())return {};
    const auto content=digest(r.contentPath,cancel,64*1024*1024);if(content.isEmpty())return {};
    QJsonArray connections{"nearby"};
    if(!m["onlineServer"].toString().isEmpty())connections.append("online");
    return {{"id","runtime.rmg.n64.v1"},{"label",r.adventure.title.left(96)},
        {"content",content},{"runtime",m["digest"]},{"settings","rmg-native-v1-private-progress-"+m["renderer"].toString()},
        {"players",players},{"runtimePlayers",4},{"lateJoin",false},{"connections",connections},{"temporaryProgress",true}};
}
ProcessCommand command(const AdventureRegistration& r,const QJsonObject& expected,const QString& address,
                       int port,int controller,const QStringList& inputPrefix) {
    ProcessCommand cmd;
    cmd.runtimeControls={{"kind","rmg"},{"game",r.adventure.id},{"netplay",expected.toVariantMap()},
                         {"temporaryProgress",true}};
    cmd.prepare=[r,expected,address,port,controller,inputPrefix](ProcessCommand& out,const std::atomic_bool& cancel)->QString {
        if(QHostAddress(address).isNull()||port<1||port>65535||controller<1||controller>4)
            return "This Nintendo 64 connection is invalid.";
        const auto identity=netplayIdentity(r,cancel,expected["players"].toInt());
        if(!sameMultiplayerGame(identity,expected))return "Your Nintendo 64 game or emulator changed. Invite again.";
        auto dir=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/trainer-rmg-XXXXXX");
        if(!dir->isValid())return "Couldn't prepare temporary Nintendo 64 progress.";
        const auto root=dir->path();
        if(!QDir().mkpath(root+"/config/RMG")||!QDir().mkpath(root+"/data")||!QDir().mkpath(root+"/cache"))
            return "Couldn't prepare temporary Nintendo 64 progress.";
        const QJsonObject session{{"protocol",1},{"address",address},{"port",port},{"controller",controller},
            {"renderer",expected["settings"].toString().endsWith("-vulkan")?"vulkan":"opengl"}};
        // Fresh XDG directories: no ordinary config, SRAM, packs, states, cheats
        // or per-game overrides can be imported or written back by this route.
        if(!write(root+"/session.json",QJsonDocument(session).toJson(QJsonDocument::Compact)))return "Couldn't prepare Nintendo 64 play.";
        out.program="/usr/bin/env";
        out.arguments={"XDG_CONFIG_HOME="+root+"/config","XDG_DATA_HOME="+root+"/data",
            "XDG_CACHE_HOME="+root+"/cache","TRAINEROS_RMG_SESSION="+root+"/session.json"};
        out.arguments+=inputPrefix;
        out.arguments<<bundleRoot()+"/bin/RMG"<<"--lib-path"<<bundleRoot()+"/lib/RMG"
            <<"--core-path"<<bundleRoot()+"/lib/RMG/Core"<<"--plugin-path"<<bundleRoot()+"/lib/RMG/Plugin"
            <<"--shared-data-path"<<bundleRoot()+"/share/RMG"<<"-f"<<"-n"<<"-q"<<r.contentPath;
        out.settled=[dir](const ProcessOutcome&){};return {};
    };
    return cmd;
}
Lobby::Lobby(QObject* parent):QObject(parent) {
    deadline_.setSingleShot(true);deadline_.setInterval(60000);
    retry_.setSingleShot(true);retry_.setInterval(150);
    connect(&deadline_,&QTimer::timeout,this,&Lobby::fail);
    connect(&retry_,&QTimer::timeout,this,&Lobby::open);
}
Lobby::~Lobby(){stop();}
void Lobby::stop() {
    active_=false;deadline_.stop();retry_.stop();
    if(socket_){socket_->disconnect(this);socket_->abort();socket_->deleteLater();socket_=nullptr;}
    if(server_){server_->disconnect(this);server_->kill();server_->waitForFinished(1000);server_->deleteLater();server_=nullptr;}
    request_={};expected_.clear();roster_.clear();name_.clear();admitted_=begin_=running_=false;controller_=0;
}
void Lobby::fail(){if(!active_)return;stop();emit failed("Couldn't connect the Nintendo 64 game. Invite your friends again.");}
void Lobby::start(const Request& request) {
    stop();request_=request;active_=true;
    if(!serverUrl(request.server)||!hex(request.token,32)||request.seats.size()<2||request.seats.size()>4
       ||request.identity["id"]!="runtime.rmg.n64.v1"||!hex(request.identity["content"].toString(),64)
       ||!hex(request.identity["runtime"].toString(),64)||request.host!=(request.slot==1)
       ||(!request.host&&(request.port<1||request.port>65535))){fail();return;}
    for(const auto& seat:request.seats) {
        const int slot=seat.toInt();const auto name=roomId(request.token)+"-"+QString::number(slot);
        if(slot<1||slot>4||expected_.contains(name)){fail();return;}expected_.append(name);
    }
    name_=roomId(request.token)+"-"+QString::number(request.slot);
    if(!expected_.contains(name_)||!expected_.contains(roomId(request.token)+"-1")){fail();return;}
    deadline_.start();
    if(!request.localServer.isEmpty()) {
        if(!request.host||!QHostAddress(request.server.host()).isLoopback()||request.server.port()!=45000){fail();return;}
        server_=new QProcess(this);
        server_->setStandardOutputFile(QProcess::nullDevice());server_->setStandardErrorFile(QProcess::nullDevice());
        connect(server_,&QProcess::errorOccurred,this,[this]{fail();});
        connect(server_,&QProcess::finished,this,[this]{fail();});
        server_->start(request.localServer,{"--name","TrainerOS","--disable-broadcast","--verify-ip","--close-on-finish","--timeout","2"});
        retry_.start();
    }else open();
}
void Lobby::open() {
    if(!active_)return;
    if(socket_){socket_->disconnect(this);socket_->abort();socket_->deleteLater();}
    socket_=new QWebSocket(QString(),QWebSocketProtocol::VersionLatest,this);
    socket_->setMaxAllowedIncomingMessageSize(16384);socket_->setMaxAllowedIncomingFrameSize(16384);
    connect(socket_,&QWebSocket::connected,this,[this]{
        QJsonObject room{{"port",request_.port},{"password",request_.token},{"MD5",request_.identity["content"]},
            {"room_name","TrainerOS-"+roomId(request_.token)},{"game_name","Nintendo 64"},{"features",QJsonObject{}}};
        send(request_.host?"request_create_room":"request_join_room",room);
    });
    connect(socket_,&QWebSocket::textMessageReceived,this,&Lobby::receive);
    connect(socket_,&QWebSocket::disconnected,this,[this]{if(active_)fail();});
    connect(socket_,&QWebSocket::errorOccurred,this,[this]{
        if(server_&&!admitted_&&socket_->error()==QAbstractSocket::ConnectionRefusedError){retry_.start();return;}
        fail();
    });
    socket_->open(request_.server);
}
void Lobby::send(QString type,QJsonObject room) {
    if(!active_||!socket_)return;
    if(room.isEmpty())room={{"port",request_.port}};
    const auto time=QByteArray::number(QDateTime::currentMSecsSinceEpoch());
    const QJsonObject packet{{"type",type},{"room",room},{"player_name",name_},
        {"client_sha",request_.identity["runtime"]},{"emulator","RMG"},{"netplay_version",17},
        {"authTime",QString::fromLatin1(time)},
        {"auth",QString::fromLatin1(QCryptographicHash::hash(time+"RMG",QCryptographicHash::Sha256).toHex())}};
    socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(packet).toJson(QJsonDocument::Compact)));
}
void Lobby::receive(const QString& text) {
    if(!active_||text.size()>16384)return;
    const auto packet=QJsonDocument::fromJson(text.toUtf8()).object();const auto type=packet["type"].toString();
    if(type=="reply_create_room"||type=="reply_join_room") {
        const auto room=packet["room"].toObject();
        if(admitted_||type!=(request_.host?"reply_create_room":"reply_join_room")||!packet.contains("accept")
           ||packet["accept"].toInt(-1)!=0||packet["player_name"]!=name_||room["MD5"]!=request_.identity["content"]
           ||room["room_name"]!="TrainerOS-"+roomId(request_.token)||!room["protected"].toBool()
           ||!room["features"].toObject().isEmpty()||room["port"].toInt()<1||room["port"].toInt()>65535
           ||(!request_.host&&room["port"].toInt()!=request_.port)){fail();return;}
        request_.port=room["port"].toInt();admitted_=true;
        send("request_players");if(request_.host)emit roomReady(request_.port);
    }else if(type=="reply_players"&&admitted_) {
        const auto names=packet["player_names"].toArray();
        if(names.size()!=4){fail();return;}
        QStringList roster;int controller=0;
        for(int i=0;i<4;++i) {
            if(!names[i].isString()){fail();return;}const auto name=names[i].toString();if(name.isEmpty())continue;
            if(!expected_.contains(name)||roster.contains(name)){fail();return;}
            roster.append(name);if(name==name_)controller=i+1;
        }
        if(!controller||(running_&&roster!=roster_)){fail();return;}
        roster_=roster;controller_=controller;
        if(request_.host&&!begin_&&roster.size()==expected_.size()){begin_=true;send("request_begin_game");}
    }else if(type=="reply_begin_game") {
        if(!admitted_||running_||!packet.contains("accept")||packet["accept"].toInt(-1)!=0
           ||roster_.size()!=expected_.size()||controller_==0){fail();return;}
        running_=true;deadline_.stop();emit launchReady(socket_->peerAddress().toString(),request_.port,controller_);
    }else if(type=="reply_edit_room")fail(); // settings/cheats cannot change after consent
}
}
