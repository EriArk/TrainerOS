#include "SocialController.h"
#include <QSettings>
#include <QCryptographicHash>
#include <algorithm>

namespace trainer {
namespace {
void sortRows(QVariantList& rows) {
    std::stable_sort(rows.begin(),rows.end(),[](const QVariant& a,const QVariant& b){
        const auto x=a.toMap(),y=b.toMap();
        if(x["pinned"].toBool()!=y["pinned"].toBool())return x["pinned"].toBool();
        if(x["lastInteraction"].toLongLong()!=y["lastInteraction"].toLongLong())return x["lastInteraction"].toLongLong()>y["lastInteraction"].toLongLong();
        const int name=x["name"].toString().localeAwareCompare(y["name"].toString());
        return name?name<0:x["id"].toString()<y["id"].toString();
    });
}
}
QVariantList SocialController::peopleRows() const {
    QMap<QString,QVariantMap> people;QSet<QString> blocked;
    const auto self=snapshot_["userId"].toString();
    for(const auto& v:snapshot_["friends"].toList()){
        auto p=v.toMap();const auto id=p["id"].toString();const int type=p["type"].toInt();
        if(type==2){blocked.insert(id);continue;}
        if(p["service"].toBool()||id.isEmpty()||id==self||!QList<int>{1,3,4}.contains(type))continue;
        p["kind"]="person";p["detail"]="";p["marker"]=type==3?"Request":type==4?"Pending":"";people[id]=p;
    }
    for(const auto& v:snapshot_["chats"].toList()){
        const auto chat=v.toMap();const auto members=chat["members"].toList();
        if(chat["service"].toBool()||chat["kind"]!="chats"||!chat["guild"].toString().isEmpty()||members.size()!=1)continue;
        const auto id=members.first().toMap()["id"].toString();
        if(id.isEmpty()||id==self||blocked.contains(id))continue;
        auto p=people.value(id);
        if(!p.value("channel").toString().isEmpty())continue; // One verified account, one provider DM.
        if(p.isEmpty())p={{"id",id},{"name",chat["name"]},{"avatar",chat["avatar"]},{"type",0},{"marker","Not friends"}};
        if(p.value("name").toString().isEmpty())p["name"]=chat["name"];
        p["kind"]="person";p["channel"]=chat["id"];p["unread"]=chat["unread"];p["muted"]=chat["muted"];
        p["lastInteraction"]=chat["lastInteraction"];p["detail"]=chat["detail"];
        if(p["avatar"].toString().isEmpty())p["avatar"]=chat["avatar"];
        people[id]=p;
    }
    QVariantList result;
    for(auto p:people){
        p["pinned"]=pinnedPeople_.contains(p["id"].toString());
        if(!peopleFilter_.isEmpty()&&!p["name"].toString().contains(peopleFilter_,Qt::CaseInsensitive))continue;
        result.append(p);
    }
    sortRows(result);return result;
}
QVariantList SocialController::communityRows() const {
    QVariantList groups,guilds,channels;
    if(!communityId_.isEmpty()){
        for(const auto& v:snapshot_["chats"].toList()){
            auto row=v.toMap();if(row["guild"]!=communityId_)continue;
            row["name"]="# "+row["name"].toString();row["section"]="Channels";channels.append(row);
        }
        std::stable_sort(channels.begin(),channels.end(),[](const QVariant& a,const QVariant& b){return a.toMap()["position"].toInt()<b.toMap()["position"].toInt();});
        return channels;
    }
    for(const auto& v:snapshot_["chats"].toList())if(v.toMap()["kind"]=="groups"&&!v.toMap()["socialHidden"].toBool()){
        auto row=v.toMap();row["pinned"]=pinnedGroups_.contains(row["id"].toString());row["section"]="Groups";groups.append(row);
    }
    for(const auto& v:snapshot_["communities"].toList()){
        auto row=v.toMap();row["pinned"]=pinnedCommunities_.contains(row["id"].toString());row["section"]="Communities";guilds.append(row);
    }
    sortRows(groups);sortRows(guilds);groups.append(guilds);return groups;
}
QString SocialController::communityTitle() const {
    for(const auto& v:snapshot_["communities"].toList())if(v.toMap()["id"]==communityId_)return v.toMap()["name"].toString();
    return "Communities";
}
void SocialController::filterPeople(){textPurpose_="people-filter";emit textRequested("Find a person",peopleFilter_,100);}
void SocialController::clearPeopleFilter(){peopleFilter_.clear();focus_=0;emit changed();}
void SocialController::communityBack(){
    if(communityId_.isEmpty())return;
    const auto id=communityId_;communityId_.clear();reading_=false;focus_=0;
    const auto list=rows();for(int i=0;i<list.size();++i)if(list[i].toMap()["id"]==id)focus_=i;
    saveSocialPreferences();emit commandRequested("face",{{"face","communities"},{"guild",QString()}});emit changed();
}
void SocialController::restoreSocialPreferences(){
    const auto account=snapshot_["userId"].toString();if(owner_.isEmpty()||account.isEmpty())return;
    const auto key="social/navigation-v3/"+QString::fromLatin1(QCryptographicHash::hash((owner_+"/"+account).toUtf8(),QCryptographicHash::Sha256).toHex());
    if(key==socialSettingsKey_)return;
    socialSettingsKey_=key;const auto saved=QSettings().value(key).toMap();
    pinnedPeople_=saved["people"].toStringList();pinnedGroups_=saved["groups"].toStringList();pinnedCommunities_=saved["communities"].toStringList();
    faceRoutes_.clear();pendingFaceRoute_.clear();
    const auto routes=saved["routes"].toMap();for(auto it=routes.begin();it!=routes.end();++it)faceRoutes_[it.key()]=it.value().toMap();
}
void SocialController::saveSocialPreferences(){
    if(socialSettingsKey_.isEmpty())return;
    QVariantMap routes;for(auto it=faceRoutes_.cbegin();it!=faceRoutes_.cend();++it)routes[it.key()]=it.value();
    QSettings().setValue(socialSettingsKey_,QVariantMap{{"people",pinnedPeople_},{"groups",pinnedGroups_},{"communities",pinnedCommunities_},{"community",communityId_},{"routes",routes}});
}
void SocialController::composerTools(){
    if(!conversation()||snapshot_["state"]!="connected")return;
    closeMenu();menuMode_="compose-tools";menuChannel_=snapshot_["channel"].toString();menuTitle_="Write a message";menuFocus_=0;
    menu_={"Write","Emoji"};menuCommands_={"compose-message","emoji-picker"};
    if(!menuChannel_.isEmpty()){
        menu_<<"Attach file"<<"Picture";menuCommands_<<"social-file"<<"picture";
        if(snapshot_["voice"].toMap()["channel"].toString().isEmpty()){menu_<<"Voice message";menuCommands_<<"record";}
    }
    if(!draft().trimmed().isEmpty()||!files_.selected().isEmpty()){menu_<<"Send";menuCommands_<<"send-draft";}
    for(int i=0;i<files_.selected().size();++i){menu_<<"Remove "+files_.selected()[i].toMap()["name"].toString();menuCommands_<<"files-remove:"+QString::number(i);}
    if(files_.busy()){menu_<<"Cancel transfer";menuCommands_<<"files-cancel";}
    emit changed();
}
void SocialController::communityInfo(){
    if(communityId_.isEmpty())return;
    closeMenu();menuGuild_=communityId_;menuChannel_.clear();menuMode_="community-info";menuTitle_=communityTitle();menuFocus_=0;
    menu_={"Invite link"};menuCommands_={"community-invite"};
    const bool owner=snapshot_["communityOwner"].toBool();
    menu_<<(owner?"Delete community":"Leave community");menuCommands_<<(owner?"ask-delete-community":"ask-leave-community");emit changed();
}
bool SocialController::dispatchConversationControls(Action action) {
    if(face_!="chats"&&face_!="communities")return false;
    if(action==Action::Back||action==Action::Left) {enterConversationPerson_.clear();enterConversationChannel_.clear();}
    if(!reading_||!conversation())return false;
    const bool hasCall=!snapshot_["voice"].toMap()["channel"].toString().isEmpty();
    if(!hasCall&&conversationControl_>=3&&conversationControl_<=5)conversationControl_=0;
    if(action==Action::Back){reading_=false;conversationControl_=-1;return true;}
    if(action==Action::ChangeView){conversationControl_=11;compose();return true;}
    if(action==Action::ToggleFavorite){conversationProfile();return true;}
    if(conversationControl_<0) {
        if(action==Action::Down&&messageFocus_>=messages().size()-1){conversationControl_=11;return true;}
        if(action==Action::Up&&messageFocus_==0){conversationControl_=hasCall?3:0;return true;}
        if(action==Action::Confirm){
            const auto message=messages().value(messageFocus_).toMap();openMenu();
            if(message["attachments"].toList().size()==1&&!menuCommands_.isEmpty()&&menuCommands_.first().startsWith("attachment:"))selectMenu(0);
            return true;
        }
        return false;
    }
    const bool header=conversationControl_<10;
    const bool callControls=conversationControl_>=3&&conversationControl_<=5;
    if(action==Action::Left||action==Action::Right) {
        const int first=callControls?3:header?0:10,last=callControls?5:header?2:14,step=action==Action::Left?-1:1;
        if(conversationControl_==first&&step<0){reading_=false;conversationControl_=-1;return true;}
        conversationControl_=qBound(first,conversationControl_+step,last);
        if(header&&conversationControl_==1&&(!currentChat()["guild"].toString().isEmpty()||!snapshot_["voice"].toMap()["available"].toBool()))conversationControl_+=step;
        if(conversationControl_==13&&!snapshot_["voice"].toMap()["channel"].toString().isEmpty())conversationControl_+=step;
        return true;
    }
    if(action==Action::Up||action==Action::Down) {
        if(callControls&&action==Action::Up){conversationControl_=0;return true;}
        if(hasCall&&header&&!callControls&&action==Action::Down){conversationControl_=3;return true;}
        if(header&&action==Action::Up){if(snapshot_["historyMore"].toBool())earlierMessages();return true;}
        if(!header&&action==Action::Down)return true;
        if(messages().isEmpty())conversationControl_=header?11:0;
        else {conversationControl_=-1;messageFocus_=header?0:messages().size()-1;}
        return true;
    }
    if(action==Action::ContextMenu||action==Action::LocalAction) {if(header)contextRow(-1);else composerTools();return true;}
    if(action!=Action::Confirm)return false;
    switch(conversationControl_) {
    case 0:conversationProfile();break;
    case 1:call();break;
    case 2:contextRow(-1);break;
    case 3:controlCall("voice-mute");break;
    case 4:controlCall("voice-output");break;
    case 5:controlCall("voice-leave");break;
    case 10:attachFile();break;
    case 11:compose();break;
    case 12:emoji();break;
    case 13:recordVoice();break;
    case 14:sendDraft();break;
    }
    return true;
}
}
