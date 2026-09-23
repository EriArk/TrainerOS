#include "SessionState.h"
#include <algorithm>

namespace trainer {
SessionState::SessionState(ShellController& shell, LocalStateStore* store, QObject* parent)
    : QObject(parent), shell_(shell), store_(store), access_(store,this) {
    connect(&shell_, &ShellController::exitRequested, this, &SessionState::requestExit);
    if (!store_) return;
    connect(&access_,&TrainerAccessController::changed,this,&SessionState::changed);
    connect(shell_.trainerSetup(),&TrainerSetupPresentation::removeRequested,this,&SessionState::requestTrainerRemoval);
    connect(&access_,&TrainerAccessController::removalRequested,this,[this](SecretPin code){
        if(entryGate_ || !restored_ || adventureActive_ || serviceActive_ || writing_ || store_->pending()
            || closing_ || switching_ || creating_ || !error_.isEmpty() || (switchGuard_ && !switchGuard_())) {
            access_.removalFailed("Finish the current operation first.");return;
        }
        if(prepareRemoval_)if(const auto e=prepareRemoval_();!e.isEmpty()){access_.removalFailed(e);return;}
        debounce_.stop();switching_=true;emit changed();
        store_->removeCurrentTrainer(code,this,[this](const QString& e){
            if(!e.isEmpty()){switching_=false;access_.removalFailed(e);emit changed();return;}
            nextTrainer_.clear();emit trainerRestartReady();
        });
    });
    connect(&access_,&TrainerAccessController::unlocked,this,[this](const QString& id){verifiedTrainer_=id;requestTrainerSwitch(id);});
    connect(&shell_,&ShellController::pinRequested,this,[this](bool family){
        if(canChangeTrainer())access_.beginManage(family);
        else shell_.showNotice("Finish the current operation first.");
    });
    connect(store_,&LocalStateStore::accessNeeded,this,[this]{
        entryGate_=true;error_.clear();shell_.trainerSetup()->configure(store_->trainers(),store_->ownerId());shell_.trainerSetup()->setFamilyReady(store_->familyProtected());
        shell_.trainerSetup()->beginStartup();focus_=0;emit changed();
    });
    connect(shell_.trainerSetup(),&TrainerSetupPresentation::closeRequested,this,[this]{if(entryGate_)shell_.trainerSetup()->beginStartup();});
    connect(&shell_,&ShellController::trainersRequested,this,&SessionState::requestTrainers);
    connect(shell_.trainerSetup(),&TrainerSetupPresentation::selectRequested,this,&SessionState::requestTrainerSwitch);
    connect(shell_.trainerSetup(),&TrainerSetupPresentation::createRequested,this,&SessionState::createTrainer);
    connect(store_,&LocalStateStore::trainersChanged,this,[this]{shell_.trainerSetup()->configure(store_->trainers(),store_->ownerId());shell_.trainerSetup()->setFamilyReady(store_->familyProtected());});
    debounce_.setSingleShot(true); debounce_.setInterval(300);
    connect(&debounce_, &QTimer::timeout, this, &SessionState::flush);
    connect(&shell_, &ShellController::changed, this, &SessionState::stateChanged);
    connect(store_, &LocalStateStore::opened, this, [this](bool success) {
        if (success) {
            entryGate_=false;verifiedTrainer_.clear();shell_.trainerSetup()->close();
            shell_.trainerSetup()->configure(store_->trainers(),store_->ownerId());shell_.trainerSetup()->setFamilyReady(store_->familyProtected());
            shell_.settings()->setTrainersAvailable(true);
            shell_.trainer()->reload();
            shell_.pokedex()->refresh();
            shell_.hall()->refreshArchive();
            shell_.refreshLibrary(); shell_.settings()->reload();
            shell_.restoreNavigation(store_->navigation());
            committed_ = store_->navigation();
            desired_ = shell_.navigationState();
            restored_ = true;
            if (closing_) flush();
        } else {entryGate_=false;error_ = store_->error();}
        focus_ = 0; emit changed();
        finishExit();
    });
    connect(store_, &LocalStateStore::pendingChanged, this, &SessionState::finishExit);
    connect(store_, &LocalStateStore::userWriteFailed, this, [this] {
        // Never close over a failed explicit Save. Its controller keeps the draft/error.
        closing_ = false; nextTrainer_.clear(); emit changed();
    });
}
bool SessionState::canChangeTrainer() const {
    return store_ && (restored_ || entryGate_) && !(creating_ || switching_ || closing_ || access_.active() || !error_.isEmpty()) && !adventureActive_ && !serviceActive_
        && !store_->pending() && (!switchGuard_ || switchGuard_());
}
void SessionState::requestTrainers() {
    if(!canChangeTrainer()){shell_.showNotice("Finish the current operation before choosing a Trainer.");return;}
    shell_.openTrainers();
}
void SessionState::requestTrainerRemoval() {
    if(entryGate_ || !canChangeTrainer()){shell_.showNotice("Open your Trainer and finish the current operation first.");return;}
    access_.beginRemoval();
}
void SessionState::createTrainer(const TrainerProfile& profile) {
    if(!canChangeTrainer()){shell_.trainerSetup()->failed("Finish the current operation and try again.");return;}
    creating_=true;shell_.trainerSetup()->setBusy(true);emit changed();
    store_->createProtectedTrainer(profile,shell_.trainerSetup()->registrationPin(),this,[this,profile](const ProfileWriteResult& result){
        creating_=false;shell_.trainerSetup()->setBusy(false);emit changed();
        if(!result.success){shell_.trainerSetup()->failed(result.error);return;}
        // Completion precedes the store pending counter update.
        QTimer::singleShot(0,this,[this,profile]{verifiedTrainer_=profile.id;requestTrainerSwitch(profile.id);});
    });
}
void SessionState::requestTrainerSwitch(const QString& id) {
    if(!canChangeTrainer()){shell_.trainerSetup()->failed("Finish the current operation and try again.");return;}
    bool found=false;for(const auto& profile:store_->trainers())if(profile.id==id)found=true;
    if(!found){shell_.trainerSetup()->failed("This Trainer is unavailable. Choose again.");return;}
    if(store_->pinProtected(id) && verifiedTrainer_!=id && (entryGate_ || id!=store_->ownerId())){access_.beginUnlock(id);emit changed();return;}
    if(entryGate_) {
        switching_=true;emit changed();store_->unlock(id,this,[this](const QString& e){
            switching_=false;if(!e.isEmpty())shell_.trainerSetup()->failed(e);emit changed();
        });return;
    }
    verifiedTrainer_.clear();
    if(id==store_->ownerId()){shell_.goToPage(shell_.page());shell_.trainer()->reload();return;}
    nextTrainer_=id;requestExit();
}
void SessionState::start() { if (store_) store_->open(); }
QString SessionState::title() const {
    if(creating_)return "Creating your Trainer";
    if(switching_ || !nextTrainer_.isEmpty())return "Changing Trainer";
    if (!error_.isEmpty()) return restored_ ? "Browsing state wasn't saved" : "Your data needs attention";
    return closing_ ? "Finishing your session" : "Opening your Trainer journal";
}
QString SessionState::message() const {
    if(creating_)return "Preparing your Trainer.";
    if(switching_)return "Opening the selected Trainer. Please wait…";
    if (!error_.isEmpty()) return error_;
    return closing_ ? "Saving your place. Your Trainer and favorites stay on this device."
                    : "Preparing your profile, favorites and last browsing position.";
}
QStringList SessionState::choices() const {
    if (error_.isEmpty()) return {};
    if (!restored_) return {"Retry", "Exit Development App"};
    if (closing_) return {"Retry", "Keep browsing", "Exit without browsing state"};
    return {"Retry", "Keep browsing"};
}
void SessionState::stateChanged() {
    if (!restored_) return;
    const auto next = shell_.navigationState();
    if (next == desired_) return;
    desired_ = next;
    if (!paused_ && !closing_) debounce_.start();
}
void SessionState::flush() {
    debounce_.stop();
    if (!store_ || !restored_ || writing_) return;
    if (desired_ == committed_) { finishExit(); return; }
    writing_ = true;
    const auto state = desired_;
    store_->saveNavigation(state, this, [this, state](const QString& error) {
        writing_ = false;
        if (!error.isEmpty()) { error_ = error; paused_ = true; focus_ = 0; }
        else {
            committed_ = state;
            if (desired_ != committed_ && !paused_) debounce_.start(closing_ ? 0 : 300);
        }
        emit changed();
        finishExit();
    });
}
void SessionState::requestExit() {
    // Closing the shell must not destroy an owned emulator process and its save
    // operation. Finish the Adventure in its own interface, then exit the shell.
    if (adventureActive_ || creating_ || switching_ || access_.active() || entryGate_) return;
    if (!store_) { closing_=true;emit changed();finishExit();return; }
    closing_ = true; paused_ = false; focus_ = 0;
    // Preserve a visible failed flush until the user chooses Retry or an explicit skip.
    if (error_.isEmpty()) flush();
    emit changed();
    finishExit();
}
void SessionState::finishExit() {
    if (creating_ || switching_ || !closing_ || serviceActive_ || !error_.isEmpty() || (store_ && (store_->opening() || store_->pending())) || writing_) return;
    if (restored_ && desired_ != committed_) { flush(); return; }
    if(!nextTrainer_.isEmpty()) {
        if(switchGuard_ && !switchGuard_()){closing_=false;nextTrainer_.clear();shell_.trainerSetup()->failed("Finish the account update and try again.");emit changed();return;}
        switching_=true;emit changed();
        store_->stageTrainerAsync(nextTrainer_,this,[this](const QString& error){
            if(error.isEmpty()){emit trainerRestartReady();return;}
            switching_=false;closing_=false;nextTrainer_.clear();shell_.trainerSetup()->failed(error);emit changed();
        });return;
    }
    emit exitReady();
}
void SessionState::activate(int index) {
    if(creating_ || switching_)return;
    const auto options = choices();
    if (index < 0 || index >= options.size()) return;
    if (index == 0) {
        error_.clear(); paused_ = false; focus_ = 0;
        if (restored_) flush(); else store_->open();
    } else if (!restored_ || index == 2) {
        // Only optional browsing state is skipped. Explicit profile/favorite writes drain first.
        error_.clear(); desired_ = committed_; paused_ = true; closing_ = true;
        debounce_.stop(); finishExit();
    } else {
        error_.clear(); paused_ = true; closing_ = false; nextTrainer_.clear();
    }
    emit changed();
}
void SessionState::dispatch(Action action) {
    if(access_.active()){
        if(!entryGate_ && !access_.busy() && (action==Action::PreviousPage || action==Action::NextPage || action==Action::Home)) {
            access_.cancel();shell_.dispatch(action);emit changed();
        } else access_.dispatch(action);
        return;
    }
    if(entryGate_) {
        if(creating_ || switching_ || store_->opening())return;
        // No page, Home, Start or paired-face destination exists before entry.
        if(action==Action::PreviousPage || action==Action::NextPage || action==Action::Home || action==Action::SystemMenu || action==Action::PreviousFace || action==Action::NextFace)return;
        if(shell_.keyboard()->isOpen())shell_.keyboard()->dispatch(action);
        else shell_.trainerSetup()->dispatch(action);
        return;
    }
    if(creating_ || switching_)return;
    if (adventureActive_) return;
    if (!blocked()) { shell_.dispatch(action); return; }
    if (restored_ && (action == Action::PreviousPage || action == Action::NextPage)) {
        closing_ = false; nextTrainer_.clear();
        if (!error_.isEmpty()) { error_.clear(); paused_ = true; }
        shell_.dispatch(action); emit changed(); return;
    }
    if (action == Action::Confirm) activate(focus_);
    else if (action == Action::Back) {
        if (!choices().isEmpty()) activate(1);
        else if (closing_) { closing_ = false; nextTrainer_.clear(); emit changed(); }
    } else if (action == Action::Up || action == Action::Left) focus_ = std::max(0, focus_ - 1);
    else if (action == Action::Down || action == Action::Right) focus_ = std::min(std::max(0, int(choices().size()) - 1), focus_ + 1);
    emit changed();
}
}
