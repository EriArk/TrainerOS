#include "RuntimeMultiplayer.h"
#include "integrations/adventure/retroarch/RetroArchHandheldLink.h"
#include "integrations/adventure/retroarch/PS2Netplay.h"
#include "core/model/GamePlayers.h"
#include "features/social/SocialController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include "core/navigation/AdventureLaunchController.h"
#include <QtConcurrent>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSettings>
#include <QUuid>
#include <QFileInfo>
#include <QTcpSocket>
#include <utility>

namespace trainer {
Q_LOGGING_CATEGORY(multiplayerLog,"trainer.multiplayer")
namespace {
QString randomToken(){return QUuid::createUuid().toString(QUuid::Id128);}
QVariantMap row(QString id,QString label,QString detail={}){return {{"id",id},{"label",label},{"detail",detail}};}
}
RuntimeMultiplayer::~RuntimeMultiplayer() {
    // QObject's own disconnect runs after value members have been destroyed.
    // A network member's close signal must not redraw a menu using freed maps.
    active_=false;
    for(auto* child:findChildren<QObject*>())QObject::disconnect(child,nullptr,this,nullptr);
    handoff_.stop();timer_.stop();invitationOpenTimer_.stop();scan_.cancel();
    nearby_.close();client_.stop();rmgLobby_.stop();stopLinkedTransfers();
}
RuntimeMultiplayer::RuntimeMultiplayer(LibraryRepository& lib,RetroArchAdapter& adapter,StandaloneAdapter& ppsspp,StandaloneAdapter& dolphinAdapter,SocialController& social,
    ProcessService& process,AdventureLaunchController& lifecycle,AdventureExitPresentation& overlay)
    :library_(lib),adapter_(adapter),social_(social),ppsspp_(ppsspp),dolphin_(dolphinAdapter),process_(process),lifecycle_(lifecycle),overlay_(overlay),preparation_(process,lifecycle,overlay,[&social]{return social.accountGeneration();},this) {
    setupHotseat();
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
    connect(&social_,&SocialController::presentationChanged,this,&RuntimeMultiplayer::changed);
    connect(social_.lobbyCoordinator(),&LobbyCoordinator::changed,this,[this]{
        const auto room=social_.lobbyCoordinator()->state();QJsonObject offer;
        if(social_.lobbyCoordinator()->hosting()) {
            const auto game=room["game"].toObject();const bool editable=room["phase"]=="choosing"||room["phase"]=="gathering";
            int occupied=0;for(const auto& m:room["members"].toArray())if(m.toObject()["seat"].toInt()>0)++occupied;
            offer={{"lobby",room["id"]},{"party",room["id"]},{"company",room["conversation"]},
                {"game",QJsonObject{{"label",game["title"].toString().isEmpty()?QString("Choosing a game"):game["title"].toString()}}},
                {"joinable",editable},{"free",qMax(0,game["seats"].toInt()-occupied)},{"capacity",game["seats"]},{"access",room["access"]},{"running",room["phase"]=="playing"}};
        }
        party_.setLobbyOffer(offer);emit changed();
    });
    connect(&social_,&SocialController::partyQuery,this,[this](QString peer){party_.query("online:"+peer);});
    connect(&social_,&SocialController::partyLeave,this,&RuntimeMultiplayer::leaveParty);
    connect(&social_,&SocialController::runtimeAction,this,[this](const QString& id){
        action(id,true);
    });
    connect(&social_,&SocialController::partyJoin,this,[this](QString peer,QJsonObject expected){
        if(!expected["lobby"].toString().isEmpty()) {
            const auto offer=party_.offer("online:"+peer);
            if(offer["lobby"]!=expected["lobby"]||!offer["joinable"].toBool()) {emit notice("The lobby changed. Open Together again.");return;}
            const auto room=expected["lobby"].toString();
            if(social_.lobbyCoordinator()->room(room)||social_.lobbyCoordinator()->join(peer.section(':',-1),room,true))social_.chooseMyLobby(room);
            return;
        }
        const auto offered=party_.offer("online:"+peer);
        if(expected["game"].toObject().isEmpty()||offered["game"]!=expected["game"]||offered["party"]!=expected["party"]) {
            emit notice("The game changed. Reopen Together before joining.");return;
        }
        party_.requestJoin("online:"+peer);
        if(party_.state()["joining"].toBool()||party_.active()){socialSurface_=true;show("multiplayer-party");}
    });
    connect(&social_,&SocialController::companyQuery,&party_,&GameParty::browseCompany);
    connect(&social_,&SocialController::companyAccessChanged,this,[this](QString company,QVariantMap access){
        if(!companyOverride_)party_.setCompanyAccess(company,access["policy"].toString(),access["allowed"].toStringList());
    });
    connect(&party_,&GameParty::notice,this,&RuntimeMultiplayer::notice);
    connect(&party_,&GameParty::startRequested,this,&RuntimeMultiplayer::startParty);
    connect(&party_,&GameParty::connectionReady,this,&RuntimeMultiplayer::frame);
    connect(&linkedSave_,&retroarch::LinkedSavePreparation::completed,this,[this](QByteArray bytes){
        linkedTransferCompleted(request_.slot,std::move(bytes));
    });
    connect(&linkedSave_,&retroarch::LinkedSavePreparation::failed,this,[this]{
        if(active_&&linkedPreparing_)fail("Couldn't prepare the linked game. Your save was kept. Try inviting again.");
    });
    connect(&party_,&GameParty::changed,this,[this]{
        if(lobbyCompletion_&&!party_.active()) {
            auto done=std::exchange(lobbyCompletion_,{});done(false,"The game connection changed. Prepare again in the same lobby.");
        }
        if(active_&&hotseat()&&(!party_.active()||party_.state()["members"].toArray().size()!=2)) {
            fail("The other player left. The shared-controller game has ended.");return;
        }
        if(active_&&(rmg()||flycast()||ds()||azahar()||vita())&&!party_.active()) {fail("The game invitation ended.");return;}
        if(active_&&rmg()&&rmgRequest_.privateLan) {
            QSet<int> expected,actual;
            for(const auto& seat:rmgRequest_.seats)expected.insert(seat.toInt());
            for(const auto& member:party_.state()["members"].toArray())actual.insert(member.toObject()["slot"].toInt());
            if(actual!=expected){fail("The Nintendo 64 party changed. Invite your friends again.");return;}
        }
        if(!exitingInvitation_.isEmpty() && exitingInvitation_!=invitationId())cancelInvitationExit();
        if(!openingInvitation_.isEmpty() && openingInvitation_!=invitationId()) {
            openingInvitation_.clear();invitationOpenTimer_.stop();
        }
        if(active_&&linked()&&!linkedPeers_.isEmpty()&&!process_.runtimeControls().contains("netplay")&&!linkedRosterMatches()) {
            fail("The linked-game invitation ended. Your save was kept.");return;
        }
        if(party_.active())partySession_=true;else companyOverride_=false;
        const auto pending=party_.pending();const auto request=pending["request"].toString();
        if(!request.isEmpty()&&request!=lastRequest_&&lifecycle_.active()&&!social_.account()["doNotDisturb"].toBool())
            emit social_.backgroundNotification("Incoming activity",social_.account()["privatePreviews"].toBool()?QString("Open Home to answer."):invitation()+" Open Home to answer.");
        lastRequest_=request;
        social_.setRuntimeInvitation(pending.toVariantMap());
        social_.setGameParty(party_.state().toVariantMap());
        auto companies=party_.companyOffers().toVariantList();
        if(lifecycle_.active())for(auto& v:companies){auto r=v.toMap();if(r["lobby"].toString().isEmpty())r["joinable"]=false;v=r;}
        social_.setCompanyParties(companies);
        for(const auto& v:social_.runtimeFriends()) {
            const auto peer=v.toMap()["id"].toString();auto offer=party_.offer("online:"+peer);
            if(!offer.isEmpty())offer["joinable"]=offer["joinable"].toBool()&&(offer["lobby"].toString().isEmpty()?(!party_.active()&&!party_.state()["joining"].toBool()&&!lifecycle_.active()):!social_.lobbyCoordinator()->room(offer["lobby"].toString()));
            social_.setGameActivity(peer,offer.toVariantMap());
        }
        if(socialSurface_&&social_.runtimeSurfaceOpen())show(socialPanel_);
        else if(overlay_.panel()=="multiplayer-party"||overlay_.panel()=="multiplayer-request")show(overlay_.panel());
        emit changed();
    });

    connect(&client_,&retroarch::NetplayClient::failed,this,[this](QString message){
        if(active_)fail(std::move(message));
    });
    connect(&rmgLobby_,&rmg::Lobby::failed,this,[this](QString error){if(active_&&rmg())fail(error);});
    connect(&rmgLobby_,&rmg::Lobby::roomReady,this,[this](int port){
        if(!active_||!rmg()||!host_)return;
        send({{"kind","rmg-ready"},{"identity",descriptor_},{"port",port},
              {"token",rmgRequest_.token},{"seats",rmgRequest_.seats}});
    });
    connect(&rmgLobby_,&rmg::Lobby::launchReady,this,[this](QString address,int port,int controller){
        if(!active_||!rmg()||lifecycle_.active())return;
        const auto record=library_.registration(game_);
        if(!record||!requestRuntimeLaunch){fail("Nintendo 64 launch is unavailable.");return;}
        QStringList inputPrefix;
        // Reuse only the device's controller launcher, before its '--'. Never
        // forward RetroArch settings/graphics/account arguments to RMG.
        const int separator=installation_.prefixArguments.indexOf("--");
        if(separator>=0)inputPrefix=installation_.prefixArguments.mid(0,separator+1);
        const auto result=requestRuntimeLaunch(rmg::command(*record,descriptor_,address,port,controller,inputPrefix),game_);
        if(!result.success){fail(result.message);return;}
        launchPending_=false;relaySent_=true;status_="Opening Nintendo 64 multiplayer...";
    });
    connect(&scan_,&QFutureWatcherBase::resultReadyAt,this,[this](int index){
        if(scan_.isCanceled())return;
        const auto result=scan_.resultAt(index);
        profiles_.insert(result.first,result.second);
        refresh(installation_,trainer_,allowed_);
    });
    connect(&scan_,&QFutureWatcherBase::finished,this,[this]{
        refresh(installation_,trainer_,allowed_);
    });
    connect(&social_,&SocialController::changed,this,[this]{
        if(!socialSurface_&&(overlay_.panel()=="multiplayer-online"||overlay_.panel()=="multiplayer-people"))show(overlay_.panel());
        emit changed();
    });
    connect(&nearby_,&LocalLinkPeer::changed,this,[this]{if(overlay_.panel()=="multiplayer-nearby"||overlay_.panel()=="multiplayer-people")show(overlay_.panel());});
    connect(&nearby_,&LocalLinkPeer::error,this,[this](QString text){
        // Discovery has its own lifecycle. A LAN listener failure must not
        // cancel an independently established Online friend game.
        if(active_&&!online_)fail(text);else emit notice(text);
    });
    connect(&process_,&ProcessService::runtimeOutput,this,&RuntimeMultiplayer::output);
    invitationOpenTimer_.setSingleShot(true);invitationOpenTimer_.setInterval(5000);
    connect(&invitationOpenTimer_,&QTimer::timeout,this,[this]{
        openingInvitation_.clear();emit notice("Couldn't open the invitation. Open Game Options and try again.");emit changed();
    });
    connect(&overlay_,&AdventureExitPresentation::changed,this,&RuntimeMultiplayer::resumeInvitation);
    connect(&preparation_,&LobbyGamePreparation::optionsRequested,this,&RuntimeMultiplayer::invitationOptionsRequested);
    connect(&lifecycle_.exitController(),&AdventureExitController::confirmationRequested,this,[this]{
        // The invitation's explicit "Saved - join" action supplies exit consent.
        // The capture and owned graceful-close/finalization barriers still run.
        if(!exitingInvitation_.isEmpty()) {
            if(exitingInvitation_==invitationId() && invitationProcess_==process_.processId()
               && invitationAccount_==social_.accountGeneration() && allowed_)
                lifecycle_.exitController().confirm();
            else cancelInvitationExit();
            return;
        }
        // Legacy exact arcade-style profiles have no persistent progress. A
        // generic title may have a real playthrough: retain its save question
        // when restarting ordinary play into a temporary multiplayer session.
        // Read without inserting a null transport into older descriptors: the
        // accepted identity must remain byte-for-byte equal at guest launch.
        if(restarting_&&!psp()&&!dolphin()&&!rmg()&&!flycast()&&!ds()&&!azahar()&&!vita()&&descriptor_.value("transport")!="netpacket"&&!descriptor_.value("id").toString().startsWith("runtime.retroarch."))
            lifecycle_.exitController().confirm();
    });
    connect(&lifecycle_.exitController(),&AdventureExitController::returnToGameRequested,this,[this]{
        exitingInvitation_.clear();emit changed();
        if(restarting_)fail("Invitation cancelled. Your game is still running.");
    });
    connect(&lifecycle_.exitController(),&AdventureExitController::failed,this,[this](QString error){
        if(!exitingInvitation_.isEmpty()){exitingInvitation_.clear();emit notice(error);emit changed();}
        if(restarting_)fail(error);
    });
    connect(&lifecycle_,&AdventureLaunchController::adventureStarted,this,[this](QString){
        update();if(!active_)return;
        if(lobbyCompletion_) {auto done=std::exchange(lobbyCompletion_,{});done(true,{});}
        if(hotseat()&&(host_||dolphin()))handoff_.begin(party_.state()["party"].toString(),host_?1:2,
            dolphin()?HotseatHandoff::NativeControl::HostAuthority:HotseatHandoff::NativeControl::SplitRoles);
        if(psp()) {
            // A running PSP is not proof of an ad hoc match. Players use the
            // original game's VS create/join controls after accepting here.
            if(host_)send({{"kind","psp-ready"},{"identity",descriptor_},{"address",request_.address}});
            relaySent_=true;deadline_=0;timer_.stop();status_="Choose Ad Hoc multiplayer in the game";return;
        }
        if(host_&&online_&&!dolphin()&&!rmg()&&!flycast()&&!ds()&&!azahar()&&!vita())pollRelay();
    });
    connect(&lifecycle_,&AdventureLaunchController::adventureFinished,this,[this](bool failed){
        if(!exitingInvitation_.isEmpty()) {
            const auto request=std::exchange(exitingInvitation_,{});
            const auto account=invitationAccount_;
            if(failed){emit notice(lifecycle_.error());emit changed();return;}
            // ProcessService emits finished only after save settlement. The
            // lifecycle restores its idle state after this signal returns.
            QTimer::singleShot(0,this,[this,request,account]{
                if(!lifecycle_.active() && allowed_ && account==social_.accountGeneration() && request==invitationId()) {
                    answer(true);emit invitationSurfaceRequested();socialSurface_=true;show("multiplayer-party");
                }
            });
            return;
        }
        if(restarting_){
            restarting_=false;
            if(failed){fail(lifecycle_.error());return;}
            const auto generation=sessionGeneration_;
            QTimer::singleShot(0,this,[this,generation]{if(active_&&generation==sessionGeneration_)launch();});
            return;
        }
        handoff_.stop();
        active_=false;deadline_=0;timer_.stop();
        client_.stop();
        rmgLobby_.stop();rmgPreparing_=false;
        // An ordinary game ending must not discard an unanswered invitation.
        if(partySession_&&party_.active()){party_.leave();partySession_=false;}
        update();
        if(!lobbyTicket_.isEmpty()) {
            const auto ticket=lobbyTicket_;
            QTimer::singleShot(0,this,[this,ticket]{social_.lobbyRuntime()->finished(ticket);});
        }
    });
    connect(&lifecycle_,&AdventureLaunchController::changed,this,[this]{
        // Preparation can fail before a child process emits adventureFinished.
        if(active_&&!restarting_&&lifecycle_.state()=="failed")fail(lifecycle_.error());
        emit changed();
    });
    connect(&timer_,&QTimer::timeout,this,[this]{
        if(deadline_&&QDateTime::currentSecsSinceEpoch()>deadline_){fail("Couldn't connect to your friend. Try inviting again.");return;}
        if(active_&&host_&&online_&&!restarting_&&!launchPending_&&!query_&&!relaySent_)pollRelay();
    });timer_.setInterval(3000);
}
void RuntimeMultiplayer::refresh(const RetroArchInstallation& installation,QString trainer,bool allowed) {
    if(trainer_!=trainer)party_.reset();
    trainer_=std::move(trainer);allowed_=allowed;
    installation_=installation;
    if(!allowed_) {
        games_.clear();
        if(!multiplayerGames_.isEmpty() || !gameModes_.isEmpty()){gameModes_.clear();onlineGames_.clear();nearbyGames_.clear();multiplayerGames_.clear();emit availabilityChanged();}
        update();return;
    }
    const auto records=library_.registrations();
    const auto pspInstallation=ppsspp_.installation();
    const auto dolphinInstallation=dolphin_.installation();
    const auto stamp=[](const QString& path){const QFileInfo f(path);return qHashMulti(0,path,f.size(),f.lastModified().toMSecsSinceEpoch());};
    quint64 revision=qHashMulti(0,stamp(installation.runtimeFile),stamp(installation.configFile),
        stamp(pspInstallation.runtimeFile),stamp(dolphin::bridgeFile()),stamp(dolphin::bridgeRoot()+"/bin/dolphin-tool"),stamp(dolphin::bridgeRoot()+"/manifest.json"),
        stamp(installation.linkedSavePython),stamp(installation.linkedSaveHelper),stamp(ps2::bundleRoot()+"/manifest.json"),stamp(rmg::bundleRoot()+"/manifest.json"),stamp(flycast::bundleRoot()+"/manifest.json"),stamp(melonds::bundleRoot()+"/manifest.json"),stamp(azahar::bundleRoot()+"/manifest.json"),stamp(vita::bundleRoot()+"/manifest.json"));
    for(auto it=installation.cores.cbegin();it!=installation.cores.cend();++it)revision=qHashMulti(revision,it.key(),stamp(it.value()));
    const auto candidate=[](const AdventureRegistration& r){return r.adventure.platformId=="ps2"||r.adventure.platformId=="psvita"||r.adventure.platformId=="n3ds"||r.adventure.platformId=="nds"||r.adventure.platformId=="psp"||r.adventure.platformId=="gc"||r.adventure.platformId=="wii"||r.adventure.platformId=="n64"||r.adventure.platformId=="dreamcast"||
        retroarch::netplaySupported(r.adventure.platformId,r.integrationConfig["core"].toString());};
    QMap<QString,int> capacities;
    QMap<QString,QVariantMap> metadata;
    for(const auto& r:records)if(candidate(r)) {
        const auto core=r.integrationConfig["core"].toString();
        const auto info=library_.artwork(r.adventure.id);metadata.insert(r.adventure.id,info);
        const int capacity=(r.adventure.platformId=="gc"||r.adventure.platformId=="wii"||r.adventure.platformId=="n64")?qBound(2,gamePlayers(info).maximum,4):
            retroarch::netplayCapacity(r.adventure.platformId,core,info,r.contentPath);
        capacities.insert(r.adventure.id,capacity);
        revision=qHashMulti(revision,r.adventure.id,stamp(r.contentPath),r.revision,core,capacity);
    }
    const auto playing=process_.runtimeControls()["game"].toString();
    if(scan_.isRunning()&&!playing.isEmpty()&&playing!=scanPriority_&&capacities.contains(playing)&&!profiles_.contains(playing)) {
        scan_.cancel();scanRevision_=0;
    }
    if(!revision)revision=1;
    // Library discovery can finish while an older disc scan is still running.
    // Retire it at the next file boundary instead of hashing the obsolete list.
    if(scan_.isRunning()&&scanRevision_!=revision)scan_.cancel();
    if(!scan_.isRunning() && (scanRevision_!=revision||!scanRevision_)) {
        scanRevision_=revision;scanPriority_=playing;
        profiles_.clear();
        auto ordered=records;
        std::stable_sort(ordered.begin(),ordered.end(),[&](const auto& a,const auto& b){
            if((a.adventure.id==playing)!=(b.adventure.id==playing))return a.adventure.id==playing;
            return QFileInfo(a.contentPath).size()<QFileInfo(b.contentPath).size();
        });
        // Publish each verified title immediately. A large disc image must not
        // hide every already-checked cartridge until the entire scan finishes.
        scan_.setFuture(QtConcurrent::run([ordered,installation,pspInstallation,dolphinInstallation,candidate,capacities](QPromise<ScannedGame>& promise){
            std::atomic_bool cancel=false;
            retroarch::NetplayDigestCache digests;
            for(const auto& r:ordered)if(candidate(r)) {
                if(promise.isCanceled())break;
                const auto identity=r.adventure.platformId=="ps2"?ps2::netplayIdentity(r,installation,cancel):r.adventure.platformId=="psvita"?vita::netplayIdentity(r,cancel):r.adventure.platformId=="n3ds"?azahar::netplayIdentity(r,cancel):r.adventure.platformId=="nds"?melonds::netplayIdentity(r,cancel):r.adventure.platformId=="dreamcast"?flycast::netplayIdentity(r,cancel):r.adventure.platformId=="n64"?rmg::netplayIdentity(r,cancel,capacities.value(r.adventure.id,2)):(r.adventure.platformId=="gc"||r.adventure.platformId=="wii")?dolphin::netplayIdentity(r,dolphinInstallation,cancel,capacities.value(r.adventure.id,2)):r.adventure.platformId=="psp"?ppsspp::netplayIdentity(r,pspInstallation,cancel):retroarch::netplayIdentity(r,installation,cancel,&digests,capacities.value(r.adventure.id,2));
                if(!identity.isEmpty())promise.addResult(ScannedGame{r.adventure.id,identity});
            }
        }));
    }
    games_.clear();
    for(auto it=profiles_.cbegin();it!=profiles_.cend();++it)
        if(metadata.contains(it.key())&&permitsMultiplayer(metadata.value(it.key()),it.value()))games_.insert(it.key(),it.value());
    QVariantList lobbyGames;
    if(allowed_)for(const auto& record:records) {
        const auto profile=games_.value(record.adventure.id);
        const auto content=profile["content"].toString();const int capacity=profile["players"].toInt(2);
        if(content.isEmpty()||capacity<1||capacity>16||!multiplayerConnectionAllowed(profile,"online"))continue;
        const auto id=QString::fromLatin1(QCryptographicHash::hash((profile["id"].toString()+"/"+content).toUtf8(),QCryptographicHash::Sha256).toHex());
        lobbyGames.append(QVariantMap{{"key",record.adventure.id},{"id",id},{"title",profile["label"].toString()},
            {"platform",record.adventure.platformId},{"mode",profile["id"].toString()},{"modeLabel",multiplayerPresentation(profile).value("label",QString("Emulator multiplayer"))},
            {"cover",library_.artwork(record.adventure.id).value("cover")},{"seats",qMin(4,capacity)},{"runtime",profile.toVariantMap()}});
        social_.lobbyRuntime()->registerService(profile["id"].toString(),this);
    }
    QVariantMap modes;
    for(auto it=games_.cbegin();it!=games_.cend();++it) {
        const auto mode=multiplayerPresentation(it.value());
        if(!mode.isEmpty())modes.insert(it.key(),QVariantList{mode});
    }
    if(additionalLobbyGames) {
        const auto additional=additionalLobbyGames(games_);lobbyGames+=additional;
        for(const auto& value:additional) {
            const auto row=value.toMap();const auto game=row["adventure"].toString();
            if(row["mode"]!="remoteplay.v1" || !games_.contains(game))continue;
            auto list=modes.value(game).toList();
            list.append(QVariantMap{{"id","remoteplay"},{"label","Remote Play"},{"nearby",false},{"online",true}});modes.insert(game,list);
        }
    }
    if(gameModes_!=modes){gameModes_=modes;emit availabilityChanged();}
    social_.setLobbyGames(lobbyGames);
    const auto visible=allowed_?games_.keys():QStringList{};
    QStringList online,nearby;
    for(const auto& id:visible) {
        if(multiplayerConnectionAllowed(games_.value(id),"online"))online.append(id);
        if(multiplayerConnectionAllowed(games_.value(id),"nearby"))nearby.append(id);
    }
    if(visible!=multiplayerGames_||online!=onlineGames_||nearby!=nearbyGames_) {
        multiplayerGames_=visible;onlineGames_=online;nearbyGames_=nearby;emit availabilityChanged();
    }
    update();
}
QString RuntimeMultiplayer::menuLabel() const {return !invitationId().isEmpty()?"Incoming activity":!social_.lobbyState().isEmpty()?"My lobby":party_.active()?"Game party":"Play together";}
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
    if(active_){fail("Invitation cancelled.");return;}
    party_.leave();emit changed();
}
bool RuntimeMultiplayer::incoming() const {return !party_.pending().isEmpty()&&!lifecycle_.active()
    &&social_.surfaceAvailable()&&!social_.account()["doNotDisturb"].toBool();}
