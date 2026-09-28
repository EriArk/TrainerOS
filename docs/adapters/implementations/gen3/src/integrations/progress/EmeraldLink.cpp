#include "EmeraldLink.h"
#include "EmeraldParty.h"
#include <QJsonArray>
#include <QtEndian>
#include <algorithm>
#include <array>
#include <cmath>

namespace trainer {
namespace {
struct Field { const char* name; int offset,size,count; };
constexpr Field fields[] = {
    {"personality",0,4,1},{"originalTrainer",4,4,1},{"nicknameCodes",8,1,10},
    {"language",18,1,1},{"sanity",19,1,1},{"trainerNameCodes",20,1,7},
    {"markings",27,1,1},{"reserved",30,2,1},
    {"species",32,2,1},{"heldItem",34,2,1},{"experience",36,4,1},
    {"ppBonuses",40,1,1},{"friendship",41,1,1},{"growthReserved",42,2,1},
    {"moves",44,2,4},{"pp",52,1,4},{"effort",56,1,6},{"condition",62,1,6},
    {"pokerus",68,1,1},{"metLocation",69,1,1},{"origins",70,2,1},
    {"individualValues",72,4,1},{"ribbons",76,4,1},{"status",80,4,1},
    {"level",84,1,1},{"mail",85,1,1},{"hp",86,2,1},{"stats",88,2,6}
};
quint32 read(const QByteArray& b,int at,int size) {
    return size==1?quint8(b[at]):size==2?qFromLittleEndian<quint16>(b.constData()+at):qFromLittleEndian<quint32>(b.constData()+at);
}
void put(QByteArray& b,int at,int size,quint32 value) {
    if(size==1)b[at]=char(value);else if(size==2)qToLittleEndian(quint16(value),b.data()+at);else qToLittleEndian(value,b.data()+at);
}
std::array<int,4> order(quint32 pid) {
    std::array<int,4> out{0,1,2,3};for(quint32 n=0;n<pid%24;++n)std::next_permutation(out.begin(),out.end());return out;
}
QString unsupported(const QByteArray& b) {
    if(b.size()!=100)return "Choose a Party member.";
    const auto mon=readEmeraldPartyMember(b);
    if(mon.kind!=PokemonSlotKind::Known || emeraldBoxRecord(b).isEmpty())return "Eggs and Pokemon carrying Mail must be traded inside the game.";
    if(quint8(b[18])!=2)return "This trade requires English Pokemon records.";
    // These need additional native evolution/personality-Dex handling. Never
    // silently exchange them without the expected in-game side effects.
    if(QList<int>{64,67,75,93,61,79,95,117,123,137,366,201,327}.contains(mon.number))
        return "Trade this species inside the game for now.";
    return {};
}
}
EmeraldLinkOffer exportEmeraldLinkRecord(const QByteArray& b) {
    const auto error=unsupported(b);if(!error.isEmpty())return {{},error};
    auto canonical=b;const auto key=read(b,0,4)^read(b,4,4);auto clear=b.mid(32,48);
    for(int p=0;p<48;p+=4)put(clear,p,4,read(clear,p,4)^key);
    const auto permutation=order(read(b,0,4));
    for(int i=0;i<4;++i)canonical.replace(32+permutation[i]*12,12,clear.mid(i*12,12));
    QJsonObject result{{"schema","emerald-individual-1"}};
    for(const auto& f:fields) {
        if(f.count==1)result[f.name]=double(read(canonical,f.offset,f.size));
        else {QJsonArray values;for(int i=0;i<f.count;++i)values.append(double(read(canonical,f.offset+i*f.size,f.size)));result[f.name]=values;}
    }
    return {result,{}};
}
QByteArray importEmeraldLinkRecord(const QJsonObject& object) {
    if(object["schema"]!="emerald-individual-1" || object.size()!=int(std::size(fields))+1)return {};
    QByteArray canonical(100,0);
    for(const auto& f:fields) {
        const auto v=object[f.name];const auto values=f.count==1?QJsonArray{v}:v.toArray();
        if(values.size()!=f.count)return {};
        for(int i=0;i<f.count;++i){
            const auto n=values[i];const auto value=n.toDouble(-1);
            if(!n.isDouble() || !std::isfinite(value) || value<0 || value>double(f.size==4?0xffffffffu:f.size==2?65535u:255u) || std::floor(value)!=value)return {};
            put(canonical,f.offset+i*f.size,f.size,quint32(value));
        }
    }
    const auto permutation=order(read(canonical,0,4));QByteArray clear;
    for(int p:permutation)clear+=canonical.mid(32+p*12,12);
    quint16 checksum=0;for(int p=0;p<48;p+=2)checksum=quint16(checksum+read(clear,p,2));
    put(canonical,28,2,checksum);const auto key=read(canonical,0,4)^read(canonical,4,4);
    for(int p=0;p<48;p+=4)put(canonical,32+p,4,read(clear,p,4)^key);
    return unsupported(canonical).isEmpty()?canonical:QByteArray{};
}
}
