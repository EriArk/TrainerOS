#include "SessionState.h"
#include <algorithm>

namespace trainer {
SessionState::SessionState(ShellController& shell, LocalStateStore* store, QObject* parent)
    : QObject(parent), shell_(shell), store_(store) {
    connect(&shell_, &ShellController::exitRequested, this, &SessionState::requestExit);
    if (!store_) return;
    debounce_.setSingleShot(true); debounce_.setInterval(300);
    connect(&debounce_, &QTimer::timeout, this, &SessionState::flush);
    connect(&shell_, &ShellController::changed, this, &SessionState::stateChanged);
    connect(store_, &LocalStateStore::opened, this, [this](bool success) {
        if (success) {
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
        closing_ = false; emit changed();
    });
}
void SessionState::start() { if (store_) store_->open(); }
QString SessionState::title() const {
    if (!error_.isEmpty()) return restored_ ? "Browsing state wasn't saved" : "Your data needs attention";
    return closing_ ? "Finishing your session" : "Opening your Trainer journal";
}
QString SessionState::message() const {
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
    if (adventureActive_) return;
    if (!store_) { emit exitReady(); return; }
    closing_ = true; paused_ = false; focus_ = 0;
    // Preserve a visible failed flush until the user chooses Retry or an explicit skip.
    if (error_.isEmpty()) flush();
    emit changed();
    finishExit();
}
void SessionState::finishExit() {
    if (!closing_ || !error_.isEmpty() || store_->opening() || store_->pending() || writing_) return;
    if (restored_ && desired_ != committed_) { flush(); return; }
    emit exitReady();
}
void SessionState::activate(int index) {
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
        error_.clear(); paused_ = true; closing_ = false;
    }
    emit changed();
}
void SessionState::dispatch(Action action) {
    if (adventureActive_) return;
    if (!blocked()) { shell_.dispatch(action); return; }
    if (restored_ && (action == Action::PreviousPage || action == Action::NextPage)) {
        closing_ = false;
        if (!error_.isEmpty()) { error_.clear(); paused_ = true; }
        shell_.dispatch(action); emit changed(); return;
    }
    if (action == Action::Confirm) activate(focus_);
    else if (action == Action::Back) {
        if (!choices().isEmpty()) activate(1);
        else if (closing_) { closing_ = false; emit changed(); }
    } else if (action == Action::Up || action == Action::Left) focus_ = std::max(0, focus_ - 1);
    else if (action == Action::Down || action == Action::Right) focus_ = std::min(std::max(0, int(choices().size()) - 1), focus_ + 1);
    emit changed();
}
}
