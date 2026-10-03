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
#include <QFileInfo>
#include <QTcpSocket>

namespace trainer {
Q_LOGGING_CATEGORY(multiplayerLog,"trainer.multiplayer")
namespace {
QString randomToken(){return QUuid::createUuid().toString(QUuid::Id128);}
QVariantMap row(QString id,QString label,QString detail={}){return {{"id",id},{"label",label},{"detail",detail}};}
}
RuntimeMultiplayer::RuntimeMultiplayer(LibraryRepository& lib,RetroArchAdapter& adapter,StandaloneAdapter& ppsspp,StandaloneAdapter& dolphinAdapter,SocialController& social,
    ProcessService& process,AdventureLaunchController& lifecycle,AdventureExitPresentation& overlay)
    :library_(lib),adapter_(adapter),social_(social),ppsspp_(ppsspp),dolphin_(dolphinAdapter),process_(process),lifecycle_(lifecycle),overlay_(overlay) {
    identity_=QSettings().value("runtimeMultiplayer/device").toString();
    if(QUuid(identity_).isNull()){identity_=QUuid::createUuid().toString(QUuid::WithoutBraces);QSettings().setValue("runtimeMultiplayer/device",identity_);}
    connect(&party_,&GameParty::outgoing,this,[this](QString peer,QJsonObject packet){
        if(peer.startsWith("nearby:"))nearby_.sendTo(peer.mid(7),packet);
        else if(peer.startsWith("online:"))social_.runtimeCommand("party-send",{{"peer",peer.mid(7)},{"content",GameParty::encode(packet)}});
    });
    connect(&social_,&SocialController::partyPacket,this,[this](QString peer,QString name,QJsonObject p){party_.receive("online:"+peer,name,p);});
    connect(&nearby_,&LocalLinkPeer::partyReceived,this,[this](QString peer,QString name,QJsonObject p){party_.receive("nearby:"+peer,name,p);});
    connect(&nearby_,&LocalLinkPeer::partyDisconnected,this,[this](QString peer){party_.disconnected("nearby:"+peer);});
    connect(&social_,&SocialController::partyFailed,this,[this](QString peer){
        if(peer=="company:"+party_.state()["company"].toString()&&!party_.state()["company"].toString().isEmpty()) {
            party_.leave();emit notice("This group is no longer available for joining.");
        }else party_.deliveryFailed("online:"+peer);
    });
    connect(&social_,&SocialController::partyReset,&party_,&GameParty::reset);
    connect(&social_,&SocialController::partyQuery,this,[this](QString peer){party_.query("online:"+peer);});
    connect(&social_,&SocialController::partyLeave,this,&RuntimeMultiplayer::leaveParty);
    connect(&social_,&SocialController::partyJoin,this,[this](QString peer){party_.requestJoin("online:"+peer);});
    connect(&social_,&SocialController::companyQuery,&party_,&GameParty::browseCompany);
    connect(&social_,&SocialController::companyAccessChanged,this,[this](QString company,QVariantMap access){
        if(!companyOverride_)party_.setCompanyAccess(company,access["policy"].toString(),access["allowed"].toStringList());
    });
    connect(&party_,&GameParty::notice,this,&RuntimeMultiplayer::notice);
    connect(&party_,&GameParty::startRequested,this,&RuntimeMultiplayer::startParty);
    connect(&party_,&GameParty::changed,this,[this]{
        if(party_.active())partySession_=true;else companyOverride_=false;
        const auto pending=party_.pending();const auto request=pending["request"].toString();
        if(!request.isEmpty()&&request!=lastRequest_&&lifecycle_.active())
            emit social_.backgroundNotification("Game invitation",invitation()+" Open Home to answer.");
        lastRequest_=request;
        social_.setGameParty(party_.state().toVariantMap());
        auto companies=party_.companyOffers().toVariantList();
        if(lifecycle_.active())for(auto& v:companies){auto r=v.toMap();r["joinable"]=false;v=r;}
        social_.setCompanyParties(companies);
        for(const auto& v:social_.runtimeFriends()) {
            const auto peer=v.toMap()["id"].toString();auto offer=party_.offer("online:"+peer);
            if(!offer.isEmpty())offer["joinable"]=offer["joinable"].toBool()&&!party_.active()&&!party_.state()["joining"].toBool()&&!lifecycle_.active();
            social_.setGameActivity(peer,offer.toVariantMap());
        }
        if(overlay_.panel()=="multiplayer-party"||overlay_.panel()=="multiplayer-request")show(overlay_.panel());
        emit changed();
    });

    connect(&client_,&retroarch::NetplayClient::failed,this,[this](QString message){
        if(active_)fail(std::move(message));
    });
    connect(&scan_,&QFutureWatcherBase::finished,this,[this]{
        games_=scan_.result();update();
        refresh(installation_,trainer_,allowed_);
    });
    connect(&social_,&SocialController::changed,this,[this]{
        if(overlay_.panel()=="multiplayer-online")show("multiplayer-online");
        emit changed();
    });
    connect(&nearby_,&LocalLinkPeer::changed,this,[this]{if(overlay_.panel()=="multiplayer-nearby")show("multiplayer-nearby");});
    connect(&nearby_,&LocalLinkPeer::error,this,[this](QString text){fail(text);});
    connect(&process_,&ProcessService::runtimeOutput,this,&RuntimeMultiplayer::output);
    connect(&lifecycle_.exitController(),&AdventureExitController::confirmationRequested,this,[this]{
        // Both players already accepted a new game. This exact profile has no
        // persistent progress; reuse normal capture/owned-window graceful exit.
        if(restarting_&&!psp()&&!dolphin())lifecycle_.exitController().confirm();
    });
    connect(&lifecycle_.exitController(),&AdventureExitController::returnToGameRequested,this,[this]{
        if(restarting_)fail("Invitation cancelled. Your game is still running.");
    });
    connect(&lifecycle_.exitController(),&AdventureExitController::failed,this,[this](QString error){
        if(restarting_)fail(error);
    });
    connect(&lifecycle_,&AdventureLaunchController::adventureStarted,this,[this](QString){
        update();if(!active_)return;
        if(psp()) {
            // A running PSP is not proof of an ad hoc match. Players use the
            // original game's VS create/join controls after accepting here.
            if(host_)send({{"kind","psp-ready"},{"identity",descriptor_},{"address",request_.address}});
            relaySent_=true;deadline_=0;timer_.stop();status_="Choose VS mode in the game";return;
        }
        if(host_&&online_&&!dolphin())pollRelay();
    });
    connect(&lifecycle_,&AdventureLaunchController::adventureFinished,this,[this](bool){
        if(restarting_){restarting_=false;QTimer::singleShot(0,this,&RuntimeMultiplayer::launch);return;}
        client_.stop();
        if(partySession_){party_.leave();partySession_=false;}
        if(active_){active_=false;deadline_=0;timer_.stop();}
        update();
    });
    connect(&lifecycle_,&AdventureLaunchController::changed,this,[this]{
        // Preparation can fail before a child process emits adventureFinished.
        if(active_&&!restarting_&&lifecycle_.state()=="failed")fail(lifecycle_.error());
        emit changed();
    });
    connect(&timer_,&QTimer::timeout,this,[this]{
        if(deadline_&&QDateTime::currentSecsSinceEpoch()>deadline_){fail("Couldn't connect to your friend. Try inviting again.");return;}
        if(active_&&host_&&online_&&!query_&&!relaySent_)pollRelay();
    });timer_.setInterval(3000);
}
void RuntimeMultiplayer::refresh(const RetroArchInstallation& installation,QString trainer,bool allowed) {
    if(trainer_!=trainer)party_.reset();
    trainer_=std::move(trainer);allowed_=allowed;
    installation_=installation;
    const auto records=library_.registrations();
    const auto pspInstallation=ppsspp_.installation();
    const auto dolphinInstallation=dolphin_.installation();
    const auto stamp=[](const QString& path){const QFileInfo f(path);return qHashMulti(0,path,f.size(),f.lastModified().toMSecsSinceEpoch());};
    quint64 revision=qHashMulti(0,stamp(installation.runtimeFile),stamp(installation.cores.value("snes9x")),
        stamp(installation.cores.value("genesis_plus_gx")),stamp(pspInstallation.runtimeFile),stamp(dolphin::bridgeFile()),stamp(dolphin::bridgeRoot()+"/manifest.json"));
    for(const auto& r:records)if(r.adventure.platformId=="snes"||r.adventure.platformId=="megadrive"||r.adventure.platformId=="psp"||r.adventure.platformId=="gc")
        revision=qHashMulti(revision,r.adventure.id,stamp(r.contentPath),r.revision,r.integrationConfig["core"].toString());
    if(!revision)revision=1;
    if(!scan_.isRunning() && (scanRevision_!=revision||!scanRevision_)) {
        scanRevision_=revision;
        scan_.setFuture(QtConcurrent::run([records,installation,pspInstallation,dolphinInstallation]{
            std::atomic_bool cancel=false;
            QMap<QString,QJsonObject> games;
            for(const auto& r:records)if(r.adventure.platformId=="snes"||r.adventure.platformId=="megadrive"||r.adventure.platformId=="psp"||r.adventure.platformId=="gc") {
                const auto identity=r.adventure.platformId=="gc"?dolphin::netplayIdentity(r,dolphinInstallation,cancel):r.adventure.platformId=="psp"?ppsspp::netplayIdentity(r,pspInstallation,cancel):retroarch::netplayIdentity(r,installation,cancel);
                if(!identity.isEmpty())games.insert(r.adventure.id,identity);
            }
            return games;
        }));
    }
    update();
}
QString RuntimeMultiplayer::menuLabel() const {return !party_.pending().isEmpty()?"Join request":party_.active()?"Game party":"Invite friend";}
bool RuntimeMultiplayer::canInvite() const {
    if(allowed_&&party_.active())return true;
    return allowed_&&games_.contains(process_.runtimeControls()["game"].toString())&&!active_
        &&!process_.runtimeControls().contains("netplay");
}
void RuntimeMultiplayer::leaveParty(){
    if(active_&&lifecycle_.active()) {
        // Keep a live seat occupied until the owned emulator actually exits.
        // Normal Home exit preserves the save/capture confirmation policy.
        if(overlay_.menuOpen())overlay_.exitFromMenu();
        return;
    }
    party_.leave();emit changed();
}
bool RuntimeMultiplayer::incoming() const {return !party_.pending().isEmpty()&&!lifecycle_.active();}
QString RuntimeMultiplayer::invitation() const {
    const auto pending=party_.pending();
    return pending["name"].toString()+(pending["joining"].toBool()?" wants to join ":" invites you to play ")+pending["game"].toObject()["label"].toString()+".";
}
void RuntimeMultiplayer::answer(bool accept) {
    party_.answer(accept);
    if(party_.host()&&party_.state()["members"].toArray().size()>1)online_=party_.state()["members"].toArray().at(1).toObject()["peer"].toString().startsWith("online:");
}
bool RuntimeMultiplayer::dispatch(Action a) {
    if(!incoming())return false;
    if(a==Action::Confirm)answer(true);else if(a==Action::Back)answer(false);return true;
}
void RuntimeMultiplayer::update() {
    if(!active_&&!party_.active()) {
        game_=process_.runtimeControls()["game"].toString();descriptor_=games_.value(game_);
    }
    const bool available=allowed_&&!games_.isEmpty()&&(!lifecycle_.active()||canInvite()||active_);
    QJsonArray caps;
    if(available)for(const auto& descriptor:games_)if(!caps.contains(descriptor))caps.append(descriptor);
    party_.configure(trainer_,caps,games_.value(process_.runtimeControls()["game"].toString()),available);
    if(social_.conversationVisible()&&!social_.runtimePeer().isEmpty())party_.query("online:"+social_.runtimePeer());
    social_.setRuntimeContext(available,{});
    social_.refreshPartyBrowse();
    nearby_.configure(identity_,trainer_);
    if(available){nearby_.open();nearby_.setVisible(true);}else if(!active_){nearby_.close();}
    if(overlay_.panel()=="multiplayer-wait")show("multiplayer-wait");
    emit changed();
}
void RuntimeMultiplayer::show(QString panel) {
    QVariantList rows;QString caption;
    if(panel=="multiplayer"&&!party_.pending().isEmpty())panel="multiplayer-request";
    if(panel=="multiplayer"&&party_.active())panel="multiplayer-party";
    if(panel=="multiplayer-request") {
        caption=invitation();rows={row("multiplayer-accept","Accept"),row("multiplayer-decline","Decline")};
    } else if(panel=="multiplayer-party") {
        const auto state=party_.state();caption=QString::number(state["members"].toArray().size())+" / "+QString::number(state["capacity"].toInt())+" players";
        bool ready=true;
        for(const auto& value:state["members"].toArray()) {
            const auto m=value.toObject();ready&=m["ready"].toBool();
            auto r=row(m["ready"].toBool()?"multiplayer-member":"multiplayer-revoke:"+m["peer"].toString(),m["name"].toString(),m["ready"].toBool()?"Ready":"Invited - select to cancel");
            r["readOnly"]=m["ready"].toBool();rows.append(r);
        }
        if(!party_.pending().isEmpty())rows.prepend(row("multiplayer-request","Join request",party_.pending()["name"].toString()));
        if(party_.host()&&!party_.running()&&ready&&state["members"].toArray().size()>1)rows.prepend(row("multiplayer-start","Start game"));
        if(party_.host()&&state["free"].toInt()>0)rows.append(row(online_?"multiplayer-online":"multiplayer-nearby","Invite friend"));
        if(party_.host()&&!state["company"].toString().isEmpty())rows.append(row("multiplayer-access","Who can join",state["access"]=="closed"?"Invitations only":state["access"]=="selected"?"Selected group members":"Ask me first"));
        rows.append(row("multiplayer-leave",party_.host()?"End party":"Leave party"));
    } else if(panel=="multiplayer-access") {
        caption="This game party";
        rows={row("multiplayer-access:default","Use my group preference"),row("multiplayer-access:request","Ask me first"),
            row("multiplayer-access:selected","Selected group members"),row("multiplayer-access:closed","Invitations only")};
    } else if(panel=="multiplayer") {
        caption=(process_.runtimeControls()["kind"]=="ppsspp"||process_.runtimeControls()["kind"]=="dolphin")?"Play together":"Start a new two-player game";
        rows={row("multiplayer-nearby","Nearby","Same local network"),row("multiplayer-online","Online friend")};
    } else if(panel=="multiplayer-nearby") {
        caption="Choose a nearby Trainer";
        for(const auto& p:nearby_.peers()){const auto r=p.toMap();rows.append(row("multiplayer-local:"+r["id"].toString(),r["name"].toString()));}
        if(rows.isEmpty())caption="Looking for nearby Trainers…";
    } else if(panel=="multiplayer-online") {
        caption="Choose a friend or group";
        if(!party_.active())for(const auto& p:social_.runtimeCompanies()){const auto r=p.toMap();rows.append(row("multiplayer-company:"+r["id"].toString(),r["name"].toString(),"Play with this group"));}
        for(const auto& p:social_.runtimeFriends()){const auto r=p.toMap();rows.append(row("multiplayer-friend:"+r["id"].toString(),r["name"].toString()));}
        if(rows.isEmpty())caption="Add a friend in Social first";
    } else {
        caption=status_;
        if(caption.isEmpty())caption="Connecting to your friend…";
    }
    rows.append(row("multiplayer-cancel","Back"));overlay_.setPanel(panel,caption,rows,"multiplayer-cancel");
}
bool RuntimeMultiplayer::action(const QString& id) {
    if(!id.startsWith("multiplayer"))return false;
    if(id=="multiplayer-access"){show(id);return true;}
    if(id.startsWith("multiplayer-access:")) {
        const auto company=party_.state()["company"].toString();auto access=social_.companyAccess(company);
        companyOverride_=id.section(':',1)!="default";
        if(companyOverride_)access["policy"]=id.section(':',1);
        party_.setCompanyAccess(company,access["policy"].toString(),access["allowed"].toStringList());show("multiplayer-party");return true;
    }
    if(id=="multiplayer-start"){party_.start();return true;}
    if(id=="multiplayer-leave"){leaveParty();overlay_.setPanel({}, {}, {});return true;}
    if(id.startsWith("multiplayer-revoke:")){party_.cancelInvite(id.section(':',1));return true;}
    if(id=="multiplayer-accept"||id=="multiplayer-decline"){answer(id=="multiplayer-accept");show(party_.active()?"multiplayer-party":"multiplayer");return true;}
    if(id=="multiplayer-party"||id=="multiplayer-request"){show(id);return true;}
    if(id=="multiplayer-cancel"){overlay_.setPanel({}, {}, {});return true;}
    if(id=="multiplayer"||id=="multiplayer-nearby"||id=="multiplayer-online"){show(id);return true;}
    if(!canInvite())return true;
    if(id.startsWith("multiplayer-company:")) {
        const auto company=id.section(':',1);const auto access=social_.companyAccess(company);
        if(party_.openCompany(company,access["policy"].toString(),access["allowed"].toStringList())){online_=true;partySession_=true;show("multiplayer-party");}
        return true;
    }
    if(id.startsWith("multiplayer-friend:")||id.startsWith("multiplayer-local:")) {
        if(!party_.active())online_=id.startsWith("multiplayer-friend:");
        partySession_=true;party_.invite((online_?"online:":"nearby:")+id.section(':',1));show("multiplayer-party");
    }
    return true;
}
void RuntimeMultiplayer::startParty(bool host,const QJsonObject& endpoint) {
    const auto selected=party_.game();QString game;
    for(auto it=games_.cbegin();it!=games_.cend();++it)if(it.value()==selected){game=it.key();break;}
    if(!allowed_||game.isEmpty()||(!host&&lifecycle_.active())){party_.leave();emit notice("Finish your current game before joining.");return;}
    game_=game;descriptor_=selected;partySession_=true;active_=true;host_=host;relaySent_=false;
    transportPeer_=host?party_.state()["members"].toArray().at(1).toObject()["peer"].toString():party_.hostPeer();
    online_=transportPeer_.startsWith("online:");
    request_={};request_.host=host;request_.relay=online_;request_.expected=selected;
    request_.nickname="TrainerOS-"+randomToken().left(12);output_.clear();
    deadline_=QDateTime::currentSecsSinceEpoch()+(dolphin()?150:75);timer_.start();
    dolphinRequest_={};dolphinRequest_.expected=selected;dolphinRequest_.host=host;dolphinRequest_.online=online_;
    if(host&&dolphin()) {
        dolphinRequest_.token=randomToken();
        for(const auto& v:party_.state()["members"].toArray())dolphinRequest_.seats.append(v.toObject()["slot"]);
        prepareHost();
    } else if(host) {
        request_.password=randomToken();
        if(psp()){if(online_)resolvePspRelay();else {request_.address=nearby_.localAddressFor(transportPeer_.mid(7));prepareHost();}}
        else if(online_)resolveRelay();else prepareHost();
    } else frame(endpoint);
    emit changed();
}
void RuntimeMultiplayer::prepareHost() {
    launchPending_=true;
    if(lifecycle_.active()) {
        restarting_=true;
        // Keep Home's owned-window lease through capture and graceful exit.
        if(!overlay_.exitFromMenu()){restarting_=false;fail("Couldn't restart this game for multiplayer.");}
    }else launch();
}
void RuntimeMultiplayer::resolveRelay() {
    status_="Connecting to the online relay…";
    QNetworkRequest request(QUrl("http://lobby.libretro.com/tunnel?name=madrid"));
    request.setTransferTimeout(7000);
    auto* reply=network_.get(request);reply->setReadBufferSize(4097);
    const auto nickname=request_.nickname;auto bytes=std::make_shared<QByteArray>();
    connect(reply,&QIODevice::readyRead,reply,[reply,bytes]{bytes->append(reply->readAll());if(bytes->size()>4096)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,bytes,nickname]{
        bytes->append(reply->readAll());reply->deleteLater();
        if(!active_||!host_||!online_||request_.nickname!=nickname)return;
        const auto endpoint=retroarch::netplayRelayEndpoint(*bytes);
        if(reply->error()!=QNetworkReply::NoError||endpoint.isEmpty()) {
            fail("Couldn't reach the online relay. Try inviting again.");return;
        }
        request_.relayEndpoint=endpoint;prepareHost();
    });
}
void RuntimeMultiplayer::resolvePspRelay() {
    status_="Connecting to the online relay…";
    request_.address="socom.cc"; // PPSSPP's upstream-listed AemuPostoffice relay.
    auto* socket=new QTcpSocket(this);auto* timeout=new QTimer(socket);timeout->setSingleShot(true);
    const auto token=request_.nickname;
    const auto finish=[this,socket,timeout,token](bool connected){
        timeout->stop();
        socket->disconnect(this);socket->abort();socket->deleteLater();
        if(!active_||!host_||request_.nickname!=token)return;
        if(connected)prepareHost();else fail("Couldn't reach the PSP relay. Your game is still running.");
    };
    connect(socket,&QTcpSocket::connected,this,[finish]{finish(true);});
    connect(socket,&QTcpSocket::errorOccurred,this,[finish](QAbstractSocket::SocketError){finish(false);});
    connect(timeout,&QTimer::timeout,this,[finish]{finish(false);});timeout->start(7000);
    socket->connectToHost(request_.address,27312);
}
void RuntimeMultiplayer::launch() {
    launchPending_=false;
    const auto record=library_.registration(game_);
    if(!record){fail("The game is no longer in your library.");return;}
    const auto result=dolphin()?dolphin_.launchNetplay(record->adventure,dolphinRequest_):psp()?ppsspp_.launchNetplay(record->adventure,
        ppsspp::NetplayRequest{descriptor_,host_,online_,request_.address,trainer_}):adapter_.launchNetplay(record->adventure,request_);
    if(!result.success)fail(result.message);
}
void RuntimeMultiplayer::send(QJsonObject packet) {
    if(partySession_){if(packet["kind"]=="ready"||packet["kind"]=="psp-ready"||packet["kind"]=="dolphin-ready")party_.ready(packet);return;}
}
void RuntimeMultiplayer::frame(const QJsonObject& packet) {
    if(!active_)return;
    if(packet["kind"]=="left"){status_="Your friend left the game";emit notice(status_);return;}
    if(dolphin()) {
        if(host_||packet["kind"]!="dolphin-ready"||lifecycle_.active()||launchPending_)return;
        if(packet["identity"].toObject()!=descriptor_){fail("Your game or emulator doesn't match your friend's.");return;}
        dolphinRequest_.address=online_?packet["code"].toString():nearby_.addressOf(transportPeer_.mid(7));
        dolphinRequest_.port=packet["port"].toInt();dolphinRequest_.slot=packet["slot"].toInt();
        dolphinRequest_.token=packet["token"].toString();dolphinRequest_.seats=packet["seats"].toArray();
        launchPending_=true;launch();return;
    }
    if(psp()) {
        if(host_||packet["kind"]!="psp-ready"||lifecycle_.active()||launchPending_)return;
        if(packet["identity"].toObject()!=descriptor_){fail("Your PSP game or emulator doesn't match your friend's.");return;}
        request_.address=online_?packet["address"].toString():nearby_.addressOf(transportPeer_.mid(7));
        launchPending_=true;launch();return;
    }
    if(host_||packet["kind"]!="ready"||lifecycle_.active()||launchPending_)return;
    if(packet["identity"].toObject()!=descriptor_){fail("Your game or emulator doesn't match your friend's.");return;}
    const auto port=packet["port"].toInt();
    if(port<1||port>65535){fail("This multiplayer address is invalid.");return;}
    request_.password=packet["password"].toString();request_.port=quint16(port);
    request_.address=online_?packet["address"].toString():nearby_.addressOf(transportPeer_.mid(7));
    request_.relaySession=packet["session"].toString();
    if(online_) {
        request_.clientPort=client_.start(request_.address,request_.port,request_.relaySession,request_.password);
        if(!request_.clientPort){fail("Couldn't prepare the multiplayer connection.");return;}
    }
    launchPending_=true;launch();
}
void RuntimeMultiplayer::pollRelay() {
    if(psp()||dolphin()||query_||!active_||!host_||!online_||relaySent_||request_.relayEndpoint.isEmpty())return;
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
    if(!active_||psp())return;
    output_+=bytes;
    while(output_.contains('\n')) {
        const auto end=output_.indexOf('\n');const auto line=output_.left(end);output_.remove(0,end+1);
        if(dolphin()) {
            constexpr auto prefix="[TrainerOSNetplay]";
            if(!line.startsWith(prefix))continue;
            const auto event=QJsonDocument::fromJson(line.mid(qstrlen(prefix))).object();
            if(event["event"]=="ready"&&host_&&!relaySent_) {
                relaySent_=true;
                send({{"kind","dolphin-ready"},{"identity",descriptor_},{"token",dolphinRequest_.token},
                    {"seats",dolphinRequest_.seats},{"port",event["port"]},{"code",event["code"]}});
                status_="Waiting for your friends...";
            } else if(event["event"]=="running") {
                deadline_=0;timer_.stop();status_="Playing together";emit changed();
            } else if(event["event"]=="error") {
                fail(event["reason"]=="traversal"?"Couldn't connect through this network. Try inviting again.":"The multiplayer connection ended. You can invite again.");
                return;
            }
            continue; // endpoint/admission fields never enter application logs
        }
        if(!line.contains("[Netplay]"))continue;
        // No launch arguments/configuration (passwords) or chat frames are logged.
        qCInfo(multiplayerLog).noquote()<<QString::fromUtf8(line.left(512));
        if(host_&&online_&&(line.contains("Switching to direct mode")||
                           line.contains("Your room is not connectable"))) {
            // RetroArch can silently abandon the requested relay after a
            // directory failure. That is not the route the friend accepted.
            // Close signalling now instead of leaving the guest waiting for
            // a relay room which will never appear. Keep owned Home -> Exit.
            fail("Couldn't reach the online relay. Exit the game and invite again.");
            return;
        }
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
    status_=std::move(text);deadline_=0;timer_.stop();
    const bool wasActive=active_;active_=false;restarting_=false;launchPending_=false;
    if(partySession_){party_.leave();partySession_=false;}
    if(wasActive&&process_.runtimeControls().contains("netplay")){
        // Keep the owned emulator/session alive for normal Home -> Exit. Killing
        // a Flatpak launcher here can orphan its emulator and lose ownership.
        overlay_.dismissMenu();
    }
    if(overlay_.menuOpen())show("multiplayer-wait");else emit notice(status_);
    emit changed();
}
}
