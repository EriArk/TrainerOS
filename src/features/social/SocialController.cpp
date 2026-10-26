#include "core/PerformanceTrace.h"
#include "SocialController.h"
#include "integrations/social/FluxerSession.h"
#include "core/experience/ExperienceProviders.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSettings>
#include <QGuiApplication>
#include <QClipboard>
#include <QUuid>
#include <algorithm>

namespace trainer {
SocialController::SocialController(QObject* parent):QObject(parent),session_(new FluxerSession) {
    connect(this,&SocialController::changed,this,[this]{QTimer::singleShot(0,this,&SocialController::reconcileLobbyPreparation);});
    lobbyReminderTimer_.setInterval(15000);lobbyReminderTimer_.start();
    connect(&lobbyReminderTimer_,&QTimer::timeout,this,&SocialController::tickLobbyReminders);
    connect(&lobby_,&LobbyCoordinator::membershipEnded,this,[this](QString room){
        lobbyReminders_.cancel(room);pendingLobbyReminder_={};
        if(auto* r=lobby_.room(room);r&&!resettingSocial_&&!lobbyLeaving_.contains(room)) {
            const auto channel=r->state()["conversation"].toString();
            if(!channel.isEmpty())emit commandRequested("lobby-chat-close",{{"lobby",room},{"channel",channel},{"close",r->hosting()}});
        }
    });
    connect(&lobbyRuntime_,&LobbyRuntime::changed,this,&SocialController::changed);
    lobbyPublishTimer_.setSingleShot(true);lobbyPublishTimer_.setInterval(1000);
    lobbyHeartbeat_.setInterval(60000);lobbyHeartbeat_.start();
    connect(&lobbyPublishTimer_,&QTimer::timeout,this,&SocialController::publishLobby);
    connect(&lobbyHeartbeat_,&QTimer::timeout,this,[this]{publishLobby();if(face_=="lobby"&&lobbyScope_!="local")emit commandRequested("lobby-directory-refresh",{});});
    connect(&lobby_,&LobbyCoordinator::roomsChanged,this,[this]{
        syncLocalLobby();
        if(myLobbiesOpen_){const auto rows=myLobbies();myLobbiesIndex_=rows.size();for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]==myLobbyFocus_)myLobbiesIndex_=i;}
        QTimer::singleShot(0,this,&SocialController::syncMyLobbies);
        syncLobbyReminder();
        lobbyPublishTimer_.start();
        QJsonArray contexts;for(auto* r:lobby_.rooms()){if(r->local())continue;auto state=r->state();if(state["id"].toString().isEmpty())continue;if(r->recoveryUncertain())state["recovering"]=true;contexts.append(state);}
        if(contexts!=lobbyContactRooms_){lobbyContactRooms_=contexts;emit commandRequested("lobby-contact-context",{{"account",snapshot_["userId"]},{"lobby",QJsonObject{{"rooms",contexts}}.toVariantMap()}});}
        syncLobbyChat();
        recentTogether_.observe(viewedLobby().state(),QDateTime::currentSecsSinceEpoch());
        const auto invite=lobby_.invitation();const auto id=invite["lobby"].toString();
        if(!id.isEmpty()&&id!=presentedLobbyInvite_) {
            toastChannel_="lobby:"+id;toastTitle_=invite["name"].toString();toastText_="Invited you to a lobby";toastTimer_.start();
            if(gameActive_)emit backgroundNotification(toastTitle_,toastText_);
            emit presentationChanged();
        }
        presentedLobbyInvite_=id;emit changed();
    });
    connect(&lobby_,&LobbyCoordinator::outgoing,this,[this](QString peer,QJsonObject packet){
        if(LocalLobbyTransport::validIdentity(peer)){if(!localLobby_.send(peer,packet))QTimer::singleShot(0,this,[this,peer]{lobby_.deliveryFailed(peer);});}
        else emit commandRequested("lobby-send",{{"peer",peer},{"packet",packet.toVariantMap()}});
    });
    connect(&localLobby_,&LocalLobbyTransport::changed,this,&SocialController::changed);
    connect(&localLobby_,&LocalLobbyTransport::received,this,[this](QString peer,QString name,QJsonObject packet){
        if(packet["ns"]=="org.traineros.lobby")lobby_.receive(peer,name,packet);
        else if(localPartyAllowed(peer,packet))emit localPartyPacket(peer,name,packet);
    });
    connect(&localLobby_,&LocalLobbyTransport::disconnected,this,[this](QString peer){lobby_.deliveryFailed(peer);emit localPartyDisconnected(peer);});
    connect(&lobby_,&LobbyCoordinator::notice,this,[this](QString message){toastTitle_="Lobby";toastText_=message;toastTimer_.start();emit presentationChanged();});
    connect(session_,&FluxerSession::lobbyPacket,this,[this](quint64 generation,QString peer,QString name,QJsonObject packet){if(generation==generation_&&!LocalLobbyTransport::validIdentity(peer))lobby_.receive(peer,name,packet);});
    connect(session_,&FluxerSession::lobbyFailed,this,[this](quint64 generation,QString peer){if(generation==generation_)lobby_.deliveryFailed(peer);});
    connect(session_,&FluxerSession::lobbyChatFinished,this,[this](quint64 generation,QString room,QString channel,QString error){
        auto* target=lobby_.room(room);if(generation!=generation_||!target)return;
        lobbyChatPending_.remove(room);lobbyChatErrors_[room]=error;
        if(!channel.isEmpty()&&target->hosting())target->setConversation(channel);
        if(room==viewedLobby().state()["id"].toString()) {
            lobbyChatStatus_=error;lobbyChatBusy_=false;
            if(!channel.isEmpty()){lobbyChatChannel_=channel;openLobbyChat();}
        }
        QTimer::singleShot(0,this,&SocialController::syncMyLobbies);
        emit changed();
    });
    connect(session_,&FluxerSession::onlineEstablished,this,[this](quint64 generation,QString self,QString peer,QString name,QString activity,bool initiator){
        if(generation!=generation_)return;
        runtimeOnline_=activity.startsWith("runtime.");
        if(runtimeOnline_){emit runtimeEstablished(activity,initiator);return;}
        if(!onlineWritable_||!link_||!link_->beginOnline(self,peer,name,activity,initiator))emit commandRequested("online-close",{});
    });
    connect(session_,&FluxerSession::onlineFrame,this,[this](quint64 generation,QJsonObject frame){if(generation!=generation_)return;if(runtimeOnline_)emit runtimeFrame(frame);else if(link_)link_->receiveOnline(frame);});
    connect(session_,&FluxerSession::onlineEnded,this,[this](quint64 generation){if(generation!=generation_)return;if(runtimeOnline_){runtimeOnline_=false;emit runtimeEnded();}else if(link_)link_->endOnline();});
    partyBrowse_.setInterval(30000);
    connect(&partyBrowse_,&QTimer::timeout,this,[this]{refreshPartyBrowse();});partyBrowse_.start();
    session_->moveToThread(&thread_);
    connect(&thread_,&QThread::finished,session_,&QObject::deleteLater);
    connect(this,&SocialController::ownerRequested,session_,&FluxerSession::setOwner);
    connect(this,&SocialController::commandRequested,session_,&FluxerSession::command);
    connect(session_,&FluxerSession::snapshot,this,&SocialController::receive);
    connect(session_,&FluxerSession::ordinaryMessage,this,[this](quint64 g,QVariantMap event){
        if(g!=generation_)return;const auto context=gameChat();
        if(context.isEmpty()||event["channel"]!=context["channel"])return;
        for(const auto& v:context["members"].toList()){const auto member=v.toMap();
            if(member["id"]!=event["author"]||!member["connected"].toBool())continue;
            event["room"]=context["room"];event["spectator"]=member["spectator"];gamePreviewChannel_=event["channel"].toString();emit gameMessage(event);return;
        }
    });
    connect(&files_,&ChatFiles::request,this,&SocialController::commandRequested);
    connect(&files_,&ChatFiles::changed,this,&SocialController::changed);
    connect(session_,&FluxerSession::fileEvent,this,[this](quint64 g,QVariantMap event){if(g==generation_)files_.receive(event);});
    connect(&files_,&ChatFiles::sent,this,[this](const QString& channel,const QString& text){
        if(drafts_.value(channel)==text){drafts_.remove(channel);saveDrafts();}emit changed();
    });
    connect(this,&SocialController::changed,this,[this]{files_.context(owner_+":"+snapshot_["userId"].toString(),snapshot_["channel"].toString());});
    connect(&stream_,&SocialStream::ended,this,[this]{emit commandRequested("voice-watch-stop",{});});
    connect(&stream_,&SocialStream::changed,this,&SocialController::changed);
    connect(session_,&FluxerSession::partyPacket,this,[this](quint64 g,QString peer,QString name,QJsonObject p){if(g==generation_)emit partyPacket(peer,name,p);});
    connect(session_,&FluxerSession::partyFailed,this,[this](quint64 g,QString peer){if(g==generation_){
        emit partyFailed(peer);
    }});
    connect(session_,&FluxerSession::groupMembershipEnded,this,[this](quint64 g,QString channel){
        if(g!=generation_)return;
        for(auto* r:lobby_.rooms())if(r->state()["conversation"]==channel&&!lobbyLeaving_.contains(r->state()["id"].toString()))r->forgetMembership();
    });
    connect(session_,&FluxerSession::partyReset,this,[this](quint64 g){if(g==generation_){gameActivities_.clear();companyParties_.clear();partyFocus_=false;if(!lobby_.activeRoom()->local())emit partyReset();}});
    connect(session_,&FluxerSession::runtimeProbeFailed,this,[this](quint64 generation,QString peer,QString message){
        if(generation==generation_)emit runtimeProbeFailed(peer,message);
    });
    connect(session_,&FluxerSession::reviewsChanged,this,[this](quint64 generation,QString identity,QVariantMap state){if(generation==generation_)emit reviewsChanged(identity,state);});
    connect(&media_,&SocialMedia::changed,this,[this]{if(mediaPreview())mediaMenu();});
    connect(session_,&FluxerSession::attachmentFinished,this,[this](quint64 generation,QString channel,int status){
        if(generation!=generation_||channel!=menuChannel_||!mediaSending_)return;
        mediaSending_=false;mediaUncertain_=status==0;
        if(status>=200&&status<300)closeMenu();
        else {mediaMenu();menuDetail_=status==0?"Delivery unknown. Close this preview and check the conversation.":"Not sent. You can try again.";emit changed();}
    });
    toastTimer_.setSingleShot(true);toastTimer_.setInterval(4500);
    connect(&toastTimer_,&QTimer::timeout,this,[this]{toastTitle_.clear();toastText_.clear();emit presentationChanged();});
    connect(session_,&FluxerSession::incomingMessage,this,[this](quint64 generation,QString channel,QString name,QString text){
        if(generation==generation_&&gameActive_&&gamePreviewChannel_==channel){gamePreviewChannel_.clear();return;}
        if(generation!=generation_||(!surfaceAvailable_&&!gameActive_)||(!menu_.isEmpty()&&!gameActive_)
            ||(!gameActive_&&conversationVisible_&&!contacts_&&face_!="friends"&&snapshot_["channel"]==channel))return;
        toastChannel_=channel;toastTitle_=std::move(name);toastText_=std::move(text);toastTimer_.start();emit presentationChanged();
        if(snapshot_.value("notificationSound",true).toBool())media_.chime();
        if(gameActive_)emit backgroundNotification(toastTitle_,toastText_);
    });
    connect(session_,&FluxerSession::sendFailed,this,[this](quint64 generation,QString channel,QString text){
        if(generation!=generation_ || !drafts_.value(channel).isEmpty())return;
        drafts_[channel]=text;draftSave_.start();emit changed();
    });
    selection_.setSingleShot(true);selection_.setInterval(140);connect(&selection_,&QTimer::timeout,this,&SocialController::preview);
    draftSave_.setSingleShot(true);draftSave_.setInterval(700);connect(&draftSave_,&QTimer::timeout,this,&SocialController::saveDrafts);
    connect(session_,&FluxerSession::mutationFinished,this,[this](quint64 generation,QString operation,QString channel,QString id,bool success){
        if(generation!=generation_)return;
        if(operation=="leave-community"||operation=="delete-community") {
            verificationGuild_.clear();verificationMethod_.clear();
            if(menuMode_=="community-progress")closeMenu();
            if(success){pinnedCommunities_.removeAll(id);if(communityId_==id)communityBack();faceRoutes_.remove("communities");saveSocialPreferences();}
            toastTitle_=success?"Community":"Could not complete action";
            toastText_=success?(operation=="delete-community"?"Community deleted":"You left the community"):snapshot_["communityStatus"].toString();toastTimer_.start();emit presentationChanged();
        }
        if((operation=="leave-group"||operation=="lobby-chat-close")&&!leavingLobby_.isEmpty()) {
            auto* r=lobby_.room(leavingLobby_);
            if(r&&r->state()["conversation"]==channel){if(success){if(lobbyCloseOnLeave_.contains(leavingLobby_)&&r->hosting())r->leave();r->forgetMembership();}lobbyCloseOnLeave_.remove(leavingLobby_);lobbyLeaving_.remove(leavingLobby_);leavingLobby_.clear();emit changed();}
        }
        if(success&&(operation=="edit-message"||operation=="delete-message"))editDrafts_.remove(channel+"/"+id);
    });
    connect(session_,&FluxerSession::communityVerificationRequired,this,[this](quint64 generation,QString guild,QString method){
        if(generation!=generation_)return;
        verificationGuild_=guild;verificationMethod_=method;
        closeMenu();menuMode_="verify-community";menuTitle_="Confirm deletion";
        menuDetail_=method=="password"?"Fluxer requires your password to delete this community.":"Fluxer requires an authenticator or backup code.";
        menu_={"Cancel",method=="password"?"Enter password":"Enter code"};menuCommands_={"cancel","verify-community"};menuFocus_=0;emit changed();
    });
    thread_.start();
}
SocialController::~SocialController() {
    syncingLocalLobby_=true;localLobby_.disconnect(this);localLobby_.close();
    saveDrafts();
    // Cancelling a pending launch publishes room state. Do it while the UI
    // members and provider still exist, not from a later member destructor.
    if(lobbyRuntime_.opening())lobbyRuntime_.cancelPending();
    QMetaObject::invokeMethod(session_,&FluxerSession::stop,Qt::BlockingQueuedConnection);
    thread_.quit();thread_.wait();
}
void SocialController::setOwner(QString owner) {
    if(owner==owner_)return;
    streamAction("stop-watch");
    resettingSocial_=true;
    syncingLocalLobby_=true;localLobby_.close();lobby_.configureLocal({}, {},false);localLobbyDirectory_.clear();
    lobby_.setStorage({});lobby_.configure({}, {},false);lobbySearch_.clear();lobbyTopic_.clear();lobbySelection_.clear();lobbyActionFocus_=0;
    resettingSocial_=false;conversationControl_=-1;enterConversationPerson_.clear();enterConversationChannel_.clear();
    lobbyDraftReturn_=false;cancelLobbyDraft();
    viewedLobbyId_.clear();myLobbiesOpen_=false;lobbyLeaving_.clear();lobbyChatKeys_.clear();lobbyChatErrors_.clear();lobbyChatPending_.clear();
    publishedLobby_.clear();publishedRooms_.clear();readySwitch_.clear();lobbyPublicationVisible_=false;lobbyGames_.clear();
    lobbyReminders_.configure({});pendingLobbyReminder_={};lobbyPlatform_.clear();lobbyMode_.clear();
    lobbyPublicationTopic_="other";
    lobbyLastRoom_.clear();lobbyRailFocused_=false;lobbyRailFocus_=1;
    lobbyRoomFocus_="game";lobbyChatExpanded_=false;lobbySpectatorChat_=false;lobbyAutoPreparation_.clear();
    recentTogether_.configure({},{});recentSelection_.clear();
    media_.clear();mediaSending_=mediaUncertain_=false;
    if(link_)link_->endOnline();
    runtimeOnline_=false;runtimeInvitation_.clear();gameActivities_.clear();companyParties_.clear();partyFocus_=false;emit partyReset();emit runtimeEnded();
    saveDrafts();draftFile_.clear();
    editDrafts_.clear();pickedPeople_.clear();dismissedNotifications_.clear();activityNotifications_.clear();presentedInvitation_.clear();notificationSettingsKey_.clear();
    toastTimer_.stop();toastTitle_.clear();toastText_.clear();emit presentationChanged();
    socialSettingsKey_.clear();peopleFilter_.clear();communityId_.clear();faceRoutes_.clear();pinnedPeople_.clear();pinnedGroups_.clear();pinnedCommunities_.clear();
    discoverPreview_.clear();discoverArea_="search";discoverPage_=0;discoverAll_=false;verificationGuild_.clear();verificationMethod_.clear();
    owner_=std::move(owner);++generation_;snapshot_.clear();drafts_.clear();menu_.clear();toastChannel_.clear();directConversationLease_.clear();
    selection_.stop();contacts_=false;searchStarted_=false;searchFocus_=-1;
    textPurpose_.clear();textChannel_.clear();textId_.clear();query_.clear();focus_=messageFocus_=0;reading_=false;
    syncingLocalLobby_=false;emit ownerRequested(owner_,generation_);emit changed();
}
void SocialController::setFace(QString face) {
    if(face=="groups")face="communities";
    if(face_==face)return;
    faceRoutes_[face_]={{"channel",snapshot_["channel"]},{"focus",focus_},{"community",communityId_}};
    saveSocialPreferences();closeMenu();selection_.stop();
    partyFocus_=false;partyIndex_=0;face_=std::move(face);reading_=false;contacts_=false;conversationControl_=-1;enterConversationPerson_.clear();enterConversationChannel_.clear();
    const auto route=faceRoutes_.value(face_);focus_=route.value("focus",0).toInt();
    if(face_=="communities")communityId_=route["community"].toString();
    if(face_!="lobby"&&face_!="recent")emit commandRequested("face",{{"face",face_},{"channel",route["channel"]},{"guild",communityId_}});
    if(face_=="friends"&&!searchStarted_&&snapshot_["state"]=="connected")runSearch();
    syncLocalLobby();
    if(face_=="lobby"){if(lobbyScope_!="local")emit commandRequested("lobby-directory-refresh",{});openLobbyChat();}
    emit changed();
}
void SocialController::setLink(NativeActivityProvider* link) {
    if(link_==link)return;
    if(link_){link_->endOnline();disconnect(link_,nullptr,this,nullptr);}
    link_=link;
    if(!link_)return;
    connect(link,&NativeActivityProvider::onlineSend,this,[this](QJsonObject frame){emit commandRequested("online-frame",frame.toVariantMap());});
    connect(link,&NativeActivityProvider::onlineClosed,this,[this]{emit commandRequested("online-close",{});});
}
void SocialController::setOnlineContext(bool available,bool writable) {
    onlineAvailable_=available;
    onlineWritable_=writable;
    publishOnlineContext();
}
void SocialController::setRuntimeContext(bool available,QVariantList capabilities) {
    if(runtimeAvailable_==available&&runtimeCapabilities_==capabilities)return;
    runtimeAvailable_=available;runtimeCapabilities_=std::move(capabilities);publishOnlineContext();
}
void SocialController::publishOnlineContext() {
    auto caps=onlineAvailable_&&onlineWritable_&&link_?link_->onlineCapabilities().toVariantList():QVariantList{};
    if(runtimeAvailable_)caps.append(runtimeCapabilities_);
    if(caps!=onlineCapabilities_){onlineCapabilities_=caps;emit commandRequested("online-capabilities",{{"activities",caps}});}
    emit commandRequested("online-available",{{"available",onlineAvailable_||runtimeAvailable_}});
}
QVariantList SocialController::runtimeFriends() const {
    QVariantList result;
    for(const auto& v:snapshot_["friends"].toList()) {
        const auto friendRow=v.toMap();if(friendRow["type"].toInt()!=1)continue;
        result.append(friendRow);
    }
    return result;
}
QVariantMap SocialController::online() const {
    auto state=snapshot_.value("online").toMap();
    if(state["incoming"].toBool())state["open"]=!presentedInvitation_.isEmpty()&&state["session"]==presentedInvitation_;
    return state;
}
void SocialController::answerOnline(bool accept){
    const auto session=snapshot_["online"].toMap()["session"].toString();
    if(!session.isEmpty())dismissedNotifications_["activity:"+session]=session;
    activityNotifications_.removeIf([&](const QVariant& v){return v.toMap()["session"]==session;});
    saveNotifications();presentedInvitation_.clear();
    emit commandRequested("online-answer",{{"accept",accept}});emit changed();
}
void SocialController::saveNotifications() {
    if(notificationSettingsKey_.isEmpty())return;
    QSettings settings;settings.setValue(notificationSettingsKey_,dismissedNotifications_);
    settings.setValue(notificationSettingsKey_+"-activities",activityNotifications_);
}
void SocialController::setSurfaceAvailable(bool available) {
    if(surfaceAvailable_==available)return;
    surfaceAvailable_=available;
    emit surfaceAvailabilityChanged();
    if(!available){
        const bool hadToast=!toastTitle_.isEmpty()||!toastText_.isEmpty();
        toastTimer_.stop();toastTitle_.clear();toastText_.clear();
        if(media_.state()=="recording")closeMenu();
        if(hadToast)emit presentationChanged();
    }
}
void SocialController::setConversationVisible(bool visible) {
    if(conversationVisible_==visible)return;
    conversationVisible_=visible;emit conversationVisibilityChanged();
}
void SocialController::presented(QString channel,QString message) {
    if(!surfaceAvailable_||!conversationVisible_||contacts_||face_=="friends"||!menu_.isEmpty()
        ||snapshot_["historyBusy"].toBool()||snapshot_["channel"]!=channel
        ||snapshot_.value("readTail",messages().isEmpty()?QString():messages().last().toMap()["id"]).toString()!=message)return;
    acknowledgeConversation(channel,message);
    emit commandRequested("read",{{"channel",channel},{"message",message}});
}
void SocialController::acknowledgeConversation(const QString& channel,const QString& message) {
    if(message.isEmpty())return;
    for(const auto& v:notifications()) {
        const auto row=v.toMap();
        if(row["id"]!=channel||row["ringing"].toBool()||row["pending"].toBool()||row["last"]!=message)continue;
        const auto stamp=notificationStamp(row);
        if(dismissedNotifications_.value(channel)==stamp)return;
        dismissedNotifications_[channel]=stamp;if(toastChannel_==channel)hideToast();saveNotifications();emit changed();return;
    }
}
QString SocialController::notificationFace() const {
    if(toastChannel_.startsWith("reminder:"))return "lobby";
    if(toastChannel_.isEmpty())return {};
    if(!lobby_.invitation().isEmpty()&&toastChannel_=="lobby:"+lobby_.invitation()["lobby"].toString())return "lobby";
    for(const auto& value:notifications()) {
        const auto row=value.toMap();
        if((row["activity"].toBool()&&row["channel"]==toastChannel_)||(row["request"].toBool()&&"request:"+row["id"].toString()==toastChannel_))return "chats";
    }
    for(const auto& value:snapshot_["chats"].toList()) {
        const auto row=value.toMap();if(row["id"]==toastChannel_ && (row["unread"].toInt()>0||row["ringing"].toBool()))
            return row["kind"]=="groups"||!row["guild"].toString().isEmpty()?QString("communities"):QString("chats");
    }
    return {};
}
QString SocialController::notificationStamp(const QVariantMap& row) const {
    if(row["request"].toBool())return "request";
    return row["last"].toString()+(row["missedCall"].toString().isEmpty()?QString():"/call/"+row["missedCall"].toString());
}
void SocialController::dismissNotificationAt(int index) {
    const auto list=notifications();if(index<0||index>=list.size())return;
    const auto row=list[index].toMap();if(row["ringing"].toBool())answerCall(row["id"].toString(),false);
    if(row["lobby"].toBool()){lobby_.answer(false);return;}
    if(row["activity"].toBool()) {
        if(row["pending"].toBool()) {
            if(row["runtime"].toBool())emit runtimeAction("multiplayer-decline:"+row["session"].toString());
            else answerOnline(false);
        }
        activityNotifications_.removeIf([&](const QVariant& v){return v.toMap()["id"]==row["id"];});
        saveNotifications();emit changed();return;
    }
    dismissedNotifications_[row["id"].toString()]=notificationStamp(row);
    if(toastChannel_==row["id"])hideToast();
    saveNotifications();
    emit changed();
}
void SocialController::hideToast(){toastTimer_.stop();toastTitle_.clear();toastText_.clear();toastChannel_.clear();emit presentationChanged();}
void SocialController::dismissNotifications() {
    const auto list=notifications();
    for(int i=list.size()-1;i>=0;--i) {
        const auto row=list[i].toMap();
        if(!row["ringing"].toBool()&&!row["pending"].toBool()&&!row["request"].toBool())dismissNotificationAt(i);
    }
}
QVariantMap SocialController::incomingCall() const {
    if(snapshot_["state"]!="connected" || snapshot_["doNotDisturb"].toBool() || snapshot_["callsSuppressed"].toBool())return {};
    for(const auto& value:notifications()) {
        const auto row=value.toMap();
        if(!row["ringing"].toBool())continue;
        bool blocked=false;
        for(const auto& friendValue:snapshot_["friends"].toList()) {
            const auto person=friendValue.toMap();if(person["type"].toInt()!=2)continue;
            for(const auto& member:row["members"].toList())if(member.toMap()["id"]==person["id"])blocked=true;
        }
        if(!blocked)return row;
    }
    return {};
}
void SocialController::respondToIncomingCall(const QString& channel,bool accept) {
    const auto call=incomingCall();
    if(call["id"]!=channel||call["callBusy"].toBool())return;
    answerCall(channel,accept);
}
bool SocialController::dispatchIncomingCall(Action action) {
    const auto call=incomingCall();if(call.isEmpty())return false;
    if(action==Action::Confirm)respondToIncomingCall(call["id"].toString(),true);
    else if(action==Action::Back)respondToIncomingCall(call["id"].toString(),false);
    return true;
}
bool SocialController::answerCall(const QString& channel,bool accept,bool allowOngoing) {
    if(snapshot_["state"]!="connected")return false;
    if(accept&&allowOngoing) {
        const auto voice=snapshot_["voice"].toMap();
        if(!voice["available"].toBool()||!voice["channel"].toString().isEmpty())return false;
        for(const auto& value:snapshot_["chats"].toList()) {
            const auto row=value.toMap();
            if(row["id"]==channel&&row["call"].toBool()&&!row["muted"].toBool()) {
                media_.clear();emit commandRequested("voice-join",{{"channel",channel}});return true;
            }
        }
    }
    for(const auto& value:notifications()) {
        const auto row=value.toMap();
        if(row["id"]!=channel||!row["ringing"].toBool())continue;
        if(accept&&!row["answerable"].toBool())return false;
        if(accept)media_.clear();
        emit commandRequested(accept?"voice-join":"voice-decline",{{"channel",channel}});
        return true;
    }
    return false;
}
QVariantList SocialController::incomingCallActions(const QString& retainedChannel) const {
    QVariantList actions;
    if(snapshot_["state"]!="connected")return actions;
    // A ring can expire while the caller is still waiting. Retain only the
    // invitation the user was looking at; do not surface arbitrary active calls.
    if(!retainedChannel.isEmpty()) {
        const auto voice=snapshot_["voice"].toMap();
        for(const auto& value:snapshot_["chats"].toList()) {
            const auto row=value.toMap();
            if(row["id"]!=retainedChannel||row["ringing"].toBool()||!row["call"].toBool()||row["muted"].toBool())continue;
            if(voice["available"].toBool()&&voice["channel"].toString().isEmpty())
                actions.append(QVariantMap{{"id","answer-call:"+retainedChannel},{"label","Join call"},{"detail",row["name"]}});
        }
    }
    for(const auto& value:notifications()) {
        const auto row=value.toMap();if(!row["ringing"].toBool())continue;
        const auto channel=row["id"].toString();
        if(row["answerable"].toBool())actions.append(QVariantMap{{"id","answer-call:"+channel},{"label","Answer call"},{"detail",row["name"]}});
        actions.append(QVariantMap{{"id","decline-call:"+channel},{"label","Decline call"},{"detail",row["name"]}});
        break;
    }
    return actions;
}
QVariantList SocialController::notifications() const {
    QVariantList result;
    const auto lobbyInvite=lobby_.invitation();
    if(!lobbyInvite.isEmpty())result.append(QVariantMap{{"id","lobby:"+lobbyInvite["lobby"].toString()},
        {"name",lobbyInvite["name"].toString()},{"detail","Lobby invitation"},{"lobby",true},{"pending",true}});
    const auto invitation=snapshot_["online"].toMap();
    for(const auto& value:activityNotifications_) {
        if(value.toMap()["lobbyReminder"].toBool()){result.append(value);continue;}
        auto row=value.toMap();const bool pending=row["runtime"].toBool()?runtimeInvitation_["request"]==row["session"]:invitation["incoming"].toBool()&&invitation["session"]==row["session"];
        row["pending"]=pending;row["detail"]=pending?QString("Incoming activity"):QString("Missed invitation");
        if(row["runtime"].toBool())row["detail"]=row["detail"].toString()+" · "+row["game"].toString();
        result.append(row);
    }
    if(snapshot_["userId"].toString().isEmpty())return result;
    for(const auto& value:snapshot_["friends"].toList()) {
        auto row=value.toMap();if(row["type"].toInt()!=3)continue;
        if(snapshot_["temporaryContacts"].toMap()["peers"].toMap()[row["id"].toString()].toMap()["temporary"].toBool())continue;
        row["request"]=true;row["detail"]="Friend request";
        if(dismissedNotifications_.value(row["id"].toString()).toString()!=notificationStamp(row))result.append(row);
    }
    for(const auto& value:snapshot_["chats"].toList()) {
        auto row=value.toMap();if(row["socialHidden"].toBool()||(row["unread"].toInt()<=0&&!row["ringing"].toBool()&&row["missedCall"].toString().isEmpty())||row["muted"].toBool())continue;
        row["request"]=false;
        const auto voice=snapshot_["voice"].toMap();
        row["answerable"]=row["ringing"].toBool()&&snapshot_["state"]=="connected"
            &&voice["available"].toBool()&&voice["channel"].toString().isEmpty();
        if(!row["ringing"].toBool()&&dismissedNotifications_.contains(row["id"].toString())&&dismissedNotifications_.value(row["id"].toString()).toString()==notificationStamp(row))continue;
        row["detail"]=row["guild"].toString().isEmpty()?(row["kind"]=="groups"?"Group · New messages":"New messages"):"Community · New messages";
        // Fluxer also counts every unread DM as a mention. Only label the
        // community mention separately; ordinary DMs remain new messages.
        if(row["mentions"].toInt()>0&&!row["guild"].toString().isEmpty())row["detail"]="You were mentioned";
        if(!row["missedCall"].toString().isEmpty())row["detail"]="Missed call";
        if(row["ringing"].toBool())row["detail"]="Incoming call";
        result.append(row);
    }
    return result;
}
QString SocialController::notificationFaceAt(int index) const {
    const auto list=notifications();if(index<0||index>=list.size())return {};
    const auto row=list[index].toMap();
    if(row["lobby"].toBool()||row["lobbyReminder"].toBool())return "lobby";
    return row["request"].toBool()||row["activity"].toBool()?QString("chats"):row["kind"]=="groups"||!row["guild"].toString().isEmpty()?QString("communities"):QString("chats");
}
void SocialController::openNotificationAt(int index) {
    const auto list=notifications();const auto face=notificationFaceAt(index);if(face.isEmpty())return;
    const auto row=list[index].toMap();
    if(row["lobbyReminder"].toBool()) {
        activityNotifications_.removeIf([&](const QVariant& v){return v.toMap()["id"]==row["id"];});saveNotifications();
        openLobby();toastTimer_.stop();toastTitle_.clear();toastText_.clear();toastChannel_.clear();emit presentationChanged();return;
    }
    if(row["lobby"].toBool()) {
        setFace("lobby");selectLobby(1);toastTimer_.stop();toastTitle_.clear();toastText_.clear();toastChannel_.clear();emit presentationChanged();return;
    }
    if(row["runtime"].toBool()) {
        if(!row["pending"].toBool()){activityNotifications_.removeIf([&](const QVariant& v){return v.toMap()["id"]==row["id"];});saveNotifications();}
        setFace("chats");
        if(row["pending"].toBool())emit runtimeAction("multiplayer-request:"+row["session"].toString());
        else {menuMode_="missed";menuTitle_=row["name"].toString();menuDetail_="This invitation has expired. Ask for a new invitation.";menu_={"Close"};menuCommands_={"cancel"};menuFocus_=0;emit changed();}
        return;
    }
    if(row["activity"].toBool()) {
        if(row["pending"].toBool()){presentedInvitation_=row["session"].toString();emit changed();return;}
        activityNotifications_.removeIf([&](const QVariant& v){return v.toMap()["id"]==row["id"];});saveNotifications();
        setFace("chats");selection_.stop();contacts_=reading_=false;
        emit commandRequested("conversation",{{"id",row["channel"]}});emit changed();return;
    }
    setFace(face);selection_.stop();reading_=false;contacts_=false;
    if(row["request"].toBool()) {
        dismissedNotifications_[row["id"].toString()]=notificationStamp(row);saveNotifications();
        profile(row["id"].toString());
        const auto people=rows();for(int i=0;i<people.size();++i)if(people[i].toMap()["id"]==row["id"]){focus_=i;break;}
    } else {
        communityId_=row["guild"].toString();
        emit commandRequested("conversation",{{"id",row["id"]}});
    }
    toastTimer_.stop();toastTitle_.clear();toastText_.clear();toastChannel_.clear();
    emit presentationChanged();emit changed();
}
void SocialController::openNotification() {
    if(toastChannel_.startsWith("reminder:")){openLobby();toastTimer_.stop();toastTitle_.clear();toastText_.clear();toastChannel_.clear();emit presentationChanged();return;}
    const auto face=notificationFace();if(face.isEmpty())return;
    if(face=="lobby"){openNotificationAt(0);return;}
    const auto list=notifications();
    for(int i=0;i<list.size();++i) {
        const auto row=list[i].toMap();
        if((row["activity"].toBool()&&row["channel"]==toastChannel_)||(row["request"].toBool()&&"request:"+row["id"].toString()==toastChannel_)) {
            openNotificationAt(i);toastTimer_.stop();toastTitle_.clear();toastText_.clear();emit presentationChanged();return;
        }
    }
    const auto channel=toastChannel_;
    setFace(face);contacts_=false;reading_=false;selection_.stop();
    for(const auto& c:snapshot_["chats"].toList())if(c.toMap()["id"]==channel)communityId_=c.toMap()["guild"].toString();
    emit commandRequested("conversation",{{"id",channel}});
    toastTimer_.stop();toastTitle_.clear();toastText_.clear();toastChannel_.clear();
    emit presentationChanged();emit changed();
}
void SocialController::receive(quint64 generation,QVariantMap snapshot) {
    PerformanceTrace::Scope perf("SocialController::receive");
    if(generation!=generation_)return;
    const auto previousCall=incomingCall()["id"].toString();
    const auto relations=[](const QVariant& list){QStringList keys;for(const auto& v:list.toList()){const auto r=v.toMap();keys.append(r["id"].toString()+":"+r["type"].toString());}keys.sort();return keys;};
    const bool contactsChanged=relations(snapshot_["friends"])!=relations(snapshot["friends"]);
    const auto roomChannel=viewedLobby().state()["conversation"].toString();
    const auto hasRoomChannel=[&](const QVariant& chats){if(roomChannel.isEmpty())return false;for(const auto& v:chats.toList())if(v.toMap()["id"]==roomChannel)return true;return false;};
    const bool conversationAdmitted=!hasRoomChannel(snapshot_["chats"])&&hasRoomChannel(snapshot["chats"]);
    const auto oldId=rows().value(focus_).toMap().value("id");
    const auto oldChannel=snapshot_.value("channel");
    const bool wasEarlier=snapshot_["historyPast"].toBool();
    const auto oldMessage=messages().value(messageFocus_).toMap().value("id");
    const bool atEnd=messageFocus_>=messages().size()-1;
    if(snapshot_["userId"]!=snapshot["userId"])directConversationLease_.clear();
    const auto oldVoice=snapshot_["voice"].toMap(),newVoice=snapshot["voice"].toMap();
    const auto streamStatus=newVoice["streamStatus"].toString();
    if(streamStatus!=oldVoice["streamStatus"].toString()&&(streamStatus.contains("unavailable")||streamStatus.startsWith("Couldn't"))) {
        toastTitle_="Game stream";toastText_=streamStatus;toastTimer_.start();emit presentationChanged();
    }
    if(viewedLobby().hosting()&&(oldVoice["streams"]!=newVoice["streams"]||oldVoice["sharing"]!=newVoice["sharing"]
        ||oldVoice["channel"]!=newVoice["channel"]))lobbyPublishTimer_.start();
    if(stream_.active()&&((!oldVoice["watching"].toString().isEmpty()&&newVoice["watching"].toString().isEmpty())
        ||newVoice["channel"]!=oldVoice["channel"]||newVoice["state"].toString().isEmpty()))stream_.close();
    const bool switchingAccount=snapshot_["userId"]!=snapshot["userId"];
    snapshot_=std::move(snapshot);
    restoreSocialPreferences();
    if(contactsChanged||conversationAdmitted)lobbyChatKey_.clear();
    lobbyRuntime_.configure(snapshot_["userId"].toString(),snapshot_["state"]=="connected");
    resettingSocial_=switchingAccount;lobby_.configure(snapshot_["userId"].toString(),snapshot_["name"].toString(),snapshot_["state"]=="connected");resettingSocial_=false;
    if(contactsChanged||conversationAdmitted)lobbyChatKeys_.clear();
    syncMyLobbies();syncLobbyChat();
    if(oldChannel!=snapshot_["channel"]){partyFocus_=false;partyIndex_=0;refreshPartyBrowse();}
    const auto audio=snapshot_["audio"].toMap();
    media_.setAudioDevices(audio["input"].toString(),audio["output"].toString(),audio.value("volume",100).toInt());
    if(snapshot_["state"]=="signed-out")media_.clear();
    if(online()["open"].toBool())menu_.clear();
    else if(menuMode_=="online"&&!menu_.isEmpty()) {
        menuDetail_=online()["status"].toString();menu_.clear();menuCommands_.clear();
        for(const auto& value:online()["actions"].toList()){const auto a=value.toMap();menu_.append(a["label"].toString());menuCommands_.append("online:"+a["id"].toString());}
        if(menu_.isEmpty()) {menu_.append("Close");menuCommands_.append("cancel");
            if(online()["stage"]=="available")menuDetail_="TrainerOS is connected, but no matching activity is ready. Choose a supported saved Adventure on both devices.";}
        menuFocus_=qBound(0,menuFocus_,int(menu_.size())-1);
    }
    if(menuMode_=="community-invite"&&!menu_.isEmpty())menuDetail_=snapshot_["communityInvite"].toString().isEmpty()
        ?snapshot_["mutationBusy"].toBool()?"Creating invitation...":snapshot_["communityStatus"].toString()
        :snapshot_["communityInvite"].toString();
    const auto accountId=snapshot_["userId"].toString();
    if(!accountId.isEmpty())bindDrafts(accountId);
    if(!owner_.isEmpty()&&!accountId.isEmpty()) {
        const auto key="social/dismissed/"+QString::fromLatin1(QCryptographicHash::hash((owner_+"/"+accountId).toUtf8(),QCryptographicHash::Sha256).toHex());
        if(key!=notificationSettingsKey_){
            notificationSettingsKey_=key;dismissedNotifications_=QSettings().value(key).toMap();
            activityNotifications_=QSettings().value(key+"-activities").toList();
            while(activityNotifications_.size()>32)activityNotifications_.removeLast();
        }
    }
    const auto invitation=snapshot_["online"].toMap();
    const auto session=invitation["session"].toString();
    if(invitation["incoming"].toBool()&&!session.isEmpty()) {
        const bool known=dismissedNotifications_.contains("activity:"+session)||std::any_of(activityNotifications_.cbegin(),activityNotifications_.cend(),[&](const QVariant& v){return v.toMap()["session"]==session;});
        if(!known) {
            presentedInvitation_.clear();
            activityNotifications_.prepend(QVariantMap{{"id","activity:"+session},{"activity",true},{"session",session},
                {"channel",invitation["channel"]},{"name",invitation["name"]},{"kind","chats"}});
            while(activityNotifications_.size()>32)activityNotifications_.removeLast();saveNotifications();
            if(!snapshot_["doNotDisturb"].toBool()&&(surfaceAvailable_||gameActive_)) {
                toastChannel_=invitation["channel"].toString();toastTitle_="Activity invitation";
                toastText_=snapshot_["privatePreviews"].toBool()?QString("Open Home notifications"):invitation["status"].toString();
                toastTimer_.start();emit presentationChanged();
                if(snapshot_.value("notificationSound",true).toBool())media_.chime();
                if(gameActive_)emit backgroundNotification(toastTitle_,toastText_);
            }
        }
    } else presentedInvitation_.clear();
    if(snapshot_.contains("friends")&&snapshot_.contains("chats")) {
        QSet<QString> existing;
        if(!session.isEmpty())existing.insert("activity:"+session);
        for(const auto& v:snapshot_["friends"].toList())if(v.toMap()["type"].toInt()==3)existing.insert(v.toMap()["id"].toString());
        for(const auto& v:snapshot_["chats"].toList())existing.insert(v.toMap()["id"].toString());
        for(auto it=dismissedNotifications_.begin();it!=dismissedNotifications_.end();)
            if(!existing.contains(it.key()))it=dismissedNotifications_.erase(it);else ++it;
    }
    const auto list=rows();focus_=qBound(-1,focus_,qMax(0,int(list.size())-1));
    for(int i=0;i<list.size();++i)if(list[i].toMap()["id"]==oldId){focus_=i;break;}
    if(oldChannel!=snapshot_["channel"]) {
        reading_=false;
        if(!contacts_)for(int i=0;i<list.size();++i)if(list[i].toMap()["id"]==snapshot_["channel"]||list[i].toMap()["channel"]==snapshot_["channel"]){focus_=i;break;}
    }
    if(!snapshot_["channel"].toString().isEmpty()&&(!enterConversationPerson_.isEmpty()||!enterConversationChannel_.isEmpty())) {
        bool matches=snapshot_["channel"]==enterConversationChannel_;
        for(const auto& p:currentChat()["members"].toList())if(p.toMap()["id"]==enterConversationPerson_)matches=true;
        if(matches){reading_=true;conversationControl_=messages().isEmpty()?11:-1;enterConversationPerson_.clear();enterConversationChannel_.clear();}
    }
    const auto log=messages();
    const bool prepended=!messages().isEmpty() && snapshot_["historyPast"].toBool() && oldMessage.isValid();
    if((atEnd&&!prepended)||oldChannel!=snapshot_["channel"]||(wasEarlier&&!snapshot_["historyPast"].toBool()))messageFocus_=qMax(0,int(log.size())-1);
    else for(int i=0;i<log.size();++i)if(log[i].toMap()["id"]==oldMessage){messageFocus_=i;break;}
    if(oldChannel!=snapshot_["channel"]||(!oldMessage.isValid()&&!log.isEmpty())) {
        const auto anchor=snapshot_["historyAnchor"].toString();
        if(!anchor.isEmpty())for(int i=0;i<log.size();++i)if(log[i].toMap()["id"]==anchor){messageFocus_=i;break;}
    }
    messageFocus_=qBound(0,messageFocus_,qMax(0,int(log.size())-1));
    if(snapshot_["state"]=="signed-out") {dismissedNotifications_.clear();activityNotifications_.clear();presentedInvitation_.clear();notificationSettingsKey_.clear();toastTimer_.stop();toastTitle_.clear();toastText_.clear();emit presentationChanged();draftSave_.stop();if(!draftFile_.isEmpty())QFile::remove(draftFile_);draftFile_.clear();drafts_.clear();editDrafts_.clear();menu_.clear();textPurpose_.clear();textChannel_.clear();searchStarted_=false;}
    if(face_=="friends"&&!searchStarted_&&snapshot_["state"]=="connected")runSearch();
    searchFocus_=qMin(searchFocus_,int(searchResults().size())-1);
    if(!snapshot_["searching"].toBool()&&!searchResults().isEmpty()&&!discoverAll_) {
        const int pageSize=query_.isEmpty()?3:6;
        discoverPage_=qBound(0,discoverPage_,(int(searchResults().size())-1)/pageSize);
        if(searchFocus_<0&&discoverArea_=="results")searchFocus_=discoverPage_*pageSize;
    }
    const auto call=incomingCall();
    if(!call.isEmpty()&&call["id"].toString()!=previousCall&&(surfaceAvailable_||gameActive_)&&snapshot_.value("notificationSound",true).toBool())media_.chime();
    emit changed();
}
QVariantMap SocialController::account() const {
    auto result=snapshot_;result.remove("friends");result.remove("chats");result.remove("messages");
    result["available"]=!owner_.isEmpty();return result;
}
QVariantList SocialController::quickPeople() const {
    if(snapshot_["userId"].toString().isEmpty() || snapshot_["state"]=="signed-out")return {};
    QMap<QString,QVariantMap> people;
    QSet<QString> excluded{snapshot_["userId"].toString()};
    const auto friends=snapshot_["friends"].toList();
    for(const auto& v:friends)if(v.toMap()["type"].toInt()==2)excluded.insert(v.toMap()["id"].toString());
    for(const auto& v:snapshot_["chats"].toList()) {
        const auto chat=v.toMap();const auto members=chat["members"].toList();
        if(chat["kind"]!="chats" || !chat["guild"].toString().isEmpty() || members.size()!=1)continue;
        const auto id=members.first().toMap()["id"].toString();
        if(chat["muted"].toBool()){excluded.insert(id);continue;}
        const auto when=chat["lastInteraction"].toLongLong();
        if(id.isEmpty() || excluded.contains(id) || when<=0 || chat["id"].toString().isEmpty())continue;
        if(people.contains(id) && people[id]["lastInteraction"].toLongLong()>=when)continue;
        people[id]={{"id",id},{"name",chat["name"]},{"avatar",chat["avatar"]},
            {"conversation",chat["id"]},{"unread",chat["unread"]},{"lastInteraction",when}};
    }
    for(const auto& v:recentTogether_.rows()) {
        const auto recent=v.toMap();const auto id=recent["id"].toString();
        if(excluded.contains(id))continue;
        auto row=people.value(id,recent);row["lastInteraction"]=qMax(row["lastInteraction"].toLongLong(),recent["last"].toLongLong()*1000);
        people[id]=row;
    }
    for(const auto& v:friends) {
        const auto person=v.toMap();const auto id=person["id"].toString();if(!people.contains(id))continue;
        if(!person["name"].toString().isEmpty())people[id]["name"]=person["name"];
        if(!person["avatar"].toString().isEmpty())people[id]["avatar"]=person["avatar"];
    }
    QVariantList result;for(auto it=people.cbegin();it!=people.cend();++it)if(!excluded.contains(it.key()))result.append(it.value());
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){const auto x=a.toMap(),y=b.toMap();
        return x["lastInteraction"]==y["lastInteraction"] ? x["id"].toString()<y["id"].toString() : x["lastInteraction"].toLongLong()>y["lastInteraction"].toLongLong();});
    while(result.size()>16)result.removeLast();return result;
}
bool SocialController::openQuickConversation(const QString& id) {
    for(const auto& value:snapshot_.value("chats").toList()) {
        const auto row=value.toMap();if(row["id"]!=id)continue;
        setFace(row["kind"]=="groups"||!row["guild"].toString().isEmpty()?"communities":"chats");
        if(!row["guild"].toString().isEmpty())communityId_=row["guild"].toString();
        contacts_=reading_=false;selection_.stop();
        emit commandRequested("conversation",{{"id",id}});emit changed();return true;
    }return false;
}
QVariantList SocialController::rows() const {
    if(face_=="chats")return peopleRows();
    if(face_=="communities")return communityRows();
    return {};
}
QString SocialController::conversationName() const {
    for(const auto& c:snapshot_.value("chats").toList())if(c.toMap()["id"]==snapshot_["channel"])return c.toMap()["name"].toString();
    return "Conversation";
}
QVariantList SocialController::hints() const {
    QVariantList result;
    auto h=[&](QString key,QString label){result.append(QVariantMap{{"button",key},{"label",label}});};
    if(!menu_.isEmpty()){h("A",menuMode_=="create-group"?"Choose friend":"Choose");if(menuMode_=="create-group"&&!pickedPeople_.isEmpty())h("Y","Create group");h("B","Close");return result;}
    if(stream_.active()){h("A","Streams");h("B","Stop watching");return result;}
    if(face_=="recent") {
        if(!recentPeople().isEmpty()) {
            if(snapshot_["state"]=="connected")h("A","Profile");
            const auto row=recentPeople().value(recentFocus()).toMap();
            if(row["canAdd"].toBool())h("X",row["friendLabel"].toString());
            h("Select","Options");
        }
        return result;
    }
    if(face_=="lobby") {
        if(lobbyChatFocused_&&lobbySpectatorChat_)h("↑↓","Read");else h("A",lobbyChatFocused_?"Message":lobbyDetailFocused_||lobbyRailFocused_?"Choose":"Open");
        h("B",lobbyChatFocused_?"Room":lobbyDetailFocused_?"Catalogue":!lobbyRailFocused_?"Categories":"Back");
        if(!lobbyDetailFocused_) {if(lobbyRailFocused_)h("\u2191\u2193","Choose");else {h("\u2190\u2192","Room / Page");h("\u2191\u2193","Room");}}
        if(!lobbyChatFocused_||!lobbySpectatorChat_)h("Select",lobbyDetailFocused_&&lobbyRows().value(lobbyFocus()).toMap()["kind"]=="own"?"Options":"Filters");
        if(lobbyChatVisible()) {h("R3",lobbyChatExpanded_?"Room":"Expand chat");if(lobbyChatFocused_)h("L3",lobbySpectatorChat_?"Players":"Spectators");}return result;
    }
    if(snapshot_["state"]=="restoring")return result;
    if(snapshot_.value("state")=="authorizing"){h("B","Cancel sign-in");return result;}
    if(snapshot_.value("state")=="signed-out"||snapshot_.isEmpty()){if(!owner_.isEmpty())h("A","Sign in");return result;}
    if(face_=="friends") {
        h("A",!discoverPreview_.isEmpty()?"Join":discoverArea_=="search"?"Search":"Open");h("B",discoverAll_?"Discover":"Back");
        if(discoverArea_=="search")h("←→","People / Communities");
    } else {
        if(contacts_)h("A",rows().value(focus_).toMap()["type"].toInt()==3?"Accept request":"Conversation");
        else {
            QString action = focus_<0?(communityOpen()?"Back":face_=="communities"?"Create":"Find"):"Open";
            if(reading_) {
                switch(conversationControl_) {
                case 0: action="Info"; break;
                case 1: action="Call"; break;
                case 2: action="Options"; break;
                case 3: action="Microphone"; break;
                case 4: action="Call sound"; break;
                case 5: action="End call"; break;
                case 10: action="Attach"; break;
                case 11: action="Write"; break;
                case 12: action="Emoji"; break;
                case 13: action="Voice message"; break;
                case 14: action="Send"; break;
                default: action="Message"; break;
                }
            }
            h("A",action);
        }
        if(conversation()){h("R3","Write");h("L3","Info");}
        if(reading_&&conversationControl_>=0&&conversationControl_<3&&snapshot_["historyMore"].toBool())h("↑","Earlier");
        if(reading_)h("B",face_=="communities"?"Channels":"People");
        else if(contacts_||communityOpen())h("B","Back");
    }
    h("Select","Options");return result;
}
void SocialController::login(){if(!owner_.isEmpty())emit commandRequested("login",{});}
void SocialController::activate(int index) {
    if(!menu_.isEmpty())return;
    if(index<0){if(communityOpen())communityBack();else collectionOptions();return;}
    const auto list=rows();if(index>=list.size())return;
    focus_=index;reading_=false;selection_.stop();const auto row=list[index].toMap();
    if(row["kind"]=="community") {
        communityId_=row["id"].toString();focus_=0;saveSocialPreferences();
        emit commandRequested("guild",{{"id",communityId_}});
    } else if(row["kind"]=="person") {
        enterConversationPerson_=row["id"].toString();enterConversationChannel_=row["channel"].toString();
        if(!row["channel"].toString().isEmpty())emit commandRequested("open",{{"id",row["channel"]}});
        else emit commandRequested("dm",{{"id",row["id"]}});
    } else {enterConversationChannel_=row["id"].toString();emit commandRequested("open",{{"id",row["id"]}});}
    if(!enterConversationChannel_.isEmpty()&&enterConversationChannel_==snapshot_["channel"]){reading_=true;conversationControl_=messages().isEmpty()?11:-1;enterConversationPerson_.clear();enterConversationChannel_.clear();}
    emit changed();
}
void SocialController::compose() {
    if(lobbyChatVisible()&&lobbySpectatorChat_)return;
    if(!conversation())return;
    selection_.stop();
    textPurpose_="message";textChannel_=snapshot_["channel"].toString();
    emit textRequested("Message - "+conversationName(),drafts_.value(textChannel_),2000);
}
void SocialController::sendDraft() {
    if(lobbyChatVisible()&&lobbySpectatorChat_)return;
    if(!menu_.isEmpty()||snapshot_["state"]!="connected")return;
    selection_.stop();
    if(!files_.selected().isEmpty()){files_.send(draft());return;}
    send();
}
void SocialController::retryMessage(const QString& id) {
    if(!conversation()||snapshot_["state"]!="connected"||!menu_.isEmpty())return;
    for(const auto& value:messages()) {
        const auto row=value.toMap();
        if(row["id"]==id&&row["retryable"].toBool()){
            emit commandRequested("retry-message",{{"channel",snapshot_["channel"]},{"id",id}});return;
        }
    }
}
void SocialController::lobbyChatTools(){
    if(!lobbyChatVisible())return;
    menu_.clear();menuCommands_.clear();menuMode_="lobby-files";menuTitle_="Lobby chat";menuDetail_.clear();menuFocus_=0;
    menuChannel_=snapshot_["channel"].toString();
    if(!lobbySpectatorChat_){
        menu_={"Write","Emoji","Attach files","Send"};menuCommands_={"lobby-write","lobby-emoji","lobby-file","lobby-send"};
        const auto files=files_.selected();for(int i=0;i<files.size();++i){menu_.append("Remove "+files[i].toMap()["name"].toString());menuCommands_.append("lobby-file-remove:"+QString::number(i));}
        if(files_.busy()){menu_.append("Cancel transfer");menuCommands_.append("lobby-file-cancel");}
    }
    const auto message=messages().value(messageFocus_).toMap();
    for(const auto& a:message["attachments"].toList()){const auto f=a.toMap();menu_.append("Download "+f["filename"].toString());menuCommands_.append("lobby-file-download:"+message["id"].toString()+":"+f["id"].toString());}
    menu_.append("Close");menuCommands_.append("cancel");emit changed();
}
void SocialController::attachFile(){if(conversation()&&!(lobbyChatVisible()&&lobbySpectatorChat_)&&snapshot_["state"]=="connected")files_.choose();}
void SocialController::downloadFile(QString message,QString attachment){
    if(!conversation()||snapshot_["state"]!="connected")return;
    const auto task=snapshot_["channel"].toString()+":"+message+":"+attachment;
    for(const auto& v:files_.downloads())if(v.toMap()["id"]==task&&v.toMap()["state"]=="done"){files_.control(task,"open");return;}
    emit filesDownloadRequested();
    emit commandRequested("files-download",{{"channel",snapshot_["channel"]},{"message",message},{"attachment",attachment}});
}
void SocialController::attachPicture() {
    if(lobbyChatVisible()&&lobbySpectatorChat_)return;
    if(!conversation()||!menu_.isEmpty()||snapshot_["state"]!="connected")return;
    selection_.stop();menuChannel_=snapshot_["channel"].toString();menuMode_="picture";
    menuCommands_={"picture"};selectMenu(0);
}
void SocialController::recordVoice() {
    if(lobbyChatVisible()&&lobbySpectatorChat_)return;
    if(!conversation()||!menu_.isEmpty()||snapshot_["state"]!="connected")return;
    if(!snapshot_["voice"].toMap()["channel"].toString().isEmpty())return;
    selection_.stop();menuChannel_=snapshot_["channel"].toString();menuMode_="record";
    menuCommands_={"record"};selectMenu(0);
}
void SocialController::call() {
    if(!conversation()||!menu_.isEmpty()||snapshot_["state"]!="connected")return;
    const auto voice=snapshot_["voice"].toMap();
    if(!voice["channel"].toString().isEmpty()){
        if(voice["channel"]!=snapshot_["channel"]&&currentChat()["guild"].toString().isEmpty()) {
            menuMode_="switch-call";menuTitle_="Switch call?";
            menuDetail_="Leave "+voice["name"].toString()+" and call "+conversationName()+"?";
            menuChannel_=snapshot_["channel"].toString();menuSubject_=voice["channel"].toString();
            menu_={"Keep current call","Switch call"};menuCommands_={"cancel","switch-call"};menuFocus_=0;emit changed();return;
        }
        menuMode_="call";menuTitle_="Call";menuDetail_=voice["name"].toString();menuChannel_=voice["channel"].toString();menuFocus_=0;
        menu_={voice["muted"].toBool()?"Turn microphone on":"Mute microphone",voice["deaf"].toBool()?"Enable call sound":"Silence call sound","Leave call"};
        menuCommands_={"voice-mute","voice-output","voice-leave"};emit changed();return;
    }
    if(!voice["available"].toBool()||!currentChat()["guild"].toString().isEmpty())return;
    selection_.stop();media_.clear();emit commandRequested("voice-join",{{"channel",snapshot_["channel"]}});
}
void SocialController::selectMessage(int index) {
    if(lobbyChatVisible()&&lobbySpectatorChat_){retainMessagePosition(index,false);return;}
    if(index<0||index>=messages().size()||!menu_.isEmpty())return;
    selection_.stop();messageFocus_=index;reading_=true;openMenu();
}
void SocialController::retainMessagePosition(int index,bool latest) {
    if(!conversation()||!menu_.isEmpty()||index<0||index>=messages().size())return;
    reading_=true;messageFocus_=latest?qMax(0,int(messages().size())-1):index;
    emit commandRequested("history-position",{{"channel",snapshot_["channel"]},{"id",latest?QString():messages().value(messageFocus_).toMap()["id"].toString()}});
    emit changed();
}
void SocialController::preserveText(QString text) {
    // Input and provider limits report errors; never silently cut a composed emoji/text draft.
    if(textPurpose_=="message"&&!textChannel_.isEmpty()){drafts_[textChannel_]=text;draftSave_.start();}
    else if(textPurpose_=="edit-message")editDrafts_[textChannel_+"/"+textId_]=text;
}
void SocialController::applyText(QString text) {
    if(textPurpose_=="people-filter"){peopleFilter_=text.trimmed();focus_=0;textPurpose_.clear();emit changed();return;}
    if(textPurpose_.startsWith("lobby-draft-")||textPurpose_=="lobby-picker-search") {
        if(textPurpose_=="lobby-picker-search") {lobbyPickerSearch_=text.trimmed();refreshLobbyPicker();}
        else if(lobbyCreateOpen_&&lobbyDraftAccount_==(lobbyDraft_["type"]=="local"?localLobby_.identity():snapshot_["userId"].toString())) {
            if(textPurpose_=="lobby-draft-name")lobbyDraft_["title"]=text.trimmed().left(80);
            else {const auto date=QDateTime::fromString(text.trimmed(),"yyyy-MM-dd HH:mm");
                if(date.isValid()&&date>QDateTime::currentDateTime()&&date<QDateTime::currentDateTime().addDays(366)) {lobbyDraft_["startsAt"]=date.toSecsSinceEpoch();lobbyCreationError_.clear();}
                else lobbyCreationError_="Choose a future date within the next year.";
            }
        }
        textPurpose_.clear();emit changed();return;
    }
    if(textPurpose_=="lobby-title"||textPurpose_=="lobby-time") {
        const auto state=viewedLobby().state();auto title=state["title"].toString();if(title.isEmpty())title=snapshot_["name"].toString().left(65)+"'s lobby";auto time=state["startsAt"].toInteger();
        if(textPurpose_=="lobby-title")title=text.trimmed();
        else {
            const auto date=QDateTime::fromString(text.trimmed(),"yyyy-MM-dd HH:mm");
            if(!date.isValid()||date<=QDateTime::currentDateTime()) {
                toastTitle_="Start time";toastText_="Enter a future local time: YYYY-MM-DD HH:mm";toastTimer_.start();emit presentationChanged();return;
            }
            time=date.toSecsSinceEpoch();
        }
        if(viewedLobby().setDetails(title,time)) {
            if(textPurpose_=="lobby-time"&&!lobbyReminders_.contains(state["id"].toString()))
                lobbyReminders_.accept(viewedLobby().state(),QDateTime::currentSecsSinceEpoch());
        } else {toastTitle_="Lobby";toastText_="Could not change these lobby details.";toastTimer_.start();emit presentationChanged();}
        textPurpose_.clear();emit changed();return;
    }
    if(textPurpose_=="lobby-search") {lobbySearch_=text.trimmed();lobbySelection_.clear();textPurpose_.clear();emit changed();return;}
    if(textSecret()) {
        const auto guild=textId_,method=textPurpose_.mid(QStringLiteral("community-secret:").size());
        textPurpose_.clear();textId_.clear();
        if(!guild.isEmpty()&&!text.isEmpty()) {
            closeMenu();menuMode_="community-progress";menuTitle_="Community";menu_={"Deleting community…"};menuCommands_={"none"};menuFocus_=0;emit changed();
            emit commandRequested("delete-community",{{"id",guild},{"method",method},{"secret",text}});
        }
        text.fill(QChar());return;
    }
    if(textPurpose_=="search"){query_=text.trimmed();discoverAll_=false;discoverPage_=0;searchFocus_=-1;runSearch();}
    else if(textPurpose_=="add")emit commandRequested("add",{{"text",text}});
    else if(textPurpose_=="create-community")emit commandRequested("create-community",{{"text",text}});
    else if(textPurpose_=="edit-message"||textPurpose_=="rename-group") {
        preserveText(text);emit commandRequested(textPurpose_,{{"channel",textChannel_},{"id",textId_},{"text",text}});
    } else if(textPurpose_=="message") {
        preserveText(text);
        if(textChannel_==snapshot_["channel"].toString()){
            if(directConversationLease_.startsWith("lobby:"))sendGameChat(directConversationLease_.mid(6),textChannel_);
            else if(directConversationLease_.startsWith("notification:"))sendNotificationChat(textChannel_);
            else if(lobbyChatVisible())sendDraft();
            else send();
        }
    } else preserveText(text);
    textPurpose_.clear();textChannel_.clear();emit changed();
}
void SocialController::send() {
    if(!conversation()||draft().trimmed().isEmpty())return;
    emit commandRequested("send",{{"channel",snapshot_["channel"]},{"text",draft()}});
    drafts_.remove(snapshot_["channel"].toString());emit changed();
    saveDrafts();emit messageQueued(snapshot_["channel"].toString());
}
QVariantList SocialController::runtimeCompanies() const {
    QVariantList result;if(snapshot_["state"]!="connected")return result;
    for(const auto& v:snapshot_["chats"].toList())if(v.toMap()["kind"]=="groups")result.append(v);
    return result;
}
bool SocialController::companyAvailable() const {return runtimeAvailable_&&(face_=="chats"||face_=="groups")&&currentChat()["kind"]=="groups";}
void SocialController::refreshPartyBrowse() {
    if(!conversationVisible_||!runtimeAvailable_)return;
    if(companyAvailable())emit companyQuery(snapshot_["channel"].toString());
    else if(!runtimePeer().isEmpty())emit partyQuery(runtimePeer());
}
QVariantList SocialController::companyParties() const {
    QVariantList result;if(!companyAvailable())return result;
    for(const auto& v:companyParties_) {
        auto row=v.toMap();
        if(row["company"]!=snapshot_["channel"]||(!gameParty_["party"].toString().isEmpty()&&row["party"]==gameParty_["party"]))continue;
        if(gameActive_&&row["lobby"].toString().isEmpty())row["joinable"]=false;
        result.append(row);
    }
    return result;
}
void SocialController::setCompanyParties(QVariantList rows) {
    if(companyParties_==rows)return;
    const auto selected=companyParties().value(partyIndex_).toMap()["peer"];
    companyParties_=std::move(rows);const auto visible=companyParties();
    for(int i=0;i<visible.size();++i)if(visible[i].toMap()["peer"]==selected)partyIndex_=i;
    partyIndex_=qBound(0,partyIndex_,qMax(0,int(visible.size())-1));
    if(visible.isEmpty())partyFocus_=false;
    emit changed();
}
void SocialController::joinCompanyParty(int index) {
    const auto row=companyParties().value(index).toMap();
    if(!row["joinable"].toBool()||(gameActive_&&row["lobby"].toString().isEmpty()))return;
    partyIndex_=index;directJoin(row["peer"].toString(),row);
}
QVariantMap SocialController::joinOffer(const QString& peer) const {
    if(gameActivities_.contains(peer))return gameActivities_.value(peer);
    for(const auto& value:companyParties_)if(value.toMap()["peer"]==peer)return value.toMap();
    return {};
}
void SocialController::addJoinChoice(QString label,const QString& command,const QString& peer,const QVariantMap& offer) {
    if(offer["game"].toMap()["privateProgress"].toBool()) {
        label+=" (save copies)";
        const QString notice="Temporary save copies are shared with the players in this session. Only your own progress returns.";
        if(!menuDetail_.contains(notice))menuDetail_+=(menuDetail_.isEmpty()?QString():"\n")+notice;
    }
    if(offer["game"].toMap()["temporaryProgress"].toBool()) {
        label+=" (temporary progress)";
        const QString notice="This session starts fresh. Progress isn't kept.";
        if(!menuDetail_.contains(notice))menuDetail_+=(menuDetail_.isEmpty()?QString():"\n")+notice;
    }
    joinChoices_[command]={{"peer",peer},{"game",offer["game"]},{"party",offer["party"].toString()},{"lobby",offer["lobby"].toString()},
        {"generation",QVariant::fromValue(generation_)},{"owner",owner_},{"account",snapshot_["userId"]}};
    menu_.append(label);menuCommands_.append(command);
}
void SocialController::directJoin(const QString& peer,const QVariantMap& offer) {
    if(!peer.isEmpty()&&offer["joinable"].toBool()&&!offer["lobby"].toString().isEmpty()) {
        emit partyJoin(peer,{{"lobby",offer["lobby"].toString()}});return;
    }
    if(peer.isEmpty()||!offer["joinable"].toBool()||gameActive_)return;
    if(offer["game"].toMap()["privateProgress"].toBool()||offer["game"].toMap()["temporaryProgress"].toBool()) {
        // Legacy direct/controller entry points must display the same disclosure
        // before emitting consent. Ordinary Together rows already do this inline.
        closeMenu();menuMode_="join";menuTitle_="Join game";menuDetail_=offer["game"].toMap()["label"].toString();menuFocus_=0;
        addJoinChoice("Join","join-reviewed",peer,offer);menu_.append("Back");menuCommands_.append("cancel");emit changed();return;
    }
    emit partyJoin(peer,{{"game",QJsonObject::fromVariantMap(offer["game"].toMap())},{"party",offer["party"].toString()}});
}
QString SocialController::companySettingsKey(const QString& company) const {
    if(owner_.isEmpty()||snapshot_["userId"].toString().isEmpty()||company.isEmpty())return {};
    return "social/companyAccess/"+owner_+"/"+snapshot_["userId"].toString()+"/"+company;
}
QVariantMap SocialController::companyAccess(const QString& company) const {
    const auto key=companySettingsKey(company);
    auto result=key.isEmpty()?QVariantMap{}:QSettings().value(key).toMap();
    if(!QStringList{"request","selected","closed"}.contains(result["policy"].toString()))result["policy"]="request";
    // Defaults are per organizer and account, never a group's mere membership.
    return result;
}
void SocialController::editCompanyAccess() {
    if(!companyAvailable())return;menuChannel_=snapshot_["channel"].toString();menuFocus_=0;companyAccessMenu();
}
void SocialController::companyAccessMenu() {
    QVariantMap chat;for(const auto& v:runtimeCompanies())if(v.toMap()["id"]==menuChannel_)chat=v.toMap();
    if(chat.isEmpty()){closeMenu();return;}
    const auto access=companyAccess(menuChannel_);const auto policy=access["policy"].toString();
    menuMode_="company-access";menuTitle_="Who can join my games";menuDetail_=chat["name"].toString();
    menu_.clear();menuCommands_.clear();
    const QList<QPair<QString,QString>> modes{{"request","Ask me first"},{"selected","Selected members can join"},{"closed","Invitations only"}};
    for(const auto& mode:modes){menu_.append((policy==mode.first?"✓ ":"○ ")+mode.second);menuCommands_.append("company-policy:"+mode.first);}
    if(policy=="selected")for(const auto& v:chat["members"].toList()) {
        const auto m=v.toMap();auto name=m["global_name"].toString();if(name.isEmpty())name=m["username"].toString();
        menu_.append((access["allowed"].toStringList().contains(m["id"].toString())?"✓ ":"○ ")+name);
        menuCommands_.append("company-member:"+m["id"].toString());
    }
    menu_.append("Done");menuCommands_.append("cancel");menuFocus_=qBound(0,menuFocus_,int(menu_.size())-1);emit changed();
}
QString SocialController::runtimePeer() const {
    const auto chat=currentChat();const auto members=chat["members"].toList();
    return face_=="chats"&&!contacts_&&chat["friend"].toBool()&&members.size()==1?members.first().toMap()["id"].toString():QString();
}
void SocialController::setGameParty(QVariantMap state){
    if(gameParty_==state)return;
    gameParty_=std::move(state);
    QVariantList peers;
    if(gameParty_["running"].toBool())for(const auto& v:gameParty_["members"].toList()) {
        const auto m=v.toMap();if(!m["ready"].toBool())continue;
        auto peer=m["peer"].toString();if(peer.isEmpty()&&!gameParty_["host"].toBool())peer=gameParty_["hostPeer"].toString();
        if(peer.startsWith("online:"))peers.append(peer.mid(7));
    }
    emit commandRequested("lobby-contact-game",{{"account",snapshot_["userId"]},{"peers",peers}});
    partyIndex_=qBound(0,partyIndex_,qMax(0,int(companyParties().size())-1));
    if(companyParties().isEmpty())partyFocus_=false;
    emit changed();
}
QVariantMap SocialController::gameActivity() const {return gameActivities_.value(runtimePeer());}
void SocialController::setGameActivity(const QString& peer,const QVariantMap& offer) {
    if(gameActivities_.value(peer)==offer)return;
    if(offer.isEmpty())gameActivities_.remove(peer);else gameActivities_[peer]=offer;
    if(peer==runtimePeer())emit changed();
}
void SocialController::joinGame() {
    directJoin(runtimePeer(),gameActivity());
}
QVariantMap SocialController::currentChat() const {
    for(const auto& row:snapshot_["chats"].toList())if(row.toMap()["id"]==snapshot_["channel"])return row.toMap();
    return {};
}
QVariantMap SocialController::menuChat() const {
    for(const auto& row:snapshot_["chats"].toList())if(row.toMap()["id"]==menuChannel_)return row.toMap();
    return {};
}
QVariantMap SocialController::person() const {
    const auto p=snapshot_["person"].toMap();
    return profileVisible()&&p["id"]==profileTarget_&&p["request"]==profileRequest_?p:QVariantMap{{"status","Loading profile..."}};
}
void SocialController::conversationProfile() {
    const auto chat=currentChat();
    if(chat["kind"]=="groups"){groupDetails();return;}
    const auto members=chat["members"].toList();
    if(members.size()==1)profile(members.first().toMap()["id"].toString());
    else if(!chat["guild"].toString().isEmpty())communityInfo();
}
void SocialController::profile(QString id) {
    if(id.isEmpty())id=snapshot_["userId"].toString();
    if(id.isEmpty())return;
    selection_.stop();profileTarget_=id;profileRequest_=QUuid::createUuid().toString();menuSubject_=id;menuMode_="profile";
    menuTitle_="Profile";menuDetail_.clear();menuFocus_=0;menu_.clear();menuCommands_.clear();
    auto add=[&](QString text,QString action){menu_.append(text);menuCommands_.append(action);};
    if(id==snapshot_["userId"].toString())add("Edit profile","communication-settings");
    else {
        add("Message","dm");
        const auto members=currentChat()["members"].toList();
        if(members.size()==1&&members.first().toMap()["id"]==id&&snapshot_["voice"].toMap()["available"].toBool())add("Call","call-controls");
        int type=0;for(const auto& v:snapshot_["friends"].toList())if(v.toMap()["id"]==id)type=v.toMap()["type"].toInt();
        if(type==3){add("Accept friend request","accept");add("Decline friend request","remove");}
        if(type==0)add("Add friend","add-id");
        if(type==1)add("Remove friend","ask-remove-friend");
        if(type==1&&snapshot_["temporaryContacts"].toMap()["peers"].toMap()[id].toMap()["canKeep"].toBool())add("Keep as friend","contact-keep");
        if(type==4)add("Cancel friend request","remove");
        add(type==2?"Unblock":"Block",type==2?"remove":"block");
    }
    add("Copy username","copy-tag");add("Close","cancel");
    emit commandRequested("profile-read",{{"id",id},{"request",profileRequest_}});emit changed();
}
void SocialController::contextRow(int index) {
    joinChoices_.clear();selection_.stop();if(index>=0)reading_=false;
    auto row=index<0?currentChat():rows().value(index).toMap();
    if(row.isEmpty()){collectionOptions();return;}
    if(row["kind"]=="chats") {
        const auto members=row["members"].toList();if(members.size()==1){row["channel"]=row["id"];row["id"]=members.first().toMap()["id"];row["kind"]="person";}
    }
    const auto kind=row["kind"].toString();menuSubject_=kind+":"+row["id"].toString();
    menuChannel_=kind=="community"?QString():kind=="person"?row["channel"].toString():row["id"].toString();
    menuGuild_=kind=="community"?row["id"].toString():row["guild"].toString();
    menuMode_="context";menuTitle_=row["name"].toString();menuDetail_.clear();menuFocus_=0;menu_.clear();menuCommands_.clear();
    const auto pins=kind=="person"?pinnedPeople_:kind=="groups"?pinnedGroups_:pinnedCommunities_;
    if(kind=="person"||kind=="groups"||kind=="community") {menu_<< (pins.contains(row["id"].toString())?"Unpin":"Pin");menuCommands_<<"pin";}
    if(!menuChannel_.isEmpty()){menu_<< (row["muted"].toBool()?"Unmute conversation":"Mute conversation");menuCommands_<<"mute";}
    if(kind=="person") {
        menu_<<"Profile";menuCommands_<<"profile:"+row["id"].toString();
        int type=row["type"].toInt();for(const auto& p:snapshot_["friends"].toList())if(p.toMap()["id"]==row["id"])type=p.toMap()["type"].toInt();
        if(type==1){menu_<<"Remove friend";menuCommands_<<"ask-remove-friend:"+row["id"].toString();}
    } else if(kind=="groups") {menu_<<"Group info"<<"Leave group";menuCommands_<<"group-info"<<"ask-leave-group";}
    else if(kind=="community") {
        const bool owner=row["owner"]==snapshot_["userId"];
        menu_<<(owner?"Delete community":"Leave community");menuCommands_<<(owner?"ask-delete-community":"ask-leave-community");
    }
    emit changed();
}
void SocialController::collectionOptions() {
    if(face_!="communities"){filterPeople();return;}
    selection_.stop();menuMode_="collection";menuTitle_="Create";menuDetail_.clear();menuFocus_=0;
    menu_={"New group","New community"};menuCommands_={"create-group","create-community"};emit changed();
}
void SocialController::emoji() {
    if(lobbyChatVisible()&&lobbySpectatorChat_)return;
    if(!conversation()||!menu_.isEmpty())return;
    selection_.stop();menuChannel_=snapshot_["channel"].toString();menuMode_="emoji";menuTitle_="Emoji";menuDetail_.clear();menuFocus_=0;
    menu_={QString::fromUtf8("👍"),QString::fromUtf8("❤️"),QString::fromUtf8("😀"),QString::fromUtf8("🎮"),QString::fromUtf8("🎉"),"More emoji..."};
    menuCommands_={"emoji:👍","emoji:❤️","emoji:😀","emoji:🎮","emoji:🎉","emoji-more"};emit changed();
}
bool SocialController::togetherAvailable() const {
    return face_=="chats" && !contacts_ && conversation() && currentChat()["friend"].toBool()
        && (rows().value(focus_).toMap()["id"]==snapshot_["channel"]||rows().value(focus_).toMap()["channel"]==snapshot_["channel"]);
}
void SocialController::latest() {
    if(!conversation())return;
    reading_=true;emit commandRequested("latest",{});emit changed();
}
void SocialController::earlierMessages() {
    if(!conversation()||!menu_.isEmpty()||snapshot_["historyBusy"].toBool()||!snapshot_["historyMore"].toBool())return;
    selection_.stop();reading_=true;emit commandRequested("older",{});emit changed();
}
void SocialController::setRuntimeInvitation(QVariantMap request) {
    if(runtimeInvitation_==request)return;
    runtimeInvitation_=std::move(request);
    const auto id=runtimeInvitation_["request"].toString();
    if(!id.isEmpty()) {
        const bool known=std::any_of(activityNotifications_.cbegin(),activityNotifications_.cend(),[&](const QVariant& v){return v.toMap()["id"]=="runtime:"+id;});
        if(!known){activityNotifications_.prepend(QVariantMap{{"id","runtime:"+id},{"session",id},{"activity",true},{"runtime",true},{"name",runtimeInvitation_["name"]},{"game",runtimeInvitation_["game"].toMap()["label"]},{"kind","chats"}});
            while(activityNotifications_.size()>32)activityNotifications_.removeLast();saveNotifications();
            if(!snapshot_["doNotDisturb"].toBool()) {
                toastTitle_="Game invitation";toastText_=snapshot_["privatePreviews"].toBool()?QString("Open Home notifications"):runtimeInvitation_["name"].toString()+" · "+runtimeInvitation_["game"].toMap()["label"].toString();
                toastTimer_.start();emit presentationChanged();
            }
        }
    }
    emit changed();
}
void SocialController::setRuntimeSurface(QString title,QString detail,QVariantList actions) {
    const auto selected=menuMode_=="runtime"?menuCommands_.value(menuFocus_):QString();
    menuMode_="runtime";menuTitle_=std::move(title);menuDetail_=std::move(detail);
    runtimeActions_=std::move(actions);menu_.clear();menuCommands_.clear();menuFocus_=0;
    for(const auto& v:runtimeActions_){const auto r=v.toMap();menu_.append(r["label"].toString()+(r["detail"].toString().isEmpty()?QString():" · "+r["detail"].toString()));menuCommands_.append(r["id"].toString());}
    if(!selected.isEmpty()) {
        const int index=menuCommands_.indexOf(selected);
        if(index>=0)menuFocus_=index;
        else {menuFocus_=menu_.size();menu_.append("No longer available");menuCommands_.append("none");runtimeActions_.append(QVariantMap{{"id","none"},{"label","No longer available"},{"enabled",false}});}
    }
    emit changed();
}
void SocialController::together() {
    if(!menu_.isEmpty()||!conversation()||contacts_)return;
    joinChoices_.clear();
    selection_.stop();menuChannel_=snapshot_["channel"].toString();menuMode_="together";
    menuTitle_="Together";menuDetail_=conversationName();menuFocus_=0;menu_.clear();menuCommands_.clear();
    auto add=[&](QString text,QString action){menu_.append(text);menuCommands_.append(action);};
    if(!gameParty_["party"].toString().isEmpty()||gameParty_["joining"].toBool())add("Game party","show-party");
    else if(gameActivity()["joinable"].toBool())addJoinChoice("Ask to join · "+gameActivity()["game"].toMap()["label"].toString(),"party-join",runtimePeer(),gameActivity());
    for(const auto& v:companyParties()) {
        const auto r=v.toMap();const auto label=r["game"].toMap()["label"].toString()+" · "+r["name"].toString();
        if(r["joinable"].toBool())addJoinChoice(label+" · Join","company-party:"+r["peer"].toString(),r["peer"].toString(),r);
        else add(label+(r["free"].toInt()==0?" · Full":" · Playing"),"none");
    }
    const auto voice=snapshot_["voice"].toMap();
    if(!voice["channel"].toString().isEmpty()) {
        add("Game streams","game-streams");
        add(voice["muted"].toBool()?"Turn microphone on":"Mute microphone","voice-mute");
        add(voice["deaf"].toBool()?"Enable call sound":"Silence call sound","voice-output");
        if(voice["channel"]==menuChannel_)add("Ring conversation","voice-ring");
        add("Leave call","voice-leave");
    } else if(voice["available"].toBool()&&currentChat()["guild"].toString().isEmpty())add(currentChat()["call"].toBool()?"Join call":"Start call","voice-join");
    if(togetherAvailable())add("Activities","native-activities");
    if(snapshot_["state"]=="connected")add(viewedLobby().state().isEmpty()?(runtimePeer().isEmpty()?"Create lobby":"Invite to a new lobby"):"My lobby","open-lobby");
    if(menu_.isEmpty())menuDetail_="No shared activity is available in this conversation yet.";
    add("Back","cancel");emit changed();
}
void SocialController::groupDetails(int index) {
    const auto chat=index<0?currentChat():rows().value(index).toMap();
    if(chat["kind"]!="groups")return;
    menuMode_="group-details";menuTitle_=chat["name"].toString();menuDetail_="Group details";menuFocus_=0;
    menuChannel_=chat["id"].toString();menu_.clear();menuCommands_.clear();
    auto add=[&](QString label,QString command){menu_.append(label);menuCommands_.append(command);};
    add("Members","members");
    if(menuChannel_==snapshot_["channel"]&&snapshot_["voice"].toMap()["available"].toBool())add("Call","call-controls");
    add("Rename group","rename-group");add("Add friend","add-member");
    if(chat["owner"]==snapshot_["userId"])add("Remove member","remove-member");
    add("Leave group","ask-leave-group");emit changed();
}
void SocialController::nativeActivities() {
    if(!menu_.isEmpty() || !togetherAvailable())return;
    selection_.stop();menuChannel_=snapshot_["channel"].toString();
    if(online()["stage"]=="connected"&&online()["channel"]==menuChannel_&&link_){
        link_->showOnline();return;
    }
    menuMode_="online";menuTitle_="Activities";menuDetail_="Checking TrainerOS...";menuFocus_=0;
    menu_={"Close"};menuCommands_={"cancel"};
    if(online()["stage"]=="connected") {menuDetail_="Finish your current session first.";emit changed();return;}
    emit commandRequested("online-probe",{{"channel",menuChannel_}});emit changed();
}
void SocialController::openMenu() {
    if(!reading_&&focus_>=0&&face_!="friends"&&!rows().isEmpty()){contextRow(focus_);return;}
    selection_.stop();menu_.clear();menuCommands_.clear();menuFocus_=0;menuMode_="options";
    menuChannel_=snapshot_["channel"].toString();menuGuild_=snapshot_["guild"].toString();
    const auto row=rows().value(focus_).toMap();menuSubject_=row["id"].toString();
    menuTitle_=snapshot_["name"].toString();menuDetail_=snapshot_["remembered"].toBool()?"Account connected":"Connected for this session";
    auto add=[&](QString label,QString command){menu_.append(label);menuCommands_.append(command);};

    const auto message=messages().value(messageFocus_).toMap();
    if(reading_&&!message.isEmpty()) {
        menuSubject_=message["id"].toString();menuTitle_=message["mine"].toBool()?"Your message":message["name"].toString();menuDetail_=message["text"].toString().left(120);
        if(message["retryable"].toBool())add("Retry sending","retry-message");
        if(message["uncertain"].toBool())add("Check delivery","latest");
        if(!message["delivery"].toString().isEmpty())add("Dismiss unsent message","discard-message");
        mediaChoices_=message["attachments"].toList();
        for(int i=0;i<mediaChoices_.size();++i) {
            const auto a=mediaChoices_[i].toMap();const auto mime=a["content_type"].toString();
            if(mime.startsWith("audio/")||mime.startsWith("image/"))add(mime.startsWith("audio/")?"Listen to recording":"Open picture","attachment:"+QString::number(i));
            add("Download "+a["filename"].toString(),"download-file:"+message["id"].toString()+":"+a["id"].toString());
        }
        if(message["editable"].toBool()){if(!message["text"].toString().isEmpty())add("Edit message","edit-message");add("Delete message","ask-delete-message");}
        if(!message["author"].toString().isEmpty())add("View profile","profile:"+message["author"].toString());
    } else {collectionOptions();return;}
    if(reading_&&snapshot_["historyPast"].toBool())add("Latest messages","latest");
    emit changed();
}
void SocialController::closeMenu() {
    joinChoices_.clear();
    if(mediaSending_)emit commandRequested("cancel-attachment",{{"channel",menuChannel_}});
    mediaSending_=false;menuMode_.clear();menu_.clear();menuCommands_.clear();media_.clear();emit changed();
}
void SocialController::mediaMenu() {
    if(!mediaPreview())return;
    menu_.clear();menuCommands_.clear();menuTitle_=media_.voice()?"Voice message":"Picture";
    menuDetail_=conversationName();
    auto add=[&](QString text,QString command){menu_.append(text);menuCommands_.append(command);};
    const auto state=media_.state();
    if(mediaSending_)menuDetail_="Sending to "+conversationName()+"...";
    else if(state=="recording"){menuDetail_="Recording · "+QString::number(media_.seconds())+" / 120 s";add("Stop recording","stop-recording");}
    else if(state=="voice"||state=="picture") {
        if(state=="voice")add(media_.playing()?"Pause preview":"Play preview · "+QString::number(media_.seconds())+" s","play-recording");
        if(menuMode_=="media-preview"&&!mediaUncertain_)add("Send to "+conversationName(),"send-media");
    } else if(state=="error")menuDetail_=media_.error();
    else menuDetail_=state=="loading"?"Loading...":"Preparing...";
    add(mediaSending_?"Cancel upload":menuMode_=="media-view"?"Close":"Discard","cancel");
    menuFocus_=qBound(0,menuFocus_,int(menu_.size())-1);emit changed();
}
void SocialController::openPeople(QString mode) {
    selection_.stop();menuMode_=mode;menu_.clear();menuCommands_.clear();menuFocus_=0;pickedPeople_.clear();
    if(mode=="create-group")menuChannel_=snapshot_["channel"].toString();
    menuTitle_=mode=="create-group"?"Bring friends together":mode=="add-member"?"Add a friend":"Remove a member";
    menuDetail_=mode=="create-group"?"Choose friends for your group":menuChat()["name"].toString();
    const auto members=menuChat()["members"].toList();QStringList memberIds;
    for(const auto& member:members)memberIds<<member.toMap()["id"].toString();
    const auto candidates=mode=="remove-member"?members:snapshot_["friends"].toList();
    for(const auto& value:candidates) {
        const auto person=value.toMap();const auto id=person["id"].toString();
        if(id==snapshot_["userId"].toString()||id.isEmpty())continue;
        if(mode!="remove-member"&&person["type"].toInt()!=1)continue;
        if(mode=="add-member"&&memberIds.contains(id))continue;
        QString name=person["name"].toString();if(name.isEmpty())name=person["global_name"].toString();if(name.isEmpty())name=person["username"].toString();
        menu_.append((mode=="create-group"?QString::fromUtf8("\xe2\x97\x8b "):QString())+name);menuCommands_.append(id);
    }
    if(menu_.isEmpty()){menu_.append(mode=="remove-member"?"No other members":"No friends available");menuCommands_.append("none");}
    emit changed();
}
void SocialController::confirmAction(QString title,QString operation,QString id) {
    menuTitle_=title;menuDetail_=operation=="delete-message"?"This message will be deleted for everyone.":operation=="leave-group"?"You will leave this conversation.":"This person will be removed from the group.";
    menuMode_="confirm";menu_={"Cancel",operation=="delete-message"?"Delete message":operation=="leave-group"?"Leave group":"Remove member"};
    menuCommands_={"cancel",operation};menuSubject_=id;menuFocus_=0;emit changed();
}
void SocialController::selectMenu(int index) {
    if(index<0||index>=menuCommands_.size()||snapshot_["mutationBusy"].toBool())return;
    const auto command=menuCommands_[index];
    if(command=="switch-call"){
        const auto target=menuChannel_,previous=menuSubject_;closeMenu();
        if(snapshot_["state"]=="connected"&&snapshot_["voice"].toMap()["channel"]==previous)
            emit commandRequested("voice-switch",{{"channel",target},{"previous",previous}});
        return;
    }
    if(command=="ask-leave-community"||command=="ask-delete-community") {
        const auto guild=menuGuild_;const auto name=menuTitle_;const bool deleting=command=="ask-delete-community";
        confirmAction((deleting?"Delete ":"Leave ")+name+"?",deleting?"delete-community":"leave-community",guild);
        menuDetail_=deleting?"This permanently deletes the community, its channels and messages for everyone.":"You will leave this community. You can rejoin with an invitation.";
        menu_[1]=deleting?"Delete community":"Leave community";emit changed();return;
    }
    if(command=="delete-community"||command=="leave-community") {
        const auto guild=menuSubject_;closeMenu();menuMode_="community-progress";menuTitle_="Community";
        menu_={command=="delete-community"?"Deleting community…":"Leaving community…"};menuCommands_={"none"};menuFocus_=0;emit changed();
        emit commandRequested(command,{{"id",guild}});return;
    }
    if(command=="verify-community") {
        const auto guild=verificationGuild_,method=verificationMethod_;closeMenu();
        textPurpose_="community-secret:"+method;textId_=guild;
        emit textRequested(method=="password"?"Fluxer password · delete community":"Authenticator / backup code",{},method=="password"?256:32);return;
    }
    if(command=="request-discovered-tag") {const auto id=menuSubject_;closeMenu();emit commandRequested("search-action",{{"id",id}});return;}
    if(command=="group-info") {
        const auto list=rows();for(int i=0;i<list.size();++i)if(list[i].toMap()["id"]==menuChannel_){groupDetails(i);return;}
        groupDetails();return;
    }
    if(command=="ask-remove-friend"||command.startsWith("ask-remove-friend:")) {
        const auto id=command=="ask-remove-friend"?menuSubject_:command.mid(QStringLiteral("ask-remove-friend:").size());
        confirmAction("Remove friend?","remove",id);menuDetail_="Your conversation history will remain.";menu_[1]="Remove friend";emit changed();return;
    }
    if(command.startsWith("lobby-file")||command=="lobby-write"||command=="lobby-send"||command=="lobby-emoji"){
        const bool valid=menuChannel_==snapshot_["channel"]&&lobbyChatVisible();closeMenu();if(!valid)return;
        if(command=="lobby-file")attachFile();
        else if(command=="lobby-write")compose();
        else if(command=="lobby-send")sendDraft();
        else if(command=="lobby-emoji")emoji();
        else if(command=="lobby-file-cancel")files_.cancel();
        else if(command.startsWith("lobby-file-remove:"))files_.remove(command.section(':',1).toInt());
        else if(command.startsWith("lobby-file-download:"))downloadFile(command.section(':',1,1),command.section(':',2));
        return;
    }

    if(command.startsWith("download-file:")) {const bool valid=menuChannel_==snapshot_["channel"];closeMenu();if(valid)downloadFile(command.section(':',1,1),command.section(':',2));return;}
    if(command=="files-cancel"){files_.cancel();closeMenu();return;}
    if(command.startsWith("files-remove:")){files_.remove(command.section(':',1).toInt());composerTools();return;}
    if(command=="call-controls"){closeMenu();call();return;}
    if(command=="pin") {
        const auto kind=menuSubject_.section(':',0,0),id=menuSubject_.section(':',1);
        auto& pins=kind=="person"?pinnedPeople_:kind=="groups"?pinnedGroups_:pinnedCommunities_;
        if(pins.contains(id))pins.removeAll(id);else pins.append(id);
        saveSocialPreferences();closeMenu();return;
    }
    if(command=="social-file"){closeMenu();attachFile();return;}
    if(command=="game-streams"){streamsMenu();return;}
    if(command.startsWith("stream:")){closeMenu();streamAction(command.mid(7));return;}
    if(command=="recent-forget") {closeMenu();recentAction("forget");return;}
    if(command.startsWith("lobby:")){closeMenu();lobbyAction(command.mid(6));return;}
    if(joinChoices_.contains(command)) {
        const auto choice=joinChoices_.value(command);const auto peer=choice["peer"].toString();const auto current=joinOffer(peer);
        const bool valid=choice["generation"].toULongLong()==generation_&&choice["owner"]==owner_&&choice["account"]==snapshot_["userId"]
            &&current["joinable"].toBool()&&(!gameActive_||!choice["lobby"].toString().isEmpty())&&current["game"]==choice["game"]&&current["party"].toString()==choice["party"].toString()
            &&current["lobby"].toString()==choice["lobby"].toString();
        closeMenu();
        if(!valid) {
            menuTitle_="Game changed";menuDetail_="Reopen Together before joining.";menu_={"Close"};menuCommands_={"cancel"};menuFocus_=0;emit changed();return;
        }
        emit partyJoin(peer,{{"game",QJsonObject::fromVariantMap(choice["game"].toMap())},{"party",choice["party"].toString()},{"lobby",choice["lobby"].toString()}});return;
    }
    if(command=="message-tools") {
        if(menuChannel_!=snapshot_["channel"].toString())return;
        menuMode_="message-tools";menuTitle_="Message tools";menuFocus_=0;
        menu_={"Write message","Attach picture","Emoji"};menuCommands_={"compose-message","picture","emoji-picker"};
        if(snapshot_["voice"].toMap()["channel"].toString().isEmpty()){menu_<<"Record voice message";menuCommands_<<"record";}
        if(!draft().trimmed().isEmpty()){menu_<<"Send draft";menuCommands_<<"send-draft";}
        emit changed();return;
    }
    if(command=="emoji-picker"){closeMenu();emoji();return;}
    if(command=="compose-message"){const auto channel=menuChannel_;closeMenu();if(channel==snapshot_["channel"].toString())compose();return;}
    if(command=="send-draft"){const auto channel=menuChannel_;closeMenu();if(channel==snapshot_["channel"].toString())sendDraft();return;}
    if(command=="collection-options"){collectionOptions();return;}
    if(command=="dm"&&profileVisible()) {
        const auto id=menuSubject_;closeMenu();contacts_=false;
        emit conversationsRequested();setFace("chats");
        emit commandRequested("dm",{{"id",id}});emit changed();return;
    }
    if(command=="emoji-more"){closeMenu();compose();return;}
    if(command.startsWith("emoji:")){drafts_[menuChannel_]+=command.mid(6);closeMenu();saveDrafts();return;}
    if(command.startsWith("profile:")){profile(command.mid(8));return;}
    if(command=="copy-tag") {
        const auto tag=person()["tag"].toString();
        if(!tag.isEmpty()&&QGuiApplication::clipboard()){QGuiApplication::clipboard()->setText(tag);menuDetail_="Username copied";emit changed();}return;
    }
    if(command.startsWith("runtime:")){closeMenu();emit runtimeAction(command.mid(8));return;}
    if(command.startsWith("join-peer:")){const auto peer=command.mid(10);closeMenu();if(gameActivities_.value(peer)["joinable"].toBool())directJoin(peer,joinOffer(peer));return;}
    if(command.startsWith("context-party:")) {
        const auto peer=command.mid(14),company=menuChannel_;closeMenu();
        for(const auto& v:companyParties_) {const auto p=v.toMap();if(p["company"]==company&&p["peer"]==peer&&p["joinable"].toBool()&&(!gameActive_||!p["lobby"].toString().isEmpty())){directJoin(peer,joinOffer(peer));break;}}return;
    }
    if(menuMode_=="runtime") {
        const auto row=runtimeActions_.value(index).toMap();
        if(row["readOnly"].toBool()||!row.value("enabled",true).toBool())return;
        // A deliberate action starts a new step. Only asynchronous updates of
        // the open step preserve its old target (including expired placeholders).
        closeMenu();emit runtimeAction(command);return;
    }
    if(command=="group-details"){groupDetails();return;}
    if(command=="together"){closeMenu();together();return;}
    if(command=="open-lobby") {
        const auto peer=runtimePeer();
        const auto chat=currentChat();
        openLobbyForPeople(peer,chat["kind"]=="groups"?chat["id"].toString():QString());return;
    }
    if(command=="community-lobby") {openLobbyForPeople();return;}
    if(command=="browse-lobbies") {openLobby();lobbyAction("browse");return;}
    if(command=="communication-settings"){closeMenu();emit communicationSettingsRequested();return;}
    if(command=="show-party"){closeMenu();emit runtimeAction("multiplayer-party");return;}
    if(command=="native-activities"){closeMenu();nativeActivities();return;}
    if(command.startsWith("company-party:")){const auto peer=command.mid(14);closeMenu();
        for(const auto& v:companyParties())if(v.toMap()["peer"]==peer&&v.toMap()["joinable"].toBool()){directJoin(peer,joinOffer(peer));break;}return;}
    if(command=="company-access"){menuFocus_=0;companyAccessMenu();return;}
    if(command.startsWith("company-policy:")||command.startsWith("company-member:")) {
        const auto key=companySettingsKey(menuChannel_);if(key.isEmpty())return;
        auto access=companyAccess(menuChannel_);
        if(command.startsWith("company-policy:"))access["policy"]=command.section(':',1);
        else {auto allowed=access["allowed"].toStringList();const auto id=command.section(':',1);
            if(allowed.contains(id))allowed.removeAll(id);else allowed.append(id);access["allowed"]=allowed;}
        QSettings().setValue(key,access);menuFocus_=index;companyAccessMenu();
        emit companyAccessChanged(menuChannel_,access);return;
    }
    if(command=="none")return;
    if(command=="cancel"){const bool defer=menuCommands_.contains("lobby:later");closeMenu();if(defer)lobbyAction("later");return;}
    if(command.startsWith("voice-")){media_.clear();emit commandRequested(command,{{"channel",menuChannel_}});menu_.clear();emit changed();return;}
    if(command=="picture") {
        mediaChoices_=media_.pictures();menu_.clear();menuCommands_.clear();menuFocus_=0;menuTitle_="Choose a picture";menuDetail_=conversationName();
        for(int i=0;i<mediaChoices_.size();++i){menu_.append(mediaChoices_[i].toMap()["name"].toString());menuCommands_.append("picture:"+QString::number(i));}
        if(menu_.isEmpty()){menu_={"Close"};menuCommands_={"cancel"};menuDetail_="Add PNG, JPEG or WebP pictures to your Pictures folder.";}emit changed();return;
    }
    if(command.startsWith("picture:")||command=="record"||command.startsWith("attachment:")) {
        if(command=="record"&&!snapshot_["voice"].toMap()["channel"].toString().isEmpty()){
            menuTitle_="Voice message";menuDetail_="Leave your call before recording a message.";menu_={"Close"};menuCommands_={"cancel"};menuFocus_=0;emit changed();return;
        }
        const auto choice=mediaChoices_.value(command.section(':',1).toInt()).toMap();
        mediaSending_=mediaUncertain_=false;menuMode_=command.startsWith("attachment:")?"media-view":"media-preview";
        if(command=="record")media_.record();else if(command.startsWith("picture:"))media_.choosePicture(choice["path"].toString());else media_.open(choice);
        mediaMenu();return;
    }
    if(command=="stop-recording"){media_.stopRecording();return;}
    if(command=="play-recording"){media_.play();return;}
    if(command=="send-media") {
        if(mediaSending_||mediaUncertain_||media_.bytes().isEmpty())return;
        mediaSending_=true;if(media_.playing())media_.play();
        emit commandRequested("attachment",{{"channel",menuChannel_},{"bytes",media_.bytes()},{"voice",media_.voice()},{"seconds",media_.seconds()},{"waveform",media_.waveform()}});
        mediaMenu();return;
    }
    if(command.startsWith("online:")){menu_.clear();emit changed();emit commandRequested("online-invite",{{"id",command.mid(7)}});return;}
    if(menuMode_=="create-group") {
        const bool selected=pickedPeople_.contains(command);
        if(selected)pickedPeople_.removeAll(command);else if(pickedPeople_.size()<49)pickedPeople_.append(command);else return;
        menu_[index]=(selected?QString::fromUtf8("\xe2\x97\x8b "):QString::fromUtf8("\xe2\x9c\x93 "))+menu_[index].mid(2);
        menuFocus_=index;emit changed();return;
    }
    if(menuMode_=="add-member"){emit commandRequested("add-member",{{"channel",menuChannel_},{"id",command}});menu_.clear();emit changed();return;}
    if(menuMode_=="remove-member"){confirmAction("Remove "+menu_[index]+"?","remove-member",command);return;}
    if(command=="party-leave"){closeMenu();emit partyLeave();return;}
    if(command=="party-join"){closeMenu();joinGame();return;}
    if(command=="members") {
        menuTitle_="Group members";menuDetail_=menuChat()["name"].toString();menuMode_="members";menu_.clear();menuCommands_.clear();menuFocus_=0;
        menu_.append(snapshot_["name"].toString()+" (You)"+(snapshot_["userId"]==menuChat()["owner"]?" · Owner":""));menuCommands_.append("profile:"+snapshot_["userId"].toString());
        for(const auto& value:menuChat()["members"].toList()){const auto m=value.toMap();auto name=m["global_name"].toString();if(name.isEmpty())name=m["username"].toString();menu_.append(name+(m["id"]==menuChat()["owner"]?" · Owner":""));menuCommands_.append("profile:"+m["id"].toString());}
        menu_.append("Close");menuCommands_.append("cancel");emit changed();return;
    }
    if(command=="contacts"){contacts_=!contacts_;menu_.clear();focus_=0;preview();emit changed();return;}
    if(command=="create-community") {
        menu_.clear();textPurpose_=command;emit textRequested("Community name",QString(),100);emit changed();return;
    }
    if(command=="community-invite") {
        menuMode_=command;menuTitle_="Invite friends";menuDetail_="Creating invitation...";
        menu_={"Close"};menuCommands_={"cancel"};menuFocus_=0;
        emit commandRequested(command,{{"id",menuGuild_.isEmpty()?snapshot_["guild"]:QVariant(menuGuild_)}});emit changed();return;
    }
    if(command=="mark-community") {emit commandRequested(command,{{"id",snapshot_["guild"]}});menu_.clear();emit changed();return;}
    if(command=="create-group"||command=="add-member"||command=="remove-member") {
        if(menuMode_!="confirm"){openPeople(command);return;}
    }
    if(command=="ask-delete-message"){confirmAction("Delete this message?","delete-message",menuSubject_);return;}
    if(command=="ask-leave-group"){confirmAction("Leave "+menuChat()["name"].toString()+"?","leave-group");return;}
    if(command=="edit-message"||command=="rename-group") {
        textPurpose_=command;textChannel_=menuChannel_;textId_=menuSubject_;
        QString text=menuChat()["name"].toString();
        if(command=="edit-message") {
            text.clear();for(const auto& m:messages())if(m.toMap()["id"]==textId_)text=m.toMap()["text"].toString();
            text=editDrafts_.value(textChannel_+"/"+textId_,text);
        }
        menu_.clear();emit textRequested(command=="edit-message"?"Edit message":"Group name",text,command=="edit-message"?2000:100);emit changed();return;
    }
    emit commandRequested(command,{{"id",menuSubject_},{"channel",menuChannel_}});menu_.clear();emit changed();
}
void SocialController::dispatch(Action action) {
    if((lobbyCreateOpen_||lobbyPickerOpen_||lobbyVoteOpen_)&&menu_.isEmpty()){dispatchLobbyCreation(action);return;}
    if(myLobbiesOpen_) {
        if(action==Action::Back)closeMyLobbies();
        else if(action==Action::Up||action==Action::Down){
            const int count=myLobbies().size();
            if(action==Action::Up&&myLobbiesIndex_==0)myLobbiesIndex_=count;
            else if(action==Action::Down&&myLobbiesIndex_==count)myLobbiesIndex_=0;
            else if(myLobbiesIndex_<count)myLobbiesIndex_=qBound(0,myLobbiesIndex_+(action==Action::Down?1:-1),count-1);
            myLobbyFocus_=myLobbies().value(myLobbiesIndex_).toMap()["id"].toString();
        }
        else if(action==Action::Confirm){const auto rows=myLobbies();if(myLobbiesIndex_<rows.size())chooseMyLobby(rows[myLobbiesIndex_].toMap()["id"].toString());else createAnotherLobby();}
        emit changed();return;
    }
    if(snapshot_["state"]=="restoring")return;
    if(stream_.active()&&menu_.isEmpty()) {
        if(action==Action::Back)streamAction("stop-watch");
        else if(action==Action::Confirm||action==Action::Secondary)streamsMenu();
        return;
    }
    if(!menu_.isEmpty()) {
        if(emojiVisible()&&(action==Action::Left||action==Action::Right))menuFocus_=qBound(0,menuFocus_+(action==Action::Left?-1:1),4);
        else if(emojiVisible()&&(action==Action::Up||action==Action::Down))menuFocus_=action==Action::Down?5:0;
        else if(action==Action::Back||action==Action::Left){if(menuMode_=="runtime")emit runtimeAction("multiplayer-cancel");else {const bool defer=menuCommands_.contains("lobby:later");closeMenu();if(defer)lobbyAction("later");}}
        else if(action==Action::Up||action==Action::Down)menuFocus_=qBound(0,menuFocus_+(action==Action::Up?-1:1),int(menu_.size())-1);
        else if(action==Action::Confirm)selectMenu(menuFocus_);
        else if(action==Action::ToggleContinue&&menuMode_=="create-group"&&!pickedPeople_.isEmpty()&&!snapshot_["mutationBusy"].toBool()) {
            emit commandRequested("create-group",{{"recipients",pickedPeople_}});menu_.clear();
        }
        emit changed();return;
    }
    if(face_=="recent") {dispatchRecent(action);return;}
    if(face_=="lobby") {dispatchLobby(action);return;}
    if(snapshot_.value("state")=="authorizing") {if(action==Action::Back)emit commandRequested("cancel-login",{});return;}
    if(snapshot_.value("state")=="signed-out"||snapshot_.isEmpty()) {if(action==Action::Confirm)login();return;}
    if(face_=="friends") {dispatchDiscover(action);return;}
    if(partyFocus_) {
        if(action==Action::Left) {if(partyIndex_>0)--partyIndex_;else partyFocus_=false;}
        else if(action==Action::Right)partyIndex_=qMin(partyIndex_+1,int(companyParties().size())-1);
        else if(action==Action::Confirm)joinCompanyParty(partyIndex_);
        else if(action==Action::Down){partyFocus_=false;reading_=true;}
        else if(action==Action::Back){partyFocus_=false;reading_=false;}
        else if(action==Action::Secondary)compose();
        else if(action==Action::LocalAction||action==Action::ContextMenu)openMenu();
        emit changed();return;
    }

    if(dispatchConversationControls(action)){emit changed();return;}

    if(action==Action::Up||action==Action::Down) {
        if(reading_&&action==Action::Up&&messageFocus_==0&&snapshot_["historyMore"].toBool()){emit commandRequested("older",{});return;}
        auto& focus=reading_?messageFocus_:focus_;const int count=reading_?messages().size():rows().size();
        focus=qBound(reading_?0:-1,focus+(action==Action::Up?-1:1),qMax(0,count-1));

    } else if(action==Action::Right&&conversation()){
        if(focus_>=0&&focus_<rows().size())activate(focus_);
        else {reading_=true;conversationControl_=messages().isEmpty()?11:-1;}
    }
    else if(action==Action::Left)reading_=false;
    else if(action==Action::Confirm){if(reading_){const auto m=messages().value(messageFocus_).toMap();
        if(m["editable"].toBool()||m["media"].toBool()||!m["delivery"].toString().isEmpty()) {
            openMenu();
            if(m["attachments"].toList().size()==1)selectMenu(0);
        }
    }else if(conversation()&&(rows().value(focus_).toMap()["id"]==snapshot_["channel"]||rows().value(focus_).toMap()["channel"]==snapshot_["channel"])){reading_=true;conversationControl_=messages().isEmpty()?11:-1;}else activate(focus_);}
    else if(action==Action::Back){if(reading_)reading_=false;else if(face_=="chats"&&!peopleFilter_.isEmpty())clearPeopleFilter();else communityBack();}
    else if(action==Action::Secondary){if(face_=="groups"&&!conversation())openPeople("create-group");
        else if(face_=="communities"&&!conversation()){textPurpose_="create-community";emit textRequested("Community name",QString(),100);}else compose();}
    else if(action==Action::ChangeView)compose();
    else if(action==Action::ToggleFavorite){if(reading_)conversationProfile();else {const auto row=rows().value(focus_).toMap();if(row["kind"]=="person")profile(row["id"].toString());else if(row["kind"]=="groups")groupDetails(focus_);else if(communityOpen())communityInfo();else collectionOptions();}}
    else if(action==Action::LocalAction||action==Action::ContextMenu)openMenu();
    if(reading_&&conversation()&&(action==Action::Up||action==Action::Down))emit commandRequested("history-position",{{"channel",snapshot_["channel"]},{"id",messageFocus_>=messages().size()-1?QString():messages().value(messageFocus_).toMap()["id"].toString()}});
    emit changed();
}
void SocialController::preview() {
    const auto row=rows().value(focus_).toMap();
    if(row.isEmpty()||(contacts_&&row["type"].toInt()!=1))return;
    activate(focus_);
}
void SocialController::showContacts() {setFace("chats");emit changed();}
void SocialController::showFriends() {showContacts();}
void SocialController::bindDrafts(const QString& accountId) {
    if(owner_.isEmpty()) {recentTogether_.configure({},accountId);return;}
    const auto key=QString::fromLatin1(QCryptographicHash::hash(("https://fluxer.app\n"+owner_+"\n"+accountId).toUtf8(),QCryptographicHash::Sha256).toHex());
    const auto file=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/social/"+key+"/drafts.json";
    QDir().mkpath(QFileInfo(file).absolutePath());
    lobbyReminders_.configure(QFileInfo(file).absolutePath()+"/lobby-reminders.json");
    syncLobbyReminder();
    recentTogether_.configure(QFileInfo(file).absolutePath()+"/recent-together.json",accountId);
    lobby_.setStorage(QFileInfo(file).absolutePath()+"/lobby.json");
    lobbyPublicationTopic_=viewedLobby().publicationTopic();
    if(file==draftFile_)return;
    saveDrafts();drafts_.clear();draftFile_=file;
    QFile input(file);if(QFileInfo(file).isSymLink()||input.size()>1024*1024||!input.open(QIODevice::ReadOnly))return;
    const auto data=QJsonDocument::fromJson(input.readAll()).object();
    for(auto it=data.begin();it!=data.end()&&drafts_.size()<128;++it)
        if(!it.key().isEmpty()&&it.key().size()<=20)drafts_[it.key()]=it.value().toString();
}
void SocialController::saveDrafts() {
    draftSave_.stop();if(draftFile_.isEmpty())return;
    QJsonObject data;for(auto it=drafts_.cbegin();it!=drafts_.cend()&&data.size()<128;++it)if(!it.value().isEmpty())data[it.key()]=it.value();
    const auto dir=QFileInfo(draftFile_).absolutePath();
    if(QFileInfo(dir).isSymLink()||QFileInfo(draftFile_).isSymLink()||!QDir().mkpath(dir))return;
    QFile::setPermissions(dir,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    QSaveFile file(draftFile_);if(!file.open(QIODevice::WriteOnly))return;
    file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);const auto bytes=QJsonDocument(data).toJson(QJsonDocument::Compact);
    if(file.write(bytes)==bytes.size())file.commit();
}
}
