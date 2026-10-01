#include "SocialController.h"
#include "integrations/social/FluxerSession.h"
#include <algorithm>

namespace trainer {
SocialController::SocialController(QObject* parent):QObject(parent),session_(new FluxerSession) {
    session_->moveToThread(&thread_);
    connect(&thread_,&QThread::finished,session_,&QObject::deleteLater);
    connect(this,&SocialController::ownerRequested,session_,&FluxerSession::setOwner);
    connect(this,&SocialController::commandRequested,session_,&FluxerSession::command);
    connect(session_,&FluxerSession::snapshot,this,&SocialController::receive);
    connect(session_,&FluxerSession::sendFailed,this,[this](quint64 generation,QString channel,QString text){
        if(generation!=generation_ || !drafts_.value(channel).isEmpty())return;
        drafts_[channel]=text;emit changed();
    });
    thread_.start();
}
SocialController::~SocialController() {
    QMetaObject::invokeMethod(session_,&FluxerSession::stop,Qt::BlockingQueuedConnection);
    thread_.quit();thread_.wait();
}
void SocialController::setOwner(QString owner) {
    if(owner==owner_)return;
    owner_=std::move(owner);++generation_;snapshot_.clear();drafts_.clear();menu_.clear();
    textPurpose_.clear();textChannel_.clear();query_.clear();focus_=messageFocus_=0;reading_=false;
    emit ownerRequested(owner_,generation_);emit changed();
}
void SocialController::setFace(QString face) {
    if(face_==face)return;
    face_=std::move(face);focus_=0;menu_.clear();reading_=false;emit changed();
}
void SocialController::receive(quint64 generation,QVariantMap snapshot) {
    if(generation!=generation_)return;
    const auto oldId=rows().value(focus_).toMap().value("id");
    const auto oldChannel=snapshot_.value("channel");
    const auto oldMessage=messages().value(messageFocus_).toMap().value("id");
    const bool atEnd=messageFocus_>=messages().size()-1;
    snapshot_=std::move(snapshot);
    const auto list=rows();focus_=qBound(0,focus_,qMax(0,int(list.size())-1));
    for(int i=0;i<list.size();++i)if(list[i].toMap()["id"]==oldId){focus_=i;break;}
    if(oldChannel!=snapshot_["channel"])reading_=false;
    const auto log=messages();
    if(atEnd||oldChannel!=snapshot_["channel"])messageFocus_=qMax(0,int(log.size())-1);
    else for(int i=0;i<log.size();++i)if(log[i].toMap()["id"]==oldMessage){messageFocus_=i;break;}
    messageFocus_=qBound(0,messageFocus_,qMax(0,int(log.size())-1));
    if(snapshot_["state"]=="signed-out") {drafts_.clear();textPurpose_.clear();textChannel_.clear();}
    emit changed();
}
QVariantMap SocialController::account() const {
    auto result=snapshot_;result.remove("friends");result.remove("chats");result.remove("messages");
    result["available"]=!owner_.isEmpty();return result;
}
QVariantList SocialController::rows() const {
    if(face_=="friends") {
        QVariantList matches;
        for(const auto& r:snapshot_.value("friends").toList())if(query_.isEmpty()||r.toMap()["name"].toString().contains(query_,Qt::CaseInsensitive))matches.append(r);
        return matches;
    }
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
    if(!menu_.isEmpty()){h("A","Choose");h("B","Close");return result;}
    if(snapshot_.value("state")=="authorizing"){h("B","Cancel sign-in");return result;}
    if(snapshot_.value("state")=="signed-out"||snapshot_.isEmpty()){if(!owner_.isEmpty())h("A","Sign in");return result;}
    if(conversation()){h("X","Write");if(!draft().trimmed().isEmpty())h("Y","Send");h(reading_?"←":"→",reading_?"People":"Read");h("B","Close chat");}
    else {if(!rows().isEmpty())h("A",face_=="friends"&&rows().value(focus_).toMap()["type"].toInt()==3?"Accept":"Open");if(face_=="friends")h("X","Search");h("Y",face_=="friends"?"Add friend":"Refresh");}
    h("Select","Options");return result;
}
void SocialController::login(){if(!owner_.isEmpty())emit commandRequested("login",{});}
void SocialController::activate(int index) {
    if(!menu_.isEmpty())return;
    const auto list=rows();if(index<0||index>=list.size())return;
    focus_=index;reading_=false;const auto row=list[index].toMap();
    const auto type=row["type"].toInt();
    if(row["kind"]=="community")emit commandRequested("guild",{{"id",row["id"]}});
    else if(face_!="friends")emit commandRequested("open",{{"id",row["id"]}});
    else if(type==1)emit commandRequested("dm",{{"id",row["id"]}});
    else if(type==3)emit commandRequested("accept",{{"id",row["id"]}});
    else openMenu();
    emit changed();
}
void SocialController::compose() {
    if(!conversation())return;
    textPurpose_="message";textChannel_=snapshot_["channel"].toString();
    emit textRequested("Message",drafts_.value(textChannel_),2000);
}
void SocialController::preserveText(QString text) {
    if(textPurpose_=="message"&&!textChannel_.isEmpty())drafts_[textChannel_]=text.left(2000);
}
void SocialController::applyText(QString text) {
    if(textPurpose_=="search"){query_=text.trimmed();focus_=0;}
    else if(textPurpose_=="add")emit commandRequested("add",{{"text",text}});
    else preserveText(text);
    textPurpose_.clear();textChannel_.clear();emit changed();
}
void SocialController::send() {
    if(!conversation()||draft().trimmed().isEmpty())return;
    emit commandRequested("send",{{"text",draft()}});
    drafts_.remove(snapshot_["channel"].toString());emit changed();
}
void SocialController::openMenu() {
    menu_.clear();menuCommands_.clear();menuFocus_=0;
    const auto row=rows().value(focus_).toMap();menuSubject_=row["id"].toString();
    auto add=[&](QString label,QString command){menu_.append(label);menuCommands_.append(command);};
    if(face_=="friends"&&!query_.isEmpty())add("Show all friends","clear-search");
    if(face_=="friends"&&!menuSubject_.isEmpty()) {
        const int type=row["type"].toInt();
        if(type==3){add("Accept request","accept");add("Decline request","remove");}
        if(type==4)add("Cancel request","remove");
        if(type==1)add("Remove friend","remove");
        if(type==2)add("Unblock","remove");else add("Block","block");
    }
    add("Refresh","refresh");add("Sign out of Fluxer","logout");emit changed();
}
void SocialController::selectMenu(int index) {
    if(index<0||index>=menuCommands_.size())return;
    if(menuCommands_[index]=="clear-search"){query_.clear();menu_.clear();focus_=0;emit changed();return;}
    emit commandRequested(menuCommands_[index],{{"id",menuSubject_}});menu_.clear();emit changed();
}
void SocialController::dispatch(Action action) {
    if(!menu_.isEmpty()) {
        if(action==Action::Back)menu_.clear();
        else if(action==Action::Up||action==Action::Down)menuFocus_=qBound(0,menuFocus_+(action==Action::Up?-1:1),int(menu_.size())-1);
        else if(action==Action::Confirm)selectMenu(menuFocus_);
        emit changed();return;
    }
    if(snapshot_.value("state")=="authorizing") {if(action==Action::Back)emit commandRequested("cancel-login",{});return;}
    if(snapshot_.value("state")=="signed-out"||snapshot_.isEmpty()) {if(action==Action::Confirm)login();return;}
    if(action==Action::Up||action==Action::Down) {
        auto& focus=reading_?messageFocus_:focus_;const int count=reading_?messages().size():rows().size();
        focus=qBound(0,focus+(action==Action::Up?-1:1),qMax(0,count-1));
    } else if(action==Action::Right&&conversation())reading_=true;
    else if(action==Action::Left)reading_=false;
    else if(action==Action::Confirm&&!reading_)activate(focus_);
    else if(action==Action::Back&&conversation()){emit commandRequested("close",{});reading_=false;}
    else if(action==Action::Secondary) {
        if(conversation())compose();
        else if(face_=="friends"){textPurpose_="search";emit textRequested("Find friends",query_,64);}
    } else if(action==Action::ToggleContinue) {if(conversation())send();else if(face_=="friends"){textPurpose_="add";emit textRequested("Add friend · username#1234",{},64);}else emit commandRequested("refresh",{});}
    else if(action==Action::LocalAction||action==Action::ContextMenu)openMenu();
    emit changed();
}
}
