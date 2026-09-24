#include "CenterActivities.h"
#include <algorithm>
#include <QStringList>

namespace trainer {
void CenterActivities::setParty(const QVariantList& actors, const QString& source, const QString& unavailable) {
    if (actors_ == actors && source_ == source && unavailable_ == unavailable) return;
    actors_ = actors; source_ = source; unavailable_ = unavailable;
    actor_ = 0; reaction_.clear(); gesture_.clear(); ++reactionSerial_;
    emit actorsChanged();
    emit changed();
}
void CenterActivities::react(const QString& gesture) {
    if (route_ != "playroom" || !hasParty()) return;
    const auto resting = [this](int index) {
        const auto condition = actors_[index].toMap()["condition"].toString();
        return condition == "Fainted" || condition == "Sleep";
    };
    gesture_ = resting(actor_) ? QStringLiteral("rest") : gesture;
    int partner = -1;
    if (gesture_ == "play") {
        for (int offset = 1; offset < actors_.size(); ++offset) {
            const int candidate = (actor_ + offset) % actors_.size();
            if (!resting(candidate)) { partner = candidate; break; }
        }
    }
    const auto name = actors_[actor_].toMap()["name"].toString();
    reaction_ = gesture_ == "rest" ? name + " is resting"
        : gesture_ == "call" ? name + " called over"
        : gesture_ == "greet" ? name + " greets you"
        : partner < 0 ? name + " plays catch"
        : name + " & " + actors_[partner].toMap()["name"].toString() + " play together";
    ++reactionSerial_;
    emit changed();
    emit reactionRequested(actor_, partner, gesture_);
}
QVariantMap CenterActivities::page() const {
    if (route_ == "menu") return {{"title", "Center activities"}, {"message", "Choose a place to visit"}, {"action", "Open"}};
    const auto title = route_ == "playroom" ? "Party Playroom" : route_ == "practice" ? "Practice" : "Link Counter";
    QString message, action = "Activities";
    if (route_ == "playroom") {
        message = hasParty() ? QString() : unavailable_.isEmpty() ? QStringLiteral("Your Party has no visitors yet.") : unavailable_;
        action = hasParty() ? "Call" : "Activities";
    } else if (!sample_) {
        message = route_ == "practice" ? "Practice needs verified Party records and supported battle rules."
            : "No supported transfer connection yet. Pairing and save changes are unavailable.";
    } else if (route_ == "practice") {
        message = stage_ == "setup" ? "Two sample partners · layout rehearsal only"
            : "Battle preview · no simulation, damage or rewards";
        action = stage_ == "setup" ? "Preview layout" : "Back to setup";
    } else {
        message = stage_ == "setup" ? "Sample peer · no discovery or connection is performed"
            : stage_ == "review" ? "Sample proposal · neither side can commit changes"
            : "Sample connection interrupted · no transaction was started";
        action = stage_ == "setup" ? "Review sample" : stage_ == "review" ? "Preview interruption" : "Reset rehearsal";
    }
    return {{"title", title}, {"message", message}, {"action", action}};
}
void CenterActivities::reset() {
    route_ = "menu"; stage_ = "setup"; reaction_.clear(); gesture_.clear(); actor_ = 0; menu_ = 0; emit changed();
}
void CenterActivities::activate(int index) {
    if (route_ == "menu") {
        if(index==3){menu_=3;emit changed();emit shopsRequested();return;}
        menu_ = std::clamp(index, 0, 2);
        route_ = QStringList{"playroom", "practice", "link"}[menu_];
        stage_ = "setup"; reaction_.clear(); actor_ = 0;
    } else if (route_ == "playroom" && hasParty()) {
        actor_ = std::clamp(index, 0, int(actors_.size()) - 1);
        react("call"); return;
    }
    else if (!sample_ || route_ == "playroom") route_ = "menu";
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
        if (action == Action::Down) menu_ = std::min(3, menu_ + 1);
    } else if (hasParty() && route_ == "playroom") {
        if (action == Action::Left || action == Action::Right) {
            actor_ = (actor_ + (action == Action::Left ? actors_.size()-1 : 1)) % actors_.size();
            reaction_.clear();
        }
        if (action == Action::Secondary) {
            react("greet"); return;
        }
        if (action == Action::LocalAction) { react("play"); return; }
    }
    emit changed();
}
}
