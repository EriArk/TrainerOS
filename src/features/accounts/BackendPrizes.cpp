#include "BackendAccount.h"
#include <QJsonArray>
#include <QUuid>
#include <QSet>
#include <cmath>

namespace trainer {
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
    prizeSnapshot_=value;prizeStale_=cached;
    prizeIndex_=qBound(0,prizeIndex_,qMax(0,int(value["items"].toArray().size())-1));return true;
}
QVariantMap BackendAccount::prizes() const {
    auto result=prizeSnapshot_.toVariantMap();
    result["busy"]=prizeBusy_;result["stale"]=prizeStale_;result["status"]=prizeStatus_;
    result["connected"]=state_=="connected";result["index"]=prizeIndex_;
    result["selected"]=prizeSnapshot_["items"].toArray().toVariantList().value(prizeIndex_).toMap();
    result["celebrating"]=!celebrationIds_.isEmpty();result["celebrationCount"]=celebrationIds_.size();
    return result;
}
void BackendAccount::selectPrize(int index) {
    prizeIndex_=qBound(0,index,qMax(0,int(prizeSnapshot_["items"].toArray().size())-1));emit changed();
}
void BackendAccount::refreshPrizes(bool earlier) {
    if(prizeBusy_||state_!="connected"||player_.isEmpty())return;
    auto path=QString("/v1/me/prizes");
    if(earlier) {
        bool ok=false;const auto cursor=prizeSnapshot_["next_before"].toString();cursor.toULongLong(&ok);
        if(!ok)return;path+="?before="+cursor;
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
    celebrationIds_={};
    for(const auto v:prizeSnapshot_["waiting"].toArray())celebrationIds_.append(v.toObject()["id"]);
    emit changed();
}
void BackendAccount::closeCelebration(bool acknowledge) {
    const auto ids=celebrationIds_;celebrationIds_={};emit changed();
    if(!acknowledge||ids.isEmpty())return;
    if(state_!="connected"||prizeBusy_){prizeStatus_="Rewards are kept. Reconnect to mark the presentation as seen.";emit changed();return;}
    prizeBusy_=true;
    request("POST","/v1/me/prizes/seen",{{"ids",ids}},[this](int status,QJsonObject value){
        prizeBusy_=false;
        if(status==200&&adoptPrizes(value)){prizeStatus_.clear();saveCommerce();}
        else prizeStatus_="Rewards are kept. The presentation can be opened again.";
        emit changed();
    },access_);
}
void BackendAccount::togglePrizeShelf() {
    if(prizeBusy_||state_!="connected"||prizeStale_)return;
    const auto selected=QJsonObject::fromVariantMap(prizeSnapshot_["items"].toArray().toVariantList().value(prizeIndex_).toMap());
    if(selected["kind"]!="beaten"&&selected["kind"]!="completed"&&selected["kind"]!="hardcore")return;
    const auto game=selected["game_id"];QJsonArray games;bool found=false;
    for(const auto v:prizeSnapshot_["shelf"].toArray()) {
        if(v.toObject()["game_id"]==game)found=true;else games.append(v.toObject()["game_id"]);
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
