#include "PartyPresentation.h"
#include <algorithm>

namespace trainer {
QString PartyPresentation::boxName() const {
    return !sample_ && snapshot_ && box_ < snapshot_->boxes.size() ? snapshot_->boxes[box_].name : QString("Box %1").arg(box_+1);
}
void PartyPresentation::setProgress(const QString& adventureId, const GameProgress& progress) {
    if (sample_) return;
    const bool matches = adventureId == id_ && !id_.isEmpty();
    const auto state = matches ? progress.availability : ProgressAvailability::Unsupported;
    const QString key = matches ? adventureId + progress.contextRevision + progress.contentRevision + progress.saveRevision + QString::number(int(state)) : QString();
    if (observationKey_ == key && availability_ == state) return;
    observationKey_ = key; availability_ = state;
    snapshot_ = matches && state == ProgressAvailability::Available ? progress.party : std::nullopt;
    detail_ = false; activitiesFocus_ = false; boxFocus_ = false;
    if (snapshot_ && sourceContext_ != progress.contextRevision) {
        sourceContext_ = progress.contextRevision; initialBoxSet_ = false; partyFocus_ = 0;
        std::fill(std::begin(storageFocus_),std::end(storageFocus_),0);
    }
    if (!initialBoxSet_ && snapshot_ && snapshot_->error.isEmpty()) { box_ = snapshot_->currentBox; initialBoxSet_ = true; }
    if (box_ >= boxCount()) box_ = 0;
    syncActors();
    emit changed();
}
void PartyPresentation::syncActors() {
    QVariantList actors;
    if (sample_) {
        // Explicit development fixture; never fills missing production records.
        for (const auto& pair : {QPair<QString,QString>{"Bulbasaur","bulbasaur/1"}, {"Pikachu","pikachu/25"}})
            actors.append(withArt({{"name",pair.first},{"target",pair.second},{"kind","known"},{"level","12"},{"condition","Healthy"}}));
    } else if (available()) {
        for (int i=0;i<snapshot_->party.size() && i<6;++i)
            if (snapshot_->party[i].kind == PokemonSlotKind::Known) actors.append(present(snapshot_->party[i],i));
    }
    activities_.setParty(actors, id_ + observationKey_, status());
}
QVariantMap PartyPresentation::present(const PokemonRecord& p, int index) const {
    const auto kind = p.kind == PokemonSlotKind::Known ? "known" : p.kind == PokemonSlotKind::Egg ? "egg" : p.kind == PokemonSlotKind::Unreadable ? "unreadable" : "empty";
    QVariantMap row{{"index",index},{"kind",kind},{"name",p.kind == PokemonSlotKind::Egg ? "Egg" : p.kind == PokemonSlotKind::Empty ? "Empty slot" : "Unreadable slot"},
        {"condition",p.condition},{"level","—"},{"hp","—"}};
    if (p.kind != PokemonSlotKind::Known) return row;
    row["name"] = p.nickname.isEmpty() ? p.speciesName : p.nickname;
    row["species"] = p.speciesName; row["target"] = p.speciesId + '/' + p.formId;
    row["level"] = QString::number(p.level); row["types"] = p.types.join(" / ");
    row["ability"] = p.ability; row["nature"] = p.nature; row["item"] = p.item;
    row["shiny"] = p.shiny;
    if (p.hp) { row["hp"] = QString("%1 / %2").arg(*p.hp).arg(p.stats[0]); row["hpRatio"] = p.stats[0] ? double(*p.hp)/p.stats[0] : 0; }
    QVariantList stats; for (auto value : p.stats) stats.append(value); row["stats"] = stats;
    QStringList moves;
    for (const auto& move : p.moves) moves.append(move.name.isEmpty() ? QStringLiteral("—") : QString("%1 · %2 / %3 PP").arg(move.name).arg(move.pp).arg(move.maxPp));
    row["moves"] = moves.join('\n');
    return withArt(row);
}
void PartyPresentation::configureArtwork(ClassicArt* art, SpriteArt* sprites) {
    art_ = art; sprites_ = sprites;
    if (art_) connect(art_, &ClassicArt::changed, this, [this] { syncActors(); emit changed(); });
    syncActors();
    emit changed();
}
void PartyPresentation::changeBox(int delta) {
    if (!available() || boxCount() == 0 || section_ != "storage" || detail_) return;
    box_ = (box_ + delta % boxCount() + boxCount()) % boxCount(); emit changed();
}
PartyPresentation::PartyPresentation(bool sample, QObject* parent) : QObject(parent), sample_(sample), activities_(sample, this) {
    connect(&activities_, &CenterActivities::changed, this, &PartyPresentation::changed);
    connect(&activities_, &CenterActivities::closeRequested, this, [this] {
        section_ = managementSection_; activitiesFocus_ = true; emit changed();
    });
    syncActors();
}
void PartyPresentation::openActivities() {
    if (section_ != "party" && section_ != "storage") return;
    managementSection_ = section_; section_ = "activities"; detail_ = false;
    activities_.reset(); emit changed();
}
QString PartyPresentation::status() const {
    if (sample_) return "Development sample · not your save · all records are read-only";
    if (availability_ == ProgressAvailability::Checking) return "Reading your team…";
    if (availability_ == ProgressAvailability::Missing) return "Save in the Adventure, then return here.";
    if (availability_ == ProgressAvailability::Unreadable) return "The save could not be read. Close the game and try again.";
    if (snapshot_) return snapshot_->error;
    if (!id_.isEmpty() && title_.isEmpty()) return "The selected Adventure is no longer linked.";
    return id_.isEmpty() ? "Choose an Adventure with a supported save to view its Party and Storage."
        : "Party and Storage reading is not available for this Adventure yet.";
}
void PartyPresentation::setAdventure(const QString& id, const QString& title) {
    if (id_ != id) {
        id_ = id; detail_ = false; partyFocus_ = 0; std::fill(std::begin(storageFocus_),std::end(storageFocus_),0); box_ = 0;
        snapshot_.reset(); observationKey_.clear(); sourceContext_.clear(); initialBoxSet_ = false; availability_ = ProgressAvailability::Unsupported;
        activitiesFocus_ = false; boxFocus_ = false; activities_.reset();
    }
    title_ = title; syncActors(); emit changed();
}
QVariantMap PartyPresentation::slot(int index) const {
    if (!sample_) {
        if (!available()) return {};
        const auto& members = section_ == "party" ? snapshot_->party : snapshot_->boxes[box_].members;
        return index >= 0 && index < members.size() ? present(members[index], index) : QVariantMap{};
    }
    QVariantMap row{{"index",index},{"name","Empty slot"},{"species",""},{"summary","No Pokémon"},
        {"hp","—"},{"level","—"},{"condition","Empty"},{"item","—"},{"moves","—"},{"kind","empty"}};
    const int record = section_ == "party" ? index : box_ == 0 ? (index < 4 ? index : index % 5 == 0 ? index % 2 : 6) : (index == 7 ? 3 : 6);
    if (record == 0) {
        row["name"]="Bulbasaur"; row["species"]="#001"; row["level"]="12"; row["hp"]="28 / 35";
        row["condition"]="Healthy"; row["item"]="None"; row["moves"]="Tackle · 31 / 35 PP\nGrowl · 40 / 40 PP\nVine Whip · PP unknown";
        row["kind"]="known"; row["summary"]="Lv. 12 · HP 28 / 35";
        row["target"]="bulbasaur/1"; row["types"]="Grass / Poison"; row["ability"]="Overgrow"; row["nature"]="Modest";
        row["hpRatio"]=28.0/35; row["stats"]=QVariantList{35,18,20,25,23,17};
    } else if (record == 1) {
        row["name"]="Pikachu"; row["species"]="#025"; row["level"]="15"; row["hp"]="0 / 38";
        row["condition"]="Fainted"; row["item"]="Unknown"; row["moves"]="Move data unavailable";
        row["kind"]="known"; row["summary"]="Lv. 15 · Fainted";
        row["target"]="pikachu/25"; row["types"]="Electric"; row["ability"]="Static"; row["nature"]="—";
        row["hpRatio"]=0.0; row["stats"]=QVariantList{38,24,17,22,20,35};
    } else if (record == 2) {
        row["name"]="Egg"; row["condition"]="Egg"; row["summary"]="Species and hatch progress unknown"; row["kind"]="egg";
    } else if (record == 3) {
        row["name"]="Unreadable slot"; row["condition"]="Unavailable"; row["summary"]="Record could not be read"; row["kind"]="unreadable";
    }
    return withArt(row);
}
QVariantMap PartyPresentation::withArt(QVariantMap row) const {
    if (row["kind"] == "known") {
        const auto target = row["target"].toString();
        row["art"] = art_ ? art_->image(target, "pokedexDetailArt") : QVariantMap{};
        QVariantMap clips, portraits;
        if (sprites_) for (const auto& asset : sprites_->choices(target))
            if (asset.toMap()["kind"] == "sprite") {
                clips[asset.toMap()["action"].toString()] = asset;
                if (asset.toMap()["action"] == "Idle") row["sprite"] = asset;
            } else if (asset.toMap()["kind"] == "portrait")
                portraits[asset.toMap()["action"].toString()] = asset;
        row["clips"] = clips;
        row["portraits"] = portraits;
    }
    return row;
}
QVariantList PartyPresentation::entries() const {
    QVariantList result;
    if (!available() || section_ == "saves" || section_ == "activities") return result;
    for (int i = 0; i < (section_ == "party" ? 6 : 30); ++i) result.append(slot(i));
    return result;
}
QVariantMap PartyPresentation::detail() const {
    return available() && (section_ == "party" || section_ == "storage") ? slot(section_ == "party" ? partyFocus_ : storageFocus_[box_]) : QVariantMap{};
}
void PartyPresentation::activate(int index) {
    if (section_ == "activities") { activities_.activate(index); return; }
    if (activitiesFocus_) { openActivities(); return; }
    if (boxFocus_) { changeBox(1); return; }
    if (detail_) { if (index == 1) openSaves(); else { detail_ = false; emit changed(); } return; }
    if (!available()) { openSaves(); return; }
    const int count = section_ == "party" ? 6 : 30;
    if (index < 0 || index >= count || section_ == "saves") return;
    (section_ == "party" ? partyFocus_ : storageFocus_[box_]) = index;
    detail_ = true; menuIndex_ = 0; emit changed();
}
void PartyPresentation::openSaves() {
    if (section_ != "saves") previousSection_ = section_;
    section_ = "saves"; detail_ = false; emit changed();
}
void PartyPresentation::returnFromSaves() { section_ = previousSection_; emit changed(); }
void PartyPresentation::dispatch(Action action) {
    if (section_ == "activities") { activities_.dispatch(action); return; }
    if (detail_) {
        if (action == Action::Back) { detail_ = false; emit changed(); }
        else if (action == Action::Confirm) activate(menuIndex_);
        else if (action == Action::Up || action == Action::Down) { menuIndex_ = 1 - menuIndex_; emit changed(); }
        return;
    }
    if (action == Action::LocalAction) { openSaves(); return; }
    if (action == Action::Secondary) { section_ = section_ == "party" ? "storage" : "party"; activitiesFocus_ = false; boxFocus_ = false; emit changed(); return; }
    if (boxFocus_) {
        if (action == Action::Left || action == Action::Right || action == Action::Confirm) changeBox(action == Action::Left ? -1 : 1);
        else if (action == Action::Down || action == Action::Back) { boxFocus_ = false; emit changed(); }
        return;
    }
    if (action == Action::Confirm) { activate(focusIndex()); return; }
    if (activitiesFocus_) {
        if (action == Action::Up || action == Action::Back) { activitiesFocus_ = false; emit changed(); }
        return;
    }
    const int lastRow = section_ == "party" ? 4 : 24;
    if (action == Action::Down && (!available() || focusIndex() >= lastRow)) { activitiesFocus_ = true; emit changed(); return; }
    if (!available() || action == Action::Back) return;
    auto& focus = section_ == "party" ? partyFocus_ : storageFocus_[box_];
    const int columns = section_ == "party" ? 2 : 6;
    const int count = section_ == "party" ? 6 : 30;
    if (section_ == "storage" && action == Action::Up && focus < columns) boxFocus_ = true;
    else if (action == Action::Left && focus % columns > 0) --focus;
    else if (action == Action::Right && focus % columns < columns - 1) ++focus;
    else if (action == Action::Up) focus = std::max(0, focus - columns);
    else if (action == Action::Down) focus = std::min(count - 1, focus + columns);
    emit changed();
}
}
