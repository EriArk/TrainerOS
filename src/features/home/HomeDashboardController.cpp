#include "HomeDashboardController.h"
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <algorithm>

namespace trainer {
HomeDashboardController::HomeDashboardController(LibraryRepository& library,AchievementProvider& provider,SocialController& social,QObject* parent)
    :QObject(parent),library_(library),provider_(provider),social_(social) {
    connect(&social_,&SocialController::changed,this,[this,account=social.account()["userId"].toString()]() mutable {
        const auto next=social_.account()["userId"].toString();
        if(next!=account){account=next;focus_="play";}
        emit changed();
    });
    connect(&provider_,&AchievementProvider::snapshotChanged,this,&HomeDashboardController::changed);
}
QVariantList HomeDashboardController::chats() const {
    auto rows=social_.quickConversations();
    for(auto& value:rows){auto row=value.toMap();row["key"]="chat:"+row["id"].toString();row["type"]="chat";value=row;}
    return rows;
}
QVariantList HomeDashboardController::activities() const {
    auto rows=social_.notifications();
    for(auto& value:rows){auto row=value.toMap();row["key"]="event:"+row["id"].toString();row["type"]="event";value=row;}
    return rows;
}
QVariantList HomeDashboardController::achievements() const {
    QVariantList rows;const auto context=provider_.context();
    if(context.accountId.isEmpty())return rows;
    QSet<QString> games;for(const auto& game:library_.adventures())games.insert(game.id);
    for(const auto& set:provider_.sets()) {
        if(!set.supported || !games.contains(set.adventureId))continue;
        const auto snapshot=provider_.snapshot(set.id);
        if(snapshot.context!=context || snapshot.setId!=set.id
           || (snapshot.state!=AchievementState::Ready && snapshot.state!=AchievementState::Offline))continue;
        for(const auto& unlock:snapshot.unlocks) {
            if(unlock.unlocked!=true)continue;
            const auto definition=std::find_if(snapshot.definitions.cbegin(),snapshot.definitions.cend(),[&](const auto& d){return d.id==unlock.achievementId;});
            if(definition==snapshot.definitions.cend())continue;
            const auto identity=QJsonDocument(QJsonArray{context.providerId,context.accountId,set.id,definition->id}).toJson(QJsonDocument::Compact);
            const auto key="ra:"+QString::fromLatin1(QCryptographicHash::hash(identity,QCryptographicHash::Sha256).toHex());
            rows.append(QVariantMap{{"key",key},{"type","achievement"},{"id",definition->id},{"set",set.id},
                {"name",definition->title},{"game",set.title},{"description",definition->description},
                {"date",unlock.earnedAt},{"offline",snapshot.state==AchievementState::Offline},
                {"mode",unlock.mode==AchievementMode::Hardcore?"Hardcore":unlock.mode==AchievementMode::Standard?"Standard":"Mode unavailable"}});
        }
    }
    std::stable_sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.toMap()["date"].toDateTime()>b.toMap()["date"].toDateTime();});
    while(rows.size()>16)rows.removeLast();
    return rows;
}
QList<QStringList> HomeDashboardController::zones() const {
    QList<QStringList> result;for(const auto& rows:{chats(),activities(),achievements()}) {
        QStringList keys;for(const auto& row:rows)keys.append(row.toMap()["key"].toString());result.append(keys);
    }return result;
}
QVariantMap HomeDashboardController::target(const QString& key) const {
    if(key=="play")return {{"type","play"},{"key","play"}};
    for(const auto& rows:{chats(),activities(),achievements()})for(const auto& row:rows)if(row.toMap()["key"]==key)return row.toMap();
    return {}; // An expired focused item never becomes a new row at its old index.
}
void HomeDashboardController::select(const QString& key) {if(!target(key).isEmpty()){focus_=key;emit changed();}}
void HomeDashboardController::move(Action action) {
    const auto groups=zones();int zone=-1,index=-1;
    for(int i=0;i<groups.size();++i)if(groups[i].contains(focus_)){zone=i;index=groups[i].indexOf(focus_);break;}
    if(action==Action::Back){reset();return;}
    if(zone<0) {
        if(action==Action::Left || action==Action::Up || action==Action::Down)for(const auto& group:groups)if(!group.isEmpty()){select(group.first());return;}
        return;
    }
    if((zone==1 && action==Action::Right) || (zone!=1 && action==Action::Right && index==groups[zone].size()-1)) {reset();return;}
    if(zone!=1 && (action==Action::Left || action==Action::Right)) {
        select(groups[zone][std::clamp(index+(action==Action::Right?1:-1),0,int(groups[zone].size())-1)]);return;
    }
    if(action==Action::Up || action==Action::Down) {
        const int delta=action==Action::Down?1:-1;
        if(zone==1 && index+delta>=0 && index+delta<groups[zone].size()){select(groups[zone][index+delta]);return;}
        for(int next=zone+delta;next>=0 && next<groups.size();next+=delta)if(!groups[next].isEmpty()){select(delta>0?groups[next].first():groups[next].last());return;}
    }
}
}
