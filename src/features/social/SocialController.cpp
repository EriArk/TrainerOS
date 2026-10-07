#include "SocialController.h"
#include "integrations/social/FluxerSession.h"
#include "features/center/LinkController.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSettings>
#include <algorithm>

namespace trainer {
SocialController::SocialController(QObject* parent):QObject(parent),session_(new FluxerSession) {
    partyBrowse_.setInterval(30000);
    connect(&partyBrowse_,&QTimer::timeout,this,[this]{refreshPartyBrowse();});partyBrowse_.start();
    session_->moveToThread(&thread_);
    connect(&thread_,&QThread::finished,session_,&QObject::deleteLater);
    connect(this,&SocialController::ownerRequested,session_,&FluxerSession::setOwner);
    connect(this,&SocialController::commandRequested,session_,&FluxerSession::command);
    connect(session_,&FluxerSession::snapshot,this,&SocialController::receive);
    connect(session_,&FluxerSession::partyPacket,this,[this](quint64 g,QString peer,QString name,QJsonObject p){if(g==generation_)emit partyPacket(peer,name,p);});
    connect(session_,&FluxerSession::partyFailed,this,[this](quint64 g,QString peer){if(g==generation_)emit partyFailed(peer);});
    connect(session_,&FluxerSession::partyReset,this,[this](quint64 g){if(g==generation_){gameActivities_.clear();companyParties_.clear();partyFocus_=false;emit partyReset();}});
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
        if(success&&(operation=="edit-message"||operation=="delete-message"))editDrafts_.remove(channel+"/"+id);
    });
    thread_.start();
}
SocialController::~SocialController() {
    saveDrafts();
    QMetaObject::invokeMethod(session_,&FluxerSession::stop,Qt::BlockingQueuedConnection);
    thread_.quit();thread_.wait();
}
void SocialController::setOwner(QString owner) {
    if(owner==owner_)return;
    media_.clear();mediaSending_=mediaUncertain_=false;
    if(link_)link_->endOnline();
    runtimeOnline_=false;runtimeInvitation_.clear();gameActivities_.clear();companyParties_.clear();partyFocus_=false;emit partyReset();emit runtimeEnded();
    saveDrafts();draftFile_.clear();
    editDrafts_.clear();pickedPeople_.clear();dismissedNotifications_.clear();activityNotifications_.clear();presentedInvitation_.clear();notificationSettingsKey_.clear();
    toastTimer_.stop();toastTitle_.clear();toastText_.clear();emit presentationChanged();
    owner_=std::move(owner);++generation_;snapshot_.clear();drafts_.clear();menu_.clear();toastChannel_.clear();
    selection_.stop();contacts_=false;searchStarted_=false;searchFocus_=-1;
    textPurpose_.clear();textChannel_.clear();query_.clear();focus_=messageFocus_=0;reading_=false;
    emit ownerRequested(owner_,generation_);emit changed();
}
void SocialController::setFace(QString face) {
    if(face_==face)return;
    const auto requestedFace=face;
    if(face=="groups")face="chats";
    closeMenu();
    partyFocus_=false;partyIndex_=0;selection_.stop();face_=std::move(face);focus_=0;menu_.clear();reading_=false;contacts_=false;
    emit commandRequested("face",{{"face",requestedFace}});
    if(face_=="friends"&&!searchStarted_&&snapshot_["state"]=="connected")runSearch();
    emit changed();
}
void SocialController::setLink(LinkController* link) {
    link_=link;
    connect(session_,&FluxerSession::onlineEstablished,this,[this](quint64 generation,QString self,QString peer,QString name,QString activity,bool initiator){
        if(generation!=generation_)return;
        runtimeOnline_=activity.startsWith("runtime.");
        if(runtimeOnline_){emit runtimeEstablished(activity,initiator);return;}
        if(!onlineWritable_||!link_||!link_->beginOnline(self,peer,name,activity,initiator))emit commandRequested("online-close",{});
    });
    connect(session_,&FluxerSession::onlineFrame,this,[this](quint64 generation,QJsonObject frame){if(generation!=generation_)return;if(runtimeOnline_)emit runtimeFrame(frame);else if(link_)link_->receiveOnline(frame);});
    connect(session_,&FluxerSession::onlineEnded,this,[this](quint64 generation){if(generation!=generation_)return;if(runtimeOnline_){runtimeOnline_=false;emit runtimeEnded();}else if(link_)link_->endOnline();});
    connect(link,&LinkController::onlineSend,this,[this](QJsonObject frame){emit commandRequested("online-frame",frame.toVariantMap());});
    connect(link,&LinkController::onlineClosed,this,[this]{emit commandRequested("online-close",{});});
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
    if(!available){toastTimer_.stop();toastTitle_.clear();toastText_.clear();if(media_.state()=="recording"){closeMenu();}}
    emit presentationChanged();
}
void SocialController::setConversationVisible(bool visible) {
    if(conversationVisible_==visible)return;
    conversationVisible_=visible;emit presentationChanged();
}
void SocialController::presented(QString channel,QString message) {
    if(!surfaceAvailable_||!conversationVisible_||contacts_||face_=="friends"||!menu_.isEmpty()
        ||snapshot_["historyBusy"].toBool()||snapshot_["channel"]!=channel
        ||snapshot_.value("readTail",messages().isEmpty()?QString():messages().last().toMap()["id"]).toString()!=message)return;
    emit commandRequested("read",{{"channel",channel},{"message",message}});
}
QString SocialController::notificationFace() const {
    if(toastChannel_.isEmpty())return {};
    for(const auto& value:notifications()) {
        const auto row=value.toMap();
        if((row["activity"].toBool()&&row["channel"]==toastChannel_)||(row["request"].toBool()&&"request:"+row["id"].toString()==toastChannel_))return "chats";
    }
    for(const auto& value:snapshot_["chats"].toList()) {
        const auto row=value.toMap();if(row["id"]==toastChannel_ && (row["unread"].toInt()>0||row["ringing"].toBool()))
            return row["guild"].toString().isEmpty()?QString("chats"):QString("communities");
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
    if(row["activity"].toBool()) {
        if(row["pending"].toBool()) {
            if(row["runtime"].toBool())emit runtimeAction("multiplayer-decline:"+row["session"].toString());
            else answerOnline(false);
        }
        activityNotifications_.removeIf([&](const QVariant& v){return v.toMap()["id"]==row["id"];});
        saveNotifications();emit changed();return;
    }
    dismissedNotifications_[row["id"].toString()]=notificationStamp(row);
    saveNotifications();
    emit changed();
}
void SocialController::dismissNotifications() {
    const auto list=notifications();
    for(int i=list.size()-1;i>=0;--i)dismissNotificationAt(i);
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
    if(snapshot_["userId"].toString().isEmpty())return result;
    const auto invitation=snapshot_["online"].toMap();
    for(const auto& value:activityNotifications_) {
        auto row=value.toMap();const bool pending=row["runtime"].toBool()?runtimeInvitation_["request"]==row["session"]:invitation["incoming"].toBool()&&invitation["session"]==row["session"];
        row["pending"]=pending;row["detail"]=pending?QString("Incoming activity"):QString("Missed invitation");
        if(row["runtime"].toBool())row["detail"]=row["detail"].toString()+" · "+row["game"].toString();
        result.append(row);
    }
    for(const auto& value:snapshot_["friends"].toList()) {
        auto row=value.toMap();if(row["type"].toInt()!=3)continue;
        row["request"]=true;row["detail"]="Friend request";
        if(dismissedNotifications_.value(row["id"].toString()).toString()!=notificationStamp(row))result.append(row);
    }
    for(const auto& value:snapshot_["chats"].toList()) {
        auto row=value.toMap();if((row["unread"].toInt()<=0&&!row["ringing"].toBool()&&row["missedCall"].toString().isEmpty())||row["muted"].toBool())continue;
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
    return row["request"].toBool()||row["activity"].toBool()?QString("chats"):row["guild"].toString().isEmpty()?QString("chats"):QString("communities");
}
void SocialController::openNotificationAt(int index) {
    const auto list=notifications();const auto face=notificationFaceAt(index);if(face.isEmpty())return;
    const auto row=list[index].toMap();
    if(row["runtime"].toBool()) {
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
    setFace(face);selection_.stop();reading_=false;contacts_=row["request"].toBool();
    if(contacts_) {
        const auto people=rows();for(int i=0;i<people.size();++i)if(people[i].toMap()["id"]==row["id"]){focus_=i;break;}
    } else emit commandRequested("conversation",{{"id",row["id"]}});
    toastTimer_.stop();toastTitle_.clear();toastText_.clear();toastChannel_.clear();
    emit presentationChanged();emit changed();
}
void SocialController::openNotification() {
    const auto face=notificationFace();if(face.isEmpty())return;
    const auto list=notifications();
    for(int i=0;i<list.size();++i) {
        const auto row=list[i].toMap();
        if((row["activity"].toBool()&&row["channel"]==toastChannel_)||(row["request"].toBool()&&"request:"+row["id"].toString()==toastChannel_)) {
            openNotificationAt(i);toastTimer_.stop();toastTitle_.clear();toastText_.clear();emit presentationChanged();return;
        }
    }
    const auto channel=toastChannel_;
    setFace(face);contacts_=false;reading_=false;selection_.stop();
    emit commandRequested("conversation",{{"id",channel}});
    toastTimer_.stop();toastTitle_.clear();toastText_.clear();toastChannel_.clear();
    emit presentationChanged();emit changed();
}
void SocialController::receive(quint64 generation,QVariantMap snapshot) {
    if(generation!=generation_)return;
    const auto oldId=rows().value(focus_).toMap().value("id");
    const auto oldChannel=snapshot_.value("channel");
    const bool wasEarlier=snapshot_["historyPast"].toBool();
    const auto oldMessage=messages().value(messageFocus_).toMap().value("id");
    const bool atEnd=messageFocus_>=messages().size()-1;
    snapshot_=std::move(snapshot);
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
    const auto list=rows();focus_=qBound(0,focus_,qMax(0,int(list.size())-1));
    for(int i=0;i<list.size();++i)if(list[i].toMap()["id"]==oldId){focus_=i;break;}
    if(oldChannel!=snapshot_["channel"]) {
        reading_=false;
        if(!contacts_)for(int i=0;i<list.size();++i)if(list[i].toMap()["id"]==snapshot_["channel"]){focus_=i;break;}
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
    emit changed();
}
QVariantMap SocialController::account() const {
    auto result=snapshot_;result.remove("friends");result.remove("chats");result.remove("messages");
    result["available"]=!owner_.isEmpty();return result;
}
QVariantList SocialController::rows() const {
    if(contacts_)return snapshot_.value("friends").toList();
    QVariantList result;
    if(face_=="communities") {
        for(const auto& g:snapshot_.value("communities").toList()) {
            result.append(g);
            if(g.toMap()["id"]==snapshot_["guild"])for(const auto& c:snapshot_.value("chats").toList())if(c.toMap()["guild"]==g.toMap()["id"]){auto row=c.toMap();row["name"]="# "+row["name"].toString();row["detail"]="Text channel";result.append(row);}
        }
    } else for(const auto& c:snapshot_.value("chats").toList())if(c.toMap()["kind"]==face_||(face_=="chats"&&c.toMap()["kind"]=="groups"))result.append(c);
    return result;
}
QString SocialController::conversationName() const {
    for(const auto& c:snapshot_.value("chats").toList())if(c.toMap()["id"]==snapshot_["channel"])return c.toMap()["name"].toString();
    return "Conversation";
}
QVariantList SocialController::hints() const {
    QVariantList result;
    auto h=[&](QString key,QString label){result.append(QVariantMap{{"button",key},{"label",label}});};
    if(!menu_.isEmpty()){h("A",menuMode_=="create-group"?"Choose friend":"Choose");if(menuMode_=="create-group"&&!pickedPeople_.isEmpty())h("Y","Create group");h("B","Close");return result;}
    if(snapshot_["state"]=="restoring")return result;
    if(snapshot_.value("state")=="authorizing"){h("B","Cancel sign-in");return result;}
    if(snapshot_.value("state")=="signed-out"||snapshot_.isEmpty()){if(!owner_.isEmpty())h("A","Sign in");return result;}
    if(face_=="friends") {
        h("X","Search");
        if(searchFocus_<0) {h("←→","Category");h("A","Enter search");}
        else {const auto action=searchResults().value(searchFocus_).toMap()["action"].toString();if(action!="Joined"&&action!="Sent")h("A",action);h("B","Search bar");}
        if(snapshot_["searchTotal"].toInt()>24)h("Y","Next page");
    } else {
        if(contacts_)h("A",rows().value(focus_).toMap()["type"].toInt()==3?"Accept request":"Conversation");
        else h("A",reading_?"Message":"Select");
        if(conversation()&&!contacts_){h("X","Write");h("Y","Together");h(reading_?"←":"→",reading_?"Conversations":"Messages");}
        if(reading_&&messageFocus_==0&&snapshot_["historyMore"].toBool())h("↑","Earlier");
        if(contacts_||reading_)h("B","Conversations");
    }
    h("Select","Options");return result;
}
void SocialController::login(){if(!owner_.isEmpty())emit commandRequested("login",{});}
void SocialController::activate(int index) {
    if(!menu_.isEmpty())return;
    const auto list=rows();if(index<0||index>=list.size())return;
    focus_=index;reading_=false;const auto row=list[index].toMap();
    const auto type=row["type"].toInt();
    if(row["kind"]=="community")emit commandRequested("guild",{{"id",row["id"]}});
    else if(!contacts_)emit commandRequested("open",{{"id",row["id"]}});
    else if(type==1)emit commandRequested("dm",{{"id",row["id"]}});
    else if(type==3)emit commandRequested("accept",{{"id",row["id"]}});
    else openMenu();
    emit changed();
}
void SocialController::compose() {
    if(!conversation())return;
    selection_.stop();
    textPurpose_="message";textChannel_=snapshot_["channel"].toString();
    emit textRequested("Message - "+conversationName(),drafts_.value(textChannel_),2000);
}
void SocialController::sendDraft() {
    if(!menu_.isEmpty()||snapshot_["state"]!="connected")return;
    selection_.stop();send();
}
void SocialController::attachPicture() {
    if(!conversation()||!menu_.isEmpty()||snapshot_["state"]!="connected")return;
    selection_.stop();menuChannel_=snapshot_["channel"].toString();menuMode_="picture";
    menuCommands_={"picture"};selectMenu(0);
}
void SocialController::recordVoice() {
    if(!conversation()||!menu_.isEmpty()||snapshot_["state"]!="connected")return;
    if(!snapshot_["voice"].toMap()["channel"].toString().isEmpty())return;
    selection_.stop();menuChannel_=snapshot_["channel"].toString();menuMode_="record";
    menuCommands_={"record"};selectMenu(0);
}
void SocialController::call() {
    if(!conversation()||!menu_.isEmpty()||snapshot_["state"]!="connected")return;
    const auto voice=snapshot_["voice"].toMap();
    if(!voice["channel"].toString().isEmpty()){together();return;}
    if(!voice["available"].toBool()||!currentChat()["guild"].toString().isEmpty())return;
    selection_.stop();media_.clear();emit commandRequested("voice-join",{{"channel",snapshot_["channel"]}});
}
void SocialController::selectMessage(int index) {
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
    if(textPurpose_=="search"){query_=text.trimmed();searchFocus_=-1;runSearch();}
    else if(textPurpose_=="add")emit commandRequested("add",{{"text",text}});
    else if(textPurpose_=="create-community")emit commandRequested("create-community",{{"text",text}});
    else if(textPurpose_=="edit-message"||textPurpose_=="rename-group") {
        preserveText(text);emit commandRequested(textPurpose_,{{"channel",textChannel_},{"id",textId_},{"text",text}});
    } else if(textPurpose_=="message") {
        preserveText(text);
        if(textChannel_==snapshot_["channel"].toString())send();
    } else preserveText(text);
    textPurpose_.clear();textChannel_.clear();emit changed();
}
void SocialController::send() {
    if(!conversation()||draft().trimmed().isEmpty())return;
    emit commandRequested("send",{{"channel",snapshot_["channel"]},{"text",draft()}});
    drafts_.remove(snapshot_["channel"].toString());emit changed();
    saveDrafts();
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
        if(gameActive_)row["joinable"]=false;
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
    if(!row["joinable"].toBool()||gameActive_)return;
    partyIndex_=index;emit partyJoin(row["peer"].toString());
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
    const auto peer=runtimePeer();if(!peer.isEmpty()&&gameActivity()["joinable"].toBool())emit partyJoin(peer);
}
QVariantMap SocialController::currentChat() const {
    for(const auto& row:snapshot_["chats"].toList())if(row.toMap()["id"]==snapshot_["channel"])return row.toMap();
    return {};
}
bool SocialController::togetherAvailable() const {
    return face_=="chats" && !contacts_ && conversation() && currentChat()["friend"].toBool()
        && rows().value(focus_).toMap()["id"]==snapshot_["channel"];
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
    if(!id.isEmpty()&&!snapshot_["userId"].toString().isEmpty()) {
        const bool known=std::any_of(activityNotifications_.cbegin(),activityNotifications_.cend(),[&](const QVariant& v){return v.toMap()["id"]=="runtime:"+id;});
        if(!known){activityNotifications_.prepend(QVariantMap{{"id","runtime:"+id},{"session",id},{"activity",true},{"runtime",true},{"name",runtimeInvitation_["name"]},{"game",runtimeInvitation_["game"].toMap()["label"]},{"kind","chats"}});
            while(activityNotifications_.size()>32)activityNotifications_.removeLast();saveNotifications();}
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
    selection_.stop();menuChannel_=snapshot_["channel"].toString();menuMode_="together";
    menuTitle_="Together";menuDetail_=conversationName();menuFocus_=0;menu_.clear();menuCommands_.clear();
    auto add=[&](QString text,QString action){menu_.append(text);menuCommands_.append(action);};
    if(!gameParty_["party"].toString().isEmpty()||gameParty_["joining"].toBool())add("Game party","show-party");
    else if(gameActivity()["joinable"].toBool())add("Ask to join · "+gameActivity()["game"].toMap()["label"].toString(),"party-join");
    for(const auto& v:companyParties()) {const auto r=v.toMap();add(r["game"].toMap()["label"].toString()+" · "+r["name"].toString()+(r["joinable"].toBool()?" · Join":r["free"].toInt()==0?" · Full":" · Playing"),r["joinable"].toBool()?"company-party:"+r["peer"].toString():"none");}
    const auto voice=snapshot_["voice"].toMap();
    if(!voice["channel"].toString().isEmpty()) {
        add(voice["muted"].toBool()?"Turn microphone on":"Mute microphone","voice-mute");
        add(voice["deaf"].toBool()?"Enable call sound":"Silence call sound","voice-output");
        if(voice["channel"]==menuChannel_)add("Ring conversation","voice-ring");
        add("Leave call","voice-leave");
    } else if(voice["available"].toBool()&&currentChat()["guild"].toString().isEmpty())add(currentChat()["call"].toBool()?"Join call":"Start call","voice-join");
    if(togetherAvailable())add("Activities","native-activities");
    if(menu_.isEmpty())menuDetail_="No shared activity is available in this conversation yet.";
    add("Back","cancel");emit changed();
}
void SocialController::groupDetails() {
    if(currentChat()["kind"]!="groups")return;
    menuMode_="group-details";menuTitle_=conversationName();menuDetail_="Group details";menuFocus_=0;
    menuChannel_=snapshot_["channel"].toString();menu_.clear();menuCommands_.clear();
    auto add=[&](QString label,QString command){menu_.append(label);menuCommands_.append(command);};
    add("Members","members");if(runtimeAvailable_)add("Who can join my games","company-access");
    add(currentChat()["muted"].toBool()?"Notifications: Off":"Notifications: On","mute");
    add("Rename group","rename-group");add("Add friend","add-member");
    if(currentChat()["owner"]==snapshot_["userId"])add("Remove member","remove-member");
    add("Leave group","ask-leave-group");add("Back","cancel");emit changed();
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
    selection_.stop();menu_.clear();menuCommands_.clear();menuFocus_=0;menuMode_="options";
    menuChannel_=snapshot_["channel"].toString();
    const auto row=rows().value(focus_).toMap();menuSubject_=row["id"].toString();
    menuTitle_=snapshot_["name"].toString();menuDetail_=snapshot_["remembered"].toBool()?"Account connected":"Connected for this session";
    auto add=[&](QString label,QString command){menu_.append(label);menuCommands_.append(command);};

    const auto message=messages().value(messageFocus_).toMap();
    if(reading_&&(message["editable"].toBool()||message["media"].toBool()||!message["delivery"].toString().isEmpty())) {
        menuSubject_=message["id"].toString();menuTitle_=message["mine"].toBool()?"Your message":message["name"].toString();menuDetail_=message["text"].toString().left(120);
        if(message["retryable"].toBool())add("Retry sending","retry-message");
        if(message["uncertain"].toBool())add("Check delivery","latest");
        if(!message["delivery"].toString().isEmpty())add("Dismiss unsent message","discard-message");
        mediaChoices_=message["attachments"].toList();
        for(int i=0;i<mediaChoices_.size();++i) {const auto a=mediaChoices_[i].toMap();add(a["content_type"].toString().startsWith("audio/")?"Listen to recording":"Open picture","attachment:"+QString::number(i));}
        if(message["editable"].toBool()){if(!message["text"].toString().isEmpty())add("Edit message","edit-message");add("Delete message","ask-delete-message");}
    } else {
        const auto voice=snapshot_["voice"].toMap();
        if(conversation()&&!contacts_&&face_!="friends"){
            add("Send picture","picture");if(voice["channel"].toString().isEmpty())add("Record voice message","record");
        }
        if(conversation()&&!contacts_&&face_!="friends")add("Together","together");
        if(face_=="chats")add(contacts_?"Conversations":"Friends & requests","contacts");
        if(contacts_&&!menuSubject_.isEmpty()) {
            const int type=row["type"].toInt();
            if(type==3){add("Accept request","accept");add("Decline request","remove");}
            if(type==4)add("Cancel request","remove");
            if(type==1)add("Remove friend","remove");
            if(type==2)add("Unblock","remove");else add("Block","block");
        }
        if(face_=="chats"&&!contacts_) {
            add("New group","create-group");
            if(currentChat()["kind"]=="groups") {
                menuTitle_=conversationName();menuDetail_="Group conversation";
                add("Group details","group-details");
            }
        }
        if(face_=="communities") {
            add("New community","create-community");
            add(snapshot_["communityOnly"].toBool()?"Show all communities":"Show TrainerOS communities","community-filter");
            if(!snapshot_["guild"].toString().isEmpty())add("Invite link","community-invite");
            if(snapshot_["communityOwner"].toBool()&&!snapshot_["communityMarked"].toBool())add("Finish TrainerOS setup","mark-community");
        }
        if(conversation()&&!contacts_&&face_!="friends"&&currentChat()["kind"]!="groups")add(currentChat()["muted"].toBool()?"Unmute conversation":"Mute conversation","mute");
        add("Refresh","refresh");add("Communication settings","communication-settings");
    }
    if(reading_&&snapshot_["historyPast"].toBool())add("Latest messages","latest");
    emit changed();
}
void SocialController::closeMenu() {
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
    menuChannel_=snapshot_["channel"].toString();
    menuTitle_=mode=="create-group"?"Bring friends together":mode=="add-member"?"Add a friend":"Remove a member";
    menuDetail_=mode=="create-group"?"Choose friends for your group":conversationName();
    const auto members=currentChat()["members"].toList();QStringList memberIds;
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
    if(menuMode_=="runtime") {
        const auto row=runtimeActions_.value(index).toMap();
        if(row["readOnly"].toBool()||!row.value("enabled",true).toBool())return;
        emit runtimeAction(command);return;
    }
    if(command=="group-details"){groupDetails();return;}
    if(command=="together"){closeMenu();together();return;}
    if(command=="communication-settings"){closeMenu();emit communicationSettingsRequested();return;}
    if(command=="show-party"){closeMenu();emit runtimeAction("multiplayer-party");return;}
    if(command=="native-activities"){closeMenu();nativeActivities();return;}
    if(command.startsWith("company-party:")){const auto peer=command.mid(14);closeMenu();
        for(const auto& v:companyParties())if(v.toMap()["peer"]==peer&&v.toMap()["joinable"].toBool()){emit partyJoin(peer);break;}return;}
    if(command=="company-access"){editCompanyAccess();return;}
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
    if(command=="cancel"){closeMenu();return;}
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
        menuTitle_="Group members";menuDetail_=conversationName();menuMode_="members";menu_.clear();menuCommands_.clear();menuFocus_=0;
        menu_.append(snapshot_["name"].toString()+" (You)"+(snapshot_["userId"]==currentChat()["owner"]?" · Owner":""));menuCommands_.append("none");
        for(const auto& value:currentChat()["members"].toList()){const auto m=value.toMap();auto name=m["global_name"].toString();if(name.isEmpty())name=m["username"].toString();menu_.append(name+(m["id"]==currentChat()["owner"]?" · Owner":""));menuCommands_.append("none");}
        menu_.append("Close");menuCommands_.append("cancel");emit changed();return;
    }
    if(command=="contacts"){contacts_=!contacts_;menu_.clear();focus_=0;preview();emit changed();return;}
    if(command=="create-community") {
        menu_.clear();textPurpose_=command;emit textRequested("Community name",QString(),100);emit changed();return;
    }
    if(command=="community-invite") {
        menuMode_=command;menuTitle_="Invite friends";menuDetail_="Creating invitation...";
        menu_={"Close"};menuCommands_={"cancel"};menuFocus_=0;
        emit commandRequested(command,{{"id",snapshot_["guild"]}});emit changed();return;
    }
    if(command=="mark-community") {emit commandRequested(command,{{"id",snapshot_["guild"]}});menu_.clear();emit changed();return;}
    if(command=="create-group"||command=="add-member"||command=="remove-member") {
        if(menuMode_!="confirm"){openPeople(command);return;}
    }
    if(command=="ask-delete-message"){confirmAction("Delete this message?","delete-message",menuSubject_);return;}
    if(command=="ask-leave-group"){confirmAction("Leave "+conversationName()+"?","leave-group");return;}
    if(command=="edit-message"||command=="rename-group") {
        textPurpose_=command;textChannel_=menuChannel_;textId_=menuSubject_;
        QString text=conversationName();
        if(command=="edit-message") {
            text.clear();for(const auto& m:messages())if(m.toMap()["id"]==textId_)text=m.toMap()["text"].toString();
            text=editDrafts_.value(textChannel_+"/"+textId_,text);
        }
        menu_.clear();emit textRequested(command=="edit-message"?"Edit message":"Group name",text,command=="edit-message"?2000:100);emit changed();return;
    }
    emit commandRequested(command,{{"id",menuSubject_},{"channel",menuChannel_}});menu_.clear();emit changed();
}
void SocialController::dispatch(Action action) {
    if(snapshot_["state"]=="restoring")return;
    if(!menu_.isEmpty()) {
        if(action==Action::Back||action==Action::Left){if(menuMode_=="runtime")emit runtimeAction("multiplayer-cancel");else closeMenu();}
        else if(action==Action::Up||action==Action::Down)menuFocus_=qBound(0,menuFocus_+(action==Action::Up?-1:1),int(menu_.size())-1);
        else if(action==Action::Confirm)selectMenu(menuFocus_);
        else if(action==Action::ToggleContinue&&menuMode_=="create-group"&&!pickedPeople_.isEmpty()&&!snapshot_["mutationBusy"].toBool()) {
            emit commandRequested("create-group",{{"recipients",pickedPeople_}});menu_.clear();
        }
        emit changed();return;
    }
    if(snapshot_.value("state")=="authorizing") {if(action==Action::Back)emit commandRequested("cancel-login",{});return;}
    if(snapshot_.value("state")=="signed-out"||snapshot_.isEmpty()) {if(action==Action::Confirm)login();return;}
    if(face_=="friends") {
        if(action==Action::Secondary||(action==Action::Confirm&&searchFocus_<0))editSearch();
        else if(action==Action::Back)searchFocus_=-1;
        else if((action==Action::Left||action==Action::Right)&&searchFocus_<0) {
            const QStringList kinds{"people","communities","traineros","invite"};const int current=kinds.indexOf(searchKind_);
            setSearchKind(kinds[(current+(action==Action::Right?1:3))%4]);
        } else if(action==Action::Down)searchFocus_=qMin(searchFocus_<0?0:searchFocus_+2,int(searchResults().size())-1);
        else if(action==Action::Up)searchFocus_=qMax(-1,searchFocus_-2);
        else if(action==Action::Left)searchFocus_=qMax(0,searchFocus_-1);
        else if(action==Action::Right)searchFocus_=qMin(searchFocus_+1,int(searchResults().size())-1);
        else if(action==Action::Confirm)activateSearch(searchFocus_);
        else if(action==Action::ToggleContinue&&snapshot_["searchTotal"].toInt()>24)runSearch(snapshot_["searchOffset"].toInt()+24<snapshot_["searchTotal"].toInt()?snapshot_["searchOffset"].toInt()+24:0);
        else if(action==Action::LocalAction||action==Action::ContextMenu)openMenu();
        emit changed();return;
    }
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

    if(action==Action::Up||action==Action::Down) {
        if(reading_&&action==Action::Up&&messageFocus_==0&&snapshot_["historyMore"].toBool()){emit commandRequested("older",{});return;}
        auto& focus=reading_?messageFocus_:focus_;const int count=reading_?messages().size():rows().size();
        focus=qBound(0,focus+(action==Action::Up?-1:1),qMax(0,count-1));
        if(!reading_)selection_.start();
    } else if(action==Action::Right&&conversation())reading_=true;
    else if(action==Action::Left)reading_=false;
    else if(action==Action::Confirm){if(reading_){const auto m=messages().value(messageFocus_).toMap();
        if(m["editable"].toBool()||m["media"].toBool()||!m["delivery"].toString().isEmpty()) {
            openMenu();
            if(m["attachments"].toList().size()==1)selectMenu(0);
        }
    }else if(!contacts_&&conversation()&&rows().value(focus_).toMap()["id"]==snapshot_["channel"])reading_=true;else activate(focus_);}
    else if(action==Action::Back){reading_=false;if(contacts_){contacts_=false;focus_=0;} }
    else if(action==Action::Secondary){if(face_=="groups"&&!conversation())openPeople("create-group");
        else if(face_=="communities"&&!conversation()){textPurpose_="create-community";emit textRequested("Community name",QString(),100);}else compose();}
    else if(action==Action::ToggleContinue)together();
    else if(action==Action::LocalAction||action==Action::ContextMenu)openMenu();
    if(reading_&&conversation()&&(action==Action::Up||action==Action::Down))emit commandRequested("history-position",{{"channel",snapshot_["channel"]},{"id",messageFocus_>=messages().size()-1?QString():messages().value(messageFocus_).toMap()["id"].toString()}});
    emit changed();
}
void SocialController::preview() {
    const auto row=rows().value(focus_).toMap();
    if(row.isEmpty()||(contacts_&&row["type"].toInt()!=1))return;
    activate(focus_);
}
void SocialController::showContacts() {
    setFace("chats");contacts_=true;focus_=0;preview();emit changed();
}
void SocialController::showFriends() {
    if(contacts_){contacts_=false;focus_=0;const auto list=rows();for(int i=0;i<list.size();++i)if(list[i].toMap()["id"]==snapshot_["channel"]){focus_=i;break;}emit changed();}
    else showContacts();
}
void SocialController::runSearch(int offset) {
    searchStarted_=true;searchFocus_=-1;
    emit commandRequested("search",{{"mode",searchKind_},{"text",query_},{"offset",offset}});
}
void SocialController::setSearchKind(QString kind) {
    if(kind==searchKind_)return;
    if(!QStringList{"people","communities","traineros","invite"}.contains(kind))return;
    searchKind_=kind;query_.clear();runSearch();emit changed();
}
void SocialController::editSearch() {
    textPurpose_="search";
    emit textRequested(searchKind_=="people"?"Find someone · username#1234":searchKind_=="invite"?"Group or community invite":"Find communities",query_,searchKind_=="invite"?256:100);
}
void SocialController::activateSearch(int index) {
    const auto row=searchResults().value(index).toMap();if(row.isEmpty()||row["action"]=="Joined"||row["action"]=="Sent")return;
    searchFocus_=index;emit commandRequested("search-action",{{"id",row["id"]}});emit changed();
}
void SocialController::bindDrafts(const QString& accountId) {
    if(owner_.isEmpty())return;
    const auto key=QString::fromLatin1(QCryptographicHash::hash(("https://fluxer.app\n"+owner_+"\n"+accountId).toUtf8(),QCryptographicHash::Sha256).toHex());
    const auto file=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/social/"+key+"/drafts.json";
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
