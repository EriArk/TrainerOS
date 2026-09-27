#include "EmeraldPractice.h"
#include "Gen3Progress.h"
#include <QJsonArray>

namespace trainer {
namespace {
template<size_t N> QJsonArray array(const std::array<int,N>& values) {
    QJsonArray out;for(int value:values)out.append(value);return out;
}
}
PracticePair emeraldPracticePair(const GameProgress& progress,int first,int second) {
    if(progress.availability!=ProgressAvailability::Available
        || gen3Edition(progress.contentRevision)!=Gen3Edition::Emerald
        || progress.contextRevision.isEmpty() || progress.saveRevision.isEmpty()
        || !progress.party || !progress.party->error.isEmpty())
        return {{},"A verified Emerald Party is required."};
    const auto& party=progress.party->party;
    if(first<0 || second<0 || first>=party.size() || second>=party.size()
        || first==second || party.size()>6)return {{},"Choose two different Party members."};
    QJsonArray members;
    for(int index:{first,second}) {
        const auto& mon=party[index];
        if(mon.kind!=PokemonSlotKind::Known || !mon.battle || !mon.hp || mon.moves.size()!=4)
            return {{},"This Party member has no verified battle record."};
        const auto& facts=*mon.battle;
        QJsonArray moves;
        for(int slot=0;slot<4;++slot) moves.append(QJsonObject{
            {"id",facts.moveIds[slot]},{"ppUps",facts.ppUps[slot]},
            {"maxPp",mon.moves[slot].maxPp}});
        members.append(QJsonObject{{"number",mon.number},{"form",mon.formId},
            {"level",mon.level},{"ability",facts.abilityId},{"nature",facts.natureId},
            {"item",mon.itemId},{"friendship",facts.friendship},{"gender",facts.gender},
            {"ivs",array(facts.ivs)},{"evs",array(facts.evs)},
            {"stats",array(mon.stats)},{"moves",moves}});
    }
    return {{{"protocol",1},{"members",members}}, {}};
}
}
