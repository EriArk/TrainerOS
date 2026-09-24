#include "SaveCenterController.h"
#include <algorithm>

namespace trainer {
const Merchant* SaveCenterController::merchant() const {
    const auto& list=snapshot_.shops.merchants;
    return merchantIndex_>=0&&merchantIndex_<list.size()?&list[merchantIndex_]:nullptr;
}
QVariantList SaveCenterController::merchants() const {
    QVariantList out;
    for(const auto& m:snapshot_.shops.merchants)out.append(QVariantMap{{"name",m.discovered?m.name:QString("???")},
        {"detail",m.discovered?(m.available?m.location:QString("Currently unavailable")):QString("Undiscovered")},{"discovered",m.discovered}});
    return out;
}
QVariantList SaveCenterController::shopStock() const {
    QVariantList out;const auto m=merchant();if(!m||!m->discovered)return out;
    for(const auto& s:m->stock)out.append(QVariantMap{{"name",s.name},{"price",s.price},{"owned",s.owned},{"maximum",m->available?s.maximum:0},{"pocket",s.pocket}});
    return out;
}
QVariantMap SaveCenterController::shopSelection() const {
    QVariantMap out;const auto m=merchant();if(!m||!m->discovered)return out;
    out["name"]=m->name;out["location"]=m->location;
    out["currency"]=m->currency==MerchantCurrency::Money?"₽":m->currency==MerchantCurrency::Coins?"Coins":"BP";
    if(stockIndex_>=0&&stockIndex_<m->stock.size()){
        const auto& s=m->stock[stockIndex_];out["item"]=s.name;out["total"]=s.price*quantity_;
        out["owned"]=s.owned;out["maximum"]=m->available?s.maximum:0;out["pocket"]=s.pocket;
    }
    return out;
}
QString SaveCenterController::shopMessage() const {
    if(busy())return shopRoute_=="confirm"?"Packing your purchase…":"Checking the shops…";
    if(!shopMessage_.isEmpty())return shopMessage_;
    if(!snapshot_.shops.error.isEmpty())return snapshot_.shops.error;
    if(!snapshot_.shops.supported)return snapshot_.error.isEmpty()?"Save in Pokémon Emerald, then visit again.":snapshot_.error;
    return {};
}
void SaveCenterController::visitShops(){
    if(!open_||!companion_||confirming_)return;
    clinicOpen_=false;shopsOpen_=true;shopRoute_="merchants";shopMessage_.clear();merchantIndex_=stockIndex_=0;quantity_=1;
    refresh();emit changed();
}
void SaveCenterController::shopActivate(int index){
    if(!shopsOpen_||busy())return;
    if(shopRoute_=="merchants"){
        if(index<0||index>=snapshot_.shops.merchants.size())return;
        merchantIndex_=index;const auto m=merchant();if(!m||!m->discovered)return;
        stockIndex_=0;quantity_=1;shopRoute_="stock";
    }else if(shopRoute_=="stock"){
        const auto m=merchant();if(!m||!m->available||index<0||index>=m->stock.size())return;
        if(stockIndex_!=index)quantity_=1;stockIndex_=index;
        if(m->stock[index].maximum<quantity_){shopMessage_="Not enough money or room in your Bag.";emit changed();return;}
        shopMessage_.clear();shopRoute_="confirm";
    }else if(shopRoute_=="confirm"){purchase();return;}
    else {shopRoute_="stock";shopMessage_.clear();quantity_=1;refresh();}
    emit changed();
}
void SaveCenterController::dispatchShop(Action action){
    if(busy())return;
    if(action==Action::Back){
        if(shopRoute_=="merchants")shopsOpen_=false;
        else if(shopRoute_=="stock")shopRoute_="merchants";
        else {shopRoute_="stock";if(!shopMessage_.isEmpty())refresh();}
        shopMessage_.clear();emit changed();return;
    }
    if(action==Action::Confirm){shopActivate(shopRoute_=="merchants"?merchantIndex_:stockIndex_);return;}
    if(shopRoute_=="merchants"){
        const int size=snapshot_.shops.merchants.size();
        merchantIndex_=std::clamp(merchantIndex_+(action==Action::Down?1:action==Action::Up?-1:0),0,std::max(0,size-1));
    }else if(shopRoute_=="stock"){
        const auto m=merchant();if(!m||m->stock.isEmpty())return;
        if(action==Action::Down||action==Action::Up){stockIndex_=std::clamp(stockIndex_+(action==Action::Down?1:-1),0,int(m->stock.size())-1);quantity_=1;shopMessage_.clear();}
        if(action==Action::Left||action==Action::Right)quantity_=std::clamp(quantity_+(action==Action::Right?1:-1),1,std::max(1,m->stock[stockIndex_].maximum));
    }
    emit changed();
}
void SaveCenterController::purchase(){
    const auto m=merchant();if(!m||!m->available||!service_||busy()||stockIndex_<0||stockIndex_>=m->stock.size())return;
    const auto r=library_.registration(selected_.adventure.id);
    if(!r||r->revision!=selected_.revision){shopRoute_="receipt";shopMessage_="This Adventure changed. Visit the shop again.";emit changed();return;}
    const MerchantPurchase request{m->id,m->stock[stockIndex_].itemId,quantity_};const auto generation=generation_;
    service_->purchase(*r,snapshot_.token,request,this,[this,generation,r=*r](const SaveBackupResult& result){
        if(result.restored)emit restored(r.adventure.id);
        if(!open_||generation!=generation_){if(!result.success)emit messageRequested(result.message);return;}
        if(result.success){snapshot_=result.snapshot;emit rowsChanged();}
        shopRoute_="receipt";shopMessage_=result.message;emit changed();
    });
}
}
