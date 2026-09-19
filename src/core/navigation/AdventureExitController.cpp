#include "AdventureExitController.h"

namespace trainer {
AdventureExitController::AdventureExitController(QObject* parent) : QObject(parent) {
    captureTimer_.setSingleShot(true);
    captureTimer_.setInterval(CaptureTimeoutMs);
    connect(&captureTimer_, &QTimer::timeout, this, [this] {
        captureCompleted(attempt_, {}, "Couldn't capture this moment. Your previous image will be kept.");
    });
}
void AdventureExitController::setAvailable(bool available) {
    if (available_ == available) return;
    available_ = available;
    if (!available_) cancel();
    emit changed();
}
void AdventureExitController::beginSession(AdventureSavePolicy policy) {
    clearAttempt();
    available_ = false; // Capability must be established for this running integration.
    policy_ = policy;
    sessionActive_ = true;
    emit changed();
}
bool AdventureExitController::requestExit() {
    if (!available_ || !sessionActive_ || phase_ != Phase::Idle) return false;
    const auto attempt = ++attempt_;
    phase_ = Phase::Capturing;
    captureTimer_.start();
    emit changed();
    // A changed observer may cancel or observe a process exit synchronously.
    if (phase_ == Phase::Capturing && attempt == attempt_) emit captureRequested(attempt);
    return true;
}
void AdventureExitController::captureCompleted(quint64 attempt, const QImage& frame, const QString& error) {
    if (!sessionActive_ || phase_ != Phase::Capturing || attempt != attempt_) return;
    captureTimer_.stop();
    // Bound retained memory, including pathological captures supplied by a
    // broken provider. The original frame is never substituted with a state.
    const bool valid = error.isEmpty() && !frame.isNull() && frame.sizeInBytes() <= 64 * 1024 * 1024;
    pendingFrame_ = valid ? frame.copy() : QImage();
    captureError_ = !pendingFrame_.isNull() ? QString()
        : !error.isEmpty() ? error : "Couldn't capture this moment. Your previous image will be kept.";
    if (policy_ == AdventureSavePolicy::VerifiedAutosave) { close(); return; }
    phase_ = Phase::Confirming;
    emit changed();
    if (phase_ == Phase::Confirming && attempt == attempt_) emit confirmationRequested();
}
bool AdventureExitController::confirm() {
    if (!sessionActive_ || phase_ != Phase::Confirming) return false;
    userConfirmed_ = true;
    close();
    return true;
}
void AdventureExitController::close() {
    const auto attempt = attempt_;
    phase_ = Phase::Closing;
    emit changed();
    if (phase_ == Phase::Closing && attempt == attempt_) {
        closeRequested_ = true;
        emit gracefulExitRequested(attempt);
    }
}
bool AdventureExitController::cancel() {
    if (phase_ != Phase::Capturing && phase_ != Phase::Confirming) return false;
    clearAttempt();
    emit changed();
    if (sessionActive_ && phase_ == Phase::Idle) emit returnToGameRequested();
    return true;
}
void AdventureExitController::gracefulExitFailed(quint64 attempt, const QString& error) {
    if (!sessionActive_ || phase_ != Phase::Closing || attempt != attempt_) return;
    clearAttempt(); // A retry must capture a fresh frame and ask again.
    emit changed();
    emit failed(error.isEmpty() ? "Couldn't exit this Adventure. You're still in the game." : error);
    if (sessionActive_ && phase_ == Phase::Idle) emit returnToGameRequested();
}
void AdventureExitController::endSession(bool cleanExit) {
    if (!sessionActive_) return;
    const bool publish = cleanExit && phase_ == Phase::Closing && closeRequested_;
    const auto frame = pendingFrame_;
    const auto attempt = attempt_;
    const bool confirmed = userConfirmed_;
    sessionActive_ = false;
    available_ = false;
    clearAttempt();
    emit changed();
    if (publish) emit completed(attempt, frame, confirmed);
}
void AdventureExitController::clearAttempt() {
    captureTimer_.stop();
    pendingFrame_ = {};
    captureError_.clear();
    userConfirmed_ = false;
    closeRequested_ = false;
    phase_ = Phase::Idle;
}
}
