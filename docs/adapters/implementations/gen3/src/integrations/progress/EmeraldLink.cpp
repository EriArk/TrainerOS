#include "EmeraldLink.h"
#include "EmeraldParty.h"
#include <QJsonArray>
#include <QFile>
#include <QJsonDocument>
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
    // These need additional native personality-Dex handling. Never
    // silently exchange them without the expected in-game side effects.
    if(QList<int>{201,327}.contains(mon.number))
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
EmeraldLinkOffer prepareEmeraldReceived(const QJsonObject& input) {
    const auto original=importEmeraldLinkRecord(input);
    if(original.isEmpty())return {{},"This Pokemon cannot be received here."};
    auto out=input;out["friendship"]=70;
    const auto before=readEmeraldPartyMember(original);const int item=before.itemId;
    // Pinned Emerald GetEvolutionTargetSpecies(EVO_MODE_TRADE). Everstone
    // blocks even ordinary trade evolution in this generation.
    struct Evolution {int species,item,target;};
    static constexpr Evolution evolutions[]={{64,0,65},{67,0,68},{75,0,76},{93,0,94},
        {61,187,186},{79,187,199},{95,199,208},{117,201,230},{123,199,212},
        {137,218,233},{366,192,367},{366,193,368}};
    int target=0;bool consume=false;
    if(item!=195)for(const auto& rule:evolutions)if(rule.species==before.number && (!rule.item || rule.item==item)){target=rule.target;consume=rule.item!=0;break;}
    if(!target)return {out,{}};
    static const auto reference=[] {QFile f(":/progress/emerald-reference.json");return f.open(QIODevice::ReadOnly)?QJsonDocument::fromJson(f.readAll()).object():QJsonObject{};}();
    static const auto learning=[] {QFile f(":/progress/emerald-shops.json");return f.open(QIODevice::ReadOnly)?QJsonDocument::fromJson(f.readAll()).object():QJsonObject{};}();
    if(learning.isEmpty())return {{},"Evolution learning data is unavailable."};
    const auto species=reference["species"].toObject();QString key;QJsonObject facts;
    for(auto it=species.begin();it!=species.end();++it)if(it.value().toObject()["number"].toInt()==target){key=it.key();facts=it.value().toObject();break;}
    if(key.isEmpty())return {{},"Evolution data is unavailable."};
    out["species"]=key.toInt();if(consume)out["heldItem"]=0;
    if(before.nickname==before.speciesName.toUpper()) {
        QJsonArray encoded;const auto chars=reference["characters"].toObject();
        for(const auto c:facts["name"].toString().toUpper()) {
            int code=-1;for(auto it=chars.begin();it!=chars.end();++it)if(it.value().toString()==QString(c)){code=it.key().toInt();break;}
            if(code<0)return {{},"The evolved name could not be encoded."};encoded.append(code);
        }
        if(encoded.size()>10)return {{},"The evolved name is too long."};while(encoded.size()<10)encoded.append(255);out["nicknameCodes"]=encoded;
    }
    // Native evolution learns level-matching moves into empty slots. A full
    // moveset is retained, as explicitly selected in the transfer review.
    auto moves=out["moves"].toArray(),pp=out["pp"].toArray();int bonuses=out["ppBonuses"].toInt();
    for(const auto& v:learning["levelMoves"].toObject()[key].toArray()) {
        const auto pair=v.toArray();if(pair.size()!=2 || pair[0].toInt()!=before.level || moves.contains(pair[1]))continue;
        for(int i=0;i<4;++i)if(moves[i].toInt()==0){moves[i]=pair[1];pp[i]=learning["moves"].toObject()[QString::number(pair[1].toInt())].toObject()["pp"].toInt();bonuses&=~(3<<(i*2));break;}
    }
    out["moves"]=moves;out["pp"]=pp;out["ppBonuses"]=bonuses;
    const auto evolving=importEmeraldLinkRecord(out);if(evolving.isEmpty())return {{},"The evolved Pokemon could not be verified."};
    const auto calculated=emeraldWithdrawRecord(evolving.left(80));if(calculated.isEmpty())return {{},"Evolution stats could not be calculated."};
    QJsonArray stats;for(int i=0;i<6;++i)stats.append(int(qFromLittleEndian<quint16>(calculated.constData()+88+2*i)));
    out["stats"]=stats;const int hp=before.hp.value_or(0);
    out["hp"]=hp?hp+stats[0].toInt()-before.stats[0]:0;
    if(importEmeraldLinkRecord(out).isEmpty())return {{},"The evolved Party data could not be verified."};
    return {out,{}};
}
}
