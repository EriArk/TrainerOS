#include "EmeraldShops.h"
#include "EmeraldParty.h"
#include <QCryptographicHash>
#include <array>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QtEndian>
#include <algorithm>

static void initShopData(){Q_INIT_RESOURCE(emerald_shops);}
namespace trainer {
namespace {
const QJsonObject& data() {
    static const auto value=[] {initShopData();QFile f(":/progress/emerald-shops.json");f.open(QIODevice::ReadOnly);return QJsonDocument::fromJson(f.readAll()).object();}();
    return value;
}
quint16 u16(const QByteArray& b,int at){return qFromLittleEndian<quint16>(b.constData()+at);}
quint32 u32(const QByteArray& b,int at){return qFromLittleEndian<quint32>(b.constData()+at);}
bool flag(const QByteArray& b,int id){return id>0 && id<0x960 && (quint8(b[0x1270+id/8])&(1<<(id%8)));}
bool news(const QByteArray& b,int id){
    for(int n=0;n<16;++n)if(quint8(b[0x2b50+4*n])==id)return quint8(b[0x2b51+4*n])==2;
    return false;
}
struct Pocket {int at,count;const char* name;};
constexpr Pocket pockets[]={{0,0,""},{0x560,30,"Items"},{0x5d8,30,"Key items"},{0x650,16,"Poké Balls"},{0x690,64,"TMs & HMs"},{0x790,46,"Berries"}};
constexpr Pocket decorPockets[]={{0x2734,10,"Desks"},{0x273e,10,"Chairs"},{0x2748,10,"Plants"},{0x2752,30,"Ornaments"},{0x2770,30,"Mats"},{0x278e,10,"Posters"},{0x2798,40,"Dolls"},{0x27c0,10,"Cushions"}};
QJsonObject item(int id){return data()["items"].toObject()[QString::number(id)].toObject();}
QJsonObject decoration(int id){return data()["decorations"].toObject()[QString::number(id)].toObject();}
bool validBag(const QByteArray& world,quint32 key) {
    for(int p:{1,2,3,4}){
        QSet<int> seen;
        for(int i=0;i<pockets[p].count;++i){
            const int at=pockets[p].at+i*4,id=u16(world,at),qty=u16(world,at+2)^quint16(key);
            if(!id){if(qty)return false;continue;}
            if(item(id)["pocket"].toInt()!=p || qty<1 || qty>(p==2?1:99) || ((p==2||p==4)&&seen.contains(id)))return false;
            seen.insert(id);
        }
    }
    for(int c=0;c<8;++c)for(int i=0;i<decorPockets[c].count;++i){
        const int id=quint8(world[decorPockets[c].at+i]);
        if(id&&(decoration(id).isEmpty()||decoration(id)["category"].toInt(-1)!=c))return false;
    }
    return true;
}
int owned(const QByteArray& world,quint32 key,int id,int pocket){
    int n=0;const auto p=pockets[pocket];for(int i=0;i<p.count;++i)if(u16(world,p.at+4*i)==id)n+=u16(world,p.at+4*i+2)^quint16(key);return n;
}
int roomFor(const QByteArray& world,quint32 key,int id,int pocket){
    int n=0;const auto p=pockets[pocket];
    for(int i=0;i<p.count;++i){const int at=p.at+4*i,other=u16(world,at);
        if(!other)n+=99;
        else if(other==id){const int room=99-(u16(world,at+2)^quint16(key));if(pocket==4)return room;n+=room;}
    }
    return pocket==4?std::min(99,n):n;
}
bool add(QByteArray& world,quint32 key,int id,int count) {
    const int pocket=item(id)["pocket"].toInt();if(pocket!=1&&pocket!=3&&pocket!=4)return false;
    if(roomFor(world,key,id,pocket)<count)return false;
    const auto p=pockets[pocket];
    for(int pass=0;pass<2 && count;++pass)for(int i=0;i<p.count && count;++i){
        const int at=p.at+4*i,other=u16(world,at);if((pass==0&&other!=id)||(pass==1&&other))continue;
        const int qty=other?(u16(world,at+2)^quint16(key)):0,amount=std::min(count,99-qty);
        qToLittleEndian(quint16(id),world.data()+at);qToLittleEndian(quint16((qty+amount)^quint16(key)),world.data()+at+2);count-=amount;
    }
    return !count;
}
bool payItems(QByteArray& world,quint32 key,const QList<MerchantPayment>& payments) {
    // Only the normal Items pocket is accepted. Clear consumed slots without
    // rearranging unrelated items; the game compacts them when opening the Bag.
    for(const auto& cost:payments){
        if(cost.quantity<=0||item(cost.itemId)["pocket"].toInt()!=1||owned(world,key,cost.itemId,1)<cost.quantity)return false;
    }
    for(const auto& cost:payments){
        int left=cost.quantity;
        for(int i=0;i<pockets[1].count&&left;++i){
            const int at=pockets[1].at+i*4;if(u16(world,at)!=cost.itemId)continue;
            const int qty=u16(world,at+2)^quint16(key),used=std::min(left,qty);
            qToLittleEndian(quint16((qty-used)^quint16(key)),world.data()+at+2);
            if(qty==used)qToLittleEndian(quint16(0),world.data()+at);
            left-=used;
        }
    }
    return true;
}
int countDecor(const QByteArray& world,int id,bool empty){
    const auto d=decoration(id);const int category=d["category"].toInt(-1);if(category<0||category>=8)return 0;
    const auto p=decorPockets[category];int count=0;
    for(int i=0;i<p.count;++i)if(quint8(world[p.at+i])==(empty?0:id))++count;
    return count;
}
bool addDecor(QByteArray& world,int id){
    if(!countDecor(world,id,true))return false;
    const auto p=decorPockets[decoration(id)["category"].toInt()];
    for(int i=0;i<p.count;++i)if(!world[p.at+i]){world[p.at+i]=char(id);return true;}
    return false;
}
struct MonPayload {
    QByteArray clear;
    int growth=0, attacks=0;
    quint32 key=0;
};
MonPayload monPayload(const QByteArray& record) {
    MonPayload p;const auto personality=u32(record,0);p.key=personality^u32(record,4);p.clear=record.mid(32,48);
    for(int i=0;i<48;i+=4)qToLittleEndian(u32(p.clear,i)^p.key,p.clear.data()+i);
    std::array<int,4> order{0,1,2,3};
    for(quint32 i=0;i<personality%24;++i)std::next_permutation(order.begin(),order.end());
    p.growth=int(std::find(order.begin(),order.end(),0)-order.begin())*12;
    p.attacks=int(std::find(order.begin(),order.end(),1)-order.begin())*12;
    return p;
}
QList<int> relearnable(const QByteArray& record) {
    QList<int> result;const auto mon=readEmeraldPartyMember(record);
    if(mon.kind!=PokemonSlotKind::Known)return result;
    const auto p=monPayload(record);
    for(const auto& value:data()["levelMoves"].toObject()[QString::number(u16(p.clear,p.growth))].toArray()){
        const auto entry=value.toArray();const int move=entry[1].toInt();
        if(entry[0].toInt()>mon.level||result.contains(move))continue;
        bool known=false;for(int i=0;i<4;++i)known|=u16(p.clear,p.attacks+2*i)==move;
        if(!known)result.append(move);
    }
    return result;
}
MerchantRecipient tutorRecipient(const QByteArray& record,int slot,int move,bool reminder=false) {
    MerchantRecipient r;r.slot=slot;
    const auto mon=readEmeraldPartyMember(record);
    r.name=mon.nickname.isEmpty()?mon.speciesName:mon.nickname;
    if(mon.kind!=PokemonSlotKind::Known){r.name=mon.kind==PokemonSlotKind::Egg?"Egg":"Unavailable";r.reason="Cannot take a lesson";return r;}
    r.identity=QString::fromLatin1(QCryptographicHash::hash(record,QCryptographicHash::Sha256).toHex());
    const auto p=monPayload(record);const auto fact=data()["tutors"].toObject()[QString::number(move)].toObject();
    if(reminder?!relearnable(record).contains(move):!fact["species"].toArray().contains(int(u16(p.clear,p.growth)))){r.reason="Cannot learn this move";return r;}
    int empty=-1;
    for(int i=0;i<4;++i){
        const int old=u16(p.clear,p.attacks+2*i);
        if(old==move){r.reason="Already knows this move";return r;}
        if(!old&&empty<0)empty=i;
    }
    for(int i=0;i<4;++i){
        const int old=u16(p.clear,p.attacks+2*i);
        const bool allowed=empty>=0?i==empty:!data()["hmMoves"].toArray().contains(old);
        r.moves.append({old?mon.moves[i].name:QString("Empty slot"),allowed});
        r.available|=allowed;
    }
    if(!r.available)r.reason="HM moves cannot be replaced here";
    return r;
}
QByteArray taughtRecord(const QByteArray& record,int move,int slot) {
    auto p=monPayload(record);auto result=record;
    qToLittleEndian(quint16(move),p.clear.data()+p.attacks+slot*2);
    p.clear[p.attacks+8+slot]=char(data()["moves"].toObject()[QString::number(move)].toObject()["pp"].toInt());
    p.clear[p.growth+8]=char(quint8(p.clear[p.growth+8])&~(3u<<(2*slot)));
    quint16 checksum=0;for(int i=0;i<48;i+=2)checksum=quint16(checksum+u16(p.clear,i));
    qToLittleEndian(checksum,result.data()+28);
    for(int i=0;i<48;i+=4)qToLittleEndian(u32(p.clear,i)^p.key,result.data()+32+i);
    return readEmeraldPartyMember(result).kind==PokemonSlotKind::Known?result:QByteArray{};
}
MerchantCurrency currency(const QString& name){
    if(name=="coins")return MerchantCurrency::Coins;
    if(name=="bp")return MerchantCurrency::BattlePoints;
    if(name=="ash")return MerchantCurrency::Ash;
    if(name=="powder")return MerchantCurrency::BerryPowder;
    return MerchantCurrency::Money;
}
int balance(const QByteArray& world,const QByteArray& trainer,MerchantCurrency kind){
    const auto key=u32(trainer,0xac);
    switch(kind){
    case MerchantCurrency::Money:return int(u32(world,0x490)^key);
    case MerchantCurrency::Coins:return u16(world,0x494)^quint16(key);
    case MerchantCurrency::BattlePoints:return u16(trainer,0xeb8);
    case MerchantCurrency::Ash:return u16(world,0x142c);
    case MerchantCurrency::BerryPowder:return int(u32(trainer,0x1f4)^key);
    }
    return 0;
}
void setBalance(QByteArray& world,QByteArray& trainer,MerchantCurrency kind,int value){
    const auto key=u32(trainer,0xac);
    switch(kind){
    case MerchantCurrency::Money:qToLittleEndian(quint32(value)^key,world.data()+0x490);break;
    case MerchantCurrency::Coins:qToLittleEndian(quint16(value^quint16(key)),world.data()+0x494);break;
    case MerchantCurrency::BattlePoints:qToLittleEndian(quint16(value),trainer.data()+0xeb8);break;
    case MerchantCurrency::Ash:qToLittleEndian(quint16(value),world.data()+0x142c);break;
    case MerchantCurrency::BerryPowder:qToLittleEndian(quint32(value)^key,trainer.data()+0x1f4);break;
    }
}
}
MerchantSnapshot readEmeraldShopBlock(const QByteArray& world,const QByteArray& trainer) {
    MerchantSnapshot out;
    if(world.size()!=0x3d88 || trainer.size()!=0xf2c || data()["version"].toInt()!=5){out.error="The shops could not be checked.";return out;}
    const auto key=u32(trainer,0xac);
    const auto money=u32(world,0x490)^key;
    if(money>999999 || !validBag(world,key)){out.error="Your Bag or decorations could not be verified.";return out;}
    if(balance(world,trainer,MerchantCurrency::Coins)>9999 || balance(world,trainer,MerchantCurrency::BattlePoints)>9999 || balance(world,trainer,MerchantCurrency::Ash)>9999
       || (u32(trainer,0x1f4)^key)>99999){out.error="Your currencies could not be verified.";return out;}
    out.supported=true;out.balance=int(money);
    const bool pyramid=(quint8(world[4])==26 && (quint8(world[5])==26||quint8(world[5])==27))
        ||(quint8(world[4])==25 && quint8(world[5])>=44&&quint8(world[5])<=59);
    if(pyramid)out.error="Finish your Battle Pyramid challenge before shopping.";
    const int weather=u16(world,0x139c+2*0x5e);
    for(const auto& value:data()["merchants"].toArray()){
        const auto d=value.toObject();Merchant m;
        bool discovered=flag(world,d["visitedFlag"].toInt());
        if(d.contains("anyFlags")){discovered=false;for(const auto& f:d["anyFlags"].toArray())discovered|=flag(world,f.toInt());}
        for(const auto& f:d["requiredFlags"].toArray())discovered=discovered&&flag(world,f.toInt());
        if(d.contains("hiddenFlag"))discovered=discovered&&!flag(world,d["hiddenFlag"].toInt());
        if(discovered){
            m.id=d["id"].toString();m.name=d["name"].toString();m.location=d["location"].toString();m.group=d["group"].toString();
            const bool reminder=d["kind"].toString()=="relearn",barter=d["kind"].toString()=="barter";
            m.category=reminder?"Move reminder":barter?"Item exchange":d["kind"].toString()=="tutor"?"Move tutor":d["kind"].toString()=="decoration"?"Decorations":d["vending"].toBool()?"Drinks":"Shop";m.discovered=true;m.available=!pyramid;
            m.section=d["section"].toString();m.itemPayment=reminder||barter;
            m.currency=currency(d["currency"].toString());m.balance=balance(world,trainer,m.currency);
            if(d.contains("requiredItem")&&!owned(world,key,d["requiredItem"].toInt(),2)){
                m.available=false;m.availability="Bring your "+item(d["requiredItem"].toInt())["name"].toString()+".";
            }
            if(d["glassWorkshop"].toBool()&&u16(world,0x1518)!=2){
                m.available=false;m.availability=u16(world,0x1518)>=10?"Collect your waiting order at the Glass Workshop first.":"Speak to the glassmaker with your Soot Sack first.";
            }
            if(d["roof"].toBool()&&weather>=1&&weather<=3){m.available=false;m.availability="Rooftop closed during the unusual weather.";}
            if(d.contains("news")&&!news(world,d["news"].toInt())){m.available=false;m.availability="The clear-out sale is not running.";}
            if(pyramid)m.availability=out.error;
            const bool discount=d.contains("discountNews")&&news(world,d["discountNews"].toInt());
            auto stock=(flag(world,d["expandedFlag"].toInt())?d["expandedStock"]:d["stock"]).toArray();
            if(reminder){
                if(quint8(world[0x234])>6){out={};out.error="Your team could not be verified.";return out;}
                for(int i=0;i<quint8(world[0x234]);++i)for(int move:relearnable(world.mid(0x238+i*100,100)))if(!stock.contains(move))stock.append(move);
                if(stock.isEmpty())m.availability="Your team has no moves to remember.";
            }
            const bool decor=d["kind"].toString()=="decoration",coins=d["kind"].toString()=="coins";
            for(const auto& id:stock){
                const int value=id.toInt();
                if(d["kind"].toString()=="tutor"||reminder){
                    const auto fact=data()["moves"].toObject()[QString::number(value)].toObject();
                    const int price=reminder?1:d["prices"].toObject()[QString::number(value)].toInt();
                    if(fact.isEmpty()||price<=0||quint8(world[0x234])>6){out={};out.error="The lessons could not be verified.";return out;}
                    MerchantStock entry{value,reminder?0:price,0,0,fact["name"].toString(),"Move lesson",reminder?"relearn":"tutor"};
                    if(reminder){const int id=d["paymentItem"].toInt();entry.payments.append({id,1,owned(world,key,id,1),item(id)["name"].toString()});}
                    for(int i=0;i<quint8(world[0x234]);++i){
                        auto recipient=tutorRecipient(world.mid(0x238+i*100,100),i,value,reminder);
                        if(recipient.available&&(reminder?entry.payments[0].owned>=1:m.balance>=price))entry.maximum=1;
                        entry.recipients.append(std::move(recipient));
                    }
                    m.stock.append(std::move(entry));continue;
                }
                if(barter){
                    const auto reward=item(value);if(reward["pocket"].toInt()!=1){out={};out.error="The exchange could not be verified.";return out;}
                    MerchantStock entry{value,0,owned(world,key,value,1),0,reward["name"].toString(),"Items","barter"};
                    for(const auto& payment:d["payments"].toObject()[QString::number(value)].toArray()){
                        const auto cost=payment.toArray();const int id=cost[0].toInt();
                        entry.payments.append({id,cost[1].toInt(),owned(world,key,id,1),item(id)["name"].toString()});
                    }
                    auto candidate=world;
                    if(!entry.payments.isEmpty()&&payItems(candidate,key,entry.payments)&&add(candidate,key,value,1))entry.maximum=1;
                    m.stock.append(std::move(entry));continue;
                }
                const auto it=decor?decoration(value):item(value);
                const int p=it["pocket"].toInt(),price=d["prices"].toObject().value(QString::number(value)).toInt(it["price"].toInt())/(discount?2:1);
                if((coins?(value!=50&&value!=500):(it.isEmpty()||(!decor&&p!=1&&p!=3&&p!=4)))||price<=0){out={};out.error="The shop stock could not be verified.";return out;}
                const int coinCount=balance(world,trainer,MerchantCurrency::Coins);
                const int room=coins?(9999-coinCount)/value:decor?countDecor(world,value,true):roomFor(world,key,value,p);
                MerchantStock entry{value,price,coins?coinCount:decor?countDecor(world,value,false):owned(world,key,value,p),std::min({decor?1:d["limit"].toInt(99),m.balance/price,room}),
                    coins?QString("%1 Coins").arg(value):it["name"].toString(),coins?QString("Coin Case"):decor?QString("Decoration storage · ")+decorPockets[it["category"].toInt()].name:QString::fromUtf8(pockets[p].name)};
                entry.kind=coins?"coins":decor?"decoration":"item";m.stock.append(entry);
            }
        }
        out.merchants.append(m);
    }
    return out;
}
EmeraldShopWrite buyEmeraldShopBlock(const QByteArray& world,const QByteArray& trainer,const MerchantPurchase& request,quint32 drinkRoll) {
    const auto state=readEmeraldShopBlock(world,trainer);
    if(!state.supported)return {{},state.error,{}};
    const auto key=u32(trainer,0xac);
    if(request.quantity<1||request.quantity>99)return {{},"Choose a quantity from 1 to 99.",{}};
    for(const auto& shop:state.merchants)if(shop.discovered&&shop.available&&shop.id==request.merchantId)
        for(const auto& stock:shop.stock)if(stock.itemId==request.itemId&&stock.kind==request.kind){
            if(request.quantity>stock.maximum)return {{},"Not enough currency or storage space.",{}};
            auto result=world,trainerResult=trainer;const bool decor=stock.kind=="decoration",coins=stock.kind=="coins";
            if(stock.kind=="tutor"||stock.kind=="relearn"){
                if(request.quantity!=1||request.partySlot<0||request.partySlot>=stock.recipients.size())return {{},"Choose a team member first.",{}};
                const auto& recipient=stock.recipients[request.partySlot];
                if(!recipient.available||recipient.identity!=request.recipientIdentity||request.moveSlot<0||request.moveSlot>=recipient.moves.size()||!recipient.moves[request.moveSlot].available)
                    return {{},"This lesson or team member changed. Choose again.",{}};
                const auto mon=taughtRecord(world.mid(0x238+request.partySlot*100,100),request.itemId,request.moveSlot);
                if(mon.isEmpty())return {{},"The learned move could not be verified.",{}};
                result.replace(0x238+request.partySlot*100,100,mon);
                if(stock.kind=="relearn"){
                    if(!payItems(result,key,stock.payments))return {{},"Bring a Heart Scale for this lesson.",{}};
                }else setBalance(result,trainerResult,shop.currency,shop.balance-stock.price);
                if(!readEmeraldShopBlock(result,trainerResult).supported)return {{},"The lesson could not be verified.",{}};
                return {result,{},recipient.name+" learned "+stock.name+"!",trainerResult};
            }
            if(stock.kind=="barter"){
                if(request.quantity!=1||!payItems(result,key,stock.payments)||!add(result,key,stock.itemId,1))return {{},"Check your materials and Bag space.",{}};
                if(!readEmeraldShopBlock(result,trainerResult).supported)return {{},"The exchange could not be verified.",{}};
                return {result,{},stock.name+" added to your Bag!",trainerResult};
            }
            if(coins)setBalance(result,trainerResult,MerchantCurrency::Coins,balance(world,trainer,MerchantCurrency::Coins)+request.itemId*request.quantity);
            else if(!(decor?addDecor(result,request.itemId):add(result,key,request.itemId,request.quantity)))return {{},"There is no room for this purchase.",{}};
            QString bonus;
            if(stock.kind=="item"&&shop.currency==MerchantCurrency::Money&&request.itemId==4&&request.quantity>=10&&add(result,key,12,1))bonus=" A Premier Ball is included!";
            if(shop.id=="lilycove-rooftop-drinks"&&drinkRoll%64==0){
                int extra=0;if(add(result,key,request.itemId,1)){++extra;if((drinkRoll/64)%64==0&&add(result,key,request.itemId,1))++extra;}
                if(extra)bonus=QString(" %1 extra drink%2 dropped!").arg(extra).arg(extra>1?"s":"");
            }
            setBalance(result,trainerResult,shop.currency,shop.balance-stock.price*request.quantity);
            if(!readEmeraldShopBlock(result,trainerResult).supported)return {{},"The purchase could not be verified.",{}};
            return {result,{},QString("%1 × %2 added to %3.%4").arg(request.quantity).arg(stock.name,coins?"your Coin Case":decor?"your decoration storage":"your Bag",bonus),trainerResult};
        }
    return {{},"This item is not available here.",{}};
}
}
