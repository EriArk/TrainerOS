#include "RuntimeMultiplayer.h"
#include "features/social/SocialController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include "core/navigation/AdventureLaunchController.h"
#include <QtConcurrent>
#include <QDateTime>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QSettings>
#include <QUuid>

namespace trainer {
Q_LOGGING_CATEGORY(multiplayerLog,"trainer.multiplayer")
namespace {
QString randomToken(){return QUuid::createUuid().toString(QUuid::Id128);}
QVariantMap row(QString id,QString label,QString detail={}){return {{"id",id},{"label",label},{"detail",detail}};}
}
RuntimeMultiplayer::RuntimeMultiplayer(LibraryRepository& lib,RetroArchAdapter& adapter,SocialController& social,
    ProcessService& process,AdventureLaunchController& lifecycle,AdventureExitPresentation& overlay)
    :library_(lib),adapter_(adapter),social_(social),process_(process),lifecycle_(lifecycle),overlay_(overlay) {
    identity_=QSettings().value("runtimeMultiplayer/device").toString();
    if(QUuid(identity_).isNull()){identity_=QUuid::createUuid().toString(QUuid::WithoutBraces);QSettings().setValue("runtimeMultiplayer/device",identity_);}
    consent_.bind(identity_,identity_);
    connect(&client_,&retroarch::NetplayClient::failed,this,[this](QString message){
        if(active_)fail(std::move(message));
    });
    connect(&scan_,&QFutureWatcherBase::finished,this,[this]{
        const auto result=scan_.result();game_=result.first;descriptor_=result.second;update();
        refresh(installation_,trainer_,allowed_);
    });
    connect(&nearby_,&LocalLinkPeer::connectedToPeer,this,[this]{
        peerId_.clear();nearby_.send({{"hello",identity_},{"name",trainer_}});
    });
    connect(&nearby_,&LocalLinkPeer::received,this,[this](QJsonObject packet){
        if(packet.contains("hello")) {
            if(!peerId_.isEmpty()||QUuid(packet["hello"].toString()).isNull())return;
            peerId_=packet["hello"].toString();peerName_=packet["name"].toString().left(48);
            if(nearby_.outgoing())consent_.probe("nearby",peerId_);
            return;
        }
        if(!peerId_.isEmpty())consent_.receive("nearby",peerId_,peerName_,packet);
    });
    connect(&consent_,&OnlineLink::outgoing,this,[this](QString,QString text){nearby_.send(OnlineLink::decode(text));});
    connect(&consent_,&OnlineLink::changed,this,[this]{
        const auto state=consent_.state();
        if(state["stage"]=="available"&&!invited_) {
            invited_=true;consent_.invite(descriptor_["id"].toString());
            if(consent_.state()["stage"]=="available")fail("Your friend's game or emulator doesn't match.");
        }
        update();
    });
    connect(&consent_,&OnlineLink::established,this,[this](QString,QString,QString,QString,bool host){begin(host,false);});
    connect(&consent_,&OnlineLink::frameReceived,this,&RuntimeMultiplayer::frame);
    connect(&social_,&SocialController::runtimeEstablished,this,[this](QString,bool host){begin(host,true);});
    connect(&social_,&SocialController::runtimeFrame,this,&RuntimeMultiplayer::frame);
    connect(&social_,&SocialController::runtimeProbeFailed,this,[this](QString peer,QString message){
        if(!active_&&onlinePerson_==peer)fail(std::move(message));
    });
    connect(&social_,&SocialController::changed,this,[this]{
        const auto state=social_.online();
        if(!onlinePerson_.isEmpty()&&state["stage"]=="available"&&state["peer"]==onlinePerson_&&!invited_) {
            invited_=true;
            bool matches=false;for(const auto& a:state["actions"].toList())if(QJsonObject::fromVariantMap(a.toMap())==descriptor_)matches=true;
            if(matches)social_.runtimeCommand("online-invite",{{"id",descriptor_["id"].toString()}});
            else fail("Your friend needs the same supported game and emulator.");
        }
        if(overlay_.panel()=="multiplayer-online")show("multiplayer-online");
        emit changed();
    });
    connect(&nearby_,&LocalLinkPeer::changed,this,[this]{if(overlay_.panel()=="multiplayer-nearby")show("multiplayer-nearby");});
    connect(&nearby_,&LocalLinkPeer::error,this,[this](QString text){fail(text);});
    connect(&process_,&ProcessService::runtimeOutput,this,&RuntimeMultiplayer::output);
    connect(&lifecycle_.exitController(),&AdventureExitController::confirmationRequested,this,[this]{
        // Both players already accepted a new game. This exact profile has no
        // persistent progress; reuse normal capture/owned-window graceful exit.
        if(restarting_)lifecycle_.exitController().confirm();
    });
    connect(&lifecycle_.exitController(),&AdventureExitController::failed,this,[this](QString error){
        if(restarting_)fail(error);
    });
    connect(&lifecycle_,&AdventureLaunchController::adventureStarted,this,[this](QString){
        update();if(!active_)return;
        if(host_&&online_)pollRelay();
    });
    connect(&lifecycle_,&AdventureLaunchController::adventureFinished,this,[this](bool){
        if(restarting_){restarting_=false;QTimer::singleShot(0,this,&RuntimeMultiplayer::launch);return;}
        client_.stop();
        if(active_){send({{"kind","left"}});active_=false;deadline_=0;timer_.stop();
            if(online_)social_.runtimeCommand("online-close");else {consent_.answer(false);nearby_.disconnectPeer();}}
        update();
    });
    connect(&lifecycle_,&AdventureLaunchController::changed,this,[this]{
        // Preparation can fail before a child process emits adventureFinished.
        if(active_&&!restarting_&&lifecycle_.state()=="failed")fail(lifecycle_.error());
        emit changed();
    });
    connect(&nearby_,&LocalLinkPeer::disconnectedFromPeer,this,[this]{
        if(!online_&&active_&&deadline_)fail("Your friend disconnected before the game started.");
    });
    connect(&social_,&SocialController::runtimeEnded,this,[this]{
        // Once joined, RetroArch owns the connection. Signalling expiry or a
        // social reconnect must not interrupt gameplay (or a background call).
        if(online_&&active_&&deadline_)fail("The invitation ended before the game started.");
    });
    connect(&timer_,&QTimer::timeout,this,[this]{
        if(deadline_&&QDateTime::currentSecsSinceEpoch()>deadline_){fail("Couldn't connect to your friend. Try inviting again.");return;}
        if(active_&&host_&&online_&&!query_&&!relaySent_)pollRelay();
    });timer_.setInterval(3000);
}
void RuntimeMultiplayer::refresh(const RetroArchInstallation& installation,QString trainer,bool allowed) {
    trainer_=std::move(trainer);allowed_=allowed;
    installation_=installation;
    const auto records=library_.registrations();
    quint64 revision=qHash(installation.runtimeFile+installation.cores.value("snes9x"));
    for(const auto& r:records)if(r.adventure.platformId=="snes")
        revision=qHashMulti(revision,r.adventure.id,r.contentPath,r.revision,r.integrationConfig["core"].toString());
    if(!revision)revision=1;
    if(!scan_.isRunning() && (scanRevision_!=revision||!scanRevision_)) {
        scanRevision_=revision;
        scan_.setFuture(QtConcurrent::run([records,installation]{
            std::atomic_bool cancel=false;
            for(const auto& r:records)if(r.adventure.platformId=="snes") {
                const auto identity=retroarch::netplayIdentity(r,installation,cancel);
                if(!identity.isEmpty())return qMakePair(r.adventure.id,identity);
            }
            return QPair<QString,QJsonObject>{};
        }));
    }
    update();
}
bool RuntimeMultiplayer::canInvite() const {
    return allowed_&&!game_.isEmpty()&&process_.runtimeControls()["game"]==game_&&!active_
        &&!process_.runtimeControls().contains("netplay");
}
bool RuntimeMultiplayer::incoming() const {
    return !lifecycle_.active()&&(consent_.state()["incoming"].toBool() ||
        (social_.online()["incoming"].toBool()&&social_.online()["activity"].toMap()["id"].toString().startsWith("runtime.")));
}
QString RuntimeMultiplayer::invitation() const {
    const auto state=consent_.state()["incoming"].toBool()?consent_.state():social_.online();
    return state["name"].toString()+" invites you to a new two-player game of Contra III.";
}
void RuntimeMultiplayer::answer(bool accept) {
    if(!incoming())return;
    if(consent_.state()["incoming"].toBool())consent_.answer(accept);else social_.answerOnline(accept);
}
bool RuntimeMultiplayer::dispatch(Action a) {
    if(!incoming())return false;
    if(a==Action::Confirm)answer(true);else if(a==Action::Back)answer(false);return true;
}
void RuntimeMultiplayer::update() {
    const bool available=allowed_&&!descriptor_.isEmpty()&&(!lifecycle_.active()||canInvite()||active_);
    const QJsonArray caps=available?QJsonArray{descriptor_}:QJsonArray{};
    social_.setRuntimeContext(available,caps.toVariantList());consent_.setAvailable(available);
    if(consent_.state()["stage"]=="idle"||consent_.state()["stage"]=="offered")consent_.setCapabilities(caps);
    nearby_.configure(identity_,trainer_);
    if(available){nearby_.open();nearby_.setVisible(true);}else if(!active_){nearby_.close();}
    if(overlay_.panel()=="multiplayer-wait")show("multiplayer-wait");
    emit changed();
}
void RuntimeMultiplayer::show(QString panel) {
    QVariantList rows;QString caption;
    if(panel=="multiplayer") {
        caption="Start a new two-player game";
        rows={row("multiplayer-nearby","Nearby","Same local network"),row("multiplayer-online","Online friend")};
    } else if(panel=="multiplayer-nearby") {
        caption="Choose a nearby Trainer";
        for(const auto& p:nearby_.peers()){const auto r=p.toMap();rows.append(row("multiplayer-local:"+r["id"].toString(),r["name"].toString()));}
        if(rows.isEmpty())caption="Looking for nearby Trainers…";
    } else if(panel=="multiplayer-online") {
        caption="Choose a friend";
        for(const auto& p:social_.runtimeFriends()){const auto r=p.toMap();rows.append(row("multiplayer-friend:"+r["id"].toString(),r["name"].toString()));}
        if(rows.isEmpty())caption="Add a friend in Social first";
    } else {
        caption=status_.isEmpty()?(online_?social_.online():consent_.state())["status"].toString():status_;
        if(caption.isEmpty())caption="Connecting to your friend…";
    }
    rows.append(row("multiplayer-cancel","Cancel"));overlay_.setPanel(panel,caption,rows,"multiplayer-cancel");
}
bool RuntimeMultiplayer::action(const QString& id) {
    if(!id.startsWith("multiplayer"))return false;
    if(id=="multiplayer-cancel") {
        onlinePerson_.clear();selection_.clear();invited_=false;timer_.stop();deadline_=0;
        if(!active_){if(online_)social_.runtimeCommand("online-close");else {consent_.answer(false);nearby_.disconnectPeer();}}
        overlay_.setPanel({}, {}, {});return true;
    }
    if(id=="multiplayer"||id=="multiplayer-nearby"||id=="multiplayer-online"){show(id);return true;}
    if(!canInvite())return true;
    invited_=false;status_.clear();deadline_=QDateTime::currentSecsSinceEpoch()+75;timer_.start();
    if(id.startsWith("multiplayer-local:")) {
        online_=false;onlinePerson_.clear();consent_.close();nearby_.disconnectPeer();nearby_.connectId(id.section(':',1));
    } else if(id.startsWith("multiplayer-friend:")) {
        online_=true;onlinePerson_=id.section(':',1);social_.runtimeCommand("runtime-probe-person",{{"id",onlinePerson_}});
    }
    show("multiplayer-wait");return true;
}
void RuntimeMultiplayer::begin(bool host,bool online) {
    if(!allowed_||descriptor_.isEmpty()||(lifecycle_.active()&&!canInvite())){fail("Close your current game before joining.");return;}
    active_=true;host_=host;online_=online;relaySent_=false;request_={};request_.host=host;request_.relay=online;request_.expected=descriptor_;
    request_.nickname="TrainerOS-"+randomToken().left(12);output_.clear();
    deadline_=QDateTime::currentSecsSinceEpoch()+60;timer_.start();
    if(host){request_.password=randomToken();launchPending_=true;
        if(lifecycle_.active()){
            restarting_=true;
            // Keep Home's owned-window lease through capture and graceful exit.
            if(!overlay_.exitFromMenu()){restarting_=false;fail("Couldn't restart this game for multiplayer.");}
        }else launch();}
    else status_="Waiting for your friend to start…";
    emit changed();
}
void RuntimeMultiplayer::launch() {
    launchPending_=false;
    const auto record=library_.registration(game_);
    if(!record){fail("The game is no longer in your library.");return;}
    const auto result=adapter_.launchNetplay(record->adventure,request_);
    if(!result.success)fail(result.message);
}
void RuntimeMultiplayer::send(QJsonObject packet) {
    if(online_)social_.runtimeCommand("online-frame",packet.toVariantMap());else consent_.sendFrame(packet);
}
void RuntimeMultiplayer::frame(const QJsonObject& packet) {
    if(!active_)return;
    if(packet["kind"]=="left"){status_="Your friend left the game";emit notice(status_);return;}
    if(host_||packet["kind"]!="ready"||lifecycle_.active()||launchPending_)return;
    if(packet["identity"].toObject()!=descriptor_){fail("Your game or emulator doesn't match your friend's.");return;}
    const auto port=packet["port"].toInt();
    if(port<1||port>65535){fail("This multiplayer address is invalid.");return;}
    request_.password=packet["password"].toString();request_.port=quint16(port);
    request_.address=online_?packet["address"].toString():nearby_.peerAddress();
    request_.relaySession=packet["session"].toString();
    if(online_) {
        request_.clientPort=client_.start(request_.address,request_.port,request_.relaySession,request_.password);
        if(!request_.clientPort){fail("Couldn't prepare the multiplayer connection.");return;}
    }
    launchPending_=true;launch();
}
void RuntimeMultiplayer::pollRelay() {
    if(query_||!active_||!host_||!online_||relaySent_)return;
    // This is the upstream public directory, not an authenticated signalling
    // service. Passwords travel only through the accepted friend invitation.
    query_=true;QNetworkRequest request(QUrl("http://lobby.libretro.com/list/"));request.setTransferTimeout(7000);
    auto* reply=network_.get(request);reply->setReadBufferSize(2*1024*1024+1);
    const auto nickname=request_.nickname;auto bytes=std::make_shared<QByteArray>();
    connect(reply,&QIODevice::readyRead,reply,[reply,bytes]{bytes->append(reply->readAll());if(bytes->size()>2*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,bytes,nickname]{
        query_=false;bytes->append(reply->readAll());reply->deleteLater();
        if(!active_||!host_||!online_||request_.nickname!=nickname||bytes->size()>2*1024*1024||reply->error()!=QNetworkReply::NoError)return;
        for(const auto& value:QJsonDocument::fromJson(*bytes).array()) {
            const auto room=value.toObject()["fields"].toObject();
            if(room["username"]!=request_.nickname||room["mitm_session"].toString().isEmpty())continue;
            send({{"kind","ready"},{"password",request_.password},{"identity",descriptor_},
                {"address",room["mitm_ip"]},{"port",room["mitm_port"]},{"session",room["mitm_session"]}});
            relaySent_=true;status_="Waiting for your friend…";break;
        }
    });
}
void RuntimeMultiplayer::output(const QByteArray& bytes) {
    if(!active_)return;
    output_+=bytes;
    while(output_.contains('\n')) {
        const auto end=output_.indexOf('\n');const auto line=output_.left(end);output_.remove(0,end+1);
        if(!line.contains("[Netplay]"))continue;
        // No launch arguments/configuration (passwords) or chat frames are logged.
        qCInfo(multiplayerLog).noquote()<<QString::fromUtf8(line.left(512));
        if(host_&&!online_&&!relaySent_&&line.contains("joined as player 1")){
            relaySent_=true;
            send({{"kind","ready"},{"password",request_.password},{"port",request_.port},{"identity",descriptor_}});
            status_="Waiting for your friend…";
        }
        if(line.contains("joined as player 2")){deadline_=0;timer_.stop();status_="Playing together";emit changed();}
        if(line.contains("Failed")||line.contains("failed")||line.contains("disconnected")){status_="Multiplayer connection ended";emit notice(status_);}
    }
    if(output_.size()>16384)output_.clear();
}
void RuntimeMultiplayer::fail(QString text) {
    client_.stop();
    status_=std::move(text);deadline_=0;timer_.stop();onlinePerson_.clear();invited_=false;
    const bool wasActive=active_;active_=false;restarting_=false;launchPending_=false;
    if(online_)social_.runtimeCommand("online-close");else {consent_.answer(false);nearby_.disconnectPeer();}
    if(wasActive&&process_.runtimeControls().contains("netplay")){
        // Keep the owned emulator/session alive for normal Home -> Exit. Killing
        // a Flatpak launcher here can orphan its emulator and lose ownership.
        overlay_.dismissMenu();
    }
    if(overlay_.menuOpen())show("multiplayer-wait");else emit notice(status_);
    emit changed();
}
}
