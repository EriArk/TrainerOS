#include "SettingsController.h"
#include <algorithm>

namespace trainer {
void SettingsController::setLibraryScanState(bool available,bool busy,const QString& result) {
    libraryAvailable_=available;libraryScanning_=busy;
    if(busy)libraryStatus_.clear();
    else if(!result.isEmpty())libraryStatus_=result;
    emit changed();
}
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
    case 7: return {row("Check controller","action","Test buttons, sticks and triggers"),row("Button layout","status","Right A confirms; bottom B goes back"),row("Page navigation","status","L1 / R1 pages; L2 / R2 secondary pages")};
    case 8: {
        QVariantList result{row("Edit Worlds","toggle",worldEditing()?"On":"Off")};
        result.append(row(libraryScanning_?"Refreshing library…":"Refresh library",
            libraryAvailable_ && !libraryScanning_?"action":"unavailable",
            libraryScanning_?"Looking for games and updated artwork":libraryAvailable_?"Find copied games and reload artwork":"Connect your game library first"));
        result.append(row("Game storage",storage_.apply?"action":"unavailable",storage_.root()));
        if(legacyTrash_)result.append(row("Previous game trash","action","Restore games removed by an earlier version"));
        return result;
    }
    case 9: return {row("Read-only saves",savePolicy_?"toggle":"unavailable",readOnlySaves()?"On - reading and backups only":"Off - allow confirmed save changes")};
    case 10: return {}; // NetworkController owns the inline connections pane.
    default: return {};
    }
}
void SettingsController::selectCategory(int index, bool enter) {
    if(storage_.busy() || clock_.busy())return;
    clock_.leave();
    storage_.close();
    category_ = std::clamp(index,0,int(categories().size())-1); row_=0; pane_=enter;
    if(category_==11)clock_.begin();
    emit changed();
}
void SettingsController::activateRow(int index) {
    if(category_==11){clock_.activate(index);return;}
    if(storage_.isOpen()){storage_.activate(index);return;}
    if(saving_)return;
    pane_=true; row_=std::clamp(index,0,std::max(0,int(controls().size())-1));
    if(category_==0 && row_<2) activate(row_);
    else if(category_==0 && row_==2) emit quickAdjustment(1,Action::Confirm);
    else if(category_==1 && row_==0) emit quickAdjustment(0,Action::Confirm);
    else if(category_==2 && row_==0) activate(3);
    else if(category_==4) emit trainerRequested(row_);
    else if(category_==5) emit deviceRequested(row_);
    else if(category_==7 && row_==0) emit controllerRequested();
    else if(category_==8 && row_==0) activate(2);
    else if(category_==8 && row_==1 && libraryAvailable_ && !libraryScanning_) emit libraryRefreshRequested();
    else if(category_==8 && row_==2) storage_.begin();
    else if(category_==8 && row_==3 && legacyTrash_) emit trashRequested();
    else if(category_==9 && savePolicy_) {
        saving_=true;error_.clear();
        savePolicy_->setReadOnly(!readOnlySaves(),this,[this](const QString& error){saving_=false;error_=error;emit changed();});
    }
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
    if(clock_.busy())return;
    if(category_==11 && pane_){clock_.dispatch(action);return;}
    if(storage_.isOpen()){storage_.dispatch(action);return;}
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
        else if(category_==9 && readOnlySaves()!=(action==Action::Right)) activateRow(0);
        else if(category_==8 && row_==0 && worldEditing()!=(action==Action::Right)) activate(2);
    }
    emit changed();
}
}
