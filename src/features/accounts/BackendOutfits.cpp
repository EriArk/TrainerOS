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
        if(!value[key].isArray()||value[key].toArray().size()>64)return false;
        QSet<QString> ids;
        for(const auto entry:value[key].toArray()) {
            const auto row=entry.toObject();const auto id=row["id"].toString();const auto match=idPattern.match(id);
            bool revisionOk=false;const auto revision=row["revision"].toString().toInt(&revisionOk);
            const int slot=row["slot_number"].toInt();const auto price=row["price"].toDouble();
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
    if(value.contains("outfits")&&(!value["outfits"].isArray()||value["outfits"].toArray().size()>64))return false;
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
    if(shopBusy_||!shopPending_.isEmpty()||state_!="connected")return;
    if(shopStale_){refreshShop();return;}
    bool owned=false;for(const auto entry:outfits()["items"].toList())owned|=entry.toMap()["id"]==id;
    if(!owned||!AvatarAssets::valid(recipe["appearance"].toObject()))return;
    shopBusy_=true;shopStatus_="Saving your look…";emit changed();
    request("PUT","/v1/me/inventory/outfits/"+id,{{"name",name},{"recipe",recipe},{"revision",revision}},[this](int status,QJsonObject result){
        shopBusy_=false;
        if(status==200&&adoptShop(result)){saveCommerce();shopStatus_="Look saved. Ready on your other devices too.";}
        else {shopStale_=true;shopStatus_=result["code"]=="outfit_changed"?"This slot changed on another device. Refresh before replacing it.":"Save not confirmed. Refresh to check the slot before trying again.";}
        emit changed();
    },access_);
}
}
