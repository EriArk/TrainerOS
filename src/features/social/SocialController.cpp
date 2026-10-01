#include "SocialController.h"
#include "integrations/social/FluxerSession.h"
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
    connect(session_,&FluxerSession::sendFailed,this,[this](quint64 generation,QString channel,QString text){
        if(generation!=generation_ || !drafts_.value(channel).isEmpty())return;
        drafts_[channel]=text;draftSave_.start();emit changed();
    });
    selection_.setSingleShot(true);selection_.setInterval(140);connect(&selection_,&QTimer::timeout,this,&SocialController::preview);
    draftSave_.setSingleShot(true);draftSave_.setInterval(700);connect(&draftSave_,&QTimer::timeout,this,&SocialController::saveDrafts);
    thread_.start();
}
SocialController::~SocialController() {
    saveDrafts();
    QMetaObject::invokeMethod(session_,&FluxerSession::stop,Qt::BlockingQueuedConnection);
    thread_.quit();thread_.wait();
}
void SocialController::setOwner(QString owner) {
    if(owner==owner_)return;
    saveDrafts();draftFile_.clear();
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
void SocialController::receive(quint64 generation,QVariantMap snapshot) {
    if(generation!=generation_)return;
    const auto oldId=rows().value(focus_).toMap().value("id");
    const auto oldChannel=snapshot_.value("channel");
    const bool wasEarlier=snapshot_["historyPast"].toBool();
    const auto oldMessage=messages().value(messageFocus_).toMap().value("id");
    const bool atEnd=messageFocus_>=messages().size()-1;
    snapshot_=std::move(snapshot);
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
    if(snapshot_["state"]=="signed-out") {draftSave_.stop();if(!draftFile_.isEmpty())QFile::remove(draftFile_);draftFile_.clear();drafts_.clear();textPurpose_.clear();textChannel_.clear();searchStarted_=false;}
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
    if(!menu_.isEmpty()){h("A","Choose");h("B","Close");return result;}
    if(snapshot_["state"]=="restoring")return result;
    if(snapshot_.value("state")=="authorizing"){h("B","Cancel sign-in");return result;}
    if(snapshot_.value("state")=="signed-out"||snapshot_.isEmpty()){if(!owner_.isEmpty())h("A","Sign in");return result;}
    if(face_=="friends") {
        h("X","Search");
        if(searchFocus_<0) {h("←→","Category");h("A","Enter search");}
        else {const auto action=searchResults().value(searchFocus_).toMap()["action"].toString();if(action!="Joined"&&action!="Sent")h("A",action);h("B","Search bar");}
        if(snapshot_["searchTotal"].toInt()>24)h("Y","Next page");
    } else {
        if(contacts_&&rows().value(focus_).toMap()["type"].toInt()==3)h("A","Accept request");
        if(conversation()){h("X","Write");if(reading_&&snapshot_["historyPast"].toBool())h("Y","Latest");else if(!draft().trimmed().isEmpty())h("Y","Send");h(reading_?"←":"→",reading_?"Conversations":"Read");if(reading_&&messageFocus_==0&&snapshot_["historyMore"].toBool())h("↑","Earlier");}
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
    textPurpose_="message";textChannel_=snapshot_["channel"].toString();
    emit textRequested("Message",drafts_.value(textChannel_),2000);
}
void SocialController::preserveText(QString text) {
    if(textPurpose_=="message"&&!textChannel_.isEmpty()){drafts_[textChannel_]=text.left(2000);draftSave_.start();}
}
void SocialController::applyText(QString text) {
    if(textPurpose_=="search"){query_=text.trimmed();searchFocus_=-1;runSearch();}
    else if(textPurpose_=="add")emit commandRequested("add",{{"text",text}});
    else preserveText(text);
    textPurpose_.clear();textChannel_.clear();emit changed();
}
void SocialController::send() {
    if(!conversation()||draft().trimmed().isEmpty())return;
    emit commandRequested("send",{{"text",draft()}});
    drafts_.remove(snapshot_["channel"].toString());emit changed();
    saveDrafts();
}
void SocialController::openMenu() {
    menu_.clear();menuCommands_.clear();menuFocus_=0;
    const auto row=rows().value(focus_).toMap();menuSubject_=row["id"].toString();
    auto add=[&](QString label,QString command){menu_.append(label);menuCommands_.append(command);};
    if(face_=="chats")add(contacts_?"Conversations":"Friends & requests","contacts");
    if(contacts_&&!menuSubject_.isEmpty()) {
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
    if(menuCommands_[index]=="contacts"){contacts_=!contacts_;menu_.clear();focus_=0;preview();emit changed();return;}
    emit commandRequested(menuCommands_[index],{{"id",menuSubject_}});menu_.clear();emit changed();
}
void SocialController::dispatch(Action action) {
    if(snapshot_["state"]=="restoring")return;
    if(!menu_.isEmpty()) {
        if(action==Action::Back)menu_.clear();
        else if(action==Action::Up||action==Action::Down)menuFocus_=qBound(0,menuFocus_+(action==Action::Up?-1:1),int(menu_.size())-1);
        else if(action==Action::Confirm)selectMenu(menuFocus_);
        emit changed();return;
    }
    if(snapshot_.value("state")=="authorizing") {if(action==Action::Back)emit commandRequested("cancel-login",{});return;}
    if(snapshot_.value("state")=="signed-out"||snapshot_.isEmpty()) {if(action==Action::Confirm)login();return;}
    if(face_=="friends") {
        if(action==Action::Secondary||(action==Action::Confirm&&searchFocus_<0))editSearch();
        else if(action==Action::Back)searchFocus_=-1;
        else if((action==Action::Left||action==Action::Right)&&searchFocus_<0) {
            const QStringList kinds{"people","communities","invite"};const int current=kinds.indexOf(searchKind_);
            setSearchKind(kinds[(current+(action==Action::Right?1:2))%3]);
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
    else if(action==Action::Confirm&&!reading_)activate(focus_);
    else if(action==Action::Back){reading_=false;if(contacts_){contacts_=false;focus_=0;} }
    else if(action==Action::Secondary)compose();
    else if(action==Action::ToggleContinue){if(reading_&&snapshot_["historyPast"].toBool())emit commandRequested("latest",{});else send();}
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
    if(!QStringList{"people","communities","invite"}.contains(kind))return;
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
        if(!it.key().isEmpty()&&it.key().size()<=20)drafts_[it.key()]=it.value().toString().left(2000);
}
void SocialController::saveDrafts() {
    draftSave_.stop();if(draftFile_.isEmpty())return;
    QJsonObject data;for(auto it=drafts_.cbegin();it!=drafts_.cend()&&data.size()<128;++it)if(!it.value().isEmpty())data[it.key()]=it.value().left(2000);
    const auto dir=QFileInfo(draftFile_).absolutePath();
    if(QFileInfo(dir).isSymLink()||QFileInfo(draftFile_).isSymLink()||!QDir().mkpath(dir))return;
    QFile::setPermissions(dir,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    QSaveFile file(draftFile_);if(!file.open(QIODevice::WriteOnly))return;
    file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);const auto bytes=QJsonDocument(data).toJson(QJsonDocument::Compact);
    if(file.write(bytes)==bytes.size())file.commit();
}
}
