#include "CenterActivities.h"
#include <algorithm>
#include <QStringList>

namespace trainer {
CenterActivities::CenterActivities(bool sample,QObject* parent):QObject(parent),sample_(sample),practice_(this),link_(this) {
    connect(&link_,&LinkController::changed,this,&CenterActivities::changed);
    connect(&link_,&LinkController::closeRequested,this,&CenterActivities::closeRequested);
    connect(&practice_,&PracticeController::changed,this,&CenterActivities::changed);
    connect(&practice_,&PracticeController::closeRequested,this,[this]{route_="playroom";emit changed();});
}
void CenterActivities::showPlace(const QString& place) {
    if(route_==place){if(place=="practice" && !practice_.isOpen())practice_.enter();if(place=="link" && !link_.isOpen())link_.enter();return;}
    if(route_=="practice")practice_.leave();
    if(route_=="link")link_.leave();
    route_=place;stage_="setup";
    if(place=="practice")practice_.enter();
    if(place=="link")link_.enter();
    emit changed();
}
void CenterActivities::setParty(const QVariantList& actors, const QString& source, const QString& unavailable) {
    if (actors_ == actors && source_ == source && unavailable_ == unavailable) return;
    actors_ = actors; source_ = source; unavailable_ = unavailable;
    actor_ = 0; reaction_.clear(); gesture_.clear(); ++reactionSerial_;
    practice_.setActors(actors);
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
    } else if (route_ == "practice") {
        message = practice_.message();
    } else if (!sample_) {
        message = "No supported transfer connection yet. Pairing and save changes are unavailable.";
    } else {
        message = stage_ == "setup" ? "Sample peer · no discovery or connection is performed"
            : stage_ == "review" ? "Sample proposal · neither side can commit changes"
            : "Sample connection interrupted · no transaction was started";
        action = stage_ == "setup" ? "Review sample" : stage_ == "review" ? "Preview interruption" : "Reset rehearsal";
    }
    return {{"title", title}, {"message", message}, {"action", action}};
}
void CenterActivities::reset() {
    link_.leave();
    practice_.leave();
    route_ = "menu"; stage_ = "setup"; reaction_.clear(); gesture_.clear(); actor_ = 0; menu_ = 0; emit changed();
}
void CenterActivities::activate(int index) {
    if(route_=="link"){link_.activate(index);return;}
    if(route_=="practice"){practice_.activate(index);return;}
    if (route_ == "menu") {
        if(index==3){menu_=3;emit changed();emit shopsRequested();return;}
        menu_ = std::clamp(index, 0, 2);
        route_ = QStringList{"playroom", "practice", "link"}[menu_];
        if(route_=="practice")practice_.enter();
        if(route_=="link")link_.enter();
        stage_ = "setup"; reaction_.clear(); actor_ = 0;
    } else if (route_ == "playroom" && hasParty()) {
        actor_ = std::clamp(index, 0, int(actors_.size()) - 1);
        react("call"); return;
    }
    else if (!sample_ || route_ == "playroom") return;
    else stage_ = stage_ == "setup" ? "review" : stage_ == "review" ? "interrupted" : "setup";
    emit changed();
}
void CenterActivities::dispatch(Action action) {
    if(route_=="link"){link_.dispatch(action);return;}
    if(route_=="practice"){practice_.dispatch(action);return;}
    if(route_=="playroom" && action==Action::Up){openPractice();return;}
    if (action == Action::Back) {
        if (route_ == "menu") { emit closeRequested(); return; }
        if (stage_ != "setup") stage_ = "setup";
        else if(route_=="link"){emit closeRequested();return;}
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
