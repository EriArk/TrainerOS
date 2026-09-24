#include "Gen3Progress.h"
#include "EmeraldParty.h"
#include <QtEndian>
#include <array>

namespace trainer {
namespace {
constexpr int Sector = 0x1000, Payload = 0xf80, SectorCount = 14;
quint16 u16(const QByteArray& bytes, int offset) { return qFromLittleEndian<quint16>(bytes.constData() + offset); }
quint32 u32(const QByteArray& bytes, int offset) { return qFromLittleEndian<quint32>(bytes.constData() + offset); }
struct Slot {
    bool valid = false;
    quint32 counter = 0;
    std::array<QByteArray, SectorCount> blocks;
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
}
