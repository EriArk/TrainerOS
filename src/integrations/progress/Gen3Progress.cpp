#include "Gen3Progress.h"
#include "EmeraldParty.h"
#include "EmeraldShops.h"
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
    if (edition == Gen3Edition::Emerald) {
        SavePokedex dex; dex.speciesCount = 386;
        for (int i = 0; i < dex.speciesCount; ++i) {
            const bool seen = bit(latest->blocks[0], 0x5c, i);
            const bool caught = bit(latest->blocks[0], 0x28, i);
            if (seen != bit(world, 0x988, i) || seen != bit(world, 0x3b24, i) || (caught && !seen))
                dex.error = "The Pokedex records could not be verified.";
            if (seen) dex.seen.insert(i + 1);
            if (caught) dex.caught.insert(i + 1);
        }
        if (!dex.error.isEmpty()) { dex.seen.clear(); dex.caught.clear(); }
        result.pokedex = std::move(dex);
        QByteArray storage;
        for (int id = 5; id < SectorCount; ++id) storage += latest->blocks[id];
        result.party = readEmeraldParty(world, storage);
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
    auto out=readEmeraldShopBlock(worldBlock(*slot),u32(slot->blocks[0],0xac));
    out.lineage=QString::fromLatin1(slot->blocks[0].left(14).toHex());
    return out;
}
MerchantWrite buyEmeraldItems(const QByteArray& save,const QString& hash,const MerchantPurchase& request) {
    const auto slot=shopSlot(save,hash);if(!slot)return {{},"This Emerald save could not be verified.",{}};
    const auto purchase=buyEmeraldShopBlock(worldBlock(*slot),u32(slot->blocks[0],0xac),request);
    if(purchase.data.isEmpty())return purchase;
    auto result=save;
    for(int id=1;id<=4;++id){
        const auto block=purchase.data.mid((id-1)*Payload,slot->blocks[id].size());
        if(block==slot->blocks[id])continue;
        const int at=slot->offsets[id];result.replace(at,block.size(),block);
        quint32 sum=0;for(int p=0;p<block.size();p+=4)sum+=u32(block,p);
        qToLittleEndian(quint16((sum>>16)+sum),result.data()+at+0xff6);
    }
    if(!readEmeraldShops(result,hash).supported)return {{},"The updated save could not be verified.",{}};
    return {result,{},purchase.message};
}
SaveHealing healEmeraldParty(const QByteArray& save, const QString& contentHash) {
    if (gen3Edition(contentHash) != Gen3Edition::Emerald)
        return {{},"Healing is currently available for the verified English Pokémon Emerald edition."};
    if (save.size() != 0x20000) return {{},"This save could not be verified."};
    const auto a = readSlot(save,0,Gen3Edition::Emerald), b = readSlot(save,SectorCount*Sector,Gen3Edition::Emerald);
    // Reading can recover one intact slot. Writing requires an unambiguous pair.
    const quint32 distance = b.counter-a.counter;
    if (!a.valid || !b.valid || !distance || distance == 0x80000000u)
        return {{},"Save once more inside Emerald, close the game, then try again."};
    const auto& latest = distance < 0x80000000u ? b : a;
    const auto before = readGen3Progress(save,Gen3Edition::Emerald);
    if (!before.party || !before.party->error.isEmpty()) return {{},"Your team could not be verified."};
    const int count = quint8(latest.blocks[1][0x234]);
    if (count < 1 || count > 6) return {{},"There are no Pokémon to heal yet."};
    QByteArray result = save;
    int healed = 0;
    for (int i=0; i<count; ++i) {
        const auto& mon = before.party->party[i];
        if (mon.kind == PokemonSlotKind::Egg) continue; // Preserve eggs byte-for-byte.
        if (mon.kind != PokemonSlotKind::Known || !mon.hp || mon.moves.size()!=4)
            return {{},"A team member could not be verified. Your save was not changed."};
        const int at = latest.offsets[1]+0x238+i*100;
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
    const auto after=readGen3Progress(result,Gen3Edition::Emerald);
    if (!after.party || !after.party->error.isEmpty()) return {{},"The healed team could not be verified."};
    for (int i=0;i<count;++i) {
        const auto& mon=after.party->party[i];
        if (mon.kind==PokemonSlotKind::Egg) continue;
        if (mon.kind!=PokemonSlotKind::Known || mon.condition!="Healthy" || mon.hp!=u16(result,sector+0x238+i*100+88))
            return {{},"The healed team could not be verified."};
        for (const auto& move:mon.moves) if (move.pp!=move.maxPp) return {{},"Move recovery could not be verified."};
    }
    return {result,{},healed};
}
}
