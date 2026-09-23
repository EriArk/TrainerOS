#include "SessionState.h"
#include <algorithm>

namespace trainer {
SessionState::SessionState(ShellController& shell, LocalStateStore* store, QObject* parent)
    : QObject(parent), shell_(shell), store_(store) {
    connect(&shell_, &ShellController::exitRequested, this, &SessionState::requestExit);
    if (!store_) return;
    connect(&shell_,&ShellController::trainersRequested,this,&SessionState::requestTrainers);
    connect(shell_.trainerSetup(),&TrainerSetupPresentation::selectRequested,this,&SessionState::requestTrainerSwitch);
    connect(shell_.trainerSetup(),&TrainerSetupPresentation::createRequested,this,&SessionState::createTrainer);
    connect(store_,&LocalStateStore::trainersChanged,this,[this]{shell_.trainerSetup()->configure(store_->trainers(),store_->ownerId());});
    debounce_.setSingleShot(true); debounce_.setInterval(300);
    connect(&debounce_, &QTimer::timeout, this, &SessionState::flush);
    connect(&shell_, &ShellController::changed, this, &SessionState::stateChanged);
    connect(store_, &LocalStateStore::opened, this, [this](bool success) {
        if (success) {
            shell_.trainerSetup()->configure(store_->trainers(),store_->ownerId());
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
        } else error_ = store_->error();
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
    return store_ && restored_ && !blocked() && !adventureActive_ && !serviceActive_
        && !store_->pending() && (!switchGuard_ || switchGuard_());
}
void SessionState::requestTrainers() {
    if(!canChangeTrainer()){shell_.showNotice("Finish the current operation before choosing a Trainer.");return;}
    shell_.openTrainers();
}
void SessionState::createTrainer(const TrainerProfile& profile) {
    if(!canChangeTrainer()){shell_.trainerSetup()->failed("Finish the current operation and try again.");return;}
    creating_=true;shell_.trainerSetup()->setBusy(true);emit changed();
    store_->createTrainerAsync(profile,this,[this,profile](const ProfileWriteResult& result){
        creating_=false;shell_.trainerSetup()->setBusy(false);emit changed();
        if(!result.success){shell_.trainerSetup()->failed(result.error);return;}
        // Completion precedes the store pending counter update.
        QTimer::singleShot(0,this,[this,profile]{requestTrainerSwitch(profile.id);});
    });
}
void SessionState::requestTrainerSwitch(const QString& id) {
    if(!canChangeTrainer()){shell_.trainerSetup()->failed("Finish the current operation and try again.");return;}
    bool found=false;for(const auto& profile:store_->trainers())if(profile.id==id)found=true;
    if(!found){shell_.trainerSetup()->failed("This Trainer is unavailable. Choose again.");return;}
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
    if(creating_)return "Preparing a personal journal. Games and game saves stay shared.";
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
    if (adventureActive_ || creating_ || switching_) return;
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
