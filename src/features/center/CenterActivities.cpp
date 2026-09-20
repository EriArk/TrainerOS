#include "CenterActivities.h"
#include <algorithm>
#include <QStringList>

namespace trainer {
QVariantMap CenterActivities::page() const {
    if (route_ == "menu") return {{"title", "Center activities"}, {"message", "Choose a place to visit"}, {"action", "A · Open"}};
    const auto title = route_ == "playroom" ? "Party Playroom" : route_ == "practice" ? "Practice" : "Link Counter";
    QString message, action = "A / B · Activities";
    if (!sample_) {
        message = route_ == "playroom" ? "A verified Party is needed before your Pokémon can visit."
            : route_ == "practice" ? "Practice needs verified Party records and supported battle rules."
            : "No supported transfer connection yet. Pairing and save changes are unavailable.";
    } else if (route_ == "playroom") {
        message = "Development actors · no sprite pack · no game data changes";
        action = "A · Call    X · Pet    ← / → · Choose";
    } else if (route_ == "practice") {
        message = stage_ == "setup" ? "Two sample partners · layout rehearsal only"
            : "Battle preview · no simulation, damage or rewards";
        action = stage_ == "setup" ? "A · Preview layout" : "A / B · Back to setup";
    } else {
        message = stage_ == "setup" ? "Sample peer · no discovery or connection is performed"
            : stage_ == "review" ? "Sample proposal · neither side can commit changes"
            : "Sample connection interrupted · no transaction was started";
        action = stage_ == "setup" ? "A · Review sample" : stage_ == "review" ? "A · Preview interruption" : "A · Reset rehearsal";
    }
    return {{"title", title}, {"message", message}, {"action", action}};
}
void CenterActivities::reset() {
    route_ = "menu"; stage_ = "setup"; reaction_.clear(); actor_ = 0; menu_ = 0; emit changed();
}
void CenterActivities::activate(int index) {
    if (route_ == "menu") {
        menu_ = std::clamp(index, 0, 2);
        route_ = QStringList{"playroom", "practice", "link"}[menu_];
        stage_ = "setup"; reaction_.clear(); actor_ = 0;
    } else if (!sample_) route_ = "menu";
    else if (route_ == "playroom") { actor_ = std::clamp(index, 0, 1); reaction_ = "Sample partner called over"; }
    else if (route_ == "practice") stage_ = stage_ == "setup" ? "preview" : "setup";
    else stage_ = stage_ == "setup" ? "review" : stage_ == "review" ? "interrupted" : "setup";
    emit changed();
}
void CenterActivities::dispatch(Action action) {
    if (action == Action::Back) {
        if (route_ == "menu") { emit closeRequested(); return; }
        if (stage_ != "setup") stage_ = "setup";
        else route_ = "menu";
        reaction_.clear();
    } else if (action == Action::Confirm) { activate(focusIndex()); return; }
    else if (route_ == "menu") {
        if (action == Action::Up) menu_ = std::max(0, menu_ - 1);
        if (action == Action::Down) menu_ = std::min(2, menu_ + 1);
    } else if (sample_ && route_ == "playroom") {
        if (action == Action::Left || action == Action::Right) { actor_ = action == Action::Left ? 0 : 1; reaction_.clear(); }
        if (action == Action::Secondary) reaction_ = "Sample partner greets you · friendship unchanged";
    }
    emit changed();
}
}
