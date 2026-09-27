#include "integrations/progress/EmeraldShops.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include "integrations/progress/Gen3Progress.h"
#include "integrations/progress/EmeraldParty.h"
#include "integrations/progress/GameProgressService.h"
#include "features/home/BadgeAssets.h"
#include <QImage>
#include <QtTest>
#include <QtEndian>
#include <QFile>
#include <QTemporaryDir>
#include <QSemaphore>
#include <QCryptographicHash>
#include <atomic>
#include <algorithm>

using namespace trainer;
namespace {
const QString EmeraldHash = "a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af";
void put16(QByteArray& b, int p, quint16 v) { qToLittleEndian(v, b.data() + p); }
void put32(QByteArray& b, int p, quint32 v) { qToLittleEndian(v, b.data() + p); }
// Original synthetic records. No game/BIOS/save bytes are distributed.
QByteArray slot(Gen3Edition edition, quint32 counter, int rotation, int mask, int caught, int seen = -1, bool inconsistentSeen = false) {
    QByteArray small(edition == Gen3Edition::Emerald ? 0xf2c : 0xf24, 0);
    QByteArray large(edition == Gen3Edition::Emerald ? 0x3d88 : 0x3d68, 0);
    QByteArray boxes(0x83d0, 0);
    for (int i = 0; i < caught; ++i) small[0x28 + i / 8] = char(quint8(small[0x28 + i / 8]) | (1 << (i % 8)));
    {
        const int seen1=edition==Gen3Edition::Emerald?0x988:0x5f8,seen2=edition==Gen3Edition::Emerald?0x3b24:0x3a18;
        for (int i = 0; i < (seen < 0 ? caught : seen); ++i) {
            for (int base : {0x5c}) small[base + i / 8] = char(quint8(small[base + i / 8]) | (1 << (i % 8)));
            for (int base : {seen1,seen2}) large[base + i / 8] = char(quint8(large[base + i / 8]) | (1 << (i % 8)));
        }
        if (inconsistentSeen) large[seen1] ^= 1;
    }
    const int flags = edition == Gen3Edition::Emerald ? 0x1270 : 0xee0;
    const int flag = edition == Gen3Edition::Emerald ? 0x867 : 0x820;
    for (int i = 0; i < 8; ++i) if (mask & (1 << i)) {
        const int at = flags + (flag + i) / 8;
        large[at] = char(quint8(large[at]) | (1 << ((flag + i) % 8)));
    }
    QByteArray result(14 * 0x1000, char(0xff));
    for (int id = 0; id < 14; ++id) {
        const QByteArray data = id == 0 ? small : id < 5 ? large.mid((id - 1) * 0xf80, 0xf80) : boxes.mid((id - 5) * 0xf80, 0xf80);
        const int at = ((id + rotation) % 14) * 0x1000;
        result.replace(at, data.size(), data);
        quint32 sum = 0;
        for (int i = 0; i < data.size(); i += 4) sum += qFromLittleEndian<quint32>(data.constData() + i);
        put16(result, at + 0xff4, id); put16(result, at + 0xff6, quint16((sum >> 16) + sum));
        put32(result, at + 0xff8, 0x08012025); put32(result, at + 0xffc, counter);
    }
    return result;
}
QByteArray save(Gen3Edition edition, quint32 a = 10, quint32 b = 11) {
    return slot(edition, a, 5, 0x09, 12) + slot(edition, b, 11, 0xa5, 241) + QByteArray(4 * 0x1000, char(0xff));
}
void write(const QString& path, const QByteArray& bytes) {
    QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(bytes), bytes.size());
}
AdventureRegistration record(const QString& id = "test") {
    AdventureRegistration r; r.adventure.id = id; r.adventure.adapterId = "retroarch";
    r.integrationConfig = {{"core", "mgba"}}; return r;
}
QByteArray pokemonFixture(quint32 personality = 0, int species = 25, bool egg = false, bool party = true) {
    QByteArray bytes(party ? 100 : 80, 0), growth(12,0), attacks(12,0), ev(12,0), misc(12,0);
    put32(bytes,0,personality); put32(bytes,4,0x12345678);
    bytes[8]=char(0xbb); bytes[9]=char(0xff); bytes[18]=2; bytes[19]=egg ? 6 : 2;
    put16(growth,0,species); put16(growth,2,215); put32(growth,4,125); growth[8]=3; // Charcoal; Pikachu level 5, three PP Ups.
    put16(attacks,0,33); attacks[8]=40; // Tackle, maximum 56 PP in Emerald.
    put32(misc,4,0x3fffffffu | (egg ? 0x40000000u : 0));
    std::array<QByteArray,4> logical{growth,attacks,ev,misc};
    std::array<int,4> order{0,1,2,3};
    for(quint32 i=0;i<personality%24;++i) std::next_permutation(order.begin(),order.end());
    QByteArray clear;for(auto index:order) clear+=logical[index];
    quint16 sum=0;for(int i=0;i<48;i+=2)sum=quint16(sum+qFromLittleEndian<quint16>(clear.constData()+i));
    put16(bytes,28,sum);
    for(int i=0;i<48;i+=4)put32(bytes,32+i,qFromLittleEndian<quint32>(clear.constData()+i)^personality^0x12345678u);
    if(party) { bytes[84]=5; put16(bytes,86,0);for(int i=0;i<6;++i)put16(bytes,88+2*i,20+i); }
    return bytes;
}
QByteArray setTestMoves(QByteArray record,const std::array<int,4>& moves,int bonuses=0xe7) {
    const auto personality=qFromLittleEndian<quint32>(record.constData()),key=personality^qFromLittleEndian<quint32>(record.constData()+4);
    std::array<int,4> order{0,1,2,3};for(quint32 n=0;n<personality%24;++n)std::next_permutation(order.begin(),order.end());
    QByteArray clear=record.mid(32,48);for(int p=0;p<48;p+=4)put32(clear,p,qFromLittleEndian<quint32>(clear.constData()+p)^key);
    const int growth=int(std::find(order.begin(),order.end(),0)-order.begin())*12,attacks=int(std::find(order.begin(),order.end(),1)-order.begin())*12;
    clear[growth+8]=char(bonuses);
    for(int m=0;m<4;++m){put16(clear,attacks+2*m,moves[m]);clear[attacks+8+m]=moves[m]?1:0;}
    quint16 sum=0;for(int p=0;p<48;p+=2)sum=quint16(sum+qFromLittleEndian<quint16>(clear.constData()+p));put16(record,28,sum);
    for(int p=0;p<48;p+=4)put32(record,32+p,qFromLittleEndian<quint32>(clear.constData()+p)^key);
    for(int level=1;level<=100;++level){record[84]=char(level);if(readEmeraldPartyMember(record).kind==PokemonSlotKind::Known)break;}
    return record;
}

}
class GameProgressTests : public QObject {
    Q_OBJECT
private slots:
    void fireRedPartyDexAndProtectedHealingUseTheirOwnLayout() {
        const QString hash="3d0c79f1627022e18765766f6cb5ea067f6b5bf7dca115552189ad65a5c3a8ac";
        for(int rotation=0;rotation<14;++rotation) {
            auto bytes=slot(Gen3Edition::FireRed,10,0,1,12)+slot(Gen3Edition::FireRed,11,rotation,255,241)+QByteArray(0x4000,0);
            const int at=0xe000+((1+rotation)%14)*0x1000;bytes[at+0x34]=1;
            bytes.replace(at+0x38,100,pokemonFixture(rotation%24));
            quint32 sum=0;for(int p=0;p<0xf80;p+=4)sum+=qFromLittleEndian<quint32>(bytes.constData()+at+p);
            put16(bytes,at+0xff6,quint16(sum+(sum>>16)));
            const auto before=readGen3Progress(bytes,Gen3Edition::FireRed);
            QVERIFY(before.party);QVERIFY(before.party->error.isEmpty());QCOMPARE(before.party->party[0].speciesId,"pikachu");
            QCOMPARE(before.party->party[0].hp.value(),0);QCOMPARE(before.party->boxes.size(),14);
            QVERIFY(before.pokedex);QVERIFY(before.pokedex->error.isEmpty());QCOMPARE(before.pokedex->caught.size(),241);
            const auto healed=healGen3Party(bytes,hash);QVERIFY2(healed.error.isEmpty(),qPrintable(healed.error));
            QCOMPARE(healed.partyCount,1);const auto after=readGen3Progress(healed.data,Gen3Edition::FireRed);
            QCOMPARE(after.party->party[0].hp.value(),20);QCOMPARE(after.party->party[0].moves[0].pp,56);
            for(int i=0;i<bytes.size();++i)if(bytes[i]!=healed.data[i])QVERIFY((i>=at+0x38 && i<at+0x38+100)||i==at+0xff6||i==at+0xff7);
            QVERIFY(healEmeraldParty(bytes,hash).data.isEmpty());
            auto wrong=bytes;wrong[at+0x38+28]^=1;QVERIFY(healGen3Party(wrong,hash).data.isEmpty());
        }
    }
    void privateFireRedReadback() {
        const auto path=qEnvironmentVariable("TRAINEROS_FIRERED_SAMPLE");if(path.isEmpty())QSKIP("Private FireRed example is optional");
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();
        const auto p=readGen3Progress(bytes,Gen3Edition::FireRed);QCOMPARE(p.availability,ProgressAvailability::Available);
        QVERIFY(p.party);QVERIFY(p.party->error.isEmpty());QVERIFY(p.pokedex);QVERIFY2(p.pokedex->error.isEmpty(),qPrintable(p.pokedex->error));
        int bad=0;for(const auto& mon:p.party->party){bad+=mon.kind==PokemonSlotKind::Unreadable;qInfo()<<mon.speciesName<<mon.level<<mon.hp.value_or(-1);}
        for(const auto& box:p.party->boxes)for(const auto& mon:box.members)bad+=mon.kind==PokemonSlotKind::Unreadable;
        QCOMPARE(bad,0);
        const auto healed=healGen3Party(bytes,"3d0c79f1627022e18765766f6cb5ea067f6b5bf7dca115552189ad65a5c3a8ac");
        QVERIFY2(healed.error.isEmpty(),qPrintable(healed.error));
    }
    void emeraldItemPaymentsAndRelearning() {
        QFile file(":/progress/emerald-shops.json");QVERIFY(file.open(QIODevice::ReadOnly));const auto facts=QJsonDocument::fromJson(file.readAll()).object();
        for(quint32 key:{0u,0x85ce1972u}){
            QByteArray world(0x3d88,0),trainer(0xf2c,0);put32(trainer,0xac,key);put32(trainer,0x1f4,key);put32(world,0x490,key);put16(world,0x494,quint16(key));
            for(auto p:{std::pair{0x560,30},std::pair{0x5d8,30},std::pair{0x650,16},std::pair{0x690,64}})for(int i=0;i<p.second;++i)put16(world,p.first+i*4+2,quint16(key));
            for(const auto& value:facts["merchants"].toArray()){
                const auto d=value.toObject();const int f=d["visitedFlag"].toInt();world[0x1270+f/8]|=char(1<<(f%8));
            }
            auto bag=[&](QByteArray& w,int slot,int item,int count){put16(w,0x560+slot*4,item);put16(w,0x562+slot*4,quint16(count)^quint16(key));};
            int exchanges=0;
            for(const auto& value:facts["merchants"].toArray()){
                const auto d=value.toObject();if(d["kind"].toString()!="barter")continue;
                for(const auto& offer:d["stock"].toArray()){
                    const int reward=offer.toInt();const auto costs=d["payments"].toObject()[QString::number(reward)].toArray();
                    auto seed=world;
                    // A completely full pocket must accept a trade when payment frees room.
                    for(int i=0;i<30;++i)bag(seed,i,13,99);
                    for(int i=0;i<costs.size();++i)bag(seed,i,costs[i].toArray()[0].toInt(),costs[i].toArray()[1].toInt());
                    const MerchantPurchase req{d["id"].toString(),reward,1,"barter"};const auto traded=buyEmeraldShopBlock(seed,trainer,req);
                    QVERIFY2(traded.error.isEmpty(),qPrintable(traded.error));QCOMPARE(traded.trainer,trainer);
                    auto expected=seed;for(int i=0;i<costs.size();++i)bag(expected,i,0,0);bag(expected,0,reward,1);QCOMPARE(traded.data,expected);
                    auto missing=seed;bag(missing,0,0,0);QVERIFY(buyEmeraldShopBlock(missing,trainer,req).data.isEmpty());
                    auto full=seed;for(int i=0;i<costs.size();++i)bag(full,i,costs[i].toArray()[0].toInt(),costs[i].toArray()[1].toInt()+1);
                    QVERIFY(buyEmeraldShopBlock(full,trainer,req).data.isEmpty());
                    auto bad=req;bad.quantity=2;QVERIFY(buyEmeraldShopBlock(seed,trainer,bad).data.isEmpty());++exchanges;
                }
            }
            QCOMPARE(exchanges,5);
            for(int order=0;order<24;++order){
                auto seed=world;seed[0x234]=1;const auto mon=setTestMoves(pokemonFixture(order,25),{33,0,0,0},3);seed.replace(0x238,100,mon);
                bag(seed,0,111,2); // Heart Scale, ordinary Items pocket.
                bool found=false;
                for(const auto& m:readEmeraldShopBlock(seed,trainer).merchants)if(m.id=="fallarbor-relearner"){
                    QSet<int> offered;for(const auto& st:m.stock){offered.insert(st.itemId);if(st.itemId!=45)continue;found=true;
                        QCOMPARE(st.payments.size(),1);QCOMPARE(st.payments[0].itemId,111);QCOMPARE(st.maximum,1);
                        MerchantPurchase req{m.id,45,1,"relearn",0,1,st.recipients[0].identity};
                        const auto learned=buyEmeraldShopBlock(seed,trainer,req);QVERIFY2(learned.error.isEmpty(),qPrintable(learned.error));QCOMPARE(learned.trainer,trainer);
                        auto expected=seed;bag(expected,0,111,1);expected.replace(0x238,100,learned.data.mid(0x238,100));QCOMPARE(learned.data,expected);
                        const auto parsed=readEmeraldPartyMember(learned.data.mid(0x238,100));QCOMPARE(parsed.moves[1].name,QString("Growl"));QCOMPARE(parsed.moves[1].pp,40);
                        auto absent=seed;bag(absent,0,0,0);QVERIFY(buyEmeraldShopBlock(absent,trainer,req).data.isEmpty());
                        req.recipientIdentity="changed";QVERIFY(buyEmeraldShopBlock(seed,trainer,req).data.isEmpty());
                    }
                    QVERIFY(offered.contains(45));QVERIFY(!offered.contains(98)); // Quick Attack requires level 11.
                }
                QVERIFY(found);
            }
        }
    }
    void emeraldLessonsRespectSpeciesSlotsAndExactMutation() {
        QFile f(":/progress/emerald-shops.json");QVERIFY(f.open(QIODevice::ReadOnly));const auto facts=QJsonDocument::fromJson(f.readAll()).object();
        for(int permutation=0;permutation<24;++permutation){
            QByteArray world(0x3d88,0),trainer(0xf2c,0);put16(trainer,0xeb8,300);
            world[0x1270+0x8a8/8]|=char(1<<(0x8a8%8));world[0x234]=2;
            const auto mon=setTestMoves(pokemonFixture(permutation,151),{33,45,84,57}); // Mew; Surf in the last slot.
            QVERIFY(readEmeraldPartyMember(mon).kind==PokemonSlotKind::Known);
            world.replace(0x238,100,mon);world.replace(0x29c,100,pokemonFixture(permutation,25,true));
            const auto state=readEmeraldShopBlock(world,trainer);QVERIFY(state.supported);int tested=0;
            for(const auto& m:state.merchants)if(m.section=="services")for(const auto& stock:m.stock)if(stock.kind=="tutor"){
                QCOMPARE(stock.recipients.size(),2);const auto& recipient=stock.recipients[0];QVERIFY(recipient.available);QVERIFY(!stock.recipients[1].available);
                QCOMPARE(recipient.moves.size(),4);QVERIFY(!recipient.moves[3].available);
                MerchantPurchase req{m.id,stock.itemId,1,"tutor",0,permutation%3,recipient.identity};
                const auto changed=buyEmeraldShopBlock(world,trainer,req);QVERIFY2(changed.error.isEmpty(),qPrintable(changed.error));
                const auto after=changed.data.mid(0x238,100);const auto parsed=readEmeraldPartyMember(after),before=readEmeraldPartyMember(mon);
                QCOMPARE(parsed.moves[req.moveSlot].name,stock.name);const int pp=facts["tutors"].toObject()[QString::number(stock.itemId)].toObject()["pp"].toInt();
                QCOMPARE(parsed.moves[req.moveSlot].pp,pp);QCOMPARE(parsed.moves[req.moveSlot].maxPp,pp);
                for(int m=0;m<4;++m)if(m!=req.moveSlot){QCOMPARE(parsed.moves[m].name,before.moves[m].name);QCOMPARE(parsed.moves[m].pp,before.moves[m].pp);QCOMPARE(parsed.moves[m].maxPp,before.moves[m].maxPp);}
                QCOMPARE(parsed.hp,before.hp);QCOMPARE(parsed.condition,before.condition);QCOMPARE(parsed.item,before.item);
                auto expectedTrainer=trainer;put16(expectedTrainer,0xeb8,300-stock.price);QCOMPARE(changed.trainer,expectedTrainer);
                auto expectedWorld=world;expectedWorld.replace(0x238,100,after);QCOMPARE(changed.data,expectedWorld);
                // Only move IDs/PP/that slot's PP-Up bits and the individual checksum may differ.
                std::array<int,4> order{0,1,2,3};for(int n=0;n<permutation;++n)std::next_permutation(order.begin(),order.end());
                const int g=int(std::find(order.begin(),order.end(),0)-order.begin())*12,a=int(std::find(order.begin(),order.end(),1)-order.begin())*12;
                for(int at=0;at<100;++at){if(at==28||at==29||at==32+g+8||at==32+a+req.moveSlot*2||at==33+a+req.moveSlot*2||at==32+a+8+req.moveSlot)continue;QCOMPARE(after[at],mon[at]);}
                auto bad=req;bad.moveSlot=3;QVERIFY(buyEmeraldShopBlock(world,trainer,bad).data.isEmpty());
                bad=req;bad.recipientIdentity="stale";QVERIFY(buyEmeraldShopBlock(world,trainer,bad).data.isEmpty());
                bad=req;bad.partySlot=1;QVERIFY(buyEmeraldShopBlock(world,trainer,bad).data.isEmpty());
                QVERIFY(buyEmeraldShopBlock(changed.data,changed.trainer,req).data.isEmpty());++tested;
            }
            QCOMPARE(tested,20);
            // Empty slots get filled first; incompatible species cannot receive a lesson.
            world[0x234]=1;world.replace(0x238,100,setTestMoves(pokemonFixture(permutation),{33,0,0,0},3));
            auto now=readEmeraldShopBlock(world,trainer);
            for(const auto& m:now.merchants)if(m.id=="frontier-tutor-left"){
                QVERIFY(!m.stock[0].recipients[0].available); // Pikachu cannot learn Soft-Boiled.
                const auto& st=m.stock[3]; // Mega Punch.
                QVERIFY(st.recipients[0].available);QVERIFY(!st.recipients[0].moves[0].available);QVERIFY(st.recipients[0].moves[1].available);
                MerchantPurchase req{m.id,st.itemId,1,"tutor",0,1,st.recipients[0].identity};QVERIFY(buyEmeraldShopBlock(world,trainer,req).error.isEmpty());
                req.moveSlot=2;QVERIFY(buyEmeraldShopBlock(world,trainer,req).data.isEmpty());
                put16(trainer,0xeb8,0);req.moveSlot=1;QVERIFY(buyEmeraldShopBlock(world,trainer,req).data.isEmpty());
            }
            world[0x238+32]^=1;now=readEmeraldShopBlock(world,trainer);
            for(const auto& m:now.merchants)if(m.section=="services")for(const auto& st:m.stock)QVERIFY(!st.recipients[0].available);
        }
    }
    void emeraldCurrenciesDebitOnlyTheSelectedWallet() {
        for(quint32 key:{0u,0x85ce1972u}){
            QByteArray world(0x3d88,0),trainer(0xf2c,0);
            put32(trainer,0xac,key);put32(trainer,0x1f4,50000^key);put16(trainer,0xeb8,5000);
            put32(world,0x490,500000^key);put16(world,0x494,5000^quint16(key));put16(world,0x142c,9000);put16(world,0x1518,2);
            for(auto p:{std::pair{0x560,30},std::pair{0x5d8,30},std::pair{0x650,16},std::pair{0x690,64}})
                for(int i=0;i<p.second;++i)put16(world,p.first+4*i+2,quint16(key));
            for(auto p:{std::pair{0,260},std::pair{1,270},std::pair{2,372}}){put16(world,0x5d8+4*p.first,p.second);put16(world,0x5da+4*p.first,1^quint16(key));}
            for(int f:{0x878,0x8a8,0x8a4,0x877,0x151})world[0x1270+f/8]|=char(1<<(f%8));
            const auto get=[](const MerchantSnapshot& s,const QString& id){for(const auto& m:s.merchants)if(m.id==id)return m;return Merchant{};};
            const auto before=readEmeraldShopBlock(world,trainer);QVERIFY(before.supported);
            int offers=0;
            for(const auto& m:before.merchants){
                if(m.currency==MerchantCurrency::Money&&m.id!="game-corner-coins")continue;
                if(!m.discovered||m.section=="services")continue;
                QVERIFY(m.available);
                for(const auto& s:m.stock){
                    const MerchantPurchase request{m.id,s.itemId,1,s.kind};
                    auto result=buyEmeraldShopBlock(world,trainer,request);QVERIFY2(result.error.isEmpty(),qPrintable(m.id+result.error));
                    const auto after=readEmeraldShopBlock(result.data,result.trainer);QVERIFY(after.supported);
                    QCOMPARE(get(after,m.id).balance,m.balance-s.price);
                    const auto target=get(after,m.id);
                    for(const auto& t:target.stock)if(t.itemId==s.itemId&&t.kind==s.kind)QCOMPARE(t.owned,s.owned+(s.kind=="coins"?s.itemId:1));
                    for(const QString& wallet:{QString("game-corner-coins"),QString("game-corner-tms"),QString("frontier-vitamin"),QString("glass-item"),QString("slateport-powder")}){
                        const auto a=get(before,wallet),b=get(after,wallet);
                        if(a.currency==m.currency)continue;
                        QCOMPARE(b.balance,a.balance+(s.kind=="coins"&&a.currency==MerchantCurrency::Coins?s.itemId:0));
                    }
                    // No unrelated trainer data, Party, story flags, storage or game statistics changes.
                    for(int i=0;i<trainer.size();++i){
                        if(m.currency==MerchantCurrency::BattlePoints&&i>=0xeb8&&i<0xeba)continue;
                        if(m.currency==MerchantCurrency::BerryPowder&&i>=0x1f4&&i<0x1f8)continue;
                        QCOMPARE(result.trainer[i],trainer[i]);
                    }
                    for(int i=0;i<world.size();++i){
                        if((i>=0x490&&i<0x496)||(i>=0x560&&i<0x5d8)||(i>=0x690&&i<0x790)||(i>=0x2734&&i<0x27ca)||(i>=0x142c&&i<0x142e))continue;
                        QCOMPARE(result.data[i],world[i]);
                    }
                    auto invalid=request;invalid.quantity=2;QVERIFY(buyEmeraldShopBlock(world,trainer,invalid).data.isEmpty());++offers;
                }
            }
            QCOMPARE(offers,58);
            // Missing purses, pending glass orders, full currency and insufficient funds reject without a candidate.
            put16(world,0x5d8,0);put16(world,0x5da,quint16(key));
            QVERIFY(!get(readEmeraldShopBlock(world,trainer),"game-corner-coins").available);
            QVERIFY(buyEmeraldShopBlock(world,trainer,{"game-corner-coins",50,1,"coins"}).data.isEmpty());
            put16(world,0x5d8,260);put16(world,0x5da,1^quint16(key));
            put16(world,0x494,9950^quint16(key));QVERIFY(buyEmeraldShopBlock(world,trainer,{"game-corner-coins",50,1,"coins"}).data.isEmpty());
            put16(world,0x494,9949^quint16(key));auto full=buyEmeraldShopBlock(world,trainer,{"game-corner-coins",50,1,"coins"});
            QVERIFY(full.error.isEmpty());QCOMPARE(get(readEmeraldShopBlock(full.data,full.trainer),"game-corner-tms").balance,9999);
            for(int state:{0,1,10,16}){put16(world,0x1518,state);QVERIFY(buyEmeraldShopBlock(world,trainer,{"glass-item",39,1}).data.isEmpty());}
            put16(world,0x1518,2);put16(world,0x142c,249);QVERIFY(buyEmeraldShopBlock(world,trainer,{"glass-item",39,1}).data.isEmpty());
            put16(trainer,0xeb8,0);QVERIFY(buyEmeraldShopBlock(world,trainer,{"frontier-vitamin",64,1}).data.isEmpty());
            put32(trainer,0x1f4,49^key);QVERIFY(buyEmeraldShopBlock(world,trainer,{"slateport-powder",30,1}).data.isEmpty());
            put32(trainer,0x1f4,100000^key);QVERIFY(!readEmeraldShopBlock(world,trainer).supported);
            put32(trainer,0x1f4,key);put16(trainer,0xeb8,10000);QVERIFY(!readEmeraldShopBlock(world,trainer).supported);
            put16(trainer,0xeb8,0);put16(world,0x142c,10000);QVERIFY(!readEmeraldShopBlock(world,trainer).supported);
            QVERIFY(!readEmeraldShopBlock(world,trainer.left(0x100)).supported);
        }
    }
    void emeraldSpecialShopsRespectStockStorageAndConditions() {
        constexpr quint32 key=0x1234abcd;
        QByteArray world(0x3d88,0),trainer(0xf2c,0);
        put32(trainer,0xac,key);put32(trainer,0x1f4,key);put16(world,0x494,quint16(key));
        for(auto p:{std::pair{0x560,30},std::pair{0x5d8,30},std::pair{0x650,16},std::pair{0x690,64}})
            for(int i=0;i<p.second;++i)put16(world,p.first+4*i+2,quint16(key));
        put32(world,0x490,999999^key);
        QVERIFY(readEmeraldShopBlock(world,trainer).supported);
        QFile file(":/progress/emerald-shops.json");QVERIFY(file.open(QIODevice::ReadOnly));
        const auto facts=QJsonDocument::fromJson(file.readAll()).object();
        auto flag=[&](int id){if(id>0)world[0x1270+id/8]=char(quint8(world[0x1270+id/8])|(1<<(id%8)));};
        for(const auto& value:facts["merchants"].toArray()){
            const auto m=value.toObject();flag(m["visitedFlag"].toInt());flag(m["expandedFlag"].toInt());
            for(auto f:m["requiredFlags"].toArray())flag(f.toInt());
        }
        world[0x2b50]=1;world[0x2b51]=2;world[0x2b54]=3;world[0x2b55]=2;
        const auto state=readEmeraldShopBlock(world,trainer);QVERIFY(state.supported);QCOMPARE(state.merchants.size(),51);
        int departments=0,offers=0;
        for(const auto& m:state.merchants){
            if(m.currency!=MerchantCurrency::Money||m.id=="game-corner-coins"||m.itemPayment)continue;
            QVERIFY2(m.discovered&&m.available,qPrintable(m.id));if(m.group=="Lilycove Department Store")++departments;
            for(const auto& item:m.stock){
                const auto bought=buyEmeraldShopBlock(world,trainer,{m.id,item.itemId,1,item.kind});
                QVERIFY2(bought.error.isEmpty(),qPrintable(m.id+": "+item.name+": "+bought.error));
                QCOMPARE(readEmeraldShopBlock(bought.data,bought.trainer).balance,999999-item.price);++offers;
                for(int at=0;at<world.size();++at){
                    if((at>=0x490&&at<0x494)||(at>=0x560&&at<0x5d8)||(at>=0x650&&at<0x790)||(at>=0x2734&&at<0x27ca))continue;
                    QCOMPARE(bought.data[at],world[at]);
                }
            }
        }
        QCOMPARE(departments,12);QVERIFY(offers>180);
        const auto get=[&](const QString& id){for(const auto& m:readEmeraldShopBlock(world,trainer).merchants)if(m.id==id)return m;return Merchant{};};
        QCOMPARE(get("slateport-market-0").stock[0].price,4900);
        QCOMPARE(get("lilycove-3f-0").stock[0].price,9800);
        const auto tm=get("lilycove-4f-0").stock[0];
        put16(world,0x690,tm.itemId);put16(world,0x692,98^quint16(key));
        QVERIFY(!buyEmeraldShopBlock(world,trainer,{"lilycove-4f-0",tm.itemId,2}).error.isEmpty());
        QVERIFY(buyEmeraldShopBlock(world,trainer,{"lilycove-4f-0",tm.itemId,1}).error.isEmpty());
        // Decorations cannot be confused with same-numbered Bag items or exceed category capacity.
        const auto doll=get("lilycove-5f-0").stock[0];
        QVERIFY(!buyEmeraldShopBlock(world,trainer,{"lilycove-5f-0",doll.itemId,1,"item"}).error.isEmpty());
        const auto bought=buyEmeraldShopBlock(world,trainer,{"lilycove-5f-0",doll.itemId,1,"decoration"});
        QCOMPARE(quint8(bought.data[0x2798]),quint8(doll.itemId));
        for(int i=0;i<40;++i)world[0x2798+i]=char(doll.itemId);
        QVERIFY(!buyEmeraldShopBlock(world,trainer,{"lilycove-5f-0",doll.itemId,1,"decoration"}).error.isEmpty());
        world[0x2798]=1;QVERIFY(!readEmeraldShopBlock(world,trainer).supported);world[0x2798]=char(doll.itemId);
        // The roof remains known, but weather and the news event control availability.
        put16(world,0x139c+2*0x5e,2);QVERIFY(!get("lilycove-rooftop-drinks").available);
        put16(world,0x139c+2*0x5e,0);world[0x2b55]=0;QVERIFY(!get("lilycove-rooftop-sale").available);
        const auto drink=get("lilycove-rooftop-drinks").stock[0];
        const auto extra=buyEmeraldShopBlock(world,trainer,{"lilycove-rooftop-drinks",drink.itemId,1},0);
        QVERIFY(extra.message.contains("2 extra drinks"));
        QCOMPARE(qFromLittleEndian<quint16>(extra.data.constData()+0x562)^quint16(key),3);
        QVERIFY(!buyEmeraldShopBlock(world,trainer,{"lilycove-rooftop-drinks",drink.itemId,2}).error.isEmpty());
    }
    void emeraldShopsFollowFlagsAndProtectPurchases() {
        for(int rotation=0;rotation<14;++rotation)for(quint32 key:{0u,0x85ce1972u}) {
            auto bytes=slot(Gen3Edition::Emerald,0xffffffffu,3,0,0)+slot(Gen3Edition::Emerald,0,rotation,0,0)+QByteArray(0x4000,char(0xff));
            const int block0=(14+rotation)*0x1000,block1=(14+(1+rotation)%14)*0x1000,block2=(14+(2+rotation)%14)*0x1000;
            auto seal=[&]{for(int id=0;id<14;++id){const int at=(14+(id+rotation)%14)*0x1000,n=id==0?0xf2c:id==4?0xf08:id==13?0x7d0:0xf80;quint32 sum=0;for(int p=0;p<n;p+=4)sum+=qFromLittleEndian<quint32>(bytes.constData()+at+p);put16(bytes,at+0xff6,quint16((sum>>16)+sum));}};
            auto flag=[&](int id){const int at=block2+0x1270-0xf80+id/8;bytes[at]=char(quint8(bytes[at])|(1<<(id%8)));};
            put32(bytes,block0+0xac,key);put32(bytes,block0+0x1f4,key);put16(bytes,block1+0x494,quint16(key));put32(bytes,block1+0x490,10000^key);
            for(auto pocket: {std::pair{0x560,30},std::pair{0x5d8,30},std::pair{0x650,16},std::pair{0x690,64}})for(int i=0;i<pocket.second;++i)put16(bytes,block1+pocket.first+4*i+2,quint16(key));
            seal();const auto hidden=readEmeraldShops(bytes,EmeraldHash);QVERIFY(hidden.supported);QCOMPARE(hidden.merchants.size(),51);
            for(const auto& m:hidden.merchants){QCOMPARE(m.name,"???");QVERIFY(m.id.isEmpty());QVERIFY(m.location.isEmpty());QVERIFY(m.stock.isEmpty());QVERIFY(!m.discovered);}
            QVERIFY(buyEmeraldItems(bytes,EmeraldHash,{"oldaletown-mart",13,1}).data.isEmpty());
            flag(0x870);seal();
            auto state=readEmeraldShops(bytes,EmeraldHash);QVERIFY(state.merchants[0].discovered);QCOMPARE(state.merchants[0].stock.size(),4);
            QVERIFY(buyEmeraldItems(bytes,EmeraldHash,{"oldaletown-mart",4,1}).data.isEmpty());
            flag(0x74);seal();state=readEmeraldShops(bytes,EmeraldHash);QCOMPARE(state.merchants[0].stock.size(),5);
            {
                MerchantPurchase basket;basket.kind="basket";basket.basket={{"oldaletown-mart",4,20,"item"},{"oldaletown-mart",13,2,"item"}};
                const auto result=buyEmeraldItems(bytes,EmeraldHash,basket);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));
                const auto snapshot=readEmeraldShops(result.data,EmeraldHash);QCOMPARE(snapshot.balance,5400);
                for(const auto& m:snapshot.merchants)if(m.id=="oldaletown-mart")for(const auto& st:m.stock){if(st.itemId==4)QCOMPARE(st.owned,20);if(st.itemId==13)QCOMPARE(st.owned,2);}
                auto rejected=basket;rejected.basket.append({"oldaletown-mart",13,99,"item"});QVERIFY(buyEmeraldItems(bytes,EmeraldHash,rejected).data.isEmpty());
                rejected=basket;rejected.basket[1].merchantId="undiscovered";QVERIFY(buyEmeraldItems(bytes,EmeraldHash,rejected).data.isEmpty());
                rejected=basket;rejected.basket[1].kind="tutor";QVERIFY(buyEmeraldItems(bytes,EmeraldHash,rejected).data.isEmpty());
                rejected=basket;rejected.basket[0].quantity=-1;QVERIFY(buyEmeraldItems(bytes,EmeraldHash,rejected).data.isEmpty());
            }
            auto purchase=buyEmeraldItems(bytes,EmeraldHash,{"oldaletown-mart",4,10});QVERIFY2(purchase.error.isEmpty(),qPrintable(purchase.error));
            const auto after=readEmeraldShops(purchase.data,EmeraldHash);QCOMPARE(after.balance,8000);QCOMPARE(after.merchants[0].stock[0].owned,10);
            QCOMPARE(qFromLittleEndian<quint16>(purchase.data.constData()+block1+0x654),quint16(12));
            QCOMPARE(qFromLittleEndian<quint16>(purchase.data.constData()+block1+0x656)^quint16(key),1);
            for(int p=0;p<bytes.size();++p){if((p>=block1+0x490&&p<block1+0x494)||(p>=block1+0x650&&p<block1+0x658)||(p>=block1+0xff6&&p<block1+0xff8))continue;QCOMPARE(purchase.data[p],bytes[p]);}
            QVERIFY(buyEmeraldItems(bytes,EmeraldHash,{"oldaletown-mart",4,0}).data.isEmpty());
            QVERIFY(buyEmeraldItems(bytes,EmeraldHash,{"oldaletown-mart",4,100}).data.isEmpty());
            QVERIFY(buyEmeraldItems(bytes,EmeraldHash,{"oldaletown-mart",4,99}).data.isEmpty());
            QVERIFY(buyEmeraldItems(bytes,"wrong",{"oldaletown-mart",4,1}).data.isEmpty());
            auto damaged=bytes;damaged[0]^=1;QVERIFY(!readEmeraldShops(damaged,EmeraldHash).supported);
            // Existing stacks fill before an empty slot; a full pocket rejects atomically.
            put16(bytes,block1+0x650,4);put16(bytes,block1+0x652,98^quint16(key));seal();
            purchase=buyEmeraldItems(bytes,EmeraldHash,{"oldaletown-mart",4,3});QVERIFY(purchase.error.isEmpty());
            QCOMPARE(qFromLittleEndian<quint16>(purchase.data.constData()+block1+0x652)^quint16(key),99);
            QCOMPARE(qFromLittleEndian<quint16>(purchase.data.constData()+block1+0x656)^quint16(key),2);
            for(int i=0;i<16;++i){put16(bytes,block1+0x650+4*i,4);put16(bytes,block1+0x652+4*i,99^quint16(key));}seal();
            QVERIFY(buyEmeraldItems(bytes,EmeraldHash,{"oldaletown-mart",4,1}).data.isEmpty());
            // A decoration lives in another logical sector; preserve every unrelated byte.
            flag(0x87b);seal();
            Merchant store;for(const auto& m:readEmeraldShops(bytes,EmeraldHash).merchants)if(m.id=="lilycove-5f-0")store=m;
            QVERIFY(!store.stock.isEmpty());const auto doll=store.stock[0];
            const auto furnished=buyEmeraldItems(bytes,EmeraldHash,{store.id,doll.itemId,1,"decoration"});
            QVERIFY2(furnished.error.isEmpty(),qPrintable(furnished.error));
            const int block3=(14+(3+rotation)%14)*0x1000,decorAt=block3+0x2798-2*0xf80;
            QCOMPARE(quint8(furnished.data[decorAt]),quint8(doll.itemId));
            for(int p=0;p<bytes.size();++p){if((p>=block1+0x490&&p<block1+0x494)||p==decorAt||(p>=block1+0xff6&&p<block1+0xff8)||(p>=block3+0xff6&&p<block3+0xff8))continue;QCOMPARE(furnished.data[p],bytes[p]);}
            // BP/Powder cross logical blocks 0 and 1 in every physical sector rotation.
            flag(0x8a8);flag(0x877);flag(0x151);
            put16(bytes,block0+0xeb8,100);put32(bytes,block0+0x1f4,5000^key);
            put16(bytes,block1+0x5d8,372);put16(bytes,block1+0x5da,1^quint16(key));seal();
            for(const auto& req:{MerchantPurchase{"frontier-vitamin",64,1},MerchantPurchase{"slateport-powder",30,1}}){
                const auto exchanged=buyEmeraldItems(bytes,EmeraldHash,req);QVERIFY2(exchanged.error.isEmpty(),qPrintable(exchanged.error));
                const bool bp=req.merchantId=="frontier-vitamin";
                if(bp)QCOMPARE(qFromLittleEndian<quint16>(exchanged.data.constData()+block0+0xeb8),99);
                else QCOMPARE(qFromLittleEndian<quint32>(exchanged.data.constData()+block0+0x1f4)^key,4950u);
                QCOMPARE(qFromLittleEndian<quint16>(exchanged.data.constData()+block1+0x560),req.itemId);
                QVERIFY(readEmeraldShops(exchanged.data,EmeraldHash).supported);
                const int wallet=block0+(bp?0xeb8:0x1f4),width=bp?2:4;
                for(int p=0;p<bytes.size();++p){
                    if((p>=wallet&&p<wallet+width)||(p>=block1+0x560&&p<block1+0x564)||(p>=block0+0xff6&&p<block0+0xff8)||(p>=block1+0xff6&&p<block1+0xff8))continue;
                    QCOMPARE(exchanged.data[p],bytes[p]);
                }
            }
            bytes[block1+0x234]=1;bytes.replace(block1+0x238,100,setTestMoves(pokemonFixture(rotation),{33,45,84,57}));seal();
            for(const auto& m:readEmeraldShops(bytes,EmeraldHash).merchants)if(m.id=="frontier-tutor-left"){
                const auto& st=m.stock[3];const MerchantPurchase req{m.id,st.itemId,1,"tutor",0,0,st.recipients[0].identity};
                const auto taught=buyEmeraldItems(bytes,EmeraldHash,req);QVERIFY(taught.error.isEmpty());
                QCOMPARE(readGen3Progress(taught.data,Gen3Edition::Emerald).party->party[0].moves[0].name,QString("Mega Punch"));
                for(int p=0;p<bytes.size();++p){if((p>=block1+0x238&&p<block1+0x29c)||(p>=block0+0xeb8&&p<block0+0xeba)||(p>=block0+0xff6&&p<block0+0xff8)||(p>=block1+0xff6&&p<block1+0xff8))continue;QCOMPARE(taught.data[p],bytes[p]);}
            }
            bytes[block1+0x234]=1;bytes.replace(block1+0x238,100,setTestMoves(pokemonFixture(rotation),{33,0,0,0}));
            flag(0x873);put16(bytes,block1+0x560,111);put16(bytes,block1+0x562,quint16(2)^quint16(key));seal();
            bool relearned=false;
            for(const auto& m:readEmeraldShops(bytes,EmeraldHash).merchants)if(m.id=="fallarbor-relearner")for(const auto& st:m.stock)if(st.itemId==45){
                const auto result=buyEmeraldItems(bytes,EmeraldHash,{m.id,45,1,"relearn",0,1,st.recipients[0].identity});
                QVERIFY2(result.error.isEmpty(),qPrintable(result.error));relearned=true;
                QCOMPARE(readGen3Progress(result.data,Gen3Edition::Emerald).party->party[0].moves[1].name,QString("Growl"));
                QCOMPARE(qFromLittleEndian<quint16>(result.data.constData()+block1+0x562)^quint16(key),1);
                for(int p=0;p<bytes.size();++p){if((p>=block1+0x238&&p<block1+0x29c)||(p>=block1+0x560&&p<block1+0x564)||(p>=block1+0xff6&&p<block1+0xff8))continue;QCOMPARE(result.data[p],bytes[p]);}
            }
            QVERIFY(relearned);
            // Known locations can be unavailable while the separate Pyramid Bag is active.
            bytes[block1+4]=26;bytes[block1+5]=26;seal();state=readEmeraldShops(bytes,EmeraldHash);
            QVERIFY(state.merchants[0].discovered);QVERIFY(!state.merchants[0].available);
            QVERIFY(buyEmeraldItems(bytes,EmeraldHash,{"oldaletown-mart",13,1}).data.isEmpty());
        }
    }
    void healingRestoresHealthAndPpWithoutChangingOtherSaveData() {
        for (int rotation=0;rotation<14;++rotation) for(int permutation=0;permutation<24;++permutation) {
            auto bytes=save(Gen3Edition::Emerald,0xffffffffu,0);
            // Reorder the current slot and install original synthetic Party records.
            bytes.replace(14*0x1000,14*0x1000,slot(Gen3Edition::Emerald,0,rotation,0xa5,241));
            const int sector=(14+(1+rotation)%14)*0x1000;
            bytes[sector+0x234]=2;
            auto mon=pokemonFixture(permutation);put32(mon,80,0x40);
            bytes.replace(sector+0x238,100,mon);
            const auto egg=pokemonFixture(permutation,25,true);
            bytes.replace(sector+0x238+100,100,egg);
            quint32 sum=0;for(int p=0;p<0xf80;p+=4)sum+=qFromLittleEndian<quint32>(bytes.constData()+sector+p);
            put16(bytes,sector+0xff6,quint16((sum>>16)+sum));
            const auto healed=healEmeraldParty(bytes,EmeraldHash);
            QVERIFY2(healed.error.isEmpty(),qPrintable(healed.error));QCOMPARE(healed.partyCount,1);
            const auto p=readGen3Progress(healed.data,Gen3Edition::Emerald);
            const auto team=p.party->party;QCOMPARE(team[0].condition,"Healthy");QCOMPARE(team[0].hp,std::optional<int>(20));
            QCOMPARE(team[0].moves[0].pp,56);QCOMPARE(p.caught,std::optional<int>(241));
            QCOMPARE(healed.data.mid(sector+0x238+100,100),egg);
            for(int i=0;i<bytes.size();++i) {
                if(i>=sector+0x238 && i<sector+0x238+100)continue;
                if(i==sector+0xff6 || i==sector+0xff7)continue;
                QCOMPARE(healed.data[i],bytes[i]);
            }
            // Personal identity, stats and all non-PP encrypted fields stay intact.
            const auto output=healed.data.mid(sector+0x238,100);
            QCOMPARE(output.left(28),mon.left(28));QCOMPARE(output.mid(84,2),mon.mid(84,2));QCOMPARE(output.mid(88),mon.mid(88));
            QByteArray clearBefore=mon.mid(32,48),clearAfter=output.mid(32,48);
            const quint32 key=quint32(permutation)^0x12345678u;
            for(int p=0;p<48;p+=4){put32(clearBefore,p,qFromLittleEndian<quint32>(clearBefore.constData()+p)^key);put32(clearAfter,p,qFromLittleEndian<quint32>(clearAfter.constData()+p)^key);}
            int changed=0;for(int p=0;p<48;++p)changed+=clearBefore[p]!=clearAfter[p];QCOMPARE(changed,1);
            QCOMPARE(healEmeraldParty(healed.data,EmeraldHash).data,healed.data);
            QVERIFY(healEmeraldParty(bytes,"wrong-rom").data.isEmpty());
            auto damaged=bytes;damaged[sector+8]^=1;QVERIFY(healEmeraldParty(damaged,EmeraldHash).data.isEmpty());
            QVERIFY(healEmeraldParty(bytes.left(0x10000),EmeraldHash).data.isEmpty());
        }
        QVERIFY(healEmeraldParty(save(Gen3Edition::Emerald,8,8),EmeraldHash).data.isEmpty());
        QVERIFY(healEmeraldParty(save(Gen3Edition::Emerald,0,0x80000000),EmeraldHash).data.isEmpty());
    }
    void emeraldSpeciesFlagsAreNationalCompleteAndConsistent() {
        const auto fixture = [](int caught, int seen, bool damaged = false) {
            return slot(Gen3Edition::Emerald,1,7,0,caught,seen,damaged) + QByteArray(18*0x1000,char(0xff));
        };
        const auto p = readGen3Progress(fixture(1,3),Gen3Edition::Emerald);
        QVERIFY(p.pokedex); QCOMPARE(p.pokedex->speciesCount,386); QVERIFY(p.pokedex->error.isEmpty());
        QCOMPARE(p.pokedex->caught,(QSet<int>{1})); QCOMPARE(p.pokedex->seen,(QSet<int>{1,2,3}));
        QVERIFY(!p.pokedex->seen.contains(4));
        const auto last = readGen3Progress(fixture(386,386),Gen3Edition::Emerald);
        QVERIFY(last.pokedex->seen.contains(386)); QVERIFY(last.pokedex->caught.contains(386));
        for (auto bytes : {fixture(1,3,true),fixture(3,1)}) {
            const auto bad = readGen3Progress(bytes,Gen3Edition::Emerald);
            QCOMPARE(bad.availability,ProgressAvailability::Available); // Other independent fields survive.
            QVERIFY(!bad.pokedex->error.isEmpty()); QVERIFY(bad.pokedex->seen.isEmpty()); QVERIFY(bad.pokedex->caught.isEmpty());
        }
        QVERIFY(readGen3Progress(save(Gen3Edition::FireRed),Gen3Edition::FireRed).pokedex);
    }
    void emeraldPartyDecryptsEveryPermutationAndPreservesBoxPositions() {
        for(int permutation=0;permutation<24;++permutation) {
            QByteArray world(0x3d88,0),boxes(0x83d0,0);world[0x234]=1;boxes[0]=13;
            world.replace(0x238,100,pokemonFixture(permutation));
            boxes.replace(4+(13*30+29)*80,80,pokemonFixture(permutation,25,false,false));
            boxes[0x8344+13*9]=char(0xbb);boxes[0x8344+13*9+1]=char(0xff);
            const auto result=readEmeraldParty(world,boxes);
            QVERIFY(result.error.isEmpty());QCOMPARE(result.party.size(),6);QCOMPARE(result.boxes.size(),14);
            QCOMPARE(result.currentBox,13);QCOMPARE(result.boxes[13].name,"A");
            const auto p=result.party[0];QCOMPARE(p.kind,PokemonSlotKind::Known);QCOMPARE(p.speciesId,"pikachu");
            QCOMPARE(p.level,5);QCOMPARE(p.hp,std::optional<int>(0));QCOMPARE(p.condition,"Fainted");
            QCOMPARE(p.moves[0].pp,40);QCOMPARE(p.moves[0].maxPp,56);
            QCOMPARE(p.item,"Charcoal");
            QCOMPARE(p.stats[3],24);QCOMPARE(p.stats[5],23);
            QCOMPARE(result.party[1].kind,PokemonSlotKind::Empty);
            QCOMPARE(result.boxes[13].members[28].kind,PokemonSlotKind::Empty);
            const auto boxed=result.boxes[13].members[29];QCOMPARE(boxed.kind,PokemonSlotKind::Known);
            QVERIFY(!boxed.hp.has_value());QCOMPARE(boxed.condition,"Stored");
            QCOMPARE(boxed.stats[0],20); // ((2*35+31)*5/100)+5+10.
        }
    }
    void emeraldPartyContainsDamageAndHidesEggSpecies() {
        QByteArray world(0x3d88,0),boxes(0x83d0,0);world[0x234]=3;
        world.replace(0x238,100,pokemonFixture(2,25,true));
        auto damaged=pokemonFixture();damaged[35]=char(quint8(damaged[35])^1);
        world.replace(0x238+100,100,damaged);world.replace(0x238+200,100,pokemonFixture());
        const auto result=readEmeraldParty(world,boxes);QVERIFY(result.error.isEmpty());
        QCOMPARE(result.party[0].kind,PokemonSlotKind::Egg);QVERIFY(result.party[0].speciesId.isEmpty());
        QCOMPARE(result.party[1].kind,PokemonSlotKind::Unreadable);QCOMPARE(result.party[2].kind,PokemonSlotKind::Known);
        QVERIFY(!readEmeraldParty(world.left(0x3000),boxes).error.isEmpty());
        QVERIFY(!readEmeraldParty(world,boxes+QByteArray(1,0)).error.isEmpty());
        world[0x234]=7;QVERIFY(!readEmeraldParty(world,boxes).error.isEmpty());
        world[0x234]=0;boxes[0]=14;QVERIFY(!readEmeraldParty(world,boxes).error.isEmpty());
    }
    void privateEmeraldPartyExample() {
        const auto path=qEnvironmentVariable("TRAINEROS_EMERALD_SAMPLE");if(path.isEmpty())QSKIP("Private save is optional");
        QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));const auto bytes=f.readAll();
        const auto p=readGen3Progress(bytes,Gen3Edition::Emerald);
        QCOMPARE(p.availability,ProgressAvailability::Available);QVERIFY(p.party.has_value());QVERIFY(p.party->error.isEmpty());
        QVERIFY(p.pokedex);QVERIFY(p.pokedex->error.isEmpty());QCOMPARE(p.pokedex->caught.size(),p.caught.value());
        int known=0,bad=0;
        for(const auto& mon:p.party->party) {known+=mon.kind==PokemonSlotKind::Known;bad+=mon.kind==PokemonSlotKind::Unreadable;qInfo()<<mon.speciesName<<mon.level<<mon.hp.value_or(-1);}
        for(const auto& box:p.party->boxes)for(const auto& mon:box.members)bad+=mon.kind==PokemonSlotKind::Unreadable;
        QVERIFY(known>0);QCOMPARE(bad,0);
    }
    void badgeArtwork() {
        const auto kanto = BadgeAssets::entries("kanto-frlg", 0x21);
        QCOMPARE(kanto.size(), 8);
        QStringList ids;
        for (const auto& row : kanto) ids.append(row.toMap()["id"].toString());
        QCOMPARE(ids, QStringList({"boulder", "cascade", "thunder", "rainbow", "soul", "marsh", "volcano", "earth"}));
        QCOMPARE(kanto[0].toMap()["state"], "earned");
        QCOMPARE(kanto[4].toMap()["state"], "unearned");
        QCOMPARE(kanto[5].toMap()["state"], "earned");
        for (const auto& set : {QString("kanto-frlg"), QString("hoenn-rse")}) {
            for (const auto& row : BadgeAssets::entries(set, 255)) {
                const auto url = row.toMap()["image"].toString();
                QVERIFY(url.startsWith("qrc:/badges/"));
                QImage image(url.mid(3));
                QCOMPARE(image.size(), QSize(192, 192));
                QVERIFY(image.hasAlphaChannel());
                QVERIFY(image.pixelColor(0, 0).alpha() == 0);
                int pixels = 0;
                for (int y = 0; y < image.height(); ++y)
                    for (int x = 0; x < image.width(); ++x) pixels += image.pixelColor(x, y).alpha() > 128;
                QVERIFY(pixels > 2500 && pixels < 31000); // Not an empty image, clipped sheet or opaque square.
            }
        }
        const auto zero = BadgeAssets::entries("hoenn-rse", 0);
        QCOMPARE(zero.first().toMap()["id"], "stone");
        QCOMPARE(zero.last().toMap()["id"], "rain");
        for (const auto& row : zero) QCOMPARE(row.toMap()["state"], "unearned");
        for (const auto& row : BadgeAssets::entries("kanto-frlg", std::nullopt)) {
            QCOMPARE(row.toMap()["state"], "unknown");
            QVERIFY(row.toMap()["image"].toString().isEmpty());
        }
        for (const auto& unsupported : {"kanto", "hoenn", "unova-b2w2", "paldea", ""})
            QVERIFY(BadgeAssets::entries(unsupported, 255).isEmpty());
    }
    void readsBothEditions() {
        for (auto edition : {Gen3Edition::Emerald, Gen3Edition::FireRed}) {
            const auto bytes = save(edition); const auto result = readGen3Progress(bytes, edition);
            QCOMPARE(result.availability, ProgressAvailability::Available);
            QCOMPARE(result.badgeMask.value(), 0xa5); QCOMPARE(result.caught.value(), 241);
            QCOMPARE(result.badgeSet, edition == Gen3Edition::Emerald ? QString("hoenn-rse") : QString("kanto-frlg"));
            auto zero = slot(edition, 5, 0, 0, 0) + QByteArray(18 * 0x1000, char(0xff));
            const auto empty = readGen3Progress(zero, edition);
            QCOMPARE(empty.availability, ProgressAvailability::Available);
            QCOMPARE(empty.badgeMask.value(), 0); QCOMPARE(empty.caught.value(), 0);
        }
    }
    void rejectsInvalidAndSelectsIntactSlot() {
        auto bytes = save(Gen3Edition::Emerald);
        bytes[14 * 0x1000 + 100] ^= 1;
        const auto recovered = readGen3Progress(bytes, Gen3Edition::Emerald);
        QCOMPARE(recovered.badgeMask.value(), 9); QCOMPARE(recovered.caught.value(), 12);
        bytes[100] ^= 1;
        QCOMPARE(readGen3Progress(bytes, Gen3Edition::Emerald).availability, ProgressAvailability::Unreadable);
        for (int size : {0, 0x10000, 0x1ffff, 0x20001, 0x40000})
            QCOMPARE(readGen3Progress(QByteArray(size, 0), Gen3Edition::Emerald).availability, ProgressAvailability::Unreadable);
        QCOMPARE(readGen3Progress(save(Gen3Edition::FireRed), Gen3Edition::Emerald).availability, ProgressAvailability::Unreadable);
    }
    void validatesSlotIdentityAndCounterWrap() {
        auto wrapped = readGen3Progress(save(Gen3Edition::Emerald, 0xffffffff, 0), Gen3Edition::Emerald);
        QCOMPARE(wrapped.caught.value(), 241);
        auto older = readGen3Progress(save(Gen3Edition::Emerald, 12, 11), Gen3Edition::Emerald);
        QCOMPARE(older.caught.value(), 12);
        QCOMPARE(readGen3Progress(save(Gen3Edition::Emerald, 10, 10), Gen3Edition::Emerald).availability, ProgressAvailability::Unreadable);
        auto bytes = save(Gen3Edition::Emerald);
        // Two duplicate IDs and a mismatched save counter must not be mixed into
        // a fictitious complete slot, even though their payload checksums pass.
        put16(bytes, 0xff4, qFromLittleEndian<quint16>(bytes.constData() + 0x1ff4));
        put32(bytes, 14 * 0x1000 + 0xffc, 55);
        QCOMPARE(readGen3Progress(bytes, Gen3Edition::Emerald).availability, ProgressAvailability::Unreadable);
    }
    void requiresFingerprintAndSafeSource() {
        QTemporaryDir dir; const auto path = dir.filePath("save.srm"); const auto bytes = save(Gen3Edition::Emerald); write(path, bytes);
        auto target = SaveTarget{"test", "", path, EmeraldHash, "verified-context", {}, true};
        const auto resolver = [&](const AdventureRegistration&) { return target; };
        const auto result = inspectGameProgress(record(), resolver);
        QCOMPARE(result.availability, ProgressAvailability::Available);
        QCOMPARE(result.saveRevision, QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
        QFile unchanged(path); QVERIFY(unchanged.open(QIODevice::ReadOnly)); QCOMPARE(unchanged.readAll(), bytes); unchanged.close();
        target.contentRevision = QString(64, 'a');
        QCOMPARE(inspectGameProgress(record(), resolver).availability, ProgressAvailability::Unsupported);
        target.contentRevision = EmeraldHash;
        auto hack = record(); hack.adventure.kind = AdventureKind::RomHack;
        QCOMPARE(inspectGameProgress(hack, resolver).availability, ProgressAvailability::Unsupported);
        auto ds = record(); ds.adventure.adapterId = "melonds";
        QCOMPARE(inspectGameProgress(ds, resolver).availability, ProgressAvailability::Unsupported);
        QVERIFY(QFile::remove(path));
        QCOMPARE(inspectGameProgress(record(), resolver).availability, ProgressAvailability::Missing);
        write(path, QByteArray(0x20000, 0));
        const auto bad = inspectGameProgress(record(), resolver);
        QCOMPARE(bad.availability, ProgressAvailability::Unreadable); QVERIFY(!bad.caught); QVERIFY(!bad.badgeMask);
        QCOMPARE(bad.contextRevision,target.contextRevision); // A same-source failure may label a previous complete Dex snapshot.
        int failureCalls = 0;
        const auto changedFailure = inspectGameProgress(record(), [&](const AdventureRegistration&) {
            auto changed = target; if (++failureCalls == 2) changed.contextRevision = "different-owner"; return changed;
        });
        QVERIFY(changedFailure.contextRevision.isEmpty());
        write(path, bytes);
        int calls = 0;
        const auto replaced = inspectGameProgress(record(), [&](const AdventureRegistration&) {
            auto changed = target; if (++calls == 2) changed.contextRevision = "another-setup"; return changed;
        });
        QCOMPARE(replaced.availability, ProgressAvailability::Unreadable); QVERIFY(!replaced.caught);
    }
    void discardsPreviousAdventureAndRefreshesReplacement() {
        QTemporaryDir dir; const auto path = dir.filePath("save.srm"); write(path, save(Gen3Edition::Emerald));
        QSemaphore entered, release;
        std::atomic_bool blockNext{true};
        GameProgressService service([&](const AdventureRegistration& r) {
            if (r.adventure.id == "old" && blockNext.exchange(false)) { entered.release(); release.acquire(); }
            return SaveTarget{r.adventure.id, "", path, EmeraldHash, "context", {}, true};
        });
        service.refresh(record("old"));
        const bool started = entered.tryAcquire(1, 2000);
        if (!started) { release.release(); QVERIFY(started); }
        service.refresh(record("new")); release.release();
        QTRY_COMPARE(service.snapshot().availability, ProgressAvailability::Available);
        QCOMPARE(service.adventureId(), QString("new")); QCOMPARE(service.snapshot().caught.value(), 241);
        write(path, slot(Gen3Edition::Emerald, 1, 0, 0, 0) + QByteArray(18 * 0x1000, char(0xff)));
        service.refresh(record("new")); QTRY_COMPARE(service.snapshot().availability, ProgressAvailability::Available);
        QCOMPARE(service.snapshot().caught.value(), 0); QCOMPARE(service.snapshot().badgeMask.value(), 0);
        blockNext = true; service.refresh(record("old"));
        const bool blocked = entered.tryAcquire(1, 2000);
        if (!blocked) { release.release(); QVERIFY(blocked); }
        service.invalidate(); release.release(); QTest::qWait(100);
        QVERIFY(service.adventureId().isEmpty()); QVERIFY(!service.snapshot().caught);
    }
    void externalExamples() {
        const auto folder = qEnvironmentVariable("TRAINEROS_PROGRESS_SAMPLE_DIR");
        if (folder.isEmpty()) QSKIP("Optional private examples were not supplied; synthetic format cases run above.");
        for (const auto& name : {QString("emerald.sav"), QString("firered.sav")}) {
            QFile file(QDir(folder).filePath(name)); QVERIFY(file.open(QIODevice::ReadOnly));
            const auto result = readGen3Progress(file.readAll(), name.startsWith("emerald") ? Gen3Edition::Emerald : Gen3Edition::FireRed);
            QCOMPARE(result.availability, ProgressAvailability::Available); QCOMPARE(result.badgeMask.value(), 255); QCOMPARE(result.caught.value(), 386);
        }
    }
};
QTEST_GUILESS_MAIN(GameProgressTests)
#include "GameProgressTests.moc"
