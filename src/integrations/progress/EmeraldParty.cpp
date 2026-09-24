#include "EmeraldParty.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtEndian>
#include <algorithm>

static void initializeEmeraldReference() { Q_INIT_RESOURCE(emerald_reference); }
namespace trainer {
namespace {
quint16 u16(const QByteArray& b, int p) { return qFromLittleEndian<quint16>(b.constData()+p); }
quint32 u32(const QByteArray& b, int p) { return qFromLittleEndian<quint32>(b.constData()+p); }
int byte(const QByteArray& b, int p) { return quint8(b[p]); }
const QJsonObject& reference() {
    static const QJsonObject data = [] {
        initializeEmeraldReference(); QFile f(":/progress/emerald-reference.json");
        if (!f.open(QIODevice::ReadOnly)) return QJsonObject{};
        return QJsonDocument::fromJson(f.readAll()).object();
    }();
    return data;
}
QString name(const QByteArray& bytes) {
    const auto chars = reference()["characters"].toObject();
    QString result;
    for (auto c : bytes) {
        const auto code = quint8(c); if (code == 0xff) break;
        // Never interpret format/control commands as text.
        result += chars.value(QString::number(code)).toString(QStringLiteral("?"));
    }
    return result.trimmed();
}
int experience(int level, const QString& growth) {
    if (level <= 1) return 1;
    const int cube = level*level*level;
    if (growth == "FAST") return 4*cube/5;
    if (growth == "SLOW") return 5*cube/4;
    if (growth == "MEDIUM_SLOW") return 6*cube/5 - 15*level*level + 100*level - 140;
    if (growth == "ERRATIC") {
        if (level <= 50) return (100-level)*cube/50;
        if (level <= 68) return (150-level)*cube/100;
        if (level <= 98) return ((1911-10*level)/3)*cube/500;
        return (160-level)*cube/100;
    }
    if (growth == "FLUCTUATING") {
        if (level <= 15) return ((level+1)/3+24)*cube/50;
        if (level <= 36) return (level+14)*cube/50;
        return (level/2+32)*cube/50;
    }
    return cube;
}
PokemonRecord unreadable() { PokemonRecord r; r.kind = PokemonSlotKind::Unreadable; return r; }
PokemonRecord pokemon(const QByteArray& bytes, bool inParty) {
    if (bytes.size() != (inParty ? 100 : 80)) return unreadable();
    if (bytes == QByteArray(bytes.size(), 0)) return {};
    const auto personality = u32(bytes,0), owner = u32(bytes,4);
    QByteArray clear = bytes.mid(32,48);
    quint16 checksum = 0;
    for (int p = 0; p < 48; p += 4) qToLittleEndian(u32(clear,p)^personality^owner,clear.data()+p);
    for (int p = 0; p < 48; p += 2) checksum = quint16(checksum + u16(clear,p));
    if (checksum != u16(bytes,28) || (byte(bytes,19)&1)) return unreadable();
    // Lexicographic physical order, indexed by the personality modulo 24.
    std::array<int,4> order{0,1,2,3};
    for (quint32 i=0;i<personality%24;++i) std::next_permutation(order.begin(),order.end());
    std::array<QByteArray,4> blocks;
    for (int i=0;i<4;++i) blocks[order[i]]=clear.mid(i*12,12);
    const auto& growth=blocks[0]; const auto& attacks=blocks[1]; const auto& ev=blocks[2]; const auto& misc=blocks[3];
    const auto species=u16(growth,0); const auto iv=u32(misc,4);
    if (!(byte(bytes,19)&2) || species==0) return unreadable();
    const auto facts=reference()["species"].toObject().value(QString::number(species)).toObject();
    if (facts.isEmpty()) return unreadable();
    const bool egg=iv & 0x40000000u;
    if (egg != bool(byte(bytes,19)&4)) return unreadable();
    PokemonRecord r;
    if (egg) { r.kind=PokemonSlotKind::Egg; r.condition="Egg"; return r; }
    r.kind=PokemonSlotKind::Known;
    r.speciesId=facts["id"].toString(); r.speciesName=facts["name"].toString(); r.number=facts["number"].toInt();
    r.formId=facts["form"].toString();
    if (r.speciesId=="deoxys") r.formId="10033"; // Emerald's non-link Speed form.
    if (r.speciesId=="unown") {
        const int letter=(((personality>>24)&3)<<6 | ((personality>>16)&3)<<4 | ((personality>>8)&3)<<2 | (personality&3))%28;
        r.formId=QString::number(letter ? 10000+letter : 201);
    }
    const int language=byte(bytes,18);
    if (language>=2 && language<=7 && language!=6) r.nickname=name(bytes.mid(8,10));
    r.shiny=((owner&0xffff)^(owner>>16)^(personality&0xffff)^(personality>>16))<8;
    for (const auto& type : facts["types"].toArray()) r.types.append(type.toString());
    const auto abilities=facts["abilities"].toArray();
    r.ability=abilities.at((iv>>31)&1).toString();
    if (r.ability.isEmpty()) r.ability=abilities.at(0).toString();
    const int nature=personality%25;
    r.nature=reference()["natures"].toObject().value(QString::number(nature)).toString();
    const auto held=u16(growth,2);
    r.item=held ? reference()["items"].toObject().value(QString::number(held)).toString() : "None";
    // Unknown item labels stay unknown without invalidating the individual.
    if (r.item.isEmpty()) r.item=QStringLiteral("—");
    const quint32 exp=u32(growth,4); const auto rate=facts["growth"].toString();
    if (exp<1 || exp>quint32(experience(100,rate))) return unreadable();
    r.level=1;
    while (r.level<100 && exp>=quint32(experience(r.level+1,rate))) ++r.level;
    auto bases=facts["base"].toArray();
    if (r.speciesId=="deoxys") bases=QJsonArray{50,95,90,180,95,90};
    std::array<int,6> calculated{};
    for (int s=0;s<6;++s) {
        const int value=(2*bases.at(s).toInt()+int((iv>>(5*s))&31)+byte(ev,s)/4)*r.level/100;
        int stat=s==0 ? (r.speciesId=="shedinja" ? 1 : value+r.level+10) : value+5;
        if (s && nature/5!=nature%5) {
            if (s-1==nature/5) stat=stat*110/100;
            if (s-1==nature%5) stat=stat*90/100;
        }
        calculated[s]=stat;
    }
    const std::array<int,6> displayOrder{0,1,2,4,5,3};
    for(int s=0;s<6;++s) r.stats[s]=calculated[displayOrder[s]];
    r.condition="Stored"; // Box records do not contain current HP or battle status.
    if (inParty) {
        if (byte(bytes,84)!=r.level || u16(bytes,86)>u16(bytes,88) || u16(bytes,88)==0) return unreadable();
        r.hp=u16(bytes,86);
        for(int s=0;s<6;++s) {
            const int stat=u16(bytes,88+displayOrder[s]*2);
            if (stat<1 || stat>999) return unreadable();
            r.stats[s]=r.speciesId=="deoxys" ? calculated[displayOrder[s]] : stat;
        }
        const auto status=u32(bytes,80);
        r.condition=*r.hp==0 ? "Fainted" : status&7 ? "Asleep" : status&0x88 ? "Poisoned" : status&0x10 ? "Burned" : status&0x20 ? "Frozen" : status&0x40 ? "Paralyzed" : "Healthy";
    }
    const auto moves=reference()["moves"].toObject();
    for(int i=0;i<4;++i) {
        const auto move=u16(attacks,2*i); const int pp=byte(attacks,8+i);
        if (!move) { if(pp) return unreadable(); r.moves.append(PokemonMove{}); continue; }
        const auto fact=moves.value(QString::number(move)).toObject();
        if(fact.isEmpty()) return unreadable();
        const int base=fact["pp"].toInt(), maximum=base+base*((byte(growth,8)>>(i*2))&3)/5;
        if(pp>maximum) return unreadable();
        r.moves.append({fact["name"].toString(),pp,maximum});
    }
    return r;
}
}
PartySnapshot readEmeraldParty(const QByteArray& world, const QByteArray& storage) {
    PartySnapshot result;
    if (world.size()!=0x3d88 || storage.size()!=0x83d0 || reference()["species"].toObject().size()!=386
        || byte(world,0x234)>6 || byte(storage,0)>=14) {
        result.error="The team and boxes could not be read."; return result;
    }
    for(int i=0;i<6;++i) result.party.append(i<byte(world,0x234) ? pokemon(world.mid(0x238+i*100,100),true) : PokemonRecord{});
    result.currentBox=byte(storage,0);
    for(int box=0;box<14;++box) {
        PokemonBox row; row.name=name(storage.mid(0x8344+box*9,9));
        if(row.name.isEmpty()) row.name=QString("Box %1").arg(box+1);
        for(int slot=0;slot<30;++slot) row.members.append(pokemon(storage.mid(4+(box*30+slot)*80,80),false));
        result.boxes.append(std::move(row));
    }
    return result;
}
}
