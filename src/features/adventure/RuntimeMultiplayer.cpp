#include "RuntimeMultiplayer.h"
#include "integrations/adventure/retroarch/RetroArchHandheldLink.h"
#include "core/model/GamePlayers.h"
#include "features/social/SocialController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include "core/navigation/AdventureLaunchController.h"
#include <QtConcurrent>
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
    connect(&social_,&SocialController::presentationChanged,this,&RuntimeMultiplayer::changed);
    connect(&social_,&SocialController::partyQuery,this,[this](QString peer){party_.query("online:"+peer);});
    connect(&social_,&SocialController::partyLeave,this,&RuntimeMultiplayer::leaveParty);
    connect(&social_,&SocialController::runtimeAction,this,[this](const QString& id){
        action(id,true);
    });
    connect(&social_,&SocialController::partyJoin,this,[this](QString peer){
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
        if(lifecycle_.active())for(auto& v:companies){auto r=v.toMap();r["joinable"]=false;v=r;}
        social_.setCompanyParties(companies);
        for(const auto& v:social_.runtimeFriends()) {
            const auto peer=v.toMap()["id"].toString();auto offer=party_.offer("online:"+peer);
            if(!offer.isEmpty())offer["joinable"]=offer["joinable"].toBool()&&!party_.active()&&!party_.state()["joining"].toBool()&&!lifecycle_.active();
            social_.setGameActivity(peer,offer.toVariantMap());
        }
        if(socialSurface_&&social_.runtimeSurfaceOpen())show(socialPanel_);
        else if(overlay_.panel()=="multiplayer-party"||overlay_.panel()=="multiplayer-request")show(overlay_.panel());
        emit changed();
    });

    connect(&client_,&retroarch::NetplayClient::failed,this,[this](QString message){
        if(active_)fail(std::move(message));
    });
    connect(&scan_,&QFutureWatcherBase::resultReadyAt,this,[this](int index){
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
    connect(&nearby_,&LocalLinkPeer::error,this,[this](QString text){fail(text);});
    connect(&process_,&ProcessService::runtimeOutput,this,&RuntimeMultiplayer::output);
    invitationOpenTimer_.setSingleShot(true);invitationOpenTimer_.setInterval(5000);
    connect(&invitationOpenTimer_,&QTimer::timeout,this,[this]{
        openingInvitation_.clear();emit notice("Couldn't open the invitation. Open Game Options and try again.");emit changed();
    });
    connect(&overlay_,&AdventureExitPresentation::changed,this,&RuntimeMultiplayer::resumeInvitation);
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
        if(restarting_&&!psp()&&!dolphin()&&descriptor_.value("transport")!="netpacket"&&!descriptor_.value("id").toString().startsWith("runtime.retroarch."))
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
        if(psp()) {
            // A running PSP is not proof of an ad hoc match. Players use the
            // original game's VS create/join controls after accepting here.
            if(host_)send({{"kind","psp-ready"},{"identity",descriptor_},{"address",request_.address}});
            relaySent_=true;deadline_=0;timer_.stop();status_="Choose VS mode in the game";return;
        }
        if(host_&&online_&&!dolphin())pollRelay();
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
        client_.stop();
        // An ordinary game ending must not discard an unanswered invitation.
        if(partySession_&&party_.active()){party_.leave();partySession_=false;}
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
        if(active_&&host_&&online_&&!restarting_&&!launchPending_&&!query_&&!relaySent_)pollRelay();
    });timer_.setInterval(3000);
}
void RuntimeMultiplayer::refresh(const RetroArchInstallation& installation,QString trainer,bool allowed) {
    if(trainer_!=trainer)party_.reset();
    trainer_=std::move(trainer);allowed_=allowed;
    installation_=installation;
    if(!allowed_) {
        games_.clear();
        if(!onlineGames_.isEmpty()){onlineGames_.clear();emit availabilityChanged();}
        update();return;
    }
    const auto records=library_.registrations();
    const auto pspInstallation=ppsspp_.installation();
    const auto dolphinInstallation=dolphin_.installation();
    const auto stamp=[](const QString& path){const QFileInfo f(path);return qHashMulti(0,path,f.size(),f.lastModified().toMSecsSinceEpoch());};
    quint64 revision=qHashMulti(0,stamp(installation.runtimeFile),stamp(installation.configFile),
        stamp(pspInstallation.runtimeFile),stamp(dolphin::bridgeFile()),stamp(dolphin::bridgeRoot()+"/manifest.json"),
        stamp(installation.linkedSavePython),stamp(installation.linkedSaveHelper));
    for(auto it=installation.cores.cbegin();it!=installation.cores.cend();++it)revision=qHashMulti(revision,it.key(),stamp(it.value()));
    const auto candidate=[](const AdventureRegistration& r){return r.adventure.platformId=="psp"||r.adventure.platformId=="gc"||
        retroarch::netplaySupported(r.adventure.platformId,r.integrationConfig["core"].toString());};
    QMap<QString,int> capacities;
    QMap<QString,QVariantMap> metadata;
    for(const auto& r:records)if(candidate(r)) {
        const auto core=r.integrationConfig["core"].toString();
        const auto info=library_.artwork(r.adventure.id);metadata.insert(r.adventure.id,info);
        const int capacity=retroarch::netplayCapacity(r.adventure.platformId,core,info,r.contentPath);
        capacities.insert(r.adventure.id,capacity);
        revision=qHashMulti(revision,r.adventure.id,stamp(r.contentPath),r.revision,core,capacity);
    }
    if(!revision)revision=1;
    if(!scan_.isRunning() && (scanRevision_!=revision||!scanRevision_)) {
        scanRevision_=revision;
        profiles_.clear();
        auto ordered=records;
        const auto playing=process_.runtimeControls()["game"].toString();
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
                const auto identity=r.adventure.platformId=="gc"?dolphin::netplayIdentity(r,dolphinInstallation,cancel):r.adventure.platformId=="psp"?ppsspp::netplayIdentity(r,pspInstallation,cancel):retroarch::netplayIdentity(r,installation,cancel,&digests,capacities.value(r.adventure.id,2));
                if(!identity.isEmpty())promise.addResult(ScannedGame{r.adventure.id,identity});
            }
        }));
    }
    games_.clear();
    for(auto it=profiles_.cbegin();it!=profiles_.cend();++it)
        if(metadata.contains(it.key())&&permitsMultiplayer(metadata.value(it.key()),it.value()))games_.insert(it.key(),it.value());
    const auto visible=allowed_?games_.keys():QStringList{};
    if(visible!=onlineGames_){onlineGames_=visible;emit availabilityChanged();}
    update();
}
QString RuntimeMultiplayer::menuLabel() const {return !party_.pending().isEmpty()?"Incoming activity":party_.active()?"Game party":"Play together";}
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
        &&!(socialSurface_&&social_.runtimeSurfaceOpen())&&!overlay_.visible()
        &&lifecycle_.exitController().phase()==AdventureExitController::Phase::Idle;
}
bool RuntimeMultiplayer::canJoinRunning(const QJsonObject& invitation) const {
    const auto current=lifecycle_.adventureId();
    return lifecycle_.state()=="running" && games_.contains(current)
        && sameMultiplayerGame(games_.value(current),invitation)
        && runningJoin.available && runningJoin.join && runningJoin.available(current,invitation);
}
void RuntimeMultiplayer::openInvitation(const QString& request) {
    if(!allowed_||request.isEmpty()||request!=invitationId()||!exitingInvitation_.isEmpty())return;
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
    return pending["name"].toString()+(pending["joining"].toBool()?" wants to join ":" invites you to play ")+pending["game"].toObject()["label"].toString()+".";
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
        game_=process_.runtimeControls()["game"].toString();descriptor_=games_.value(game_);
    }
    // Receiving an invitation is independent of the current game's netplay
    // support. An unrelated single-player game may keep running while saving.
    const bool available=allowed_&&!games_.isEmpty();
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
        bool ready=true;
        for(const auto& value:state["members"].toArray()) {
            const auto m=value.toObject();ready&=m["ready"].toBool();
            auto r=row(m["ready"].toBool()?"multiplayer-member":"multiplayer-revoke:"+m["peer"].toString(),m["name"].toString(),m["ready"].toBool()?"Ready":"Invited - select to cancel");
            r["readOnly"]=m["ready"].toBool();rows.append(r);
        }
        if(!party_.pending().isEmpty())rows.prepend(row("multiplayer-request","Join request",party_.pending()["name"].toString()));
        if(party_.host()&&!party_.running()&&ready&&state["members"].toArray().size()>1)rows.prepend(row("multiplayer-start","Start game"));
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
        QSet<QString> identities;
        if(!party_.active())for(const auto& p:social_.runtimeCompanies()) {const auto r=p.toMap();const auto id="multiplayer-company:"+r["id"].toString();if(identities.contains(id))continue;identities.insert(id);rows.append(row(id,r["name"].toString(),"Group"));}
        if(!party_.active()||online_)for(const auto& p:social_.runtimeFriends()) {const auto r=p.toMap();const auto id="multiplayer-friend:"+r["id"].toString();if(identities.contains(id))continue;identities.insert(id);rows.append(row(id,r["name"].toString(),"Online friend"));}
        // Discovery UUIDs have no authenticated account binding. Never merge
        // nearby and online identities just because their display names match.
        if(!party_.active()||!online_)for(const auto& p:nearby_.peers()) {const auto r=p.toMap();const auto id="multiplayer-local:"+r["id"].toString();if(identities.contains(id))continue;identities.insert(id);rows.append(row(id,r["name"].toString(),"Nearby"));}
        if(rows.isEmpty())caption="Looking for nearby Trainers. Friends appear when connected.";
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
    rows.append(row("multiplayer-cancel","Back"));
    if(socialSurface_){socialPanel_=panel;social_.setRuntimeSurface(panel=="multiplayer-people"?"Invite players":panel=="multiplayer-request"?"Incoming activity":"Game party",caption,rows);}
    else overlay_.setPanel(panel,caption,rows,"multiplayer-cancel");
}
bool RuntimeMultiplayer::action(const QString& id,bool fromSocial) {
    if(!id.startsWith("multiplayer"))return false;
    socialSurface_=fromSocial;
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
    for(auto it=games_.cbegin();it!=games_.cend();++it)if(sameMultiplayerGame(it.value(),selected)){game=it.key();break;}
    // Preserve the running edition when several compatible handheld games exist.
    const auto current=process_.runtimeControls()["game"].toString();
    for(auto it=games_.cbegin();it!=games_.cend();++it)
        if(sameMultiplayerGame(it.value(),selected)&&it.value()["content"]==selected["content"]){game=it.key();break;}
    if(games_.contains(current)&&sameMultiplayerGame(games_.value(current),selected))game=current;
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
    stopLinkedTransfers();linkedPeers_.clear();linkedPreparing_=linkedPrepared_=false;linkedEndpoint_={};
    game_=game;descriptor_=selected;partySession_=true;active_=true;host_=host;relaySent_=false;
    transportPeer_=host?party_.state()["members"].toArray().at(1).toObject()["peer"].toString():party_.hostPeer();
    online_=transportPeer_.startsWith("online:");
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
    deadline_=QDateTime::currentSecsSinceEpoch()+(dolphin()?150:75);timer_.start();
    const auto record=library_.registration(game_);
    if(!record){fail("The game is no longer in your library.");return;}
    if(!permitsMultiplayer(library_.artwork(game_),descriptor_)){fail("Multiplayer isn't available for this game.");return;}
    if(linked()&&!linkedPrepared_){if(host_)prepareLinked();return;}
    const auto result=dolphin()?dolphin_.launchNetplay(record->adventure,dolphinRequest_):psp()?ppsspp_.launchNetplay(record->adventure,
        ppsspp::NetplayRequest{descriptor_,host_,online_,request_.address,trainer_}):adapter_.launchNetplay(record->adventure,request_);
    if(!result.success)fail(result.message);
    // The launch worker owns its snapshot; do not retain peer progress in UI state.
    request_.ownSram.clear();request_.peerSrams.clear();
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
    if(partySession_){if(packet["kind"]=="ready"||packet["kind"]=="psp-ready"||packet["kind"]=="dolphin-ready")party_.ready(packet);return;}
}
void RuntimeMultiplayer::frame(const QJsonObject& packet) {
    if(!active_)return;
    if(packet["kind"]=="left"){status_="Your friend left the game";emit notice(status_);return;}
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
        const auto joined=QRegularExpression("joined as player ([2-4])").match(QString::fromUtf8(line));
        if(joined.hasMatch()){deadline_=0;timer_.stop();status_="Playing together";emit changed();}
        if(line.contains("Failed")||line.contains("failed")||line.contains("disconnected")){status_="Multiplayer connection ended";emit notice(status_);}
    }
    if(output_.size()>16384)output_.clear();
}
void RuntimeMultiplayer::fail(QString text) {
    ++sessionGeneration_;
    client_.stop();
    stopLinkedTransfers();linkedPeers_.clear();linkedPreparing_=linkedPrepared_=false;linkedEndpoint_={};
    request_.ownSram.clear();request_.peerSrams.clear();
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
