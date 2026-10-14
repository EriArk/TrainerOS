#include "RemotePlayRuntime.h"
#include "features/social/SocialController.h"
#include "integrations/adventure/retroarch/RetroArchRemotePlay.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <utility>
#ifdef Q_OS_LINUX
#include <unistd.h>
#include <fcntl.h>
#endif

namespace trainer {
namespace {
QString identity(const QJsonObject& profile) {
    return QString::fromLatin1(QCryptographicHash::hash("remoteplay.v1/"+QJsonDocument(profile).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
}
}
RemotePlayRuntime::RemotePlayRuntime(LibraryRepository& library,RetroArchAdapter& adapter,SocialController& social,
        ProcessService& process,AdventureLaunchController& lifecycle,LobbyGamePreparation& preparation,QObject* parent)
    :LobbyRuntimeService(parent),library_(library),adapter_(adapter),social_(social),process_(process),lifecycle_(lifecycle),preparation_(preparation),connection_(*social.lobbyCoordinator(),this) {
    connect(&connection_,&LobbyConnection::connected,this,&RemotePlayRuntime::connected);
    connect(&connection_,&LobbyConnection::ended,this,&RemotePlayRuntime::failed);
    connect(&connection_,&LobbyConnection::mediaReady,this,[this]{
        if(!host_&&ownedPid_&&process_.processId()==ownedPid_&&!announced_) {
            announced_=true;deadline_.stop();
            if(!process_.runtimeCommand("remoteplay-start"))failed("Couldn't start Remote Play.");
        }
    });
    connect(social_.lobbyCoordinator(),&LobbyCoordinator::changed,this,&RemotePlayRuntime::reconcile);
    connect(&lifecycle_,&AdventureLaunchController::changed,this,[this]{
        if(opening_&&lifecycle_.state()=="failed")failed(lifecycle_.error());
    });
    connect(&lifecycle_,&AdventureLaunchController::adventureStarted,this,[this](QString id){
        if(!opening_||id!=game_)return;
        opening_=false;ownedPid_=process_.processId();started_=true;
        const auto operation=operation_;
        QTimer::singleShot(0,this,[this,operation]{if(operation==operation_){complete(true);reconcile();}});
    });
    connect(&lifecycle_,&AdventureLaunchController::adventureFinished,this,[this](bool){
        if(!started_)return;
        started_=false;ownedPid_=0;const auto ticket=ticket_;
        disconnectMedia();connection_.stop();
        QTimer::singleShot(0,this,[this,ticket]{if(!ticket.isEmpty())social_.lobbyRuntime()->finished(ticket);});
    });
    connect(&media_,&QProcess::readyReadStandardOutput,this,&RemotePlayRuntime::mediaOutput);
    connect(&media_,&QProcess::readyReadStandardError,this,[this]{media_.readAllStandardError();});
    connect(&media_,&QProcess::finished,this,[this]{if(!stopping_&&!ticket_.isEmpty())failed("The Remote Play stream ended. Your game is kept.");});
    connect(&media_,&QProcess::errorOccurred,this,[this](auto error){if(error==QProcess::FailedToStart)failed("Couldn't open the Remote Play stream.");});
    deadline_.setSingleShot(true);deadline_.setInterval(45000);
    connect(&deadline_,&QTimer::timeout,this,[this]{failed("Remote Play didn't become ready. Prepare again.");});
}
RemotePlayRuntime::~RemotePlayRuntime(){disconnectMedia();connection_.stop();}
bool RemotePlayRuntime::configure(const QString& directory) {
    if(started_||!operation_.isEmpty())return false;
    root_.clear();ice_={};
#ifdef Q_OS_LINUX
    const QFileInfo base(directory);if(!base.isDir()||base.isSymLink()||!base.isAbsolute())return false;
    QFile file(directory+"/manifest.json");if(!file.open(QIODevice::ReadOnly)||file.size()>16384)return false;
    const auto manifest=QJsonDocument::fromJson(file.readAll()).object();if(manifest["recipe"]!="remoteplay.v1")return false;
    const auto files=manifest["files"].toObject();
    const QStringList required{"stream_host","stream_guest","stream_transport","libmoonlight-common-c.so","run-game.py","capture-game.py","capture-game-audio.py","lobby_lease.py"};
    if(files.size()!=required.size())return false;
    for(const auto& name:required) {
        const QFileInfo info(directory+"/"+name);QFile content(info.absoluteFilePath());
        if(!info.isFile()||info.isSymLink()||info.size()>64*1024*1024||!content.open(QIODevice::ReadOnly))return false;
        if(QString::fromLatin1(QCryptographicHash::hash(content.readAll(),QCryptographicHash::Sha256).toHex())!=files[name].toString())return false;
    }
    // Public address discovery only. Media/input use the selected direct ICE
    // path; there is no TURN account, relay allocation or owner-server traffic.
    // Same default as the upstream WebRTC Trickle ICE sample. Local candidates
    // remain available when public STUN is unreachable.
    ice_={"stun:stun.l.google.com:19302"};
    root_=base.canonicalFilePath();return true;
#else
    Q_UNUSED(directory);return false;
#endif
}
QVariantList RemotePlayRuntime::choices(const QMap<QString,QJsonObject>& profiles) {
    games_.clear();QVariantList rows;if(root_.isEmpty())return rows;
    for(auto it=profiles.cbegin();it!=profiles.cend();++it) {
        if(!hostSupported||!hostSupported(it.value()))continue;
        const auto record=library_.registration(it.key());if(!record||record->removed||!record->contentAvailable)continue;
        games_.insert(it.key(),it.value());
        rows.append(QVariantMap{{"key",it.key()+":remoteplay"},{"adventure",it.key()},{"id",identity(it.value())},
            {"title",record->adventure.title},{"platform",record->adventure.platformId},{"mode","remoteplay.v1"},
            {"modeLabel","Remote Play"},{"seats",2},{"runtime",it.value().toVariantMap()}});
    }
    return rows;
}
bool RemotePlayRuntime::validPlayers(const QJsonObject& context) const {
    int count=0;bool owner=false,guest=false;
    for(const auto& value:context["members"].toArray()) {
        const auto m=value.toObject();if(m["spectator"].toBool())continue;
        ++count;if(!m["connected"].toBool())return false;
        if(m["id"]==context["owner"]&&m["seat"]==1)owner=true;
        else if(m["id"]!=context["owner"]&&m["seat"]==2)guest=true;else return false;
    }
    return count==2&&owner&&guest;
}
bool RemotePlayRuntime::hostUnchanged() const {
    if(!host_)return true;
    const auto record=library_.registration(game_);
    return record&&!record->removed&&record->contentAvailable&&record->revision==revision_&&record->contentPath==contentPath_
        &&games_.value(game_)==profile_&&hostSupported&&hostSupported(profile_)
        &&ownerDevice&&ownerDevice()==device_;
}
void RemotePlayRuntime::prepare(QString operation,QJsonObject context,Completion done) {
    const auto game=context["game"].toObject();const auto profile=game["runtime"].toObject();
    if(root_.isEmpty()||!allowed||!allowed()||started_||connection_.active()||!operation_.isEmpty()
       ||context["self"]!=social_.account()["userId"].toString()||context.contains("liveAdmission")
       ||game["mode"]!="remoteplay.v1"||game["seats"]!=2||game["id"]!=identity(profile)||!validPlayers(context)) {
        done(false,"Remote Play needs the host on P1 and one connected guest on P2.");return;
    }
    host_=context["self"]==context["owner"];game_.clear();profile_=profile;context_=context;account_=social_.accountGeneration();
    if(host_) {
        for(auto it=games_.cbegin();it!=games_.cend();++it)if(it.value()==profile){game_=it.key();break;}
        const auto record=library_.registration(game_);device_=ownerDevice?ownerDevice():QString();
        if(!record||retroarch::remotePlayPortSettings({device_}).isEmpty()) {done(false,"Choose a supported game and a local controller for P1.");return;}
        revision_=record->revision;contentPath_=record->contentPath;
    } else game_="remoteplay:"+game["id"].toString(); // No ROM or guest save is opened.
    operation_=operation;
    preparation_.prepare(operation,[this,operation,done](bool ok,QString error){
        if(operation!=operation_)return;
        if(ok&&(account_!=social_.accountGeneration()||!allowed||!allowed()||!hostUnchanged())){ok=false;error="The game or account changed. Prepare again.";}
        done(ok,error);
    });
}
void RemotePlayRuntime::launch(QString operation,QString ticket,QJsonObject context,Completion done) {
    if(operation!=operation_||lifecycle_.active()||!allowed||!allowed()||!hostUnchanged()||account_!=social_.accountGeneration()
       ||context["id"]!=context_["lobby"]||context["generation"]!=context_["generation"]||context["game"]!=context_["game"]
       ||context["ticket"]!=ticket||context["phase"]!="launching"||!validPlayers(context)) {done(false,"The game context changed. Prepare again.");return;}
    ticket_=ticket;completion_=std::move(done);deadline_.start();
    if(!connection_.start(context_["self"].toString(),ticket,root_+"/stream_transport",ice_))failed("Couldn't prepare the Remote Play connection.");
}
void RemotePlayRuntime::connected(QString config) {
    if(operation_.isEmpty()||account_!=social_.accountGeneration()||!hostUnchanged()){failed("The game context changed. Prepare again.");return;}
    config_=config;connectionId_=connection_.id();opening_=true;
    AdventureResult result{false,"Remote Play launch is unavailable."};
    if(host_) {
        const auto record=library_.registration(game_);
        if(record)result=adapter_.launchRemotePlay(record->adventure,{device_});
    } else if(requestGuestLaunch) {
        ProcessCommand command;command.program="/usr/bin/env";
        command.arguments={"LD_LIBRARY_PATH="+root_,"TRAINEROS_REMOTE_PLAY_MANAGED=1",root_+"/stream_guest",config_};
        command.runtimeControls={{"kind","remoteplay"},{"game",game_},{"temporaryProgress",true},{"recordHistory",false},{"title",context_["game"].toObject()["title"].toString()}};
        result=requestGuestLaunch(command,game_);
    }
    if(!result.success){opening_=false;failed(result.message);}
}
void RemotePlayRuntime::complete(bool ok,QString error) {auto done=std::exchange(completion_,{});if(done)done(ok,error);}
void RemotePlayRuntime::reconcile() {
    if(host_&&started_&&media_.state()==QProcess::NotRunning&&!config_.isEmpty()&&connection_.active()
       &&social_.lobbyCoordinator()->state()["phase"]=="playing"&&lifecycle_.state()=="running")capture();
}
void RemotePlayRuntime::capture() {
#ifdef Q_OS_LINUX
    if(process_.processId()!=ownedPid_||process_.runtimeControls()["remotePlay"].toMap()["port"]!=2){failed("The host controller binding changed.");return;}
    auto input=social_.lobbyRuntime()->input();
    if(!input->authorize(connection_.peer(),connectionId_,ticket_,2)){failed("The guest no longer owns P2.");return;}
    const int fd=input->takeDescriptor(connectionId_);if(fd<0){failed("Couldn't reserve P2.");return;}
    auto env=QProcessEnvironment::systemEnvironment();env.insert("TRAINEROS_LOBBY_INPUT_FD","3");env.insert("LD_LIBRARY_PATH",root_);media_.setProcessEnvironment(env);
    media_.setChildProcessModifier([fd]{if(fd!=3)dup2(fd,3);fcntl(3,F_SETFD,0);});
    stopping_=false;output_.clear();
    // QProcess's child modifier executes before started/FailedToStart. Retain
    // the descriptor until that point; the child owns its dup, not a public path.
    const auto closeFd=std::make_shared<int>(fd);
    const auto release=[closeFd]{if(*closeFd>=0){::close(*closeFd);*closeFd=-1;}};
    connect(&media_,&QProcess::started,this,release,Qt::SingleShotConnection);
    connect(&media_,&QProcess::errorOccurred,this,[release](auto){release();},Qt::SingleShotConnection);
    media_.start("/usr/bin/python3",{root_+"/run-game.py",QString::number(ownedPid_),root_+"/stream_host",config_});
#endif
}
void RemotePlayRuntime::mediaOutput() {
    output_+=media_.readAllStandardOutput();if(output_.size()>8192){failed("Invalid Remote Play response.");return;}
    while(output_.contains('\n')) {
        const auto at=output_.indexOf('\n');const auto object=QJsonDocument::fromJson(output_.left(at)).object();output_.remove(0,at+1);
        if(object==QJsonObject{{"event","ready"}}&&!announced_) {
            announced_=true;deadline_.stop();if(!connection_.announceMedia())failed("Couldn't reach the guest.");
        }
    }
}
void RemotePlayRuntime::disconnectMedia() {
    stopping_=true;deadline_.stop();social_.lobbyRuntime()->input()->release(connectionId_);
    if(media_.state()!=QProcess::NotRunning&&!media_.waitForFinished(4000)) {
        media_.terminate();if(!media_.waitForFinished(4000)){media_.kill();media_.waitForFinished(1000);}
    }
#ifdef Q_OS_LINUX
    media_.setChildProcessModifier({});
#endif
    config_.clear();connectionId_.clear();output_.clear();announced_=false;
}
void RemotePlayRuntime::failed(QString error) {
    if(stopping_&&operation_.isEmpty())return;
    disconnectMedia();connection_.stop();complete(false,error);
    if(!host_&&ownedPid_&&process_.processId()==ownedPid_)process_.runtimeCommand("remoteplay-close");
    if(!error.isEmpty())emit notice(error);
}
void RemotePlayRuntime::cancel(QString operation) {
    if(operation!=operation_)return;
    preparation_.cancel(operation);operation_.clear();completion_={};
    if(!host_&&opening_&&lifecycle_.adventureId()==game_&&lifecycle_.active())lifecycle_.cancel();
    opening_=false;
    disconnectMedia();connection_.stop();
    if(!host_&&ownedPid_&&process_.processId()==ownedPid_)process_.runtimeCommand("remoteplay-close");
    // The host continues its owned game and ordinary protected save lifecycle.
    ticket_.clear();
}
}
