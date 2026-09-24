#include "EmeraldShops.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
struct Pocket {int at,count;const char* name;};
constexpr Pocket pockets[]={{0,0,""},{0x560,30,"Items"},{0x5d8,30,"Key items"},{0x650,16,"Poké Balls"},{0x690,64,"TMs & HMs"},{0x790,46,"Berries"}};
QJsonObject item(int id){return data()["items"].toObject()[QString::number(id)].toObject();}
bool validBag(const QByteArray& world,quint32 key) {
    for(int p:{1,3})for(int i=0;i<pockets[p].count;++i){
        const int at=pockets[p].at+i*4,id=u16(world,at),qty=u16(world,at+2)^quint16(key);
        if(!id){if(qty)return false;continue;}
        if(item(id)["pocket"].toInt()!=p || qty<1 || qty>99)return false;
    }
    return true;
}
int owned(const QByteArray& world,quint32 key,int id,int pocket){
    int n=0;const auto p=pockets[pocket];for(int i=0;i<p.count;++i)if(u16(world,p.at+4*i)==id)n+=u16(world,p.at+4*i+2)^quint16(key);return n;
}
int roomFor(const QByteArray& world,quint32 key,int id,int pocket){
    int n=0;const auto p=pockets[pocket];for(int i=0;i<p.count;++i){const int at=p.at+4*i,other=u16(world,at);if(!other)n+=99;else if(other==id)n+=99-(u16(world,at+2)^quint16(key));}return n;
}
bool add(QByteArray& world,quint32 key,int id,int count) {
    const int pocket=item(id)["pocket"].toInt();if(pocket!=1&&pocket!=3)return false;
    if(roomFor(world,key,id,pocket)<count)return false;
    const auto p=pockets[pocket];
    for(int pass=0;pass<2 && count;++pass)for(int i=0;i<p.count && count;++i){
        const int at=p.at+4*i,other=u16(world,at);if((pass==0&&other!=id)||(pass==1&&other))continue;
        const int qty=other?(u16(world,at+2)^quint16(key)):0,amount=std::min(count,99-qty);
        qToLittleEndian(quint16(id),world.data()+at);qToLittleEndian(quint16((qty+amount)^quint16(key)),world.data()+at+2);count-=amount;
    }
    return !count;
}
}
MerchantSnapshot readEmeraldShopBlock(const QByteArray& world,quint32 key) {
    MerchantSnapshot out;
    if(world.size()!=0x3d88 || data()["merchants"].toArray().size()!=11){out.error="The shops could not be checked.";return out;}
    const auto money=u32(world,0x490)^key;
    if(money>999999 || !validBag(world,key)){out.error="Your Bag or money could not be verified.";return out;}
    out.supported=true;out.balance=int(money);
    const bool pyramid=(quint8(world[4])==26 && (quint8(world[5])==26||quint8(world[5])==27))
        ||(quint8(world[4])==25 && quint8(world[5])>=44&&quint8(world[5])<=59);
    if(pyramid)out.error="Finish your Battle Pyramid challenge before shopping.";
    for(const auto& value:data()["merchants"].toArray()){
        const auto d=value.toObject();Merchant m;
        if(flag(world,d["visitedFlag"].toInt())){
            m.id=d["id"].toString();m.name=d["name"].toString();m.location=d["location"].toString();m.category="Poké Mart";m.discovered=true;m.available=!pyramid;
            const auto stock=(flag(world,d["expandedFlag"].toInt())?d["expandedStock"]:d["stock"]).toArray();
            for(const auto& id:stock){const auto it=item(id.toInt());const int p=it["pocket"].toInt(),price=it["price"].toInt();
                if((p!=1&&p!=3)||price<=0){out={};out.error="The shop stock could not be verified.";return out;}
                m.stock.append({id.toInt(),price,owned(world,key,id.toInt(),p),std::min({99,int(money)/price,roomFor(world,key,id.toInt(),p)}),it["name"].toString(),QString::fromUtf8(pockets[p].name)});
            }
        }
        out.merchants.append(m);
    }
    return out;
}
MerchantWrite buyEmeraldShopBlock(const QByteArray& world,quint32 key,const MerchantPurchase& request) {
    const auto state=readEmeraldShopBlock(world,key);
    if(!state.supported)return {{},state.error,{}};
    if(request.quantity<1||request.quantity>99)return {{},"Choose a quantity from 1 to 99.",{}};
    for(const auto& shop:state.merchants)if(shop.discovered&&shop.available&&shop.id==request.merchantId)
        for(const auto& stock:shop.stock)if(stock.itemId==request.itemId){
            if(request.quantity>stock.maximum)return {{},"Not enough money or room in your Bag.",{}};
            auto result=world;
            if(!add(result,key,request.itemId,request.quantity))return {{},"Your Bag is full.",{}};
            const bool bonus=request.itemId==4&&request.quantity>=10&&add(result,key,12,1);
            qToLittleEndian(quint32(state.balance-stock.price*request.quantity)^key,result.data()+0x490);
            const auto checked=readEmeraldShopBlock(result,key);
            if(!checked.supported)return {{},"The purchase could not be verified.",{}};
            return {result,{},QString("%1 × %2 added to your Bag.%3").arg(request.quantity).arg(stock.name,bonus?" A Premier Ball is included!":"")};
        }
    return {{},"This item is not available here.",{}};
}
}
