#include "BackendAccount.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QUuid>
#include <QSet>
#include <cmath>

namespace trainer {
namespace {
bool identity(const QString& id){return id.size()==36&&!QUuid(id).isNull();}
bool whole(QJsonValue value,double min=0){return value.isDouble()&&std::isfinite(value.toDouble())&&value.toDouble()>=min&&value.toDouble()<=2147483647&&std::floor(value.toDouble())==value.toDouble();}
bool appearance(const QString& id){return QStringList{"aurora","dusk","orbit"}.contains(id);}
}
QString BackendAccount::commercePath() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/commerce/"+key_+".json";
}
bool BackendAccount::saveCommerce() {
    if(key_.isEmpty())return false;
    const auto data=QJsonDocument(QJsonObject{{"prizes",prizeSnapshot_},{"snapshot",shopSnapshot_},{"pending",shopPending_},{"wardrobe",wardrobeState_},{"outfitWrites",outfitWrites_}}).toJson(QJsonDocument::Compact);
    if(services_.writeCommerce)return services_.writeCommerce(key_,data);
    const auto path=commercePath(),directory=QFileInfo(path).absolutePath();
    if(QFileInfo(directory).isSymLink()||QFileInfo(path).isSymLink()||!QDir().mkpath(directory))return false;
    QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))return false;
    file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    return file.write(data)==data.size()&&file.commit();
}
void BackendAccount::loadCommerce() {
    QByteArray bytes;
    if(services_.readCommerce)bytes=services_.readCommerce(key_);
    else {QFile file(commercePath());if(!QFileInfo(file).isSymLink()&&file.size()<=2097152&&file.open(QIODevice::ReadOnly))bytes=file.readAll();}
    if(bytes.size()>2097152)return;
    const auto data=QJsonDocument::fromJson(bytes).object();
    adoptShop(data["snapshot"].toObject(),true);
    adoptPrizes(data["prizes"].toObject(),true);
    wardrobeState_=data["wardrobe"].toObject();
    outfitWrites_=data["outfitWrites"].toObject();
    const auto pending=data["pending"].toObject();
    if(identity(pending["player_id"].toString())&&identity(pending["operation_id"].toString())
        &&(pending["offer_id"].toString().startsWith("profile.")||pending["offer_id"].toString().startsWith("outfit.slot."))&&pending["offer_id"].toString().size()<=64
        &&whole(pending["offer_revision"],1)&&whole(pending["expected_price"],1))shopPending_=pending;
}
bool BackendAccount::adoptShop(const QJsonObject& value,bool cached) {
    const auto player=value["player_id"].toString();bool revisionOk=false;const auto revision=value["revision"].toString().toULongLong(&revisionOk);
    if(!identity(player)||(!cached&&player!=player_)||!revisionOk||!value["items"].isArray()||!value["catalogue"].isArray()
        ||value["items"].toArray().size()>100||value["catalogue"].toArray().size()>100)return false;
    if(shopSnapshot_["player_id"]==player&&revision<shopSnapshot_["revision"].toString().toULongLong())return false;
    for(const auto name:{"items","catalogue"}) {
        QSet<QString> ids;
        for(const auto row:value[name].toArray()) {
            const auto item=row.toObject();const auto id=item["id"].toString();bool ok=false;item["revision"].toString().toInt(&ok);
            if(!appearance(item["appearance"].toString())||id!="profile."+item["appearance"].toString()||ids.contains(id)||!ok
                ||!whole(item["price"],1)||!whole(item["minimum_level"],1)||item["kind"]!="permanent_license"
                ||item["title"].toString().isEmpty()||item["title"].toString().size()>128||item["description"].toString().size()>512)return false;
            if(QString(name)=="items"&&!identity(item["purchase_id"].toString()))return false;
            ids.insert(id);
        }
    }
    const auto equipped=value["equipped"].toString();bool owned=equipped.isEmpty();
    for(const auto row:value["items"].toArray())owned|=row.toObject()["id"]==equipped;
    if(!owned||!validOutfits(value))return false;
    shopSnapshot_=value;shopSnapshot_.remove("receipt");shopStale_=cached;
    shopIndex_=qBound(0,shopIndex_,qMax(0,int(shopItems().size())-1));return true;
}
QVariantList BackendAccount::shopItems() const {
    QVariantList result{QVariantMap{{"id",""},{"title","Classic"},{"description","Your original TrainerOS scene. Always free."},{"appearance",""},{"owned",true},{"price",0},{"equipped",shopSnapshot_["equipped"].toString().isEmpty()}}};
    QJsonArray rows=shopSnapshot_["catalogue"].toArray();QSet<QString> owned,listed;
    for(const auto row:rows)listed.insert(row.toObject()["id"].toString());
    for(const auto row:shopSnapshot_["items"].toArray()) {const auto id=row.toObject()["id"].toString();owned.insert(id);if(!listed.contains(id))rows.append(row);}
    for(const auto row:rows){auto value=row.toObject().toVariantMap();value["owned"]=owned.contains(value["id"].toString());value["equipped"]=value["id"].toString()==shopSnapshot_["equipped"].toString();result.append(value);}
    return result;
}
QString BackendAccount::profileAppearance() const {
    const auto equipped=shopSnapshot_["equipped"].toString();
    for(const auto item:shopSnapshot_["items"].toArray())if(item.toObject()["id"]==equipped)return item.toObject()["appearance"].toString();
    return {};
}
QVariantMap BackendAccount::shop() const {
    const auto rows=shopItems();const auto selected=rows.value(shopIndex_).toMap();
    const bool connected=state_=="connected"&&!player_.isEmpty();
    const bool pending=!shopPending_.isEmpty()&&shopPending_["player_id"]==player_;
    QString action=shopBusy_?"Please wait…":!connected?"Sign in to TrainerOS":pending?"Resolve pending purchase":selected["equipped"].toBool()?"Applied":selected["owned"].toBool()?"Apply appearance":"Review purchase";
    return {{"items",rows},{"selected",selected},{"index",shopIndex_},{"busy",shopBusy_},{"stale",shopStale_},{"connected",connected},
        {"review",shopReview_.toVariantMap()},{"pending",pending},{"status",shopStatus_},{"action",action}};
}
void BackendAccount::selectShopItem(int index){if(shopBusy_||!shopReview_.isEmpty())return;shopIndex_=qBound(0,index,qMax(0,int(shopItems().size())-1));emit changed();}
void BackendAccount::cancelPurchaseReview(){shopReview_={};emit changed();}
void BackendAccount::refreshShop() {
    if(shopBusy_)return;
    if(state_!="connected"||player_.isEmpty()) {
        shopBusy_=true;emit changed();
        request("GET","/v1/shop/items",{},[this](int status,QJsonObject value){
            shopBusy_=false;
            if(status==200 && value["catalogue"].isArray() && value["catalogue"].toArray().size()<=100) {
                QJsonArray safe;
                for(const auto row:value["catalogue"].toArray()) {
                    const auto item=row.toObject();
                    if(appearance(item["appearance"].toString())&&item["id"]=="profile."+item["appearance"].toString()
                        &&whole(item["price"],1)&&item["kind"]=="permanent_license")safe.append(row);
                }
                shopSnapshot_["catalogue"]=safe;
                QJsonObject catalogue{{"outfit_catalogue",value["outfit_catalogue"]}};
                if(value.contains("outfit_catalogue")&&validOutfits(catalogue))shopSnapshot_["outfit_catalogue"]=value["outfit_catalogue"];
            }
            shopStatus_="Browse freely. Sign in to buy and restore your permanent items.";emit changed();
        });return;
    }
    if(!shopSnapshot_.isEmpty()&&shopSnapshot_["player_id"]!=player_){shopSnapshot_={};shopIndex_=0;saveCommerce();}
    if(!shopPending_.isEmpty()&&shopPending_["player_id"]==player_){recoverPurchase();return;}
    shopBusy_=true;emit changed();
    request("GET","/v1/me/inventory",{},[this](int status,QJsonObject value){
        shopBusy_=false;
        if(status==200&&adoptShop(value)){saveCommerce();shopStatus_="Permanent purchases belong to your account.";}
        else {shopStale_=true;shopStatus_="Could not refresh your items. Your last applied appearance remains available offline.";}
        emit changed();
    },access_);
}
void BackendAccount::activateShopItem() {
    if(shopBusy_)return;
    if(state_!="connected"){activate();return;}
    if(!shopPending_.isEmpty()) {
        if(shopPending_["player_id"]==player_)recoverPurchase(true);
        else {shopStatus_="A purchase belongs to a previous sign-in. Restore that account to resolve it first.";emit changed();}
        return;
    }
    if(shopStale_){refreshShop();return;}
    const auto selected=QJsonObject::fromVariantMap(shopItems().value(shopIndex_).toMap());
    if(selected["equipped"].toBool())return;
    if(!selected["owned"].toBool()) {shopReview_=selected;emit changed();return;}
    shopBusy_=true;emit changed();
    request("PUT","/v1/me/inventory/appearance",{{"offer_id",selected["id"].toString().isEmpty()?QJsonValue():selected["id"]},{"revision",shopSnapshot_["revision"]}},[this](int status,QJsonObject result){
        shopBusy_=false;
        if(status==200&&adoptShop(result)){saveCommerce();shopStatus_="Appearance applied.";}
        else {shopStale_=true;shopStatus_="Your inventory changed or the connection failed. Refresh before applying.";}
        emit changed();
    },access_);
}
void BackendAccount::confirmPurchase() {
    if(shopBusy_||shopReview_.isEmpty()||state_!="connected"||!shopPending_.isEmpty())return;
    shopPending_={{"player_id",player_},{"operation_id",QUuid::createUuid().toString(QUuid::WithoutBraces)},
        {"offer_id",shopReview_["id"]},{"offer_revision",shopReview_["revision"].toString().toInt()},{"expected_price",shopReview_["price"]}};
    if(!saveCommerce()){shopPending_={};shopStatus_="Could not save purchase recovery data. Nothing was sent or charged.";emit changed();return;}
    shopReview_={};recoverPurchase(true);
}
void BackendAccount::recoverPurchase(bool retry) {
    if(shopBusy_||state_!="connected"||shopPending_["player_id"]!=player_)return;
    shopBusy_=true;shopStatus_="Checking your purchase…";emit changed();
    auto body=shopPending_;body.remove("player_id");
    request(retry?"POST":"GET",retry?"/v1/me/purchases":"/v1/me/purchases/"+body["operation_id"].toString(),retry?body:QJsonObject{},
        [this](int status,QJsonObject result){finishPurchase(status,std::move(result));},access_);
}
void BackendAccount::finishPurchase(int status,QJsonObject result) {
    shopBusy_=false;
    const auto receipt=result["receipt"].toObject();
    const bool exact=receipt["player_id"]==player_&&receipt["operation_id"]==shopPending_["operation_id"]&&receipt["offer_id"]==shopPending_["offer_id"]
        &&receipt["price"]==shopPending_["expected_price"]&&receipt["offer_revision"]==shopPending_["offer_revision"]&&identity(receipt["id"].toString());
    if(status==200&&exact&&adoptShop(result)) {
        const auto pending=shopPending_;shopPending_={};
        if(!saveCommerce())shopPending_=pending;
        shopStatus_=receipt["offer_id"].toString().startsWith("outfit.slot.")?"Slot unlocked permanently. Save your current look into it.":"Purchased permanently. Select Apply to use this appearance.";
        refreshProgression(false);
    } else if(status==409&&QStringList{"offer_changed","offer_unavailable","already_owned","insufficient_sparks","level_required"}.contains(result["code"].toString())) {
        const auto code=result["code"].toString();const auto pending=shopPending_;shopPending_={};
        if(!saveCommerce())shopPending_=pending;
        shopStale_=true;
        shopStatus_=code=="insufficient_sparks"?"Not enough Sparks. No charge was made.":code=="already_owned"?"Already owned. No second charge was made.":"The offer changed or is unavailable. No charge was made. Refresh and review it again.";
        refreshProgression(false);
    } else {shopStale_=true;shopStatus_="Purchase not yet confirmed. Resolve it to check or retry the same operation; no second purchase will be created.";}
    emit changed();
}
}
