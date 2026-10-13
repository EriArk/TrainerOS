#include "SocialController.h"

namespace trainer {
QVariantList SocialController::recentPeople() const {
    auto rows=recentTogether_.rows();
    for(auto& value:rows) {
        auto row=value.toMap();int type=0;
        for(const auto& v:snapshot_["friends"].toList())if(v.toMap()["id"]==row["id"])type=v.toMap()["type"].toInt();
        row["type"]=type;row["friendLabel"]=type==1?"Friends":type==2?"Blocked":type==3?"Accept request":type==4?"Request sent":"Add friend";
        row["canAdd"]=snapshot_["state"]=="connected"&&type!=1&&type!=2&&type!=4;value=row;
        const auto contact=snapshot_["temporaryContacts"].toMap()["peers"].toMap()[row["id"].toString()].toMap();
        const auto error=snapshot_["temporaryContacts"].toMap()["error"].toString();
        row["contactStatus"]=error.isEmpty()?contact["status"]:error;
        if(contact["temporary"].toBool()&&type!=2) {
            row["friendLabel"]=contact["canKeep"].toBool()?"Keep as friend":contact["pendingKeep"].toBool()?"Keeping friendship…":"Please wait…";
            row["canAdd"]=error.isEmpty()&&snapshot_["state"]=="connected"&&type==1&&contact["canKeep"].toBool();
        }
        value=row;
    }
    return rows;
}
int SocialController::recentFocus() const {
    const auto rows=recentPeople();for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]==recentSelection_)return i;return 0;
}
void SocialController::selectRecent(int index) {
    const auto rows=recentPeople();if(index<0||index>=rows.size())return;
    recentSelection_=rows[index].toMap()["id"].toString();emit changed();
}
void SocialController::recentAction(QString action) {
    const auto row=recentPeople().value(recentFocus()).toMap();if(row.isEmpty())return;
    const auto id=row["id"].toString();
    if(action=="forget") {recentTogether_.forget(id);recentSelection_.clear();emit changed();return;}
    if(snapshot_["state"]!="connected")return;
    if(action=="profile")profile(id);
    if(action=="friend"&&row["canAdd"].toBool())emit commandRequested("recent-add",{{"id",id}});
}
void SocialController::dispatchRecent(Action action) {
    if(action==Action::Up||action==Action::Down)selectRecent(qBound(0,recentFocus()+(action==Action::Down?1:-1),qMax(0,int(recentPeople().size())-1)));
    else if(action==Action::Confirm)recentAction("profile");
    else if(action==Action::Secondary)recentAction("friend");
    else if((action==Action::ContextMenu||action==Action::LocalAction)&&!recentPeople().isEmpty()) {
        menuMode_="recent";menuTitle_=recentPeople()[recentFocus()].toMap()["name"].toString();menuDetail_.clear();menuFocus_=0;
        menu_={"Remove from this list","Back"};menuCommands_={"recent-forget","cancel"};emit changed();
    }
}
}
