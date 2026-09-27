#include "Gen3Progress.h"
#include "EmeraldParty.h"
#include "EmeraldShops.h"
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QtEndian>
#include <array>
#include <algorithm>

namespace trainer {
namespace {
constexpr int Sector = 0x1000, Payload = 0xf80, SectorCount = 14;
quint16 u16(const QByteArray& bytes, int offset) { return qFromLittleEndian<quint16>(bytes.constData() + offset); }
quint32 u32(const QByteArray& bytes, int offset) { return qFromLittleEndian<quint32>(bytes.constData() + offset); }
struct Slot {
    bool valid = false;
    quint32 counter = 0;
    std::array<QByteArray, SectorCount> blocks;
    std::array<int, SectorCount> offsets{};
};
Slot readSlot(const QByteArray& bytes, int base, Gen3Edition edition) {
    Slot result;
    for (int sector = 0; sector < SectorCount; ++sector) {
        const int offset = base + sector * Sector;
        const auto id = u16(bytes, offset + 0xff4);
        if (id >= SectorCount || !result.blocks[id].isEmpty() || u32(bytes, offset + 0xff8) != 0x08012025) return {};
        const auto counter = u32(bytes, offset + 0xffc);
        if (sector && result.counter != counter) return {};
        result.counter = counter;
        const int length = id == 0 ? (edition == Gen3Edition::Emerald ? 0xf2c : 0xf24)
            : id == 4 ? (edition == Gen3Edition::Emerald ? 0xf08 : 0xee8) : id == 13 ? 0x7d0 : Payload;
        quint32 sum = 0;
        for (int word = 0; word < length; word += 4) sum += u32(bytes, offset + word);
        if (quint16((sum >> 16) + sum) != u16(bytes, offset + 0xff6)) return {};
        result.blocks[id] = bytes.mid(offset, length);
        result.offsets[id] = offset;
    }
    result.valid = true;
    return result;
}
bool bit(const QByteArray& data, int offset, int index) {
    return (quint8(data[offset + index / 8]) & (1u << (index % 8))) != 0;
}
}

std::optional<Gen3Edition> gen3Edition(const QString& hash) {
    // Full verified retail images; corresponding pret SHA-1 references and
    // format sources are recorded in docs/GAME_PROGRESS.md.
    if (hash == "a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af") return Gen3Edition::Emerald;
    if (hash == "3d0c79f1627022e18765766f6cb5ea067f6b5bf7dca115552189ad65a5c3a8ac"
        || hash == "729041b940afe031302d630fdbe57c0c145f3f7b6d9b8eca5e98678d0ca4d059") return Gen3Edition::FireRed;
    return {};
}

GameProgress readGen3Progress(const QByteArray& save, Gen3Edition edition) {
    GameProgress result;
    result.availability = ProgressAvailability::Unreadable;
    result.message = "This in-game save could not be verified.";
    // Extra containers/RTC trailers need their own verified transport support.
    if (save.size() != 0x20000) return result;
    const auto first = readSlot(save, 0, edition);
    const auto second = readSlot(save, SectorCount * Sector, edition);
    if (!first.valid && !second.valid) return result;
    const Slot* latest = first.valid ? &first : &second;
    if (first.valid && second.valid) {
        const quint32 distance = second.counter - first.counter;
        if (distance == 0x80000000u || (distance == 0 && first.blocks != second.blocks)) return result;
        if (distance && distance < 0x80000000u) latest = &second;
    }
    QByteArray world;
    for (int id = 1; id <= 4; ++id) world += latest->blocks[id];
    const int flags = edition == Gen3Edition::Emerald ? 0x1270 : 0xee0;
    const int firstBadge = edition == Gen3Edition::Emerald ? 0x867 : 0x820;
    int mask = 0, caught = 0;
    for (int i = 0; i < 8; ++i) if (bit(world, flags, firstBadge + i)) mask |= 1 << i;
    for (int i = 0; i < 386; ++i) if (bit(latest->blocks[0], 0x28, i)) ++caught;
    result.availability = ProgressAvailability::Available;
    result.badgeMask = mask; result.caught = caught;
    result.badgeSet = edition == Gen3Edition::Emerald ? "hoenn-rse" : "kanto-frlg";
    result.provider = edition == Gen3Edition::Emerald ? "gen3-emerald-v1" : "gen3-firered-v1";
    result.message = "Last in-game save · National Pokédex";
    {
        const int seen1=edition==Gen3Edition::Emerald?0x988:0x5f8,seen2=edition==Gen3Edition::Emerald?0x3b24:0x3a18;
        SavePokedex dex; dex.speciesCount = 386;
        for (int i = 0; i < dex.speciesCount; ++i) {
            const bool seen = bit(latest->blocks[0], 0x5c, i);
            const bool caught = bit(latest->blocks[0], 0x28, i);
            if (seen != bit(world, seen1, i) || seen != bit(world, seen2, i) || (caught && !seen))
                dex.error = "The Pokedex records could not be verified.";
            if (seen) dex.seen.insert(i + 1);
            if (caught) dex.caught.insert(i + 1);
        }
        if (!dex.error.isEmpty()) { dex.seen.clear(); dex.caught.clear(); }
        result.pokedex = std::move(dex);
        QByteArray storage;
        for (int id = 5; id < SectorCount; ++id) storage += latest->blocks[id];
        result.party = readGen3Party(world, storage,edition);
        const auto distance=quint32(second.counter-first.counter);
        result.party->canRelease=edition==Gen3Edition::Emerald && first.valid && second.valid && distance && distance!=0x80000000u && result.party->error.isEmpty();
        result.party->canManage=edition==Gen3Edition::Emerald && first.valid && second.valid && distance && distance!=0x80000000u && result.party->error.isEmpty();
    }
    return result;
}

namespace {
std::optional<Slot> shopSlot(const QByteArray& save,const QString& hash) {
    if(gen3Edition(hash)!=Gen3Edition::Emerald || save.size()!=0x20000)return {};
    const auto a=readSlot(save,0,Gen3Edition::Emerald),b=readSlot(save,14*Sector,Gen3Edition::Emerald);
    const quint32 distance=b.counter-a.counter;
    if(!a.valid||!b.valid||!distance||distance==0x80000000u)return {};
    return distance<0x80000000u?b:a;
}
QByteArray worldBlock(const Slot& slot){QByteArray out;for(int id=1;id<=4;++id)out+=slot.blocks[id];return out;}
}
MerchantSnapshot readEmeraldShops(const QByteArray& save,const QString& hash) {
    const auto slot=shopSlot(save,hash);
    if(!slot){MerchantSnapshot out;out.error="Save in the supported English Emerald edition, then visit again.";return out;}
    auto out=readEmeraldShopBlock(worldBlock(*slot),slot->blocks[0]);
    out.lineage=QString::fromLatin1(slot->blocks[0].left(14).toHex());
    return out;
}
MerchantWrite buyEmeraldItems(const QByteArray& save,const QString& hash,const MerchantPurchase& request) {
    const auto slot=shopSlot(save,hash);if(!slot)return {{},"This Emerald save could not be verified.",{}};
    auto purchase=EmeraldShopWrite{worldBlock(*slot),{}, {},slot->blocks[0]};
    if(request.kind=="basket") {
        if(request.basket.isEmpty()||request.basket.size()>32)return {{},"Choose items for your basket first.",{}};
        for(const auto& line:request.basket) {
            if(line.quantity<1||line.quantity>99||!QStringList{"item","decoration","coins","barter"}.contains(line.kind))
                return {{},"This basket could not be verified.",{}};
            // Single-sale counters (decorations, vending and exchanges) still
            // run their exact game rule for each unit, inside one transaction.
            bool single = line.kind=="barter" || line.kind=="decoration";
            for(const auto& m:readEmeraldShopBlock(purchase.data,purchase.trainer).merchants)
                if(m.id==line.merchantId)for(const auto& offer:m.stock)
                    if(offer.itemId==line.itemId&&offer.kind==line.kind&&offer.maximum==1)single=true;
            const int repeats=single?line.quantity:1;
            for(int n=0;n<repeats;++n) {
                purchase=buyEmeraldShopBlock(purchase.data,purchase.trainer,{line.merchantId,line.itemId,single?1:line.quantity,line.kind},QRandomGenerator::global()->bounded(4096u));
                if(purchase.data.isEmpty())return {{},purchase.error,{}};
            }
        }
        purchase.message="Your purchases are in your Bag and storage!";
    } else {
        if(!request.basket.isEmpty())return {{},"This purchase could not be verified.",{}};
        purchase=buyEmeraldShopBlock(purchase.data,purchase.trainer,request,QRandomGenerator::global()->bounded(4096u));
    }
    if(purchase.data.isEmpty())return {{},purchase.error,{}};
    auto result=save;
    for(int id=0;id<=4;++id){
        const auto block=id==0?purchase.trainer:purchase.data.mid((id-1)*Payload,slot->blocks[id].size());
        if(block==slot->blocks[id])continue;
        const int at=slot->offsets[id];result.replace(at,block.size(),block);
        quint32 sum=0;for(int p=0;p<block.size();p+=4)sum+=u32(block,p);
        qToLittleEndian(quint16((sum>>16)+sum),result.data()+at+0xff6);
    }
    if(!readEmeraldShops(result,hash).supported)return {{},"The updated save could not be verified.",{}};
    return {result,{},purchase.message};
}
PartyMoveResult moveEmeraldPokemon(const QByteArray& save,const QString& hash,const PartyMove& request) {
    const auto slot=shopSlot(save,hash);
    if(!slot)return {{},"Moving is available only for a verified English Emerald save.",{}};
    if(request.saveRevision.isEmpty() || request.saveRevision!=QString::fromLatin1(QCryptographicHash::hash(save,QCryptographicHash::Sha256).toHex()))
        return {{},"The save changed. Read your team again before moving.",{}};
    auto world=worldBlock(*slot);QByteArray storage;for(int id=5;id<14;++id)storage+=slot->blocks[id];
    const auto before=readEmeraldParty(world,storage);
    const int count=quint8(world[0x234]);
    auto valid=[](PokemonPosition p){return p.box>=-1 && p.box<14 && p.slot>=0 && p.slot<(p.box<0?6:30);};
    if(!before.error.isEmpty() || count<1 || count>6 || !valid(request.from) || !valid(request.to))return {{},"Choose a valid Pokemon and destination.",{}};
    const auto from=request.from,to=request.to;
    if(from.box==to.box && from.slot==to.slot)return {save,{},"Already in this place."};
    auto record=[&](PokemonPosition p){return p.box<0?world.mid(0x238+p.slot*100,100):storage.mid(4+(p.box*30+p.slot)*80,80);};
    auto member=[&](PokemonPosition p){return p.box<0?before.party[p.slot]:before.boxes[p.box].members[p.slot];};
    const auto source=record(from),destination=record(to);
    if(member(from).kind!=PokemonSlotKind::Known)return {{},"Only a readable, hatched Pokemon can be moved here.",{}};
    if(from.box<0 && from.slot>=count)return {{},"Choose a team member first.",{}};
    if(from.box<0 && to.box<0) {
        if(to.slot>=count || member(to).kind==PokemonSlotKind::Unreadable || member(to).kind==PokemonSlotKind::Empty)
            return {{},"Choose another occupied team position.",{}};
        world.replace(0x238+from.slot*100,100,destination);world.replace(0x238+to.slot*100,100,source);
    } else {
        // No implicit replacement or exchange: transfers require an empty slot.
        if(member(to).kind!=PokemonSlotKind::Empty || destination!=QByteArray(destination.size(),0))return {{},"This place is occupied. Choose an empty slot.",{}};
        if(to.box<0) {
            if(count==6 || to.slot!=count)return {{},"Choose the first empty team position.",{}};
            const auto withdrawn=emeraldWithdrawRecord(source);
            if(withdrawn.isEmpty())return {{},"This Pokemon cannot leave storage here.",{}};
            world.replace(0x238+count*100,100,withdrawn);world[0x234]=char(count+1);
        } else {
            const auto boxed=emeraldBoxRecord(source);
            if(boxed.isEmpty())return {{},"Remove this Pokemon's Mail inside the game before moving it.",{}};
            storage.replace(4+(to.box*30+to.slot)*80,80,boxed);
        }
        if(from.box<0) {
            int able=0;for(int i=0;i<count;++i) {
                if(before.party[i].kind==PokemonSlotKind::Unreadable)return {{},"The whole team must be readable before depositing.",{}};
                if(i!=from.slot && before.party[i].kind==PokemonSlotKind::Known && before.party[i].hp.value_or(0)>0)++able;
            }
            if(!able)return {{},"Keep a Pokemon that can battle in your team.",{}};
            for(int i=from.slot;i<count-1;++i)world.replace(0x238+i*100,100,world.mid(0x238+(i+1)*100,100));
            world.replace(0x238+(count-1)*100,100,QByteArray(100,0));world[0x234]=char(count-1);
        } else storage.replace(4+(from.box*30+from.slot)*80,80,QByteArray(80,0));
    }
    auto result=save;
    for(int id=1;id<14;++id) {
        const auto block=id<5?world.mid((id-1)*Payload,slot->blocks[id].size()):storage.mid((id-5)*Payload,slot->blocks[id].size());
        if(block==slot->blocks[id])continue;
        const int at=slot->offsets[id];result.replace(at,block.size(),block);
        quint32 sum=0;for(int p=0;p<block.size();p+=4)sum+=u32(block,p);
        qToLittleEndian(quint16((sum>>16)+sum),result.data()+at+0xff6);
    }
    const auto after=readGen3Progress(result,Gen3Edition::Emerald);
    if(!after.party || !after.party->error.isEmpty() || !after.party->canManage)return {{},"The moved Pokemon could not be verified.",{}};
    const auto& placed=to.box<0?after.party->party[to.slot]:after.party->boxes[to.box].members[to.slot];
    if(placed.kind!=PokemonSlotKind::Known || placed.speciesId!=member(from).speciesId || placed.level!=member(from).level)
        return {{},"The destination could not be verified.",{}};
    return {result,{},from.box<0 && to.box<0?"Team order saved.":"Pokemon moved."};
}
PokemonReleaseResult releaseEmeraldPokemon(const QByteArray& save,const QString& hash,const PokemonRelease& request) {
    const auto slot=shopSlot(save,hash);
    if(!slot)return {{},"Release is available only for a verified English Emerald save.",{}};
    if(request.saveRevision.isEmpty() || request.saveRevision!=QString::fromLatin1(QCryptographicHash::hash(save,QCryptographicHash::Sha256).toHex()))
        return {{},"The save changed. Read your Pokemon again before releasing.",{}};
    const auto from=request.from;
    if(from.box < -1 || from.box>=14 || from.slot<0 || from.slot>=(from.box<0?6:30))
        return {{},"Choose a valid Pokemon.",{}};
    auto world=worldBlock(*slot);QByteArray storage;for(int id=5;id<14;++id)storage+=slot->blocks[id];
    const auto before=readEmeraldParty(world,storage);
    const int count=quint8(world[0x234]);
    if(!before.error.isEmpty() || count<1 || count>6 || (from.box<0 && from.slot>=count))return {{},"Your collection could not be verified.",{}};
    const auto& source=from.box<0?before.party[from.slot]:before.boxes[from.box].members[from.slot];
    if(source.kind!=PokemonSlotKind::Known)return {{},"Only a readable, hatched Pokemon can be released.",{}};
    const auto raw=from.box<0?world.mid(0x238+from.slot*100,100):storage.mid(4+(from.box*30+from.slot)*80,80);
    if(emeraldBoxRecord(raw).isEmpty())return {{},"Remove this Pokemon's Mail inside the game first.",{}};
    // Native Emerald's release policy: keep two individuals in the collection,
    // another living non-Egg in Party, and another holder of each restricted move.
    // Source: pret/pokeemerald 5eff786, pokemon_storage_system.c.
    QStringList restricted{"Surf","Dive"};
    if(quint8(world[4])==16 && (quint8(world[5])==10 || quint8(world[5])==14))restricted<<"Strength"<<"Rock Smash";
    QStringList needed;
    for(const auto& move:source.moves)if(restricted.contains(move.name) && !needed.contains(move.name))needed<<move.name;
    int total=0,able=0;
    auto inspect=[&](const PokemonRecord& mon,bool selected,bool party) {
        if(mon.kind==PokemonSlotKind::Unreadable)return false;
        if(mon.kind!=PokemonSlotKind::Empty)++total;
        if(!selected && mon.kind==PokemonSlotKind::Known) {
            if(party && mon.hp.value_or(0)>0)++able;
            for(const auto& move:mon.moves)needed.removeAll(move.name);
        }
        return true;
    };
    for(int i=0;i<6;++i) {
        // Reject a count/slot mismatch rather than hiding an uncounted individual.
        if(i<count ? before.party[i].kind==PokemonSlotKind::Empty : world.mid(0x238+i*100,100)!=QByteArray(100,0))
            return {{},"Your team could not be verified.",{}};
        if(!inspect(before.party[i],from.box<0 && from.slot==i,true))return {{},"Your collection must be readable before releasing.",{}};
    }
    for(int box=0;box<14;++box)for(int i=0;i<30;++i)
        if(!inspect(before.boxes[box].members[i],from.box==box && from.slot==i,false))return {{},"Your collection must be readable before releasing.",{}};
    if(from.box<0 && !able)return {{},"Keep a Pokemon that can battle in your team.",{}};
    if(total<3)return {{},"Keep at least two Pokemon in your collection.",{}};
    if(!needed.isEmpty())return {{},"Another Pokemon must know "+needed.join(" and ")+" first.",{}};
    // Exact native PurgeMonOrBoxMon plus CompactPartySlots semantics. No Bag,
    // Dex, identity, mail, other slot or older save-bank mutation is permitted.
    if(from.box<0) {
        for(int i=from.slot;i<count-1;++i)world.replace(0x238+i*100,100,world.mid(0x238+(i+1)*100,100));
        world.replace(0x238+(count-1)*100,100,QByteArray(100,0));world[0x234]=char(count-1);
    } else storage.replace(4+(from.box*30+from.slot)*80,80,QByteArray(80,0));
    auto result=save;
    for(int id=1;id<14;++id) {
        const auto block=id<5?world.mid((id-1)*Payload,slot->blocks[id].size()):storage.mid((id-5)*Payload,slot->blocks[id].size());
        if(block==slot->blocks[id])continue;
        const int at=slot->offsets[id];result.replace(at,block.size(),block);
        quint32 sum=0;for(int p=0;p<block.size();p+=4)sum+=u32(block,p);
        qToLittleEndian(quint16((sum>>16)+sum),result.data()+at+0xff6);
    }
    const auto after=readGen3Progress(result,Gen3Edition::Emerald);
    if(!after.party || !after.party->canRelease)return {{},"The updated collection could not be verified.",{}};
    // Reread packed sectors against the prepared logical blocks, including
    // cross-sector box records. Independent tests check the full allowed delta.
    const auto verified=shopSlot(result,hash);if(!verified)return {{},"The updated save could not be verified.",{}};
    QByteArray verifiedStorage;for(int id=5;id<14;++id)verifiedStorage+=verified->blocks[id];
    if(worldBlock(*verified)!=world || verifiedStorage!=storage)return {{},"The updated collection could not be verified.",{}};
    return {result,{},"Pokemon released. A backup is available in the Center."};
}
static SaveHealing healVerifiedGen3Party(const QByteArray& save, const QString& contentHash) {
    const auto edition=gen3Edition(contentHash);
    if (!edition)
        return {{},"Healing is unavailable for this exact game build."};
    if (save.size() != 0x20000) return {{},"This save could not be verified."};
    const auto a = readSlot(save,0,*edition), b = readSlot(save,SectorCount*Sector,*edition);
    // Reading can recover one intact slot. Writing requires an unambiguous pair.
    const quint32 distance = b.counter-a.counter;
    if (!a.valid || !b.valid || !distance || distance == 0x80000000u)
        return {{},"Save once more inside the game, close the game, then try again."};
    const auto& latest = distance < 0x80000000u ? b : a;
    const auto before = readGen3Progress(save,*edition);
    if (!before.party || !before.party->error.isEmpty()) return {{},"Your team could not be verified."};
    const int partyAt=*edition==Gen3Edition::FireRed?0x38:0x238;
    const int count = quint8(latest.blocks[1][partyAt-4]);
    if (count < 1 || count > 6) return {{},"There are no Pokémon to heal yet."};
    QByteArray result = save;
    int healed = 0;
    for (int i=0; i<count; ++i) {
        const auto& mon = before.party->party[i];
        if (mon.kind == PokemonSlotKind::Egg) continue; // Preserve eggs byte-for-byte.
        if (mon.kind != PokemonSlotKind::Known || !mon.hp || mon.moves.size()!=4)
            return {{},"A team member could not be verified. Your save was not changed."};
        const int at = latest.offsets[1]+partyAt+i*100;
        const quint32 personality=u32(save,at), key=personality^u32(save,at+4);
        QByteArray clear=save.mid(at+32,48);
        for (int p=0;p<48;p+=4) qToLittleEndian(u32(clear,p)^key,clear.data()+p);
        std::array<int,4> order{0,1,2,3};
        for (quint32 p=0;p<personality%24;++p) std::next_permutation(order.begin(),order.end());
        const int attacks=int(std::find(order.begin(),order.end(),1)-order.begin())*12;
        for (int m=0;m<4;++m) clear[attacks+8+m]=char(mon.moves[m].maxPp);
        quint16 checksum=0;
        for (int p=0;p<48;p+=2) checksum=quint16(checksum+u16(clear,p));
        qToLittleEndian(checksum,result.data()+at+28);
        for (int p=0;p<48;p+=4) qToLittleEndian(u32(clear,p)^key,result.data()+at+32+p);
        qToLittleEndian(quint32(0),result.data()+at+80);
        qToLittleEndian(u16(save,at+88),result.data()+at+86);
        ++healed;
    }
    if (!healed) return {{},"Your team contains only Eggs. They do not need treatment."};
    const int sector=latest.offsets[1]; quint32 sum=0;
    for (int p=0;p<Payload;p+=4) sum+=u32(result,sector+p);
    qToLittleEndian(quint16((sum>>16)+sum),result.data()+sector+0xff6);
    const auto after=readGen3Progress(result,*edition);
    if (!after.party || !after.party->error.isEmpty()) return {{},"The healed team could not be verified."};
    for (int i=0;i<count;++i) {
        const auto& mon=after.party->party[i];
        if (mon.kind==PokemonSlotKind::Egg) continue;
        if (mon.kind!=PokemonSlotKind::Known || mon.condition!="Healthy" || mon.hp!=u16(result,sector+partyAt+i*100+88))
            return {{},"The healed team could not be verified."};
        for (const auto& move:mon.moves) if (move.pp!=move.maxPp) return {{},"Move recovery could not be verified."};
    }
    return {result,{},healed};
}
SaveHealing healEmeraldParty(const QByteArray& save,const QString& hash) {
    if(gen3Edition(hash)!=Gen3Edition::Emerald)return {{},"Healing is unavailable for this exact Emerald build."};
    return healVerifiedGen3Party(save,hash);
}
SaveHealing healGen3Party(const QByteArray& save,const QString& hash) { return healVerifiedGen3Party(save,hash); }

}
