#include "EmeraldShops.h"
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
    for(int p:{1,3,4}){
        QSet<int> seen;
        for(int i=0;i<pockets[p].count;++i){
            const int at=pockets[p].at+i*4,id=u16(world,at),qty=u16(world,at+2)^quint16(key);
            if(!id){if(qty)return false;continue;}
            if(item(id)["pocket"].toInt()!=p || qty<1 || qty>99 || (p==4&&seen.contains(id)))return false;
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
}
MerchantSnapshot readEmeraldShopBlock(const QByteArray& world,quint32 key) {
    MerchantSnapshot out;
    if(world.size()!=0x3d88 || data()["version"].toInt()!=2){out.error="The shops could not be checked.";return out;}
    const auto money=u32(world,0x490)^key;
    if(money>999999 || !validBag(world,key)){out.error="Your Bag or decorations could not be verified.";return out;}
    out.supported=true;out.balance=int(money);
    const bool pyramid=(quint8(world[4])==26 && (quint8(world[5])==26||quint8(world[5])==27))
        ||(quint8(world[4])==25 && quint8(world[5])>=44&&quint8(world[5])<=59);
    if(pyramid)out.error="Finish your Battle Pyramid challenge before shopping.";
    const int weather=u16(world,0x139c+2*0x5e);
    for(const auto& value:data()["merchants"].toArray()){
        const auto d=value.toObject();Merchant m;
        bool discovered=flag(world,d["visitedFlag"].toInt());
        for(const auto& f:d["requiredFlags"].toArray())discovered=discovered&&flag(world,f.toInt());
        if(d.contains("hiddenFlag"))discovered=discovered&&!flag(world,d["hiddenFlag"].toInt());
        if(discovered){
            m.id=d["id"].toString();m.name=d["name"].toString();m.location=d["location"].toString();m.group=d["group"].toString();
            m.category=d["kind"].toString()=="decoration"?"Decorations":d["vending"].toBool()?"Drinks":"Shop";m.discovered=true;m.available=!pyramid;
            if(d["roof"].toBool()&&weather>=1&&weather<=3){m.available=false;m.availability="Rooftop closed during the unusual weather.";}
            if(d.contains("news")&&!news(world,d["news"].toInt())){m.available=false;m.availability="The clear-out sale is not running.";}
            if(pyramid)m.availability=out.error;
            const bool discount=d.contains("discountNews")&&news(world,d["discountNews"].toInt());
            const auto stock=(flag(world,d["expandedFlag"].toInt())?d["expandedStock"]:d["stock"]).toArray();
            const bool decor=d["kind"].toString()=="decoration";
            for(const auto& id:stock){
                const auto it=decor?decoration(id.toInt()):item(id.toInt());const int p=it["pocket"].toInt(),price=it["price"].toInt()/(discount?2:1);
                if(it.isEmpty()||(!decor&&p!=1&&p!=3&&p!=4)||price<=0){out={};out.error="The shop stock could not be verified.";return out;}
                const int room=decor?countDecor(world,id.toInt(),true):roomFor(world,key,id.toInt(),p);
                MerchantStock entry{id.toInt(),price,decor?countDecor(world,id.toInt(),false):owned(world,key,id.toInt(),p),std::min({decor?1:d["limit"].toInt(99),int(money)/price,room}),it["name"].toString(),decor?QString("Decoration storage · ")+decorPockets[it["category"].toInt()].name:QString::fromUtf8(pockets[p].name)};
                entry.kind=decor?"decoration":"item";m.stock.append(entry);
            }
        }
        out.merchants.append(m);
    }
    return out;
}
MerchantWrite buyEmeraldShopBlock(const QByteArray& world,quint32 key,const MerchantPurchase& request,quint32 drinkRoll) {
    const auto state=readEmeraldShopBlock(world,key);
    if(!state.supported)return {{},state.error,{}};
    if(request.quantity<1||request.quantity>99)return {{},"Choose a quantity from 1 to 99.",{}};
    for(const auto& shop:state.merchants)if(shop.discovered&&shop.available&&shop.id==request.merchantId)
        for(const auto& stock:shop.stock)if(stock.itemId==request.itemId&&stock.kind==request.kind){
            if(request.quantity>stock.maximum)return {{},"Not enough money or storage space.",{}};
            auto result=world;const bool decor=stock.kind=="decoration";
            if(!(decor?addDecor(result,request.itemId):add(result,key,request.itemId,request.quantity)))return {{},"There is no room for this purchase.",{}};
            QString bonus;
            if(!decor&&request.itemId==4&&request.quantity>=10&&add(result,key,12,1))bonus=" A Premier Ball is included!";
            if(shop.id=="lilycove-rooftop-drinks"&&drinkRoll%64==0){
                int extra=0;if(add(result,key,request.itemId,1)){++extra;if((drinkRoll/64)%64==0&&add(result,key,request.itemId,1))++extra;}
                if(extra)bonus=QString(" %1 extra drink%2 dropped!").arg(extra).arg(extra>1?"s":"");
            }
            qToLittleEndian(quint32(state.balance-stock.price*request.quantity)^key,result.data()+0x490);
            if(!readEmeraldShopBlock(result,key).supported)return {{},"The purchase could not be verified.",{}};
            return {result,{},QString("%1 × %2 added to %3.%4").arg(request.quantity).arg(stock.name,decor?"your decoration storage":"your Bag",bonus)};
        }
    return {{},"This item is not available here.",{}};
}
}
