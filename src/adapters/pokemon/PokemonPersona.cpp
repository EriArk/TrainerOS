#include "PokemonPersona.h"
namespace trainer {
PokemonPersona::PokemonPersona(TrainerRepository& repository):TrainerController(repository),picker_(this) {
    connect(&picker_,&SpeciesPicker::changed,this,&TrainerController::changed);
    connect(&picker_,&SpeciesPicker::selected,this,[this](const QString& id){if(!editing_ || saving_)return;draft_.favoritePokemonId=id;error_.clear();emit changed();});
}
void PokemonPersona::configure(LibraryRepository* library, PokedexReferenceProvider* reference,
        PokedexProgressRepository* journal, HallOfFameRepository* archive) {
    library_ = library; reference_ = reference; journal_ = journal; archive_ = archive;
    picker_.setReference(reference);
}
void PokemonPersona::refreshOverview() {
    if (!library_ || !reference_ || !journal_ || !archive_) return;
    const auto reference = reference_->load();
    if (reference.success) {
        names_.clear();
        for (const auto& entry : reference.entries) names_.insert(entry.id, entry.name);
    }
    overview_ = trainerOverview(*library_, reference, *journal_, *archive_);
    emit changed();
}
QVariantList PokemonPersona::overview() const {
    const auto count = [](const std::optional<int>& value) { return value ? QString::number(*value) : QString("—"); };
    const auto seconds = overview_.recordedSeconds;
    const QString time = !seconds ? "—" : *seconds < 60 ? "< 1 min"
        : *seconds < 3600 ? QString::number(*seconds / 60) + " min"
        : QString("%1 h %2 m").arg(*seconds / 3600).arg((*seconds % 3600) / 60);
    return {QVariantMap{{"label", "ADVENTURES"}, {"value", QString::number(overview_.adventures)}},
        QVariantMap{{"label", "WORLDS"}, {"value", QString::number(overview_.worlds)}},
        QVariantMap{{"label", "FAVORITE MARKS"}, {"value", count(overview_.favorites)}},
        QVariantMap{{"label", "MEMORIES"}, {"value", count(overview_.memories)}},
        QVariantMap{{"label", "RECORDED TIME"}, {"value", time}}};
}
QString PokemonPersona::favoriteLabel(const QString& id) const {
    if (id.isEmpty()) return "Not chosen";
    return names_.value(id, id.left(1).toUpper() + id.mid(1));
}
}
