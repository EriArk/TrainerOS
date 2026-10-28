#include "BackendAccount.h"
#include "features/trainer/AvatarAssets.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <QUuid>
#include <QSet>
#include <algorithm>

namespace trainer {
bool BackendAccount::validOutfits(const QJsonObject& value) const {
    QSet<QString> owned;
    static const QRegularExpression idPattern("^outfit\\.slot\\.([1-9][0-9]?)$");
    for(const auto key:{"outfit_catalogue","outfit_items"}) {
        if(!value.contains(key))continue; // Older servers have no slots.
        if(!value[key].isArray()||value[key].toArray().size()>4096)return false;
        QSet<QString> ids;
        for(const auto entry:value[key].toArray()) {
            const auto row=entry.toObject();const auto id=row["id"].toString();const auto match=idPattern.match(id);
            bool revisionOk=false;const auto revision=row["revision"].toString().toInt(&revisionOk);
            const int slot=row["slot_number"].toInt();const auto price=row["price"].toDouble();
            if(id.startsWith("outfit.level.")) {
                bool levelOk=false;const int level=id.mid(13).toInt(&levelOk);
                if(QString(key)!="outfit_items"||!levelOk||level<3||level%3||slot!=64+level||ids.contains(id)
                    ||row["purpose"]!="outfit_slot"||row["kind"]!="permanent_license"||price!=0||!revisionOk||revision<1
                    ||QUuid(row["grant_id"].toString()).isNull()||row["title"].toString().isEmpty()||row["title"].toString().size()>128)return false;
                owned.insert(id);ids.insert(id);continue;
            }
            if(!match.hasMatch()||slot<1||slot>64||slot!=match.captured(1).toInt()||ids.contains(id)
                ||row["purpose"]!="outfit_slot"||row["kind"]!="permanent_license"||!revisionOk||revision<1
                ||price<1||price>2147483647||price!=qint64(price)||row["title"].toString().isEmpty()||row["title"].toString().size()>128)return false;
            if(QString(key)=="outfit_items") {
                if(QUuid(row["purchase_id"].toString()).isNull())return false;
                owned.insert(id);
            }
            ids.insert(id);
        }
    }
    if(value.contains("outfits")&&(!value["outfits"].isArray()||value["outfits"].toArray().size()>4096))return false;
    QSet<QString> saved;
    for(const auto entry:value["outfits"].toArray()) {
        const auto row=entry.toObject();const auto id=row["offer_id"].toString();bool ok=false;
        const auto revision=row["revision"].toString().toULongLong(&ok);
        const auto recipe=row["recipe"].toObject();
        if(!owned.contains(id)||saved.contains(id)||!ok||revision==0||row["player_id"]!=value["player_id"]
            ||row["name"].toString().isEmpty()||row["name"].toString().size()>64||recipe["schema"]!=1
            ||!QRegularExpression("^[a-f0-9]{64}$").match(recipe["packRevision"].toString()).hasMatch()
            ||!AvatarAssets::valid(recipe["appearance"].toObject()))return false;
        saved.insert(id);
    }
    return true;
}
QVariantMap BackendAccount::outfits() const {
    QVariantList rows;QSet<QString> owned;QVariantMap next;
    const bool same=player_.isEmpty()||shopSnapshot_["player_id"]==player_;
    if(same)for(const auto entry:shopSnapshot_["outfit_items"].toArray()) {
        auto row=entry.toObject();owned.insert(row["id"].toString());
        for(const auto saved:shopSnapshot_["outfits"].toArray())if(saved.toObject()["offer_id"]==row["id"])row["saved"]=saved;
        const auto local=outfitWrites_[row["id"].toString()].toObject();
        if(local["player_id"]==shopSnapshot_["player_id"] && AvatarAssets::valid(local["recipe"].toObject()["appearance"].toObject())) {
            row["saved"]=local;row["local"]=true;row["conflict"]=local["conflict"].toBool();
        }
        rows.append(row.toVariantMap());
    }
    std::sort(rows.begin(),rows.end(),[](const QVariant& a,const QVariant& b){return a.toMap()["slot_number"].toInt()<b.toMap()["slot_number"].toInt();});
    for(const auto entry:shopSnapshot_["outfit_catalogue"].toArray()) {
        const auto row=entry.toObject();if(!owned.contains(row["id"].toString())){next=row.toVariantMap();break;}
    }
    return {{"items",rows},{"next",next},{"busy",shopBusy_},{"stale",shopStale_},
        {"connected",state_=="connected"&&!player_.isEmpty()},{"status",shopStatus_},
        {"review",shopReview_.toVariantMap()},{"pending",!shopPending_.isEmpty()}};
}
void BackendAccount::reviewOutfitSlot() {
    if(shopBusy_)return;
    if(state_!="connected"){activate();return;}
    if(!shopPending_.isEmpty()) {
        if(shopPending_["player_id"]==player_)recoverPurchase(true);
        else {shopStatus_="A purchase belongs to a previous sign-in. Restore that account to resolve it first.";emit changed();}
        return;
    }
    if(shopStale_){refreshShop();return;}
    shopReview_=QJsonObject::fromVariantMap(outfits()["next"].toMap());emit changed();
}
void BackendAccount::saveOutfit(const QString& id,const QString& name,const QJsonObject& recipe,const QString& revision) {
    queueOutfit(id,name,recipe,revision);
}
bool BackendAccount::storeWardrobeState(const QJsonObject& value) {
    const auto previous=wardrobeState_;wardrobeState_=value;
    if(saveCommerce())return true;
    wardrobeState_=previous;return false;
}
bool BackendAccount::queueOutfit(const QString& id,const QString& name,const QJsonObject& recipe,const QString& revision,const QJsonObject& wardrobe) {
    bool owned=false;for(const auto entry:outfits()["items"].toList())owned|=entry.toMap()["id"]==id;
    if(!owned||!AvatarAssets::valid(recipe["appearance"].toObject())||name.trimmed().isEmpty()||name.size()>64)return false;
    const auto previous=outfitWrites_,previousState=wardrobeState_;auto pending=outfitWrites_[id].toObject();
    const auto base=pending.isEmpty()?revision:pending["revision"].toString();
    pending={{"offer_id",id},{"name",name.trimmed()},{"recipe",recipe},{"revision",base},
        {"player_id",shopSnapshot_["player_id"]},{"conflict",pending["conflict"].toBool()}};
    outfitWrites_[id]=pending;
    if(!wardrobe.isEmpty())wardrobeState_=wardrobe;
    if(!saveCommerce()){outfitWrites_=previous;wardrobeState_=previousState;shopStatus_="Could not save this look on the device.";emit changed();return false;}
    shopStatus_="Saved on this device";emit changed();flushOutfits();return true;
}
void BackendAccount::flushOutfits() {
    if(outfitFlushing_||shopBusy_||shopStale_||!shopPending_.isEmpty()||state_!="connected")return;
    QString id;QJsonObject pending;
    for(auto it=outfitWrites_.begin();it!=outfitWrites_.end();++it) {
        const auto candidate=it.value().toObject();
        if(candidate["player_id"]==player_&&!candidate["conflict"].toBool()) {id=it.key();pending=candidate;break;}
    }
    if(id.isEmpty()||!AvatarAssets::valid(pending["recipe"].toObject()["appearance"].toObject()))return;
    bool owned=false;for(const auto row:shopSnapshot_["outfit_items"].toArray())owned|=row.toObject()["id"]==id;
    if(!owned)return;
    QJsonObject remote;for(const auto row:shopSnapshot_["outfits"].toArray())if(row.toObject()["offer_id"]==id)remote=row.toObject();
    if(remote["recipe"]==pending["recipe"]&&remote["name"]==pending["name"]) {
        outfitWrites_.remove(id);saveCommerce();emit changed();return;
    }
    if(remote.value("revision").toString("0")!=pending["revision"].toString()) {
        pending["conflict"]=true;outfitWrites_[id]=pending;saveCommerce();
        shopStatus_="This set changed on another device. Choose which version to keep.";emit changed();return;
    }
    outfitFlushing_=true;shopBusy_=true;
    request("PUT","/v1/me/inventory/outfits/"+id,{{"name",pending["name"]},{"recipe",pending["recipe"]},{"revision",pending["revision"]}},[this,id,pending](int status,QJsonObject result){
        outfitFlushing_=false;shopBusy_=false;
        if(status==200&&adoptShop(result)) {
            auto latest=outfitWrites_[id].toObject();
            if(latest==pending)outfitWrites_.remove(id);
            else {for(const auto row:result["outfits"].toArray())if(row.toObject()["offer_id"]==id)latest["revision"]=row.toObject()["revision"];outfitWrites_[id]=latest;}
            saveCommerce();shopStatus_=outfitWrites_.isEmpty()?"Saved":"Saved on this device";
        }else {shopStale_=true;shopStatus_="Saved on this device. Reconnect or refresh to sync.";}
        emit changed();
    },access_);
}
void BackendAccount::resolveOutfitConflict(const QString& id,bool keepLocal) {
    auto pending=outfitWrites_[id].toObject();if(pending.isEmpty()||pending["player_id"]!=player_)return;
    if(keepLocal){
        QJsonObject remote;for(const auto row:shopSnapshot_["outfits"].toArray())if(row.toObject()["offer_id"]==id)remote=row.toObject();
        pending["revision"]=remote.value("revision").toString("0");pending["conflict"]=false;outfitWrites_[id]=pending;
    }else outfitWrites_.remove(id);
    saveCommerce();emit changed();
}
}
