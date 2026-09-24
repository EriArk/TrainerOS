#include "SettingsController.h"
#include <algorithm>

namespace trainer {
void SettingsController::reload() { if (repository_ && !saving_) { value_ = repository_->preferences(); emit changed(); } }
void SettingsController::activate(int index) {
    if(index<0 || index>3 || saving_) return;
    auto candidate = value_;
    if (index == 0) {
        const QStringList themes{"turquoise", "red", "green", "blue", "orange"};
        candidate.theme = themes[(themes.indexOf(value_.theme) + 1) % themes.size()];
    } else if(index==3) candidate.videoPreviews=!candidate.videoPreviews;
    else if(index==2) candidate.worldEditing=!candidate.worldEditing;
    else candidate.reducedMotion = !candidate.reducedMotion;
    error_.clear(); saving_ = true; emit changed();
    const auto completed = [this, candidate](const QString& error) {
        saving_ = false; error_ = error;
        if (error.isEmpty()) value_ = candidate;
        else emit messageRequested(error);
        emit changed();
    };
    if (repository_) repository_->savePreferences(candidate, this, completed); else completed({});
}
QVariantList SettingsController::controls() const {
    const auto row = [](const char* title, const char* kind, const QString& detail) {
        return QVariantMap{{"title",title},{"kind",kind},{"detail",detail}};
    };
    switch(category_) {
    case 0: return {row("Shell color","theme",theme()),row("Reduced motion","toggle",reducedMotion()?"On":"Off"),row("Brightness","brightness","")};
    case 1: return {row("Volume","volume",""),row("Interface sounds","unavailable","Sound packs are not available yet"),row("Background music","unavailable","Music playback is not available yet")};
    case 2: return {row("Video previews","toggle",videoPreviews()?"On · silent playback":"Off"),row("Adventure pictures","status","Clean exit pictures on Home and in Choose Adventure"),row("Pokedex illustrations","status","Illustrations and animated companions")};
    case 3: return {row("Charger vibration","unavailable","Patterns have not been verified on this handheld"),row("Device lighting","unavailable","Lighting support has not been verified")};
    case 4: return {row("Trainer profile","action","Name, emblem and favorite"),row("RetroAchievements","action","Manage your account"), trainersAvailable_ ? row("Trainers","action","Choose a player or create a Trainer") : row("Separate Trainers & PIN","unavailable","Not available yet"),row("Trainer PIN",trainersAvailable_?"action":"unavailable","Set, change or remove your PIN"),row("Family code",trainersAvailable_?"action":"unavailable","A parent can reset forgotten PINs")};
    case 5: return {row("Refresh status","action",""),row("Restart","action",""),row("Power off","action","")};
    case 7: return {row("Check controller","action","Test buttons, sticks and triggers"),row("Button layout","status","Right A confirms; bottom B goes back"),row("Page navigation","status","L1 / R1 pages; L2 / R2 paired views")};
    case 8: {
        QVariantList result{row("Edit Worlds","toggle",worldEditing()?"On":"Off")};
        if(legacyTrash_)result.append(row("Previous game trash","action","Restore games removed by an earlier version"));
        return result;
    }
    default: return {};
    }
}
void SettingsController::selectCategory(int index, bool enter) {
    category_ = std::clamp(index,0,8); row_=0; pane_=enter;
    emit changed();
}
void SettingsController::activateRow(int index) {
    pane_=true; row_=std::clamp(index,0,std::max(0,int(controls().size())-1));
    if(category_==0 && row_<2) activate(row_);
    else if(category_==0 && row_==2) emit quickAdjustment(1,Action::Confirm);
    else if(category_==1 && row_==0) emit quickAdjustment(0,Action::Confirm);
    else if(category_==2 && row_==0) activate(3);
    else if(category_==4) emit trainerRequested(row_);
    else if(category_==5) emit deviceRequested(row_);
    else if(category_==7 && row_==0) emit controllerRequested();
    else if(category_==8 && row_==0) activate(2);
    else if(category_==8 && row_==1) emit trashRequested();
    emit changed();
}
void SettingsController::cycleTheme(int direction) {
    if(saving_) return;
    const QStringList themes{"turquoise","red","green","blue","orange"};
    auto candidate=value_;
    candidate.theme=themes[(themes.indexOf(value_.theme)+(direction<0?4:1))%5];
    error_.clear();saving_=true;emit changed();
    const auto completed=[this,candidate](const QString& error){saving_=false;error_=error;if(error.isEmpty())value_=candidate;else emit messageRequested(error);emit changed();};
    if(repository_) repository_->savePreferences(candidate,this,completed); else completed({});
}
void SettingsController::dispatch(Action action) {
    if(action==Action::Back) {
        if(pane_) {pane_=false;emit changed();} else emit closeRequested();
        return;
    }
    if(!pane_) {
        if(action==Action::Up) selectCategory(category_-1,false);
        else if(action==Action::Down) selectCategory(category_+1,false);
        else if(action==Action::Confirm || action==Action::Right) selectCategory(category_,true);
        return;
    }
    if(category_==6) {if(action==Action::Confirm){pane_=false;emit changed();}return;}
    if(action==Action::Up) row_=std::max(0,row_-1);
    if(action==Action::Down) row_=std::min(int(controls().size())-1,row_+1);
    if(action==Action::Confirm) {activateRow(row_);return;}
    if(action==Action::Left || action==Action::Right) {
        if(category_==0 && row_==0) cycleTheme(action==Action::Left?-1:1);
        else if(category_==0 && row_==1 && reducedMotion()!=(action==Action::Right)) activate(1);
        else if(category_==0 && row_==2) emit quickAdjustment(1,action);
        else if(category_==1 && row_==0) emit quickAdjustment(0,action);
        else if(category_==2 && row_==0 && videoPreviews()!=(action==Action::Right)) activate(3);
        else if(category_==8 && row_==0 && worldEditing()!=(action==Action::Right)) activate(2);
    }
    emit changed();
}
}
