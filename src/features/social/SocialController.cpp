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
#include <algorithm>

namespace trainer {
SocialController::SocialController(QObject* parent):QObject(parent),session_(new FluxerSession) {
    session_->moveToThread(&thread_);
    connect(&thread_,&QThread::finished,session_,&QObject::deleteLater);
    connect(this,&SocialController::ownerRequested,session_,&FluxerSession::setOwner);
    connect(this,&SocialController::commandRequested,session_,&FluxerSession::command);
    connect(session_,&FluxerSession::snapshot,this,&SocialController::receive);
    toastTimer_.setSingleShot(true);toastTimer_.setInterval(4500);
    connect(&toastTimer_,&QTimer::timeout,this,[this]{toastTitle_.clear();toastText_.clear();emit presentationChanged();});
    connect(session_,&FluxerSession::incomingMessage,this,[this](quint64 generation,QString channel,QString name,QString text){
        if(generation!=generation_||!surfaceAvailable_||!menu_.isEmpty()
            ||(conversationVisible_&&!contacts_&&face_!="friends"&&snapshot_["channel"]==channel))return;
        toastTitle_=std::move(name);toastText_=std::move(text);toastTimer_.start();emit presentationChanged();
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
    if(link_)link_->endOnline();
    saveDrafts();draftFile_.clear();
    editDrafts_.clear();pickedPeople_.clear();
    toastTimer_.stop();toastTitle_.clear();toastText_.clear();emit presentationChanged();
    owner_=std::move(owner);++generation_;snapshot_.clear();drafts_.clear();menu_.clear();
    selection_.stop();contacts_=false;searchStarted_=false;searchFocus_=-1;
    textPurpose_.clear();textChannel_.clear();query_.clear();focus_=messageFocus_=0;reading_=false;
    emit ownerRequested(owner_,generation_);emit changed();
}
void SocialController::setFace(QString face) {
    if(face_==face)return;
    selection_.stop();face_=std::move(face);focus_=0;menu_.clear();reading_=false;contacts_=false;
    emit commandRequested("face",{{"face",face_}});
    if(face_=="friends"&&!searchStarted_&&snapshot_["state"]=="connected")runSearch();
    emit changed();
}
void SocialController::setLink(LinkController* link) {
    link_=link;
    connect(session_,&FluxerSession::onlineEstablished,this,[this](quint64 generation,QString self,QString peer,QString name,QString activity,bool initiator){
        if(generation!=generation_)return;
        if(!onlineWritable_||!link_||!link_->beginOnline(self,peer,name,activity,initiator))emit commandRequested("online-close",{});
    });
    connect(session_,&FluxerSession::onlineFrame,this,[this](quint64 generation,QJsonObject frame){if(generation==generation_&&link_)link_->receiveOnline(frame);});
    connect(session_,&FluxerSession::onlineEnded,this,[this](quint64 generation){if(generation==generation_&&link_)link_->endOnline();});
    connect(link,&LinkController::onlineSend,this,[this](QJsonObject frame){emit commandRequested("online-frame",frame.toVariantMap());});
    connect(link,&LinkController::onlineClosed,this,[this]{emit commandRequested("online-close",{});});
}
void SocialController::setOnlineContext(bool available,bool writable) {
    if(onlineAvailable_!=available){onlineAvailable_=available;emit commandRequested("online-available",{{"available",available}});}
    onlineWritable_=writable;
    const auto caps=writable&&link_?link_->onlineCapabilities().toVariantList():QVariantList{};
    if(caps!=onlineCapabilities_){onlineCapabilities_=caps;emit commandRequested("online-capabilities",{{"activities",caps}});}
}
void SocialController::answerOnline(bool accept){emit commandRequested("online-answer",{{"accept",accept}});}
void SocialController::setSurfaceAvailable(bool available) {
    if(surfaceAvailable_==available)return;
    surfaceAvailable_=available;
    if(!available){toastTimer_.stop();toastTitle_.clear();toastText_.clear();}
    emit presentationChanged();
}
void SocialController::setConversationVisible(bool visible) {
    if(conversationVisible_==visible)return;
    conversationVisible_=visible;emit presentationChanged();
}
void SocialController::presented(QString channel,QString message) {
    if(!surfaceAvailable_||!conversationVisible_||contacts_||face_=="friends"||!menu_.isEmpty()
        ||snapshot_["historyBusy"].toBool()||snapshot_["channel"]!=channel||messages().isEmpty()
        ||messages().last().toMap()["id"]!=message)return;
    emit commandRequested("read",{{"channel",channel},{"message",message}});
}
void SocialController::receive(quint64 generation,QVariantMap snapshot) {
    if(generation!=generation_)return;
    const auto oldId=rows().value(focus_).toMap().value("id");
    const auto oldChannel=snapshot_.value("channel");
    const bool wasEarlier=snapshot_["historyPast"].toBool();
    const auto oldMessage=messages().value(messageFocus_).toMap().value("id");
    const bool atEnd=messageFocus_>=messages().size()-1;
    snapshot_=std::move(snapshot);
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
    const auto list=rows();focus_=qBound(0,focus_,qMax(0,int(list.size())-1));
    for(int i=0;i<list.size();++i)if(list[i].toMap()["id"]==oldId){focus_=i;break;}
    if(oldChannel!=snapshot_["channel"]) {
        reading_=false;
        if(!contacts_)for(int i=0;i<list.size();++i)if(list[i].toMap()["id"]==snapshot_["channel"]){focus_=i;break;}
    }
    const auto log=messages();
    if(atEnd||oldChannel!=snapshot_["channel"]||(wasEarlier&&!snapshot_["historyPast"].toBool()))messageFocus_=qMax(0,int(log.size())-1);
    else for(int i=0;i<log.size();++i)if(log[i].toMap()["id"]==oldMessage){messageFocus_=i;break;}
    messageFocus_=qBound(0,messageFocus_,qMax(0,int(log.size())-1));
    if(snapshot_["state"]=="signed-out") {toastTimer_.stop();toastTitle_.clear();toastText_.clear();emit presentationChanged();draftSave_.stop();if(!draftFile_.isEmpty())QFile::remove(draftFile_);draftFile_.clear();drafts_.clear();editDrafts_.clear();menu_.clear();textPurpose_.clear();textChannel_.clear();searchStarted_=false;}
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
    } else for(const auto& c:snapshot_.value("chats").toList())if(c.toMap()["kind"]==face_)result.append(c);
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
        if(face_=="groups"&&!conversation())h("X","New group");
        if(face_=="communities"&&!conversation())h("X","New community");
        if(face_=="communities"&&!reading_&&draft().trimmed().isEmpty())h("Y",snapshot_["communityOnly"].toBool()?"All communities":"TrainerOS only");
        if(reading_&&messages().value(messageFocus_).toMap()["editable"].toBool())h("A","Message");
        if(contacts_&&rows().value(focus_).toMap()["type"].toInt()==3)h("A","Accept request");
        if(conversation()){h(reading_||contacts_?"X":"A","Write");if(reading_&&snapshot_["historyPast"].toBool())h("Y","Latest");else if(togetherAvailable())h("Y","Together");else if(!draft().trimmed().isEmpty())h("Y","Send");h(reading_?"←":"→",reading_?"Conversations":"Read");if(reading_&&messageFocus_==0&&snapshot_["historyMore"].toBool())h("↑","Earlier");}
        if(contacts_||reading_)h("B",contacts_?"Conversations":"List");
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
QVariantMap SocialController::currentChat() const {
    for(const auto& row:snapshot_["chats"].toList())if(row.toMap()["id"]==snapshot_["channel"])return row.toMap();
    return {};
}
bool SocialController::togetherAvailable() const {
    return face_=="chats" && !contacts_ && conversation() && currentChat()["friend"].toBool()
        && rows().value(focus_).toMap()["id"]==snapshot_["channel"];
}
void SocialController::together() {
    if(!menu_.isEmpty() || !togetherAvailable())return;
    selection_.stop();menuChannel_=snapshot_["channel"].toString();
    if(online()["stage"]=="connected"&&online()["channel"]==menuChannel_&&link_){
        link_->showOnline();return;
    }
    menuMode_="online";menuTitle_="Together";menuDetail_="Checking TrainerOS...";menuFocus_=0;
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
    if(reading_&&message["editable"].toBool()) {
        menuSubject_=message["id"].toString();menuTitle_="Your message";menuDetail_=message["text"].toString().left(120);
        add("Edit message","edit-message");add("Delete message","ask-delete-message");
    } else {
        if(face_=="chats")add(contacts_?"Conversations":"Friends & requests","contacts");
        if(contacts_&&!menuSubject_.isEmpty()) {
            const int type=row["type"].toInt();
            if(type==3){add("Accept request","accept");add("Decline request","remove");}
            if(type==4)add("Cancel request","remove");
            if(type==1)add("Remove friend","remove");
            if(type==2)add("Unblock","remove");else add("Block","block");
        }
        if(face_=="groups") {
            add("New group","create-group");
            if(currentChat()["kind"]=="groups") {
                menuTitle_=conversationName();menuDetail_="Group conversation";
                add("Rename group","rename-group");add("Add friend","add-member");
                if(currentChat()["owner"]==snapshot_["userId"])add("Remove member","remove-member");
                add("Leave group","ask-leave-group");
            }
        }
        if(face_=="communities") {
            add("New community","create-community");
            add(snapshot_["communityOnly"].toBool()?"Show all communities":"Show TrainerOS communities","community-filter");
            if(!snapshot_["guild"].toString().isEmpty())add("Invite link","community-invite");
            if(snapshot_["communityOwner"].toBool()&&!snapshot_["communityMarked"].toBool())add("Finish TrainerOS setup","mark-community");
        }
        if(conversation()&&!contacts_&&face_!="friends")add(currentChat()["muted"].toBool()?"Unmute conversation":"Mute conversation","mute");
        add(snapshot_["doNotDisturb"].toBool()?"Do not disturb: On":"Do not disturb: Off","dnd");
        add(snapshot_["privatePreviews"].toBool()?"Private notifications: On":"Private notifications: Off","private");
        add("Refresh","refresh");add("Sign out of Fluxer","logout");
    }
    emit changed();
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
    if(command=="none")return;
    if(command=="cancel"){menu_.clear();emit changed();return;}
    if(command.startsWith("online:")){menu_.clear();emit changed();emit commandRequested("online-invite",{{"id",command.mid(7)}});return;}
    if(menuMode_=="create-group") {
        const bool selected=pickedPeople_.contains(command);
        if(selected)pickedPeople_.removeAll(command);else if(pickedPeople_.size()<49)pickedPeople_.append(command);else return;
        menu_[index]=(selected?QString::fromUtf8("\xe2\x97\x8b "):QString::fromUtf8("\xe2\x9c\x93 "))+menu_[index].mid(2);
        menuFocus_=index;emit changed();return;
    }
    if(menuMode_=="add-member"){emit commandRequested("add-member",{{"channel",menuChannel_},{"id",command}});menu_.clear();emit changed();return;}
    if(menuMode_=="remove-member"){confirmAction("Remove "+menu_[index]+"?","remove-member",command);return;}
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
        if(action==Action::Back)menu_.clear();
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
    if(action==Action::Up||action==Action::Down) {
        if(reading_&&action==Action::Up&&messageFocus_==0&&snapshot_["historyMore"].toBool()){emit commandRequested("older",{});return;}
        auto& focus=reading_?messageFocus_:focus_;const int count=reading_?messages().size():rows().size();
        focus=qBound(0,focus+(action==Action::Up?-1:1),qMax(0,count-1));
        if(!reading_)selection_.start();
    } else if(action==Action::Right&&conversation())reading_=true;
    else if(action==Action::Left)reading_=false;
    else if(action==Action::Confirm){if(reading_){if(messages().value(messageFocus_).toMap()["editable"].toBool())openMenu();}else if(!contacts_&&conversation()&&rows().value(focus_).toMap()["id"]==snapshot_["channel"])compose();else activate(focus_);}
    else if(action==Action::Back){reading_=false;if(contacts_){contacts_=false;focus_=0;} }
    else if(action==Action::Secondary){if(face_=="groups"&&!conversation())openPeople("create-group");
        else if(face_=="communities"&&!conversation()){textPurpose_="create-community";emit textRequested("Community name",QString(),100);}else compose();}
    else if(action==Action::ToggleContinue){if(reading_&&snapshot_["historyPast"].toBool())emit commandRequested("latest",{});
        else if(togetherAvailable())together();else if(face_=="communities"&&!reading_&&draft().trimmed().isEmpty())emit commandRequested("community-filter",{});else send();}
    else if(action==Action::LocalAction||action==Action::ContextMenu)openMenu();
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
