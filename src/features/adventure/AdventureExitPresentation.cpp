#include "AdventureExitPresentation.h"

namespace trainer {
using Phase = AdventureExitController::Phase;
AdventureExitPresentation::AdventureExitPresentation(AdventureExitController& exit, QObject* parent)
    : QObject(parent), exit_(exit), phase_(exit.phase()) {
    closeTimer_.setSingleShot(true);
    closeTimer_.setInterval(8000);
    connect(&closeTimer_, &QTimer::timeout, this, [this] {
        if (phase_ == Phase::Closing) { slowClose_ = true; emit changed(); }
    });
    connect(&exit_, &AdventureExitController::changed, this, [this] {
        if (phase_ != exit_.phase()) {
            phase_ = exit_.phase();
            resetInput();
            slowClose_ = false;
            closeTimer_.stop();
            // Every attempt needs a new proven lease, including retries.
            if (phase_ == Phase::Idle || phase_ == Phase::Capturing) isolated_ = false;
            if (phase_ == Phase::Closing) closeTimer_.start();
        }
        emit changed();
    });
}
bool AdventureExitPresentation::visible() const { return phase_ == Phase::Confirming || phase_ == Phase::Closing; }
bool AdventureExitPresentation::confirming() const { return phase_ == Phase::Confirming; }
void AdventureExitPresentation::resetInput() {
    ++generation_;
    ready_ = previousConfirm_ = previousBack_ = false;
}
void AdventureExitPresentation::setInputIsolated(bool value) {
    if (isolated_ == value) return;
    isolated_ = value; resetInput(); emit changed();
}
void AdventureExitPresentation::setWindowFocused(bool value) {
    if (focused_ == value) return;
    focused_ = value; resetInput(); emit changed();
}
void AdventureExitPresentation::updateInput(quint64 generation, const ExitInputSnapshot& input) {
    if (generation != generation_ || !confirming() || !isolated_ || !focused_) return;
    if (!input.connected) { resetInput(); emit changed(); return; }
    if (!ready_) {
        if (input.allReleased && !input.confirm && !input.back) { ready_ = true; emit changed(); }
        return;
    }
    const bool back = input.back && !previousBack_;
    const bool confirm = input.confirm && !previousConfirm_;
    previousConfirm_ = input.confirm; previousBack_ = input.back;
    // Simultaneous A+B is always the non-destructive choice.
    if (back) cancel();
    else if (confirm && !input.back) this->confirm();
}
void AdventureExitPresentation::confirm() { if (ready_ && confirming()) exit_.confirm(); }
void AdventureExitPresentation::cancel() { if (ready_ && confirming()) exit_.cancel(); }
}
