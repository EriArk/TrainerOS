#include "PartyPresentation.h"
#include <algorithm>

namespace trainer {
void PartyPresentation::configureArtwork(ClassicArt* art, SpriteArt* sprites) {
    art_ = art; sprites_ = sprites;
    if (art_) connect(art_, &ClassicArt::changed, this, &PartyPresentation::changed);
    emit changed();
}
void PartyPresentation::changeBox(int delta) {
    if (!sample_ || section_ != "storage" || detail_) return;
    box_ = (box_ + delta % 2 + 2) % 2; emit changed();
}
PartyPresentation::PartyPresentation(bool sample, QObject* parent) : QObject(parent), sample_(sample), activities_(sample, this) {
    connect(&activities_, &CenterActivities::changed, this, &PartyPresentation::changed);
    connect(&activities_, &CenterActivities::closeRequested, this, [this] {
        section_ = managementSection_; activitiesFocus_ = true; emit changed();
    });
}
void PartyPresentation::openActivities() {
    if (section_ != "party" && section_ != "storage") return;
    managementSection_ = section_; section_ = "activities"; detail_ = false;
    activities_.reset(); emit changed();
}
QString PartyPresentation::status() const {
    if (sample_) return "Development sample · not your save · all records are read-only";
    if (!id_.isEmpty() && title_.isEmpty()) return "The selected Adventure is no longer linked.";
    return id_.isEmpty() ? "Choose an Adventure with a supported save to view its Party and Storage."
        : "Party and Storage reading is not available for this Adventure yet.";
}
void PartyPresentation::setAdventure(const QString& id, const QString& title) {
    if (id_ != id) {
        id_ = id; detail_ = false; partyFocus_ = 0; storageFocus_[0] = storageFocus_[1] = 0; box_ = 0;
        activitiesFocus_ = false; boxFocus_ = false; activities_.reset();
    }
    title_ = title; emit changed();
}
QVariantMap PartyPresentation::slot(int index) const {
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
    if (row["kind"] == "known") {
        const auto target = row["target"].toString();
        row["art"] = art_ ? art_->image(target, "pokedexDetailArt") : QVariantMap{};
        if (sprites_) for (const auto& asset : sprites_->choices(target))
            if (asset.toMap()["kind"] == "sprite" && asset.toMap()["action"] == "Idle") { row["sprite"] = asset; break; }
    }
    return row;
}
QVariantList PartyPresentation::entries() const {
    QVariantList result;
    if (!sample_ || section_ == "saves" || section_ == "activities") return result;
    for (int i = 0; i < (section_ == "party" ? 6 : 30); ++i) result.append(slot(i));
    return result;
}
QVariantMap PartyPresentation::detail() const {
    return sample_ && (section_ == "party" || section_ == "storage") ? slot(section_ == "party" ? partyFocus_ : storageFocus_[box_]) : QVariantMap{};
}
void PartyPresentation::activate(int index) {
    if (section_ == "activities") { activities_.activate(index); return; }
    if (activitiesFocus_) { openActivities(); return; }
    if (boxFocus_) { changeBox(1); return; }
    if (detail_) { if (index == 1) openSaves(); else { detail_ = false; emit changed(); } return; }
    if (!sample_) { openSaves(); return; }
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
    if (action == Action::Down && (!sample_ || focusIndex() >= lastRow)) { activitiesFocus_ = true; emit changed(); return; }
    if (!sample_ || action == Action::Back) return;
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