bool RuntimeMultiplayer::invitationBadge() const {
    return allowed_&&!invitationId().isEmpty()&&exitingInvitation_.isEmpty()
        &&openingInvitation_.isEmpty()&&!social_.account()["doNotDisturb"].toBool()
        &&!(socialSurface_&&social_.runtimeSurfaceOpen())&&!(social_.surfaceAvailable()&&social_.face()=="lobby")&&!overlay_.visible()
        &&lifecycle_.exitController().phase()==AdventureExitController::Phase::Idle;
}
QString RuntimeMultiplayer::invitationId() const {
    const auto invite=social_.lobbyCoordinator()->invitation();
    if(!invite.isEmpty())return "lobby:"+invite["host"].toString()+":"+invite["lobby"].toString();
    return party_.pending()["request"].toString();
}
bool RuntimeMultiplayer::canJoinRunning(const QJsonObject& invitation) const {
    const auto current=lifecycle_.adventureId();
    return lifecycle_.state()=="running" && games_.contains(current)
        && sameMultiplayerGame(games_.value(current),invitation)
        && runningJoin.available && runningJoin.join && runningJoin.available(current,invitation);
}
void RuntimeMultiplayer::openInvitation(const QString& request) {
    if(!allowed_||request.isEmpty()||request!=invitationId()||!exitingInvitation_.isEmpty())return;
    if(request.startsWith("lobby:")) {
        social_.openLobby();
        for(int i=0;i<social_.lobbyRows().size();++i)if(social_.lobbyRows()[i].toMap()["kind"]=="invitation") {social_.selectLobby(i);break;}
        return;
    }
    if(!lifecycle_.active()){emit invitationSurfaceRequested();socialSurface_=true;show("multiplayer-request");return;}
    if(overlay_.menuOpen()&&!lifecycle_.minimized()){socialSurface_=false;show("multiplayer-request");return;}
    if(lifecycle_.state()!="running"||!lifecycle_.exitController().available()) {
        emit notice("Game controls are temporarily unavailable. The invitation is still in Notifications.");return;
    }
    openingInvitation_=request;invitationOpenTimer_.start();
    emit changed();emit invitationOptionsRequested();
}
void RuntimeMultiplayer::resumeInvitation() {
    if(openingInvitation_.isEmpty()||!overlay_.menuOpen()||!overlay_.ready()||lifecycle_.minimized())return;
    const auto request=std::exchange(openingInvitation_,{});invitationOpenTimer_.stop();
    if(request==invitationId()&&allowed_){socialSurface_=false;show("multiplayer-request");}
    emit changed();
}
void RuntimeMultiplayer::cancelInvitationExit() {
    if(exitingInvitation_.isEmpty())return;
    exitingInvitation_.clear();
    // A close already delivered to the owned emulator cannot be undone. Its
    // normal save settlement continues, but no stale invitation is accepted.
    lifecycle_.exitController().cancel();
    emit changed();
}
QString RuntimeMultiplayer::invitation() const {
    const auto pending=party_.pending();
    const auto game=pending["game"].toObject();
    return pending["name"].toString()+(pending["joining"].toBool()?" wants to join ":" invites you to play ")+game["label"].toString()+"."+(usesSharedController(game)?" Take turns with one controller.":QString())+(game["privateProgress"].toBool()?" Save copies are shared with this session's players. Only your own progress returns.":game["temporaryProgress"].toBool()?" This session starts fresh. Progress isn't kept.":QString());
}
void RuntimeMultiplayer::answer(bool accept) {
    if(accept && lifecycle_.active() && !party_.pending()["joining"].toBool() && !canJoinRunning(party_.pending()["game"].toObject())) {
        if(!allowed_ || invitationId().isEmpty() || !exitingInvitation_.isEmpty())return;
        if(!overlay_.menuOpen() || lifecycle_.minimized()){openInvitation(invitationId());return;}
        if(!overlay_.ready())return;
        exitingInvitation_=invitationId();invitationAccount_=social_.accountGeneration();invitationProcess_=process_.processId();
        if(!overlay_.exitFromMenu()){exitingInvitation_.clear();emit notice("Your game couldn't close safely. Keep playing and try again.");}
        emit changed();return;
    }
    party_.answer(accept);
    if(party_.host()&&party_.state()["members"].toArray().size()>1)online_=party_.state()["members"].toArray().at(1).toObject()["peer"].toString().startsWith("online:");
}
bool RuntimeMultiplayer::dispatch(Action a) {
    if(!incoming())return false;
    if(a==Action::Confirm)answer(true);else if(a==Action::Back)answer(false);return true;
}
void RuntimeMultiplayer::update() {
    if(!active_&&!party_.active()) {
        const auto current=process_.runtimeControls()["game"].toString();
        if(game_!=current)controllerMode_="separate";
        game_=current;descriptor_=selectedControllerProfile(games_.value(game_));
    }
    // Receiving an invitation is independent of the current game's netplay
    // support. An unrelated single-player game may keep running while saving.
    const bool available=allowed_&&!games_.isEmpty();
    QJsonArray caps;
    if(available)for(const auto& descriptor:games_)if(!caps.contains(descriptor))caps.append(descriptor);
    const bool rooms=allowed_&&social_.account()["state"]=="connected";
    party_.configure(trainer_,caps,selectedControllerProfile(games_.value(process_.runtimeControls()["game"].toString())),available||rooms);
    if(social_.conversationVisible()&&!social_.runtimePeer().isEmpty())party_.query("online:"+social_.runtimePeer());
    social_.setRuntimeContext(available||rooms,{});
    social_.refreshPartyBrowse();
    nearby_.configure(identity_,trainer_);
    if(available){nearby_.open();nearby_.setVisible(true);}else if(!active_){nearby_.close();}
    if(overlay_.panel()=="multiplayer-wait")show("multiplayer-wait");
    emit changed();
}
void RuntimeMultiplayer::show(QString panel) {
    QVariantList rows;QString caption;
    const auto offered=party_.active()?party_.game():descriptor_;
    const bool onlineAvailable=multiplayerConnectionAllowed(offered,"online");
    const bool nearbyAvailable=multiplayerConnectionAllowed(offered,"nearby");
    if(panel=="multiplayer"&&!party_.pending().isEmpty())panel="multiplayer-request";
    if(panel=="multiplayer"&&party_.active())panel="multiplayer-party";
    if(panel=="multiplayer")panel="multiplayer-people";
    if(panel=="multiplayer-request") {
        const auto request=party_.pending()["request"].toString();
        caption=request.isEmpty()?"This invitation is no longer available.":invitation();
        if(!request.isEmpty()) {
            const bool close=lifecycle_.active()&&!party_.pending()["joining"].toBool()&&!canJoinRunning(party_.pending()["game"].toObject());
            rows={row("multiplayer-accept:"+request,close?"Saved — join":"Accept",close?"Close this game, then join your friend":QString()),
                  row("multiplayer-later:"+request,"Keep for later",close?"Continue playing and save first":"Keep the invitation in the corner"),
                  row("multiplayer-decline:"+request,"Decline")};
        }
    } else if(panel=="multiplayer-party") {
        const auto state=party_.state();caption=state["game"].toObject()["label"].toString()+" · "+QString::number(state["members"].toArray().size())+" / "+QString::number(state["capacity"].toInt())+" players";
        if(usesSharedController(state["game"].toObject())) {
            auto info=row("multiplayer-mode-info","Take turns with one controller","Pass it from Game Options when your turn ends.");
            info["readOnly"]=true;rows.append(info);
            for(const auto& action:gameMenuActions())if(action.toMap()["id"]!="multiplayer")rows.append(action);
        }
        if(state["game"].toObject()["temporaryProgress"].toBool()) {
            auto info=row("multiplayer-info","Temporary progress","This session starts fresh. Progress isn't kept.");
            info["readOnly"]=true;rows.append(info);
        }
        if(state["game"].toObject()["id"].toString().startsWith("runtime.ppsspp.")) {
            auto info=row("multiplayer-info","Choose Ad Hoc in the game","Game compatibility varies; original online servers are separate.");
            info["readOnly"]=true;rows.append(info);
        }
        bool ready=true;
        for(const auto& value:state["members"].toArray()) {
            const auto m=value.toObject();ready&=m["ready"].toBool();
            auto r=row(m["ready"].toBool()?"multiplayer-member":"multiplayer-revoke:"+m["peer"].toString(),m["name"].toString(),m["ready"].toBool()?"Ready":"Invited - select to cancel");
            r["readOnly"]=m["ready"].toBool();rows.append(r);
        }
        if(!party_.pending().isEmpty())rows.prepend(row("multiplayer-request","Join request",party_.pending()["name"].toString()));
        if(party_.host()&&!party_.running()&&ready&&state["members"].toArray().size()>1)rows.prepend(row("multiplayer-start","Start game",party_.game()["privateProgress"].toBool()?"Save copies shared; only your progress returns.":QString()));
        if(party_.host()&&state["free"].toInt()>0)rows.append(row("multiplayer-people","Invite players"));
        if(party_.host()&&!state["company"].toString().isEmpty())rows.append(row("multiplayer-access","Who can join",state["access"]=="closed"?"Invitations only":state["access"]=="selected"?"Selected group members":"Ask me first"));
        if(state["joining"].toBool())caption="Join request sent · "+state["game"].toObject()["label"].toString();
        if(party_.active()||state["joining"].toBool())rows.append(row("multiplayer-leave",state["joining"].toBool()?"Cancel join request":party_.host()?"End party":"Leave party"));
        else caption="No active game party";
    } else if(panel=="multiplayer-access") {
        caption="This game party";
        rows={row("multiplayer-access:default","Use group setting"),row("multiplayer-access:request","Ask me first"),
            row("multiplayer-access:selected","Selected group members"),row("multiplayer-access:closed","Invitations only")};
    } else if(panel=="multiplayer-people") {
        caption="Choose a person or group";
        if(!party_.active()&&games_.value(game_)["controllerModes"].toArray().contains("pass-p1"))
            rows.append(row("multiplayer-mode:"+game_+":"+(controllerMode_=="separate"?"pass-p1":"separate"),
                controllerModeLabel(),"Change how the controllers are used"));
        QSet<QString> identities;
        if(onlineAvailable&&!party_.active())for(const auto& p:social_.runtimeCompanies()) {const auto r=p.toMap();const auto id="multiplayer-company:"+r["id"].toString();if(identities.contains(id))continue;identities.insert(id);rows.append(row(id,r["name"].toString(),"Group"));}
        if(onlineAvailable&&(!party_.active()||online_))for(const auto& p:social_.runtimeFriends()) {const auto r=p.toMap();const auto id="multiplayer-friend:"+r["id"].toString();if(identities.contains(id))continue;identities.insert(id);rows.append(row(id,r["name"].toString(),"Online friend"));}
        // Discovery UUIDs have no authenticated account binding. Never merge
        // nearby and online identities just because their display names match.
        if(nearbyAvailable&&(!party_.active()||!online_))for(const auto& p:nearby_.peers()) {const auto r=p.toMap();const auto id="multiplayer-local:"+r["id"].toString();if(identities.contains(id))continue;identities.insert(id);rows.append(row(id,r["name"].toString(),"Nearby"));}
        if(!onlineAvailable)caption="Nearby play";
        if(rows.isEmpty())caption=onlineAvailable?"Looking for nearby Trainers. Friends appear when connected.":"Looking for nearby Trainers. Online play is unavailable.";
    } else if(panel=="multiplayer-nearby") {
        caption="Choose a nearby Trainer";
        if(nearbyAvailable)for(const auto& p:nearby_.peers()){const auto r=p.toMap();rows.append(row("multiplayer-local:"+r["id"].toString(),r["name"].toString()));}
        if(rows.isEmpty())caption="Looking for nearby Trainers…";
    } else if(panel=="multiplayer-online") {
        caption="Choose a friend or group";
        if(onlineAvailable&&!party_.active())for(const auto& p:social_.runtimeCompanies()){const auto r=p.toMap();rows.append(row("multiplayer-company:"+r["id"].toString(),r["name"].toString(),"Play with this group"));}
        if(onlineAvailable)for(const auto& p:social_.runtimeFriends()){const auto r=p.toMap();rows.append(row("multiplayer-friend:"+r["id"].toString(),r["name"].toString()));}
        if(rows.isEmpty())caption=onlineAvailable?"Add a friend in Social first":"Online play is unavailable.";
    } else {
        caption=status_;
        if(caption.isEmpty())caption="Connecting to your friend…";
    }
    rows.append(row("multiplayer-cancel","Back"));
    if(socialSurface_){socialPanel_=panel;social_.setRuntimeSurface(panel=="multiplayer-people"?"Invite players":panel=="multiplayer-request"?"Incoming activity":"Game party",caption,rows);}
    else overlay_.setPanel(panel,caption,rows,"multiplayer-cancel");
}
bool RuntimeMultiplayer::action(const QString& id,bool fromSocial) {
    if(!id.startsWith("multiplayer"))return false;
    if(preparation_.action(id))return true;
    if(id=="multiplayer"&&social_.account()["state"]=="connected") {
        if(!invitationId().isEmpty())openInvitation(invitationId());
        else if(!social_.lobbyState().isEmpty())social_.openLobby();
        else beginTogether(lifecycle_.adventureId());
        return true;
    }
    if(id=="multiplayer-party"&&!lobbyTicket_.isEmpty()) {social_.chooseMyLobby(social_.lobbyCoordinator()->state()["id"].toString());return true;}
    socialSurface_=fromSocial;
    if(id.startsWith("multiplayer-pass:")) {
        if(active_&&hotseat()&&handoff_.canPass()&&id.section(':',1,1)==party_.state()["party"].toString()&&
           id.section(':',2,2)==QString::number(handoff_.epoch())&&id.count(':')==2&&
           process_.runtimeControls().contains("hotseat")) {
            // This action was deliberately consumed. Return focus to Continue
            // before its epoch-bound button disappears; keep stale clicks inert.
            if(!fromSocial&&overlay_.menuOpen())overlay_.setPanel({}, {}, {});
            handoff_.pass();
        }
        return true;
    }
    if(id.startsWith("multiplayer-mode:")) {
        const auto mode=id.section(':',-1),game=id.mid(17).section(':',0,-2);
        if(!active_&&!party_.active()&&canInvite()&&game==game_&&
           !multiplayerModeProfile(games_.value(game_),mode).isEmpty()) {
            if(!fromSocial&&overlay_.menuOpen())overlay_.setPanel({}, {}, {});
            controllerMode_=mode;update();show("multiplayer-people");
        }
        return true;
    }
    if(id.startsWith("multiplayer-later:")) {
        if(id.section(':',1)==invitationId()) {
            if(socialSurface_){socialSurface_=false;social_.closeMenu();}
            else overlay_.dismissMenu();
        }
        return true;
    }
    if(id.startsWith("multiplayer-request:")||id.startsWith("multiplayer-accept:")||id.startsWith("multiplayer-decline:")) {
        if(id.section(':',1)!=party_.pending()["request"].toString())return true;
        if(id.startsWith("multiplayer-request:"))show("multiplayer-request");
        else {answer(id.startsWith("multiplayer-accept:"));if(exitingInvitation_.isEmpty())show(party_.active()?"multiplayer-party":"multiplayer-request");}
        return true;
    }
    if(id=="multiplayer-access"){show(id);return true;}
    if(id.startsWith("multiplayer-access:")) {
        const auto company=party_.state()["company"].toString();auto access=social_.companyAccess(company);
        companyOverride_=id.section(':',1)!="default";
        if(companyOverride_)access["policy"]=id.section(':',1);
        party_.setCompanyAccess(company,access["policy"].toString(),access["allowed"].toStringList());show("multiplayer-party");return true;
    }
    if(id=="multiplayer-start"){if(socialSurface_)social_.closeMenu();party_.start();return true;}
    if(id=="multiplayer-leave"){leaveParty();if(socialSurface_)social_.closeMenu();else overlay_.setPanel({}, {}, {});return true;}
    if(id.startsWith("multiplayer-revoke:")){party_.cancelInvite(id.mid(QStringLiteral("multiplayer-revoke:").size()));return true;}
    if(id=="multiplayer-accept"||id=="multiplayer-decline"){answer(id=="multiplayer-accept");show(party_.active()?"multiplayer-party":"multiplayer");return true;}
    if(id=="multiplayer-party"||id=="multiplayer-request"){show(id);return true;}
    if(id=="multiplayer-cancel"){if(socialSurface_){socialSurface_=false;social_.closeMenu();}else if(overlay_.panel()=="multiplayer-request")overlay_.dismissMenu();else overlay_.setPanel({}, {}, {});return true;}
    if(id=="multiplayer"||id=="multiplayer-people"||id=="multiplayer-nearby"||id=="multiplayer-online"){if(canInvite()||id=="multiplayer"&&!invitationId().isEmpty())show(id);return true;}
    if(!canInvite()) {if(fromSocial)emit notice("Start a supported multiplayer game before inviting players.");return true;}
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
    for(auto it=games_.cbegin();it!=games_.cend();++it)if(supportsMultiplayerSession(it.value(),selected)){game=it.key();break;}
    // Preserve the running edition when several compatible handheld games exist.
    const auto current=process_.runtimeControls()["game"].toString();
    for(auto it=games_.cbegin();it!=games_.cend();++it)
        if(supportsMultiplayerSession(it.value(),selected)&&it.value()["content"]==selected["content"]){game=it.key();break;}
    if(games_.contains(current)&&supportsMultiplayerSession(games_.value(current),selected))game=current;
    if(!lobbyTicket_.isEmpty()) {
        const auto record=library_.registration(lobbyGame_);
        game=record&&!record->removed&&record->contentAvailable&&record->revision==lobbyRevision_
            &&record->contentPath==lobbyPath_&&games_.value(lobbyGame_)==lobbyDescriptor_
            &&supportsMultiplayerSession(lobbyDescriptor_,selected)?lobbyGame_:QString();
    }
    if(!allowed_||game.isEmpty()||!permitsMultiplayer(library_.artwork(game),selected)) {
        party_.leave();emit notice("Multiplayer isn't available for this game.");return;
    }
    if(!host&&lifecycle_.active()) {
        if(canJoinRunning(selected)&&runningJoin.join(lifecycle_.adventureId(),selected,endpoint)) {
            game_=lifecycle_.adventureId();descriptor_=selected;partySession_=active_=true;host_=false;
            deadline_=0;timer_.stop();status_="Playing together";overlay_.dismissMenu();emit changed();return;
        }
        // A replacement game may have started after consent. Never close that
        // process, or reinterpret a missing native join as permission to restart.
        party_.leave();emit notice("The running game can't join this invitation. Open the invitation again after saving.");return;
    }
    ++sessionGeneration_;
    handoff_.stop();
    rmgLobby_.stop();rmgPreparing_=false;rmgRequest_={};flycastRequest_={};dsRequest_={};azaharRequest_={};vitaRequest_={};
    stopLinkedTransfers();linkedPeers_.clear();linkedPreparing_=linkedPrepared_=false;linkedEndpoint_={};
    game_=game;descriptor_=selected;partySession_=true;active_=true;host_=host;relaySent_=false;
    const auto roster=party_.state()["members"].toArray();
    transportPeer_=host?(roster.size()>1?roster.at(1).toObject()["peer"].toString():QString()):party_.hostPeer();
    online_=!lobbyTicket_.isEmpty()||transportPeer_.startsWith("online:");
    request_={};request_.host=host;request_.relay=online_;request_.expected=selected;
    request_.localContent=games_.value(game)["content"].toString();
    request_.slot=host?1:endpoint["slot"].toInt();
    if(linked()) {
        const auto roster=party_.state()["members"].toArray();
        request_.linkedPlayers=roster.size();
        for(const auto& v:roster) {
            const auto m=v.toObject();const int slot=m["slot"].toInt();
            if(slot<1||slot>roster.size()||linkedPeers_.contains(slot)||!m["ready"].toBool()) {
                fail("The game party changed. Invite your friends again.");return;
            }
            linkedPeers_[slot]=m["peer"].toString();
        }
        if(request_.linkedPlayers<2||request_.linkedPlayers>descriptor_["players"].toInt(2)||
           (!host&&endpoint["machines"].toInt()!=request_.linkedPlayers)) {
            fail("This linked-game invitation is invalid.");return;
        }
    }
    request_.nickname="TrainerOS-"+randomToken().left(12);output_.clear();
    deadline_=QDateTime::currentSecsSinceEpoch()+((ds()||azahar()||vita()||(rmg()&&online_))?180:dolphin()?150:75);timer_.start();
    dolphinRequest_={};dolphinRequest_.expected=selected;dolphinRequest_.host=host;dolphinRequest_.online=online_;
    if(host&&vita()) {
        vitaRequest_={selected,{},true,online_,randomToken()+randomToken(),nearby_.localAddressFor(transportPeer_.mid(7))};
        prepareHost();
    } else if(host&&azahar()) {
        azaharRequest_={selected,{},true,online_,randomToken()+randomToken(),nearby_.localAddressFor(transportPeer_.mid(7))};
        prepareHost();
    } else if(host&&ds()) {
        dsRequest_={selected,{},true,online_,randomToken()+randomToken(),nearby_.localAddressFor(transportPeer_.mid(7))};
        prepareHost();
    } else if(host&&flycast()) {
        flycastRequest_={selected,true,online_,nearby_.addressOf(transportPeer_.mid(7)),randomToken(),{}};
        prepareHost();
    } else if(host&&rmg()) {
        rmgRequest_.host=true;rmgRequest_.identity=selected;rmgRequest_.token=randomToken();
        for(const auto& v:party_.state()["members"].toArray())rmgRequest_.seats.append(v.toObject()["slot"]);
        prepareHost();
    } else if(host&&dolphin()) {
        dolphinRequest_.token=randomToken();
        for(const auto& v:party_.state()["members"].toArray())dolphinRequest_.seats.append(v.toObject()["slot"]);
        prepareHost();
    } else if(host) {
        request_.password=randomToken();
        if(psp()){if(online_)resolvePspRelay();else {request_.address=nearby_.localAddressFor(transportPeer_.mid(7));prepareHost();}}
        else if(online_)resolveRelay();else prepareHost();
    } else if(linked())prepareLinked(endpoint);else frame(endpoint);
    emit changed();
}
void RuntimeMultiplayer::prepareHost() {
    launchPending_=true;
    if(lifecycle_.active()) {
        restarting_=true;
        // The player may need time to save before confirming the ordinary
        // game's exit. Its capture/close controller has its own bounded waits;
        // a network connection has not started yet.
        deadline_=0;timer_.stop();
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
    if(!active_)return; // A cancelled invitation must not launch a queued game.
    deadline_=QDateTime::currentSecsSinceEpoch()+((ds()||azahar()||vita()||(rmg()&&online_))?180:dolphin()?150:75);timer_.start();
    const auto record=library_.registration(game_);
    if(!record){fail("The game is no longer in your library.");return;}
    if(!permitsMultiplayer(library_.artwork(game_),descriptor_)){fail("Multiplayer isn't available for this game.");return;}
    if(vita()) {
        if(!requestRuntimeLaunch){fail("Vita multiplayer isn't ready on this device.");return;}
        QStringList inputPrefix;const auto end=installation_.prefixArguments.indexOf("--");
        if(end>=0)inputPrefix=installation_.prefixArguments.mid(0,end+1);
        const auto result=requestRuntimeLaunch(vita::command(*record,vitaRequest_,inputPrefix),game_);
        if(!result.success)fail(result.message);
        return;
    }
    if(azahar()) {
        if(!requestRuntimeLaunch){fail("3DS multiplayer isn't ready on this device.");return;}
        QStringList inputPrefix;const auto end=installation_.prefixArguments.indexOf("--");
        if(end>=0)inputPrefix=installation_.prefixArguments.mid(0,end+1);
        const auto result=requestRuntimeLaunch(azahar::command(*record,azaharRequest_,inputPrefix),game_);
        if(!result.success)fail(result.message);
        return;
    }
    if(ds()) {
        if(!requestRuntimeLaunch||saveBackupRoot.isEmpty()||!resolveSaveTarget){fail("DS save protection isn't ready on this device.");return;}
        QStringList inputPrefix;const auto end=installation_.prefixArguments.indexOf("--");
        if(end>=0)inputPrefix=installation_.prefixArguments.mid(0,end+1);
        const auto result=requestRuntimeLaunch(melonds::command(*record,dsRequest_,saveBackupRoot,resolveSaveTarget,inputPrefix),game_);
        if(!result.success)fail(result.message);
        return;
    }
    if(flycast()) {
        if(!requestRuntimeLaunch){fail("Dreamcast multiplayer isn't ready on this device.");return;}
        QStringList inputPrefix;const auto end=installation_.prefixArguments.indexOf("--");
        if(end>=0)inputPrefix=installation_.prefixArguments.mid(0,end+1);
        const auto result=requestRuntimeLaunch(flycast::command(*record,flycastRequest_,inputPrefix),game_);
        if(!result.success)fail(result.message);
        return;
    }
    if(descriptor_["id"]=="runtime.retroarch.ps2.pcsx2.v1") {
        if(!requestRuntimeLaunch){fail("PlayStation 2 multiplayer isn't ready on this device.");return;}
        const auto result=requestRuntimeLaunch(ps2::command(*record,installation_,request_),game_);
        if(!result.success)fail(result.message);
        return;
    }
    if(rmg()){prepareRmg();return;}
    if(linked()&&!linkedPrepared_){if(host_)prepareLinked();return;}
    const auto result=dolphin()?dolphin_.launchNetplay(record->adventure,dolphinRequest_):psp()?ppsspp_.launchNetplay(record->adventure,
        ppsspp::NetplayRequest{descriptor_,host_,online_,request_.address,trainer_}):adapter_.launchNetplay(record->adventure,request_);
    if(!result.success)fail(result.message);
    // The launch worker owns its snapshot; do not retain peer progress in UI state.
    request_.ownSram.clear();request_.peerSrams.clear();
}
void RuntimeMultiplayer::prepareRmg() {
    if(!active_||rmgPreparing_)return;
    rmgPreparing_=true;launchPending_=true;status_="Connecting Nintendo 64 players...";
    const auto generation=sessionGeneration_;
    auto* watcher=new QFutureWatcher<QJsonObject>(this);
    connect(watcher,&QFutureWatcherBase::finished,this,[this,watcher,generation]{
        const auto installed=watcher->result();watcher->deleteLater();
        if(!active_||!rmg()||generation!=sessionGeneration_)return;
        if(installed.isEmpty()||installed["digest"]!=descriptor_["runtime"]){fail("The Nintendo 64 setup changed. Invite again.");return;}
        rmgRequest_.identity=descriptor_;rmgRequest_.host=host_;
        if(online_&&((host_&&installed["privateLanVersion"]==1)||rmgRequest_.privateLan)) {
            if(installed["privateLanVersion"]!=1||!requestRuntimeLaunch){fail("Nintendo 64 Online isn't ready on this device.");return;}
            const auto record=library_.registration(game_);
            if(!record){fail("The game is no longer in your library.");return;}
            rmgRequest_.privateLan=true;
            QStringList inputPrefix;const auto end=installation_.prefixArguments.indexOf("--");
            if(end>=0)inputPrefix=installation_.prefixArguments.mid(0,end+1);
            const auto result=requestRuntimeLaunch(rmg::privateCommand(*record,rmgRequest_,inputPrefix),game_);
            if(!result.success){fail(result.message);return;}
            launchPending_=false;status_="Opening Nintendo 64 multiplayer...";
            return;
        }
        if(online_)rmgRequest_.server=QUrl(installed["onlineServer"].toString());
        else {
            const auto address=host_?QString("127.0.0.1"):nearby_.addressOf(transportPeer_.mid(7));
            rmgRequest_.server=QUrl("ws://"+address+":45000");
            if(host_)rmgRequest_.localServer=rmg::bundleRoot()+"/bin/rmg-netplay-server";
        }
        rmgLobby_.start(rmgRequest_);
    });
    watcher->setFuture(QtConcurrent::run([]{const std::atomic_bool cancel=false;return rmg::bundle(cancel);}));
}
void RuntimeMultiplayer::prepareLinked(const QJsonObject& endpoint) {
    if(!active_||linkedPreparing_||linkedPrepared_)return;
    if(!host_&&(endpoint["kind"]!="linked-prepare"||endpoint["identity"].toObject()!=descriptor_||
       endpoint["preparation"].toObject()["mode"]!=(online_?"online":"nearby"))) {
        fail("This linked-game invitation is invalid.");return;
    }
    const auto record=library_.registration(game_);
    if(!record){fail("The game is no longer in your library.");return;}
    linkedPreparing_=true;launchPending_=true;status_="Preparing your linked game...";
    deadline_=QDateTime::currentSecsSinceEpoch()+90;timer_.start();emit changed();
    auto* watcher=new QFutureWatcher<retroarch::LinkedSaveSeed>(this);
    const auto generation=sessionGeneration_;
    connect(watcher,&QFutureWatcherBase::finished,this,[this,watcher,generation,endpoint]{
        const auto seed=watcher->result();watcher->deleteLater();
        if(!active_||generation!=sessionGeneration_||!linkedPreparing_)return;
        if(!seed.error.isEmpty()){fail(seed.error);return;}
        request_.ownSram=seed.bytes;request_.ownSramExisted=seed.existed;request_.ownSramOriginalSize=seed.originalSize;
        auto config=host_?QJsonObject{}:endpoint["preparation"].toObject();
        config["host"]=host_;config["mode"]=online_?"online":"nearby";
        config["content"]=descriptor_["content"];config["size"]=seed.bytes.size();
        if(!host_) {
            config["data"]=QString::fromLatin1(seed.bytes.toBase64());
            if(!online_)config["address"]=nearby_.addressOf(transportPeer_.mid(7));
            linkedSave_.start(installation_.linkedSavePython,installation_.linkedSaveHelper,config);
            return;
        }
        for(int slot=2;slot<=request_.linkedPlayers;++slot) {
            if(!active_||generation!=sessionGeneration_||!linkedPreparing_)return;
            auto* transfer=new retroarch::LinkedSavePreparation(this);linkedTransfers_[slot]=transfer;
            connect(transfer,&retroarch::LinkedSavePreparation::ready,this,[this,generation,slot](QJsonObject value){
                if(active_&&generation==sessionGeneration_&&linkedPreparing_)
                    party_.prepareLinked(slot,{{"kind","linked-prepare"},{"identity",descriptor_},{"preparation",value}});
            });
            connect(transfer,&retroarch::LinkedSavePreparation::completed,this,[this,generation,slot](QByteArray bytes){
                if(generation==sessionGeneration_)linkedTransferCompleted(slot,std::move(bytes));
            });
            connect(transfer,&retroarch::LinkedSavePreparation::failed,this,[this,generation]{
                if(active_&&generation==sessionGeneration_&&linkedPreparing_)
                    fail("Couldn't prepare everyone's linked game. Your save was kept. Try inviting again.");
            });
            if(!online_)config["address"]=nearby_.localAddressFor(linkedPeers_.value(slot).mid(7));
            transfer->start(installation_.linkedSavePython,installation_.linkedSaveHelper,config);
        }
    });
    watcher->setFuture(QtConcurrent::run([r=*record,i=installation_,expected=descriptor_]{
        const std::atomic_bool cancel=false;
        if(!sameMultiplayerGame(retroarch::netplayIdentity(r,i,cancel),expected))
            return retroarch::LinkedSaveSeed{{},false,"Your game or emulator changed. Invite your friend again."};
        return retroarch::linkedSaveSeed(r,i);
    }));
}
void RuntimeMultiplayer::stopLinkedTransfers() {
    linkedSave_.stop();
    const auto transfers=linkedTransfers_;linkedTransfers_.clear();
    for(auto* transfer:transfers){transfer->disconnect(this);transfer->stop();transfer->deleteLater();}
}
bool RuntimeMultiplayer::linkedRosterMatches() const {
    if(!party_.active())return false;
    QMap<int,QString> current;
    for(const auto& v:party_.state()["members"].toArray()) {
        const auto m=v.toObject();const int slot=m["slot"].toInt();
        if(current.contains(slot)||!m["ready"].toBool())return false;
        current[slot]=m["peer"].toString();
    }
    return current==linkedPeers_;
}
void RuntimeMultiplayer::linkedTransferCompleted(int slot,QByteArray bytes) {
    if(!active_||!linkedPreparing_)return;
    if(host_) {
        if(slot<2||slot>request_.linkedPlayers||request_.peerSrams.contains(slot)||bytes.size()!=request_.ownSram.size()) {
            fail("Couldn't prepare the linked game. Your save was kept.");return;
        }
        request_.peerSrams[slot]=std::move(bytes);
        if(request_.peerSrams.size()!=request_.linkedPlayers-1)return;
    } else if(!bytes.isEmpty()) {fail("Couldn't prepare the linked game. Your save was kept.");return;}
    stopLinkedTransfers();linkedPreparing_=false;linkedPrepared_=true;launchPending_=false;
    if(host_)launch();else if(!linkedEndpoint_.isEmpty()){const auto endpoint=linkedEndpoint_;linkedEndpoint_={};frame(endpoint);}
}
void RuntimeMultiplayer::send(QJsonObject packet) {
    if(partySession_){if(packet["kind"]=="ready"||packet["kind"]=="psp-ready"||packet["kind"]=="dolphin-ready"||packet["kind"]=="rmg-ready"||packet["kind"]=="flycast-ready"||packet["kind"]=="ds-ready"||packet["kind"]=="azahar-ready"||packet["kind"]=="vita-ready")party_.ready(packet);return;}
}
void RuntimeMultiplayer::frame(const QJsonObject& packet) {
    if(!active_)return;
    if(vita()) {
        if(host_||packet["kind"]!="vita-ready"||lifecycle_.active()||launchPending_)return;
        if(packet["identity"].toObject()!=descriptor_||packet["slot"]!=QJsonValue(2)) {
            fail("Your Vita game or invitation changed. Invite again.");return;
        }
        vitaRequest_={descriptor_,packet["connection"].toObject(),false,online_,packet["key"].toString(),nearby_.addressOf(transportPeer_.mid(7))};
        launchPending_=true;launch();return;
    }
    if(azahar()) {
        if(host_||packet["kind"]!="azahar-ready"||lifecycle_.active()||launchPending_)return;
        if(packet["identity"].toObject()!=descriptor_||packet["slot"]!=QJsonValue(2)) {
            fail("Your 3DS game or invitation changed. Invite again.");return;
        }
        azaharRequest_={descriptor_,packet["connection"].toObject(),false,online_,packet["key"].toString(),nearby_.addressOf(transportPeer_.mid(7))};
        launchPending_=true;launch();return;
    }
    if(ds()) {
        if(host_||packet["kind"]!="ds-ready"||lifecycle_.active()||launchPending_)return;
        if(packet["identity"].toObject()!=descriptor_||packet["slot"]!=QJsonValue(2)) {
            fail("Your DS game or invitation changed. Invite again.");return;
        }
        dsRequest_={descriptor_,packet["connection"].toObject(),false,online_,packet["key"].toString(),nearby_.addressOf(transportPeer_.mid(7))};
        launchPending_=true;launch();return;
    }
    if(packet["kind"]=="left"){
        if(hotseat())fail("Your friend left the game.");
        else {status_="Your friend left the game";emit notice(status_);}return;
    }
    if(flycast()) {
        if(host_||packet["kind"]!="flycast-ready"||lifecycle_.active()||launchPending_)return;
        if(packet["identity"].toObject()!=descriptor_||packet["slot"].toInt()!=2){fail("Your Dreamcast game or emulator doesn't match.");return;}
        flycastRequest_={descriptor_,false,online_,nearby_.addressOf(transportPeer_.mid(7)),packet["token"].toString(),packet["key"].toString()};
        launchPending_=true;launch();return;
    }
    if(rmg()) {
        if(host_||(packet["kind"]!="rmg-ready"&&packet["kind"]!="rmg-private-ready")||lifecycle_.active()||launchPending_)return;
        if(packet["identity"].toObject()!=descriptor_){fail("Your Nintendo 64 game or emulator doesn't match.");return;}
        rmgRequest_.token=packet["token"].toString();rmgRequest_.port=packet["port"].toInt();
        rmgRequest_.seats=packet["seats"].toArray();rmgRequest_.slot=packet["slot"].toInt();
        rmgRequest_.privateLan=packet["kind"]=="rmg-private-ready";
        rmgRequest_.connection=packet["connection"].toObject();
        if(rmgRequest_.privateLan&&!online_){fail("This Nintendo 64 invitation changed. Invite again.");return;}
        prepareRmg();return;
    }
    if(linked()&&!host_&&!linkedPrepared_&&packet["kind"]=="ready") {
        if(linkedPreparing_&&packet["identity"].toObject()==descriptor_&&packet["machines"].toInt()==request_.linkedPlayers)linkedEndpoint_=packet;
        return;
    }
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
    if(linked()&&packet["machines"].toInt()!=request_.linkedPlayers){fail("The linked game changed. Invite your friends again.");return;}
    const auto port=packet["port"].toInt();
    if(port<1||port>65535){fail("This multiplayer address is invalid.");return;}
    request_.password=packet["password"].toString();request_.port=quint16(port);
    request_.address=online_?packet["address"].toString():nearby_.addressOf(transportPeer_.mid(7));
    request_.relaySession=packet["session"].toString();
    if(hotseat()) {
        handoff_.begin(party_.state()["party"].toString(),request_.slot);
        request_.clientPort=client_.startHotseat(request_.address,request_.port,
            online_?request_.relaySession:QString(),online_?request_.password:QString(),online_);
        if(!request_.clientPort){fail("Couldn't prepare the shared-controller connection.");return;}
    } else if(online_) {
        request_.clientPort=client_.start(request_.address,request_.port,request_.relaySession,request_.password);
        if(!request_.clientPort){fail("Couldn't prepare the multiplayer connection.");return;}
    }
    launchPending_=true;launch();
}
void RuntimeMultiplayer::pollRelay() {
    if(psp()||dolphin()||rmg()||flycast()||ds()||azahar()||vita()||query_||!active_||!host_||!online_||relaySent_||request_.relayEndpoint.isEmpty())return;
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
            relaySent_=true;status_="Waiting for your friend…";
            if(!lobbyTicket_.isEmpty()&&supportsLive(descriptor_)) {
                deadline_=0;timer_.stop();status_="Playing";
            }
            emit changed();break;
        }
    });
}
void RuntimeMultiplayer::output(const QByteArray& bytes) {
    if(!active_||psp())return;
    output_+=bytes;
    while(output_.contains('\n')) {
        const auto end=output_.indexOf('\n');const auto line=output_.left(end);output_.remove(0,end+1);
        if(vita()) {
            const auto event=vita::event(line);
            if(event["event"]=="ready"&&host_&&!relaySent_) {
                const auto endpoint=event["endpoint"].toObject();
                if(endpoint["mode"]!=(online_?"online":"nearby")){fail("The Vita connection changed. Invite again.");return;}
                relaySent_=true;send({{"kind","vita-ready"},{"identity",descriptor_},{"key",vitaRequest_.identity},{"connection",endpoint}});
                status_="Waiting for your friend...";
            } else if(event["event"]=="running") {
                deadline_=0;timer_.stop();status_="Playing together";emit changed();
            } else if(event["event"]=="error") {fail("Vita multiplayer ended. You can invite again.");return;}
            continue;
        }
        if(azahar()) {
            const auto event=azahar::event(line);
            if(event["event"]=="ready"&&host_&&!relaySent_) {
                const auto endpoint=event["endpoint"].toObject();
                if(endpoint["mode"]!=(online_?"online":"nearby")){fail("The 3DS connection changed. Invite again.");return;}
                relaySent_=true;send({{"kind","azahar-ready"},{"identity",descriptor_},{"key",azaharRequest_.identity},{"connection",endpoint}});
                status_="Waiting for your friend...";
            } else if(event["event"]=="running") {
                deadline_=0;timer_.stop();status_="Playing together";emit changed();
            } else if(event["event"]=="error") {fail("3DS multiplayer ended. You can invite again.");return;}
            continue;
        }
        if(ds()) {
            const auto event=melonds::event(line);
            if(event["event"]=="ready"&&host_&&!relaySent_) {
                const auto endpoint=event["endpoint"].toObject();
                if(endpoint["mode"]!=(online_?"online":"nearby")){fail("The DS connection changed. Invite again.");return;}
                relaySent_=true;send({{"kind","ds-ready"},{"identity",descriptor_},{"key",dsRequest_.identity},{"connection",endpoint}});
                status_="Waiting for your friend...";
            } else if(event["event"]=="running") {
              deadline_=0;timer_.stop();status_="Playing together";emit changed();
            } else if(event["event"]=="error") {fail("DS multiplayer ended. Your original save was kept.");return;}
            continue;
        }
        if(flycast()) {
            const auto event=flycast::event(line);
            if(event["event"]=="ready"&&host_&&!relaySent_ &&
               (online_?!event["key"].toString().isEmpty():event["key"].toString().isEmpty())) {
                relaySent_=true;
                send({{"kind","flycast-ready"},{"identity",descriptor_},{"token",flycastRequest_.token},{"key",event["key"]}});
                status_="Waiting for your friend...";
            } else if(event["event"]=="running") {
                deadline_=0;timer_.stop();status_="Playing together";emit changed();
            } else if(event["event"]=="error") {fail("Dreamcast multiplayer ended. You can invite again.");return;}
            continue;
        }
        if(rmg()) {
            if(rmgRequest_.privateLan) {
                const auto event=rmg::privateEvent(line);
                if(event["event"]=="ready"&&host_) {
                    const int slot=event["slot"].toInt();
                    if(!party_.readyMember(slot,{{"kind","rmg-private-ready"},{"identity",descriptor_},
                        {"token",rmgRequest_.token},{"seats",rmgRequest_.seats},{"connection",event["endpoint"]}})) {
                        fail("The Nintendo 64 party changed. Invite again.");return;
                    }
                    status_="Waiting for your friends...";emit changed();
                }else if(event["event"]=="running") {
                    deadline_=0;timer_.stop();status_="Playing together";emit changed();
                }else if(event["event"]=="error") {fail("Nintendo 64 multiplayer ended. You can invite again.");return;}
                continue;
            }
            if(line=="[TrainerOSRmg] running") {deadline_=0;timer_.stop();status_="Playing together";emit changed();}
            else if(line=="[TrainerOSRmg] error") {fail("Nintendo 64 multiplayer ended. You can invite again.");return;}
            continue;
        }
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
                if(!hotseat()){deadline_=0;timer_.stop();status_="Playing together";emit changed();}
            } else if(event["event"]=="controller"&&hotseat()&&host_) {
                const auto sequence=event["sequence"].toInteger(-1);const int owner=event["owner"].toInt(-1);
                if(sequence<1||owner<0||owner>2){fail("The game couldn't confirm controller ownership.");return;}
                handoff_.authorityObserved(quint64(sequence),owner);
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
        const auto joined=QRegularExpression("joined as player ([2-4])").match(QString::fromUtf8(line));
        if(joined.hasMatch()&&!hotseat()){
            deadline_=0;timer_.stop();status_="Playing together";
            if(host_&&!lobbyTicket_.isEmpty()&&supportsLive(descriptor_))emit notice("Player "+joined.captured(1)+" joined");
            emit changed();
        }
        if(line.contains("Failed")||line.contains("failed")||line.contains("disconnected")){
            if(hotseat()){fail("The shared-controller connection ended. Invite your friend again.");return;}
            status_="Multiplayer connection ended";emit notice(status_);
        }
    }
    if(output_.size()>16384)output_.clear();
}
void RuntimeMultiplayer::fail(QString text) {
    auto lobbyDone=std::exchange(lobbyCompletion_,{});
    ++sessionGeneration_;
    const bool wasActive=active_,endHotseat=wasActive&&hotseat();
    const bool cancelPreparation=endHotseat&&!restarting_&&process_.preparing()&&lifecycle_.adventureId()==game_;
    active_=false;restarting_=false;launchPending_=false;handoff_.stop();
    // A refused or lost handoff must not leave either temporary copy accepting
    // input independently. Address only this prepared native child through stdin.
    // An ordinary game waiting for save consent never carries this capability.
    if(cancelPreparation)process_.stop();
    else if(endHotseat&&process_.runtimeControls().contains("hotseat")) {
        if(process_.processId()) {
            if(!process_.runtimeCommand("hotseat-end"))text+=" Close the game from Game Options.";
        } else if(process_.active())process_.stop();
    }
    rmgLobby_.stop();rmgPreparing_=false;rmgRequest_={};flycastRequest_={};dsRequest_={};azaharRequest_={};vitaRequest_={};
    client_.stop();
    stopLinkedTransfers();linkedPeers_.clear();linkedPreparing_=linkedPrepared_=false;linkedEndpoint_={};
    request_.ownSram.clear();request_.peerSrams.clear();
    status_=std::move(text);deadline_=0;timer_.stop();
    if(partySession_){party_.leave();partySession_=false;}
    if(wasActive&&process_.runtimeControls().contains("netplay")){
        // Keep the owned emulator/session alive for normal Home -> Exit. Killing
        // a Flatpak launcher here can orphan its emulator and lose ownership.
        overlay_.dismissMenu();
    }
    if(overlay_.menuOpen())show("multiplayer-wait");else emit notice(status_);
    emit changed();
    if(lobbyDone)lobbyDone(false,status_);
}
}
