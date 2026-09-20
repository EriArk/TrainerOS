#include "PartyPresentation.h"
#include <algorithm>

namespace trainer {
QString PartyPresentation::status() const {
    if (sample_) return "Development sample · not your save · all records are read-only";
    if (!id_.isEmpty() && title_.isEmpty()) return "The selected Adventure is no longer linked. Y chooses another.";
    return id_.isEmpty() ? "Y chooses an Adventure. Party and Storage need a supported save reader."
        : "Party and Storage reading is not available for this Adventure yet.";
}
void PartyPresentation::setAdventure(const QString& id, const QString& title) {
    if (id_ != id) {
        id_ = id; detail_ = false; partyFocus_ = 0; storageFocus_[0] = storageFocus_[1] = 0; box_ = 0;
    }
    title_ = title; emit changed();
}
QVariantMap PartyPresentation::slot(int index) const {
    QVariantMap row{{"index",index},{"name","Empty slot"},{"species",""},{"summary","No Pokémon"},
        {"hp","—"},{"level","—"},{"condition","Empty"},{"item","—"},{"moves","—"},{"kind","empty"}};
    const int record = section_ == "party" ? index : box_ == 0 ? (index < 3 ? index : 6) : (index == 5 ? 3 : 6);
    if (record == 0) {
        row["name"]="Bulbasaur"; row["species"]="#001"; row["level"]="12"; row["hp"]="28 / 35";
        row["condition"]="Healthy"; row["item"]="None"; row["moves"]="Tackle · 31 / 35 PP\nGrowl · 40 / 40 PP\nVine Whip · PP unknown";
        row["kind"]="known"; row["summary"]="Lv. 12 · HP 28 / 35";
    } else if (record == 1) {
        row["name"]="Pikachu"; row["species"]="#025"; row["level"]="15"; row["hp"]="0 / 38";
        row["condition"]="Fainted"; row["item"]="Unknown"; row["moves"]="Move data unavailable";
        row["kind"]="known"; row["summary"]="Lv. 15 · Fainted";
    } else if (record == 2) {
        row["name"]="Egg"; row["condition"]="Egg"; row["summary"]="Species and hatch progress unknown"; row["kind"]="egg";
    } else if (record == 3) {
        row["name"]="Unreadable slot"; row["condition"]="Unavailable"; row["summary"]="Record could not be read"; row["kind"]="unreadable";
    }
    return row;
}
QVariantList PartyPresentation::entries() const {
    QVariantList result;
    if (!sample_ || section_ == "saves") return result;
    for (int i = 0; i < (section_ == "party" ? 6 : 12); ++i) result.append(slot(i));
    return result;
}
QVariantMap PartyPresentation::detail() const {
    return sample_ && section_ != "saves" ? slot(focusIndex()) : QVariantMap{};
}
void PartyPresentation::activate(int index) {
    if (detail_) { detail_ = false; emit changed(); return; }
    if (!sample_) { openSaves(); return; }
    const int count = section_ == "party" ? 6 : 12;
    if (index < 0 || index >= count || section_ == "saves") return;
    (section_ == "party" ? partyFocus_ : storageFocus_[box_]) = index;
    detail_ = true; emit changed();
}
void PartyPresentation::openSaves() {
    if (section_ != "saves") previousSection_ = section_;
    section_ = "saves"; detail_ = false; emit changed();
}
void PartyPresentation::returnFromSaves() { section_ = previousSection_; emit changed(); }
void PartyPresentation::dispatch(Action action) {
    if (detail_) {
        if (action == Action::Back || action == Action::Confirm) { detail_ = false; emit changed(); }
        return;
    }
    if (action == Action::LocalAction) { openSaves(); return; }
    if (action == Action::Secondary) { section_ = section_ == "party" ? "storage" : "party"; emit changed(); return; }
    if (action == Action::Confirm) { activate(focusIndex()); return; }
    if (!sample_ || action == Action::Back) return;
    auto& focus = section_ == "party" ? partyFocus_ : storageFocus_[box_];
    const int columns = section_ == "party" ? 2 : 4;
    const int count = section_ == "party" ? 6 : 12;
    // Horizontal edge presses switch sample boxes; shoulder buttons remain global.
    if (section_ == "storage" && ((action == Action::Right && focus % 4 == 3) || (action == Action::Left && focus % 4 == 0))) {
        box_ = 1 - box_;
    } else {
        const int delta = action == Action::Left ? -1 : action == Action::Right ? 1 : action == Action::Up ? -columns : action == Action::Down ? columns : 0;
        focus = std::clamp(focus + delta, 0, count - 1);
    }
    emit changed();
}
}
