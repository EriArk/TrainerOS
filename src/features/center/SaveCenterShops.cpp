#include "SaveCenterController.h"
#include <algorithm>
#include <QMap>

namespace trainer {
namespace {
QString section(const Merchant& m){return !m.discovered?QString("unknown"):m.section.isEmpty()?QString("marts"):m.section;}
bool lesson(const QString& kind){return kind=="tutor"||kind=="relearn";}
QString paymentText(const MerchantStock& stock){
    QStringList lines;for(const auto& p:stock.payments)lines.append(QString("%1 × %2").arg(p.quantity).arg(p.name));return lines.join(" + ");
}
QString matchText(const QString& s){QString out;for(auto c:s.normalized(QString::NormalizationForm_D))if(c.isLetterOrNumber())out+=c.toLower();return out;}
bool matches(const QString& text,const QString& query){return matchText(text).contains(matchText(query));}
QString currencyName(MerchantCurrency c){switch(c){case MerchantCurrency::Coins:return "Coins";case MerchantCurrency::BattlePoints:return "BP";case MerchantCurrency::Ash:return "Ash";case MerchantCurrency::BerryPowder:return "Berry Powder";default:return "₽";}}
const QStringList sectionIds{"marts","stores","specialists","exchanges","services","unknown"};
const QStringList sectionNames{"Poké Marts","Stores","Specialists","Exchanges","Services","Undiscovered"};
}
QVariantList SaveCenterController::shopCategories() const {
    QVariantList out;
    if(!shopQuery_.isEmpty()||!shopLocation_.isEmpty())out.append(QVariantMap{{"id","all"},{"name","Results"}});
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
    QVariantList out;const auto st=stock();if(!st)return out;
    for(const auto& r:st->recipients)out.append(QVariantMap{{"name",r.name},{"available",r.available},{"detail",r.available?QString("Can learn"):r.reason}});
    return out;
}
QVariantList SaveCenterController::shopMoves() const {
    QVariantList out;const auto st=stock();if(!st)return out;
    const auto& list=st->recipients;if(recipientIndex_<0||recipientIndex_>=list.size())return out;
    for(const auto& move:list[recipientIndex_].moves)out.append(QVariantMap{{"name",move.name},{"available",move.available},{"detail",move.available?QString("Replace"):QString("Keep")}});
    return out;
}

int SaveCenterController::shopBalance() const {
    const auto m=merchant();return snapshot_.shops.supported&&m&&m->discovered&&!m->itemPayment?m->balance:-1;
}
QList<int> SaveCenterController::merchantRows() const {
    QList<int> out;QStringList groups;const auto& list=snapshot_.shops.merchants;
    for(int i=0;i<list.size();++i){const auto& m=list[i];
        if(shopCategory_!="all" && section(m)!=shopCategory_)continue;
        if(!shopQuery_.isEmpty()||!shopLocation_.isEmpty()) {
            if(!m.discovered||(!shopLocation_.isEmpty()&&m.location!=shopLocation_))continue;
            bool hit=matches(m.name+" "+m.group+" "+m.location,shopQuery_);
            for(const auto& st:m.stock)hit|=matches(st.name,shopQuery_);
            if(!hit)continue;
        }
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
    for(int index:stockRows()){const auto& s=m->stock[index];out.append(QVariantMap{{"name",s.name},{"price",s.price},{"owned",s.owned},{"maximum",m->available?s.maximum:0},{"pocket",s.pocket},{"payment",paymentText(s)}});}
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
    if(const auto st=stock()){
        const auto& s=*st;out["item"]=s.name;out["total"]=s.price*quantity_;
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
    clinicOpen_=false;shopsOpen_=true;if(shopRoute_!="stock")shopRoute_="merchants";shopMessage_.clear();
    normalizeShopCategory();
    refresh();emit changed();
}
void SaveCenterController::shopActivate(int index){
    if(!shopsOpen_||busy())return;
    if(shopRoute_=="basket"){if(basket_.isEmpty()){shopRoute_=basketReturn_;emit changed();}else buyBasket();return;}
    if(shopRoute_=="locations") {
        const auto rows=shopLocations();if(index<0||index>=rows.size())return;
        shopLocation_=index?rows[index]:QString();shopCategory_="all";shopGroup_.clear();merchantIndex_=stockIndex_=0;quantity_=1;shopRoute_="merchants";normalizeShopCategory();emit changed();return;
    }
    if(shopRoute_=="merchants"){
        if(merchantRows().isEmpty()){refresh();return;}
        if(index<0||index>=merchantRows().size())return;
        merchantIndex_=index;stockIndex_=0;quantity_=1;const auto m=merchant();if(!m||!m->discovered)return;
        if(shopGroup_.isEmpty()&&!m->group.isEmpty()){groupParentIndex_=merchantIndex_;shopGroup_=m->group;merchantIndex_=0;}
        else {stockIndex_=0;quantity_=1;shopRoute_="stock";}
    }else if(shopRoute_=="stock"){
        const auto m=merchant();if(!m||!m->available||index<0||index>=stockRows().size())return;
        if(stockIndex_!=index)quantity_=1;
        stockIndex_=index;const auto& offer=*stock();
        if(lesson(offer.kind)){
            for(const auto& p:offer.payments)if(p.owned<p.quantity){shopMessage_="Bring a Heart Scale for this lesson.";emit changed();return;}
            if(m->balance<offer.price){shopMessage_="Not enough BP for this lesson.";emit changed();return;}
            recipientIndex_=lessonMoveIndex_=0;quantity_=1;shopMessage_.clear();shopRoute_="recipients";emit changed();return;
        }
        if(offer.maximum<quantity_){shopMessage_="Not enough payment or Bag space.";emit changed();return;}
        auto found=std::find_if(basket_.begin(),basket_.end(),[&](const auto& line){return line.merchantId==m->id&&line.itemId==offer.itemId&&line.kind==offer.kind;});
        if(found!=basket_.end())found->quantity=std::min(99,found->quantity+quantity_);
        else if(basket_.size()<32)basket_.append({m->id,offer.itemId,quantity_,offer.kind});
        else {shopMessage_="Your basket is full.";emit changed();return;}
        shopMessage_="Added to basket";
    }else if(shopRoute_=="recipients"){
        const auto st=stock();if(!st)return;
        const auto& list=st->recipients;
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
        if(shopRoute_=="basket"){shopRoute_=basketReturn_;}
        else if(shopRoute_=="locations")shopRoute_="merchants";
        else if(shopRoute_=="merchants"){if(shopGroup_.isEmpty())shopsOpen_=false;else {shopGroup_.clear();merchantIndex_=groupParentIndex_;stockIndex_=0;quantity_=1;}}
        else if(shopRoute_=="stock")shopRoute_="merchants";
        else if(shopRoute_=="moves")shopRoute_="recipients";
        else if(shopRoute_=="confirm"&&lesson(shopSelection()["kind"].toString()))shopRoute_="moves";
        else {shopRoute_="stock";if(!shopMessage_.isEmpty())refresh();}
        shopMessage_.clear();emit changed();return;
    }
    if(action==Action::LocalAction&&(shopRoute_=="merchants"||shopRoute_=="stock")){openBasket();return;}
    if(shopRoute_=="merchants" && action==Action::Secondary){emit shopSearchRequested(shopQuery_);return;}
    if(shopRoute_=="merchants" && action==Action::ToggleContinue){shopLocationIndex_=std::max(0,int(shopLocations().indexOf(shopLocation_)));shopRoute_="locations";emit changed();return;}
    if(action==Action::Confirm){shopActivate(shopRoute_=="locations"?shopLocationIndex_:shopRoute_=="basket"?basketIndex_:shopRoute_=="merchants"?merchantIndex_:shopRoute_=="recipients"?recipientIndex_:shopRoute_=="moves"?lessonMoveIndex_:stockIndex_);return;}
    if(shopRoute_=="locations") {
        shopLocationIndex_=std::clamp(shopLocationIndex_+(action==Action::Down?1:action==Action::Up?-1:0),0,int(shopLocations().size())-1);
    }else if(shopRoute_=="basket") {
        if(!basket_.isEmpty()) {
            basketIndex_=std::clamp(basketIndex_+(action==Action::Down?1:action==Action::Up?-1:0),0,int(basket_.size())-1);
            auto& line=basket_[basketIndex_];
            if(action==Action::Left||action==Action::Right)line.quantity=std::clamp(line.quantity+(action==Action::Right?1:-1),1,99);
            if(action==Action::Secondary){basket_.removeAt(basketIndex_);basketIndex_=std::max(0,std::min(basketIndex_,int(basket_.size())-1));}
            shopMessage_.clear();
        }
    }else if(shopRoute_=="merchants"){
        if(shopGroup_.isEmpty()&&(action==Action::Left||action==Action::Right)){
            const auto cats=shopCategories();int index=0;for(int i=0;i<cats.size();++i)if(cats[i].toMap()["id"].toString()==shopCategory_)index=i;
            if(!cats.isEmpty())chooseShopCategory((index+(action==Action::Right?1:cats.size()-1))%cats.size());
            return;
        }
        const int size=merchantRows().size();
        const int next=std::clamp(merchantIndex_+(action==Action::Down?1:action==Action::Up?-1:0),0,std::max(0,size-1));
        if(next!=merchantIndex_){merchantIndex_=next;stockIndex_=0;quantity_=1;shopMessage_.clear();}
    }else if(shopRoute_=="stock"){
        const auto m=merchant();if(!m||stockRows().isEmpty())return;
        if(action==Action::Down||action==Action::Up){stockIndex_=std::clamp(stockIndex_+(action==Action::Down?1:-1),0,int(stockRows().size())-1);quantity_=1;shopMessage_.clear();}
        if(action==Action::Left||action==Action::Right)quantity_=std::clamp(quantity_+(action==Action::Right?1:-1),1,std::max(1,stock()?stock()->maximum:1));
    }
    else if(shopRoute_=="recipients"||shopRoute_=="moves"){
        auto& index=shopRoute_=="recipients"?recipientIndex_:lessonMoveIndex_;
        const int count=shopRoute_=="recipients"?shopRecipients().size():shopMoves().size();
        index=std::clamp(index+(action==Action::Down?1:action==Action::Up?-1:0),0,std::max(0,count-1));
    }
    emit changed();
}
void SaveCenterController::purchase(){
    if(readOnly()){shopMessage_="Read-only saves is on. Change it in Settings to buy.";emit changed();return;}
    const auto m=merchant();const auto st=stock();if(!m||!st||!m->available||!service_||busy())return;
    const auto r=library_.registration(selected_.adventure.id);
    if(!r||r->revision!=selected_.revision){shopRoute_="receipt";shopMessage_="This Adventure changed. Visit the shop again.";emit changed();return;}
    MerchantPurchase request{m->id,st->itemId,quantity_,st->kind};
    if(lesson(request.kind)){
        const auto& list=st->recipients;
        if(recipientIndex_<0||recipientIndex_>=list.size())return;
        request.partySlot=list[recipientIndex_].slot;request.moveSlot=lessonMoveIndex_;request.recipientIdentity=list[recipientIndex_].identity;
    }
    const auto generation=generation_;
    writing_=true;service_->purchase(*r,snapshot_.token,request,this,[this,generation,r=*r](const SaveBackupResult& result){
        writing_=false;
        if(result.restored)emit restored(r.adventure.id);
        if(!open_||generation!=generation_){if(!result.success)emit messageRequested(result.message);return;}
        if(result.success){snapshot_=result.snapshot;emit rowsChanged();}
        shopRoute_="receipt";shopMessage_=result.message;emit changed();
    });
}
QList<int> SaveCenterController::stockRows() const {
    QList<int> rows;const auto m=merchant();if(!m||!m->discovered)return rows;
    const bool all=matches(m->name+" "+m->group+" "+m->location,shopQuery_);
    for(int i=0;i<m->stock.size();++i)if(all||matches(m->stock[i].name,shopQuery_))rows.append(i);
    return rows;
}
const MerchantStock* SaveCenterController::stock() const {
    const auto m=merchant();const auto rows=stockRows();return m&&stockIndex_>=0&&stockIndex_<rows.size()?&m->stock[rows[stockIndex_]]:nullptr;
}
void SaveCenterController::resetShopContext(){basket_.clear();shopQuery_.clear();shopLocation_.clear();shopCategory_="marts";shopGroup_.clear();merchantIndex_=stockIndex_=basketIndex_=0;}
void SaveCenterController::applyShopSearch(const QString& text){
    if(!shopsOpen_||busy()||shopRoute_!="merchants")return;
    shopQuery_=text.trimmed().left(64);shopCategory_="all";shopGroup_.clear();merchantIndex_=stockIndex_=0;quantity_=1;normalizeShopCategory();emit changed();
}
QStringList SaveCenterController::shopLocations() const {
    QStringList rows;for(const auto& m:snapshot_.shops.merchants)if(m.discovered&&!m.location.isEmpty()&&!rows.contains(m.location))rows.append(m.location);
    rows.sort(Qt::CaseInsensitive);rows.prepend("All places");return rows;
}
int SaveCenterController::basketCount() const {int n=0;for(const auto& line:basket_)n+=line.quantity;return n;}
QVariantList SaveCenterController::basketRows() const {
    QVariantList rows;
    for(const auto& line:basket_){
        QVariantMap row{{"name",QString("%1 × Unavailable item").arg(line.quantity)},{"detail","Remove this item or check the Adventure again."}};
        for(const auto& m:snapshot_.shops.merchants)if(m.id==line.merchantId&&m.discovered)for(const auto& st:m.stock)if(st.itemId==line.itemId&&st.kind==line.kind){
            QStringList payment;for(const auto& p:st.payments)payment.append(QString("%1 × %2").arg(p.quantity*line.quantity).arg(p.name));
            row={{"name",QString("%1 × %2").arg(line.quantity).arg(st.name)},{"detail",m.name+" · "+(st.payments.isEmpty()?currencyName(m.currency)+" "+QString::number(st.price*line.quantity):payment.join(" + "))}};
        }
        rows.append(row);
    }
    return rows;
}
QString SaveCenterController::basketTotal() const {
    QMap<QString,int> costs;
    for(const auto& line:basket_)for(const auto& m:snapshot_.shops.merchants)if(m.id==line.merchantId&&m.discovered)for(const auto& st:m.stock)if(st.itemId==line.itemId&&st.kind==line.kind){
        if(st.payments.isEmpty()){const auto key=currencyName(m.currency);costs[key]+=st.price*line.quantity;}
        else for(const auto& cost:st.payments){costs[cost.name]+=cost.quantity*line.quantity;}
    }
    QStringList rows;for(auto it=costs.begin();it!=costs.end();++it)rows.append(QString("%1  %2").arg(it.key()).arg(it.value()));
    return rows.join("\n\n");
}
void SaveCenterController::openBasket(){if(!shopsOpen_||busy()||(shopRoute_!="merchants"&&shopRoute_!="stock"))return;basketReturn_=shopRoute_;shopRoute_="basket";basketIndex_=0;shopMessage_.clear();emit changed();}
void SaveCenterController::buyBasket(){
    if(readOnly()){shopMessage_="Read-only saves is on. Change it in Settings to buy.";emit changed();return;}
    if(basket_.isEmpty()||!service_||busy())return;
    const auto r=library_.registration(selected_.adventure.id);
    if(!r||r->revision!=selected_.revision){shopMessage_="This Adventure changed. Reopen the shops.";emit changed();return;}
    MerchantPurchase request;request.kind="basket";request.basket=basket_;const auto generation=generation_;
    writing_=true;service_->purchase(*r,snapshot_.token,request,this,[this,generation,r=*r](const SaveBackupResult& result){
        writing_=false;
        if(result.restored)emit restored(r.adventure.id);
        if(!open_||generation!=generation_){if(!result.success)emit messageRequested(result.message);return;}
        if(result.success||result.restored){basket_.clear();basketIndex_=0;shopRoute_="receipt";}
        if(result.success){snapshot_=result.snapshot;emit rowsChanged();}else refresh();
        shopMessage_=result.message;emit changed();
    });
}

}
