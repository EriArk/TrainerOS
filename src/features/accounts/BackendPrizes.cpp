#include "BackendAccount.h"
#include <QJsonArray>
#include <QUuid>
#include <QSet>
#include <cmath>

namespace trainer {
namespace {
QJsonArray grouped(const QJsonArray& rows,bool mergeLevels=false) {
    QJsonArray result;QHash<QString,int> positions;
    for(const auto v:rows) {
        const auto row=v.toObject();
        const auto key=row["game_id"].toInt()>0?"game:"+QString::number(row["game_id"].toInt()):
            row["level"].toInt()>0?(mergeLevels?QString("levels"):"level:"+QString::number(row["level"].toInt())):row["id"].toString();
        if(!positions.contains(key)){positions[key]=result.size();result.append(QJsonObject{{"key",key},{"grants",QJsonArray{}}});}
        auto group=result[positions[key]].toObject();auto grants=group["grants"].toArray();grants.append(row);group["grants"]=grants;result[positions[key]]=group;
    }
    return result;
}
QJsonObject describe(QJsonObject group) {
    auto grants=group["grants"].toArray();if(grants.isEmpty())return {};
    auto result=grants.first().toObject();QStringList kinds;bool trophy=false,slot=false;int lowest=0,highest=0;
    for(const auto v:grants) {
        const auto row=v.toObject();const auto kind=row["kind"].toString();
        if(!kinds.contains(kind))kinds.append(kind);
        slot|=kind=="outfit_slot";
        const int level=row["level"].toInt();if(level>0){lowest=lowest?qMin(lowest,level):level;highest=qMax(highest,level);}
        if(kind=="beaten"||kind=="completed"||kind=="hardcore") {
            trophy=true;
            if(result["kind"]=="clothing"||kind=="hardcore"||(kind=="completed"&&result["kind"]!="hardcore"))result=row;
        }
    }
    if(lowest)result["title"]=lowest==highest?"Level "+QString::number(lowest):QString("Levels %1–%2").arg(lowest).arg(highest);
    result["grants"]=grants;result["kinds"]=QJsonArray::fromStringList(kinds);result["trophy"]=trophy;result["slot"]=slot;
    return result;
}
QJsonArray collection(const QJsonObject& snapshot) {
    QJsonArray result;
    for(const auto v:snapshot.contains("collection")?snapshot["collection"].toArray():grouped(snapshot["items"].toArray()))result.append(describe(v.toObject()));
    return result;
}
}
bool BackendAccount::adoptPrizes(const QJsonObject& value,bool cached) {
    const auto player=value["player_id"].toString();bool ok=false;
    const auto revision=value["revision"].toString().toULongLong(&ok);
    if(QUuid(player).isNull()||(!cached&&player!=player_)||!ok)return false;
    if(prizeSnapshot_["player_id"]==player&&revision<prizeSnapshot_["revision"].toString().toULongLong())return false;
    auto integer=[](QJsonValue v,int min){return v.isDouble()&&std::isfinite(v.toDouble())&&v.toDouble()>=min&&v.toDouble()<=2147483647&&std::floor(v.toDouble())==v.toDouble();};
    if(!integer(value["pending"],0))return false;
    for(const auto key:{"items","waiting"}) {
        if(!value[key].isArray()||value[key].toArray().size()>100)return false;
        QSet<QString> ids;
        for(const auto v:value[key].toArray()) {
            const auto row=v.toObject();const auto id=row["id"].toString();
            if(QUuid(id).isNull()||ids.contains(id)||row["player_id"]!=player||row["title"].toString().isEmpty()
                ||row["title"].toString().size()>256||!QStringList{"clothing","outfit_slot","beaten","completed","hardcore"}.contains(row["kind"].toString())
                ||!QDateTime::fromString(row["earned_at"].toString(),Qt::ISODateWithMs).isValid())return false;
            ids.insert(id);
        }
    }
    if(!value["shelf"].isArray()||value["shelf"].toArray().size()>5||!value["upcoming"].isArray()||value["upcoming"].toArray().size()>3)return false;
    QSet<int> games;
    for(const auto v:value["shelf"].toArray()) {
        const auto row=v.toObject();const auto game=row["game_id"].toInt();
        if(!integer(row["game_id"],1)||games.contains(game)||row["title"].toString().size()>256)return false;
        games.insert(game);
    }
    for(const auto v:value["upcoming"].toArray())if(!integer(v.toObject()["level"],2)||!v.toObject()["slot"].isBool())return false;
    if(value.contains("collection")) {
        if(!value["collection"].isArray()||value["collection"].toArray().size()>100)return false;
        for(const auto v:value["collection"].toArray()) {
            const auto grants=v.toObject()["grants"].toArray();if(grants.isEmpty())return false;
            for(const auto g:grants){const auto row=g.toObject();if(row["player_id"]!=player||QUuid(row["id"].toString()).isNull())return false;}
        }
    }
    prizeSnapshot_=value;prizeStale_=cached;
    prizeIndex_=qBound(0,prizeIndex_,qMax(0,int(collection(value).size())-1));return true;
}
QVariantMap BackendAccount::prizes() const {
    auto result=prizeSnapshot_.toVariantMap();
    result["busy"]=prizeBusy_;result["stale"]=prizeStale_;result["status"]=prizeStatus_;
    result["connected"]=state_=="connected";result["index"]=prizeIndex_;
    const auto items=collection(prizeSnapshot_).toVariantList();result["items"]=items;
    result["selected"]=items.value(prizeIndex_).toMap();
    result["next_before"]=prizeSnapshot_.contains("collection")?prizeSnapshot_["collection_before"].toVariant():prizeSnapshot_["next_before"].toVariant();
    result["celebrating"]=!celebrationIds_.isEmpty();result["celebrationCount"]=celebrationIds_.size();
    result["ceremony"]=celebrationIds_.isEmpty()?QVariantMap{}:describe(celebrationGroups_.first().toObject()).toVariantMap();
    result["ceremonyRemaining"]=celebrationIds_.isEmpty()?0:celebrationGroups_.size();
    return result;
}
void BackendAccount::selectPrize(int index) {
    prizeIndex_=qBound(0,index,qMax(0,int(collection(prizeSnapshot_).size())-1));emit changed();
}
void BackendAccount::refreshPrizes(bool earlier) {
    if(prizeBusy_||state_!="connected"||player_.isEmpty())return;
    auto path=QString("/v1/me/prizes");
    if(earlier) {
        const bool groups=prizeSnapshot_.contains("collection");
        bool ok=false;const auto cursor=prizeSnapshot_[groups?"collection_before":"next_before"].toString();cursor.toULongLong(&ok);
        if(!ok)return;path+=(groups?"?group_before=":"?before=")+cursor;
    }
    prizeBusy_=true;emit changed();
    request("GET",path,{},[this](int status,QJsonObject value){
        prizeBusy_=false;
        if(status==200&&adoptPrizes(value)){prizeStatus_.clear();saveCommerce();}
        else {prizeStale_=true;prizeStatus_="Could not refresh rewards. Your collection is kept.";}
        emit changed();
    },access_);
}
void BackendAccount::celebratePrizes() {
    if(!celebrationIds_.isEmpty())return;
    celebrationGroups_=grouped(prizeSnapshot_["waiting"].toArray(),true);
    celebrationIds_={};
    for(const auto v:prizeSnapshot_["waiting"].toArray())celebrationIds_.append(v.toObject()["id"]);
    emit changed();
}
void BackendAccount::closeCelebration(bool acknowledge) {
    if(!acknowledge){celebrationIds_={};celebrationGroups_={};emit changed();return;}
    if(celebrationIds_.isEmpty()||prizeBusy_)return;
    QJsonArray ids;for(const auto v:celebrationGroups_.first().toObject()["grants"].toArray())ids.append(v.toObject()["id"]);
    if(state_!="connected"||prizeBusy_){prizeStatus_="Rewards are kept. Reconnect to mark the presentation as seen.";emit changed();return;}
    prizeBusy_=true;emit changed();
    request("POST","/v1/me/prizes/seen",{{"ids",ids}},[this,ids](int status,QJsonObject value){
        prizeBusy_=false;
        if(status==200&&adoptPrizes(value)){
            // A delayed reply must not advance a newly opened presentation.
            if(!celebrationGroups_.isEmpty()&&celebrationGroups_.first().toObject()["grants"].toArray().first().toObject()["id"]==ids.first()) {
                celebrationGroups_.removeAt(0);QJsonArray left;
                for(const auto id:celebrationIds_)if(!ids.contains(id))left.append(id);celebrationIds_=left;
            }
            prizeStatus_.clear();saveCommerce();
        }
        else prizeStatus_="Rewards are kept. The presentation can be opened again.";
        emit changed();
    },access_);
}
void BackendAccount::togglePrizeShelf(bool celebration) {
    if(prizeBusy_||state_!="connected"||prizeStale_)return;
    if(collection(prizeSnapshot_).isEmpty()&&(!celebration||celebrationIds_.isEmpty()))return;
    const auto selected=celebration&&!celebrationIds_.isEmpty()?describe(celebrationGroups_.first().toObject()):collection(prizeSnapshot_).at(prizeIndex_).toObject();
    if(selected["kind"]!="beaten"&&selected["kind"]!="completed"&&selected["kind"]!="hardcore")return;
    const auto game=selected["game_id"];QJsonArray games;bool found=false;
    for(const auto v:prizeSnapshot_["shelf"].toArray()) {
        if(v.toObject()["game_id"]==game){if(celebration)return;found=true;}else games.append(v.toObject()["game_id"]);
    }
    if(!found) {
        if(games.size()==5){prizeStatus_="Your shelf has five trophies. Remove one to make room.";emit changed();return;}
        games.append(game);
    }
    prizeBusy_=true;emit changed();
    request("PUT","/v1/me/prizes/shelf",{{"games",games}},[this](int status,QJsonObject value){
        prizeBusy_=false;
        if(status==200&&adoptPrizes(value)){prizeStatus_.clear();saveCommerce();}
        else {prizeStale_=true;prizeStatus_="Could not change your shelf. Refresh and try again.";}
        emit changed();
    },access_);
}
}
