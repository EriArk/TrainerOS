#include "RemotePlayRuntime.h"
#include "features/social/SocialController.h"
#include "integrations/adventure/retroarch/RetroArchRemotePlay.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QUuid>
#include <utility>
#ifdef Q_OS_LINUX
#include <unistd.h>
#include <fcntl.h>
#endif

namespace trainer {
namespace {
int capacity(const QJsonObject& profile) {
    const auto id=profile["id"].toString();const int players=profile["players"].toInt(2);
    const bool four=id.contains("fceumm")||id.contains("snes9x");
    return players<2?0:qMin(players,four?4:2);
}
retroarch::RemotePlayRequest ports(const QString& device,const QJsonObject& profile) {
    const int count=capacity(profile);return {device,count,count>2&&profile["id"].toString().contains("snes9x")};
}
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
        mediaReady_=true;startGuestMedia();
    });
    connect(social_.lobbyCoordinator(),&LobbyCoordinator::changed,this,&RemotePlayRuntime::reconcile);
    connect(&lifecycle_,&AdventureLaunchController::changed,this,[this]{
        if(opening_&&lifecycle_.state()=="failed")failed(lifecycle_.error());
    });
    connect(&lifecycle_,&AdventureLaunchController::adventureStarted,this,[this](QString id){
        if(!opening_||id!=game_)return;
        opening_=false;ownedPid_=process_.processId();started_=true;if(host_)deadline_.stop();
        const auto operation=operation_;
        QTimer::singleShot(0,this,[this,operation]{if(operation==operation_){complete(true);startGuestMedia();reconcile();}});
    });
    connect(&lifecycle_,&AdventureLaunchController::adventureFinished,this,[this](bool){
        if(!started_)return;
        started_=false;ownedPid_=0;const auto ticket=ticket_;
        disconnectMedia();connection_.stop();
        QTimer::singleShot(0,this,[this,ticket]{if(!ticket.isEmpty())social_.lobbyRuntime()->finished(ticket);});
    });
    connect(&media_,&RemotePlayHost::ready,this,[this](QString id){
        for(const auto& name:peers_.keys()) {auto peer=peers_.value(name);if(peer->connection&&peer->connection->id()==id) {
            peer->deadline->stop();
            if(!peer->connection->announceMedia())peerFailed(name,peer,"Couldn't reach a Remote Play guest.");
            return;
        }}
    });
    connect(&media_,&RemotePlayHost::ended,this,[this](QString id){
        for(const auto& name:peers_.keys()) {auto peer=peers_.value(name);if(peer->connection&&peer->connection->id()==id) {
            peerFailed(name,peer,"A Remote Play guest disconnected. Your game is kept.");return;
        }}
    });
    connect(&media_,&RemotePlayHost::failed,this,&RemotePlayRuntime::failed);
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
    const QStringList required{"stream_host","stream_guest","stream_transport","libmoonlight-common-c.so","run-game.py","capture-game.py","capture-game-audio.py","lobby_lease.py","run-group.py"};
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
        if(!hostSupported||!hostSupported(it.value())||!capacity(it.value()))continue;
        const auto record=library_.registration(it.key());if(!record||record->removed||!record->contentAvailable)continue;
        games_.insert(it.key(),it.value());
        rows.append(QVariantMap{{"key",it.key()+":remoteplay"},{"adventure",it.key()},{"id",identity(it.value())},
            {"title",record->adventure.title},{"platform",record->adventure.platformId},{"mode","remoteplay.v1"},
            {"modeLabel","Remote Play"},{"cover",library_.artwork(it.key()).value("cover")},{"seats",capacity(it.value())},{"runtime",it.value().toVariantMap()}});
    }
    return rows;
}
bool RemotePlayRuntime::validPlayers(const QJsonObject& context) const {
    const int seatCount=context["game"].toObject()["seats"].toInt();QSet<int> seats;bool owner=false;
    for(const auto& value:context["members"].toArray()) {
        const auto m=value.toObject();if(m["spectator"].toBool())continue;
        const int seat=m["seat"].toInt();
        if(!m["connected"].toBool()||seat<1||seat>seatCount||seats.contains(seat))return false;
        seats.insert(seat);if(m["id"]==context["owner"]&&seat==1)owner=true;
    }
    return owner&&seats.size()>=2&&seatCount>=2&&seatCount<=4;
}
bool RemotePlayRuntime::hostUnchanged() const {
    if(!host_)return true;
    const auto record=library_.registration(game_);
    return record&&!record->removed&&record->contentAvailable&&record->revision==revision_&&record->contentPath==contentPath_
        &&games_.value(game_)==profile_&&hostSupported&&hostSupported(profile_)
        &&ownerDevice&&ownerDevice()==device_;
}
bool RemotePlayRuntime::supportsLive(const QJsonObject& profile) const {
    return !root_.isEmpty()&&capacity(profile)>0&&allowed&&allowed();
}
RemotePlayRuntime::PeerPtr RemotePlayRuntime::addPeer(const QString& name,const QString& admission,int port) {
    if(peers_.contains(name)||peers_.size()>=3)return {};
    for(const auto& other:peers_)if(other->port==port)return {};
    auto peer=std::make_shared<Peer>();peer->connection=new LobbyConnection(*social_.lobbyCoordinator(),this);
    peer->deadline=new QTimer(this);peer->deadline->setSingleShot(true);peer->deadline->setInterval(45000);
    peer->admission=admission;peer->port=port;peers_.insert(name,peer);
    connect(peer->connection,&LobbyConnection::connected,this,[this,name,peer](QString config){hostConnected(name,peer,config);});
    connect(peer->connection,&LobbyConnection::ended,this,[this,name,peer](QString error){peerFailed(name,peer,error);});
    connect(peer->deadline,&QTimer::timeout,this,[this,name,peer]{peerFailed(name,peer,"A guest couldn't connect. Your game is kept.");});
    peer->deadline->start();return peer;
}
bool RemotePlayRuntime::admitLive(QString ticket,QString peer,QString operation,int seat) {
    if(!host_||!started_||opening_||failing_||ticket!=ticket_||seat<2||seat>capacity(profile_)||peer.isEmpty()||QUuid(operation).isNull()
       ||!supportsLive(profile_)||account_!=social_.accountGeneration()||!hostUnchanged()
       ||lifecycle_.state()!="running"||process_.processId()!=ownedPid_||!ownedPid_
       ||process_.runtimeControls()["remotePlay"].toMap()["ports"]!=capacity(profile_))return false;
    return bool(addPeer(peer,operation,seat));
}
void RemotePlayRuntime::releaseLive(QString name,QString operation) {
    auto peer=peers_.value(name);if(!host_||!peer||peer->admission!=operation)return;
    peerFailed(name,peer,{});
}
void RemotePlayRuntime::peerFailed(const QString& name,const PeerPtr& peer,QString error) {
    if(peers_.value(name)!=peer)return;
    if(social_.lobbyCoordinator()->state()["phase"]=="launching") {failed(error);return;}
    peers_.remove(name);peer->deadline->stop();peer->deadline->deleteLater();
    const auto id=peer->connection->id();social_.lobbyRuntime()->input()->release(id);
    media_.remove(id);peer->connection->stop();peer->connection->deleteLater();
    if(host_&&started_)social_.lobbyCoordinator()->releaseRemotePlayer(name,ticket_,peer->admission);
    if(!error.isEmpty())emit notice(error);
}
void RemotePlayRuntime::prepare(QString operation,QJsonObject context,Completion done) {
    problem_.clear();
    const auto game=context["game"].toObject();const auto profile=game["runtime"].toObject();
    const bool live=context.contains("liveAdmission");bool players=validPlayers(context);
    if(live) {
        bool owner=false,guest=false;int count=0;
        for(const auto& value:context["members"].toArray()) {
            const auto m=value.toObject();++count;
            if(m["id"]==context["owner"])owner=m["connected"].toBool()&&!m["spectator"].toBool()&&m["seat"]==1;
            else if(m["id"]==context["self"])guest=m["connected"].toBool()&&m["spectator"].toBool()&&m["seat"]==0;
        }
        players=count==2&&owner&&guest&&context["liveAdmission"]==operation&&!QUuid(operation).isNull();
    }
    if(root_.isEmpty()||!allowed||!allowed()||started_||connection_.active()||!operation_.isEmpty()
       ||context["self"]!=social_.account()["userId"].toString()
       ||game["mode"]!="remoteplay.v1"||!capacity(profile)||game["seats"]!=capacity(profile)||game["id"]!=identity(profile)||!players) {
        done(false,"Remote Play needs the host on P1 and connected guests in separate controller seats.");return;
    }
    host_=context["self"]==context["owner"];game_.clear();profile_=profile;context_=context;account_=social_.accountGeneration();
    if(host_) {
        for(auto it=games_.cbegin();it!=games_.cend();++it)if(it.value()==profile){game_=it.key();break;}
        const auto record=library_.registration(game_);device_=ownerDevice?ownerDevice():QString();
        if(!record||record->removed||!record->contentAvailable){problem_="local-content";done(false,"The host game is missing. Choose your local file to verify it.");return;}
        if(retroarch::remotePlayPortSettings(ports(device_,profile_)).isEmpty()) {done(false,"Choose a supported game and a local controller for P1.");return;}
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
    const bool live=context_.contains("liveAdmission");bool admitted=!live;
    for(const auto& value:context["members"].toArray()) {
        const auto m=value.toObject();if(live&&m["id"]==context_["self"]&&m["admission"]==operation)admitted=true;
    }
    if(operation!=operation_||lifecycle_.active()||!allowed||!allowed()||!hostUnchanged()||account_!=social_.accountGeneration()
       ||context["id"]!=context_["lobby"]||context["generation"]!=context_["generation"]||context["game"]!=context_["game"]
       ||context["ticket"]!=ticket||context["phase"]!=(live?"playing":"launching")||!admitted||!validPlayers(context)) {done(false,"The game context changed. Prepare again.");return;}
    ticket_=ticket;completion_=std::move(done);deadline_.start();
    if(host_) {
        for(const auto& value:context["members"].toArray()) {
            const auto m=value.toObject();if(m["spectator"].toBool()||m["id"]==context_["self"])continue;
            const auto name=m["id"].toString();auto peer=addPeer(name,{},m["seat"].toInt());
            if(!peer||!peer->connection->start(context_["self"].toString(),ticket,root_+"/stream_transport",ice_,"game",name)) {
                failed("Couldn't prepare a Remote Play connection.");return;
            }
        }
    }else if(!connection_.start(context_["self"].toString(),ticket,root_+"/stream_transport",ice_))failed("Couldn't prepare the Remote Play connection.");
}
void RemotePlayRuntime::launchHost() {
    opening_=true;const auto record=library_.registration(game_);
    const auto result=record?adapter_.launchRemotePlay(record->adventure,ports(device_,profile_)):AdventureResult{false,"The host game changed."};
    if(!result.success){opening_=false;failed(result.message);}
}
void RemotePlayRuntime::hostConnected(const QString& name,const PeerPtr& peer,QString config) {
    if(peers_.value(name)!=peer)return;
    if(operation_.isEmpty()||account_!=social_.accountGeneration()||!hostUnchanged()){peerFailed(name,peer,"The game context changed.");return;}
    peer->config=config;stopping_=false;
    if(!started_&&!opening_)launchHost();else reconcile();
}
void RemotePlayRuntime::connected(QString config) {
    if(host_||operation_.isEmpty()||account_!=social_.accountGeneration()){failed("The game context changed. Prepare again.");return;}
    stopping_=false;opening_=true;AdventureResult result{false,"Remote Play launch is unavailable."};
    if(requestGuestLaunch) {
        ProcessCommand command;command.program="/usr/bin/env";
        command.arguments={"LD_LIBRARY_PATH="+root_,"TRAINEROS_REMOTE_PLAY_MANAGED=1",root_+"/stream_guest",config};
        command.runtimeControls={{"kind","remoteplay"},{"game",game_},{"temporaryProgress",true},{"recordHistory",false},{"title",context_["game"].toObject()["title"].toString()}};
        result=requestGuestLaunch(command,game_);
    }
    if(!result.success){opening_=false;failed(result.message);}
}
void RemotePlayRuntime::complete(bool ok,QString error) {auto done=std::exchange(completion_,{});if(done)done(ok,error);}
void RemotePlayRuntime::reconcile() {
    if(!host_||!started_||failing_)return;
    const auto state=social_.lobbyCoordinator()->state();
    for(const auto& name:peers_.keys()) {
        auto peer=peers_.value(name);if(!peer)continue;
        if(!peer->admission.isEmpty()&&!peer->connection->active()) {
            for(const auto& value:state["members"].toArray()) {
                const auto m=value.toObject();
                if(state["ticket"]==ticket_&&state["phase"]=="playing"&&m["id"]==name&&m["admission"]==peer->admission) {
                    if(!peer->connection->start(context_["self"].toString(),ticket_,root_+"/stream_transport",ice_,"game",name))peerFailed(name,peer,"Couldn't reconnect Remote Play.");
                    break;
                }
            }
        }
        if(peers_.value(name)==peer&&!peer->registered&&!peer->config.isEmpty()&&peer->connection->active()
           &&state["phase"]=="playing"&&lifecycle_.state()=="running")capture(name,peer);
    }
}
void RemotePlayRuntime::startGuestMedia() {
    if(!host_&&mediaReady_&&ownedPid_&&process_.processId()==ownedPid_&&!announced_) {
        announced_=true;deadline_.stop();
        if(!process_.runtimeCommand("remoteplay-start"))failed("Couldn't start Remote Play.");
    }
}
void RemotePlayRuntime::capture(const QString& name,const PeerPtr& peer) {
    if(process_.processId()!=ownedPid_||process_.runtimeControls()["remotePlay"].toMap()["ports"]!=capacity(profile_)) {failed("The host controller binding changed.");return;}
    auto input=social_.lobbyRuntime()->input();const auto id=peer->connection->id();
    if(!input->authorize(name,id,ticket_,peer->port)){peerFailed(name,peer,"The guest no longer owns that controller.");return;}
    const int fd=input->takeDescriptor(id);
    if(fd<0||!media_.add(ownedPid_,root_,id,peer->port,peer->config,fd)){peerFailed(name,peer,"Couldn't open the guest stream.");return;}
    peer->registered=true;
}
void RemotePlayRuntime::disconnectMedia() {
    stopping_=true;deadline_.stop();const auto peers=std::exchange(peers_,{});
    for(const auto& peer:peers) {
        peer->deadline->stop();peer->deadline->deleteLater();
        social_.lobbyRuntime()->input()->release(peer->connection->id());
        peer->connection->stop();peer->connection->deleteLater();
    }
    media_.stop();announced_=mediaReady_=false;
}
void RemotePlayRuntime::failed(QString error) {
    if(failing_||(stopping_&&operation_.isEmpty()))return;
    failing_=true;
    const auto peers=peers_;const auto ticket=ticket_;
    disconnectMedia();connection_.stop();complete(false,error);
    if(host_&&started_) {
        if(social_.lobbyCoordinator()->state()["phase"]=="launching")social_.lobbyRuntime()->finished(ticket);
        else for(auto it=peers.cbegin();it!=peers.cend();++it)social_.lobbyCoordinator()->releaseRemotePlayer(it.key(),ticket,it.value()->admission);
    }
    if(!host_&&ownedPid_&&process_.processId()==ownedPid_)process_.runtimeCommand("remoteplay-close");
    failing_=false;
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
