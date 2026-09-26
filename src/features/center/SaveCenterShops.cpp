#include "SaveCenterController.h"
#include <algorithm>

namespace trainer {
namespace {
QString section(const Merchant& m){return !m.discovered?QString("unknown"):m.section.isEmpty()?QString("marts"):m.section;}
bool lesson(const QString& kind){return kind=="tutor"||kind=="relearn";}
QString paymentText(const MerchantStock& stock){
    QStringList lines;for(const auto& p:stock.payments)lines.append(QString("%1 × %2").arg(p.quantity).arg(p.name));return lines.join(" + ");
}
const QStringList sectionIds{"marts","stores","specialists","exchanges","services","unknown"};
const QStringList sectionNames{"Poké Marts","Stores","Specialists","Exchanges","Services","Undiscovered"};
}
QVariantList SaveCenterController::shopCategories() const {
    QVariantList out;
    for(int i=0;i<sectionIds.size();++i){
        bool present=false;for(const auto& m:snapshot_.shops.merchants)if(section(m)==sectionIds[i])present=true;
        if(present)out.append(QVariantMap{{"id",sectionIds[i]},{"name",sectionNames[i]}});
    }
    return out;
}
void SaveCenterController::chooseShopCategory(int index){
    if(!shopsOpen_||busy()||shopRoute_!="merchants")return;
    const auto cats=shopCategories();if(index<0||index>=cats.size())return;
    shopCategory_=cats[index].toMap()["id"].toString();shopGroup_.clear();groupParentIndex_=merchantIndex_=stockIndex_=0;quantity_=1;shopMessage_.clear();emit changed();
}
void SaveCenterController::normalizeShopCategory(){
    const auto cats=shopCategories();
    for(const auto& c:cats)if(c.toMap()["id"].toString()==shopCategory_)return;
    if(cats.isEmpty())return;
    shopCategory_=cats[0].toMap()["id"].toString();shopGroup_.clear();
    groupParentIndex_=merchantIndex_=stockIndex_=0;quantity_=1;
}
QVariantList SaveCenterController::shopRecipients() const {
    QVariantList out;const auto m=merchant();if(!m||stockIndex_<0||stockIndex_>=m->stock.size())return out;
    for(const auto& r:m->stock[stockIndex_].recipients)out.append(QVariantMap{{"name",r.name},{"available",r.available},{"detail",r.available?QString("Can learn"):r.reason}});
    return out;
}
QVariantList SaveCenterController::shopMoves() const {
    QVariantList out;const auto m=merchant();if(!m||stockIndex_<0||stockIndex_>=m->stock.size())return out;
    const auto& list=m->stock[stockIndex_].recipients;if(recipientIndex_<0||recipientIndex_>=list.size())return out;
    for(const auto& move:list[recipientIndex_].moves)out.append(QVariantMap{{"name",move.name},{"available",move.available},{"detail",move.available?QString("Replace"):QString("Keep")}});
    return out;
}

int SaveCenterController::shopBalance() const {
    const auto m=merchant();return snapshot_.shops.supported&&m&&m->discovered&&!m->itemPayment?m->balance:-1;
}
QList<int> SaveCenterController::merchantRows() const {
    QList<int> out;QStringList groups;const auto& list=snapshot_.shops.merchants;
    for(int i=0;i<list.size();++i){const auto& m=list[i];
        if(section(m)!=shopCategory_)continue;
        if(!shopGroup_.isEmpty()){if(m.discovered&&m.group==shopGroup_)out.append(i);continue;}
        if(m.discovered&&!m.group.isEmpty()){if(groups.contains(m.group))continue;groups.append(m.group);}
        out.append(i);
    }
    return out;
}
const Merchant* SaveCenterController::merchant() const {
    const auto rows=merchantRows();
    return merchantIndex_>=0&&merchantIndex_<rows.size()?&snapshot_.shops.merchants[rows[merchantIndex_]]:nullptr;
}
QVariantList SaveCenterController::merchants() const {
    QVariantList out;
    for(int index:merchantRows()){const auto& m=snapshot_.shops.merchants[index];const bool group=shopGroup_.isEmpty()&&!m.group.isEmpty();
        out.append(QVariantMap{{"name",m.discovered?(group?m.group:m.name):QString("???")},
            {"detail",m.discovered?(group?QString("Departments"):m.available?m.category:QString("Currently unavailable")):QString("Undiscovered")},{"discovered",m.discovered}});
    }
    return out;
}
QVariantList SaveCenterController::shopStock() const {
    QVariantList out;const auto m=merchant();if(!m||!m->discovered)return out;
    for(const auto& s:m->stock)out.append(QVariantMap{{"name",s.name},{"price",s.price},{"owned",s.owned},{"maximum",m->available?s.maximum:0},{"pocket",s.pocket},{"payment",paymentText(s)}});
    return out;
}
QVariantMap SaveCenterController::shopSelection() const {
    QVariantMap out;const auto m=merchant();if(!m||!m->discovered)return out;
    out["name"]=shopGroup_.isEmpty()&&!m->group.isEmpty()?m->group:m->name;out["location"]=m->location;
    out["counter"]=m->name;
    switch(m->currency){
    case MerchantCurrency::Money:out["currency"]="₽";break;
    case MerchantCurrency::Coins:out["currency"]="Coins";break;
    case MerchantCurrency::BattlePoints:out["currency"]="BP";break;
    case MerchantCurrency::Ash:out["currency"]="Ash";break;
    case MerchantCurrency::BerryPowder:out["currency"]="Berry Powder";break;
    }
    if(stockIndex_>=0&&stockIndex_<m->stock.size()){
        const auto& s=m->stock[stockIndex_];out["item"]=s.name;out["total"]=s.price*quantity_;
        if(lesson(s.kind)&&recipientIndex_>=0&&recipientIndex_<s.recipients.size()){
            const auto& r=s.recipients[recipientIndex_];out["recipient"]=r.name;
            if(lessonMoveIndex_>=0&&lessonMoveIndex_<r.moves.size())out["oldMove"]=r.moves[lessonMoveIndex_].name;
        }
        out["lesson"]=lesson(s.kind);out["payment"]=paymentText(s);
        QStringList remaining;for(const auto& p:s.payments)remaining.append(QString("%1: %2 → %3").arg(p.name).arg(p.owned).arg(p.owned-p.quantity));
        out["paymentRemaining"]=remaining.join("\n");
        out["kind"]=s.kind;out["owned"]=s.owned;out["maximum"]=m->available?s.maximum:0;out["pocket"]=s.pocket;
    }
    return out;
}
QString SaveCenterController::shopMessage() const {
    if(busy())return shopRoute_=="confirm"?"Packing your purchase…":"Checking the shops…";
    if(!shopMessage_.isEmpty())return shopMessage_;
    const auto m=merchant();if(m&&!m->availability.isEmpty())return m->availability;
    if(!snapshot_.shops.error.isEmpty())return snapshot_.shops.error;
    if(!snapshot_.shops.supported)return snapshot_.error.isEmpty()?"Save in Pokémon Emerald, then visit again.":snapshot_.error;
    return {};
}
void SaveCenterController::visitShops(){
    if(!open_||!companion_||confirming_)return;
    shopGroup_.clear();groupParentIndex_=0;clinicOpen_=false;shopsOpen_=true;shopRoute_="merchants";shopMessage_.clear();merchantIndex_=stockIndex_=0;quantity_=1;
    normalizeShopCategory();
    refresh();emit changed();
}
void SaveCenterController::shopActivate(int index){
    if(!shopsOpen_||busy())return;
    if(shopRoute_=="merchants"){
        if(index<0||index>=merchantRows().size())return;
        merchantIndex_=index;stockIndex_=0;quantity_=1;const auto m=merchant();if(!m||!m->discovered)return;
        if(shopGroup_.isEmpty()&&!m->group.isEmpty()){groupParentIndex_=merchantIndex_;shopGroup_=m->group;merchantIndex_=0;}
        else {stockIndex_=0;quantity_=1;shopRoute_="stock";}
    }else if(shopRoute_=="stock"){
        const auto m=merchant();if(!m||!m->available||index<0||index>=m->stock.size())return;
        if(stockIndex_!=index)quantity_=1;
        stockIndex_=index;
        if(lesson(m->stock[index].kind)){
            for(const auto& p:m->stock[index].payments)if(p.owned<p.quantity){shopMessage_="Bring a Heart Scale for this lesson.";emit changed();return;}
            if(m->balance<m->stock[index].price){shopMessage_="Not enough BP for this lesson.";emit changed();return;}
            recipientIndex_=lessonMoveIndex_=0;quantity_=1;shopMessage_.clear();shopRoute_="recipients";emit changed();return;
        }
        if(m->stock[index].maximum<quantity_){shopMessage_="Not enough payment or Bag space.";emit changed();return;}
        shopMessage_.clear();shopRoute_="confirm";
    }else if(shopRoute_=="recipients"){
        const auto m=merchant();if(!m||stockIndex_>=m->stock.size())return;
        const auto& list=m->stock[stockIndex_].recipients;
        if(index<0||index>=list.size()||!list[index].available)return;
        recipientIndex_=index;lessonMoveIndex_=0;shopMessage_.clear();shopRoute_="moves";
    }else if(shopRoute_=="moves"){
        const auto rows=shopMoves();if(index<0||index>=rows.size()||!rows[index].toMap()["available"].toBool())return;
        lessonMoveIndex_=index;shopMessage_.clear();shopRoute_="confirm";
    }else if(shopRoute_=="confirm"){purchase();return;}
    else {shopRoute_="stock";shopMessage_.clear();quantity_=1;refresh();}
    emit changed();
}
void SaveCenterController::dispatchShop(Action action){
    if(busy())return;
    if(action==Action::Back){
        if(shopRoute_=="merchants"){if(shopGroup_.isEmpty())shopsOpen_=false;else {shopGroup_.clear();merchantIndex_=groupParentIndex_;stockIndex_=0;quantity_=1;}}
        else if(shopRoute_=="stock")shopRoute_="merchants";
        else if(shopRoute_=="moves")shopRoute_="recipients";
        else if(shopRoute_=="confirm"&&lesson(shopSelection()["kind"].toString()))shopRoute_="moves";
        else {shopRoute_="stock";if(!shopMessage_.isEmpty())refresh();}
        shopMessage_.clear();emit changed();return;
    }
    if(action==Action::Confirm){shopActivate(shopRoute_=="merchants"?merchantIndex_:shopRoute_=="recipients"?recipientIndex_:shopRoute_=="moves"?lessonMoveIndex_:stockIndex_);return;}
    if(shopRoute_=="merchants"){
        if(shopGroup_.isEmpty()&&(action==Action::Left||action==Action::Right)){
            const auto cats=shopCategories();int index=0;for(int i=0;i<cats.size();++i)if(cats[i].toMap()["id"].toString()==shopCategory_)index=i;
            if(!cats.isEmpty())chooseShopCategory((index+(action==Action::Right?1:cats.size()-1))%cats.size());
            return;
        }
        const int size=merchantRows().size();
        const int next=std::clamp(merchantIndex_+(action==Action::Down?1:action==Action::Up?-1:0),0,std::max(0,size-1));
        if(next!=merchantIndex_){merchantIndex_=next;stockIndex_=0;quantity_=1;shopMessage_.clear();}
    }else if(shopRoute_=="stock"){
        const auto m=merchant();if(!m||m->stock.isEmpty())return;
        if(action==Action::Down||action==Action::Up){stockIndex_=std::clamp(stockIndex_+(action==Action::Down?1:-1),0,int(m->stock.size())-1);quantity_=1;shopMessage_.clear();}
        if(action==Action::Left||action==Action::Right)quantity_=std::clamp(quantity_+(action==Action::Right?1:-1),1,std::max(1,m->stock[stockIndex_].maximum));
    }
    else if(shopRoute_=="recipients"||shopRoute_=="moves"){
        auto& index=shopRoute_=="recipients"?recipientIndex_:lessonMoveIndex_;
        const int count=shopRoute_=="recipients"?shopRecipients().size():shopMoves().size();
        index=std::clamp(index+(action==Action::Down?1:action==Action::Up?-1:0),0,std::max(0,count-1));
    }
    emit changed();
}
void SaveCenterController::purchase(){
    const auto m=merchant();if(!m||!m->available||!service_||busy()||stockIndex_<0||stockIndex_>=m->stock.size())return;
    const auto r=library_.registration(selected_.adventure.id);
    if(!r||r->revision!=selected_.revision){shopRoute_="receipt";shopMessage_="This Adventure changed. Visit the shop again.";emit changed();return;}
    MerchantPurchase request{m->id,m->stock[stockIndex_].itemId,quantity_,m->stock[stockIndex_].kind};
    if(lesson(request.kind)){
        const auto& list=m->stock[stockIndex_].recipients;
        if(recipientIndex_<0||recipientIndex_>=list.size())return;
        request.partySlot=list[recipientIndex_].slot;request.moveSlot=lessonMoveIndex_;request.recipientIdentity=list[recipientIndex_].identity;
    }
    const auto generation=generation_;
    service_->purchase(*r,snapshot_.token,request,this,[this,generation,r=*r](const SaveBackupResult& result){
        if(result.restored)emit restored(r.adventure.id);
        if(!open_||generation!=generation_){if(!result.success)emit messageRequested(result.message);return;}
        if(result.success){snapshot_=result.snapshot;emit rowsChanged();}
        shopRoute_="receipt";shopMessage_=result.message;emit changed();
    });
}
}
