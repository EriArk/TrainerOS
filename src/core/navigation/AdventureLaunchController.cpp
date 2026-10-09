#include "AdventureLaunchController.h"

namespace trainer {
AdventureLaunchController::AdventureLaunchController(ProcessService& process, QObject* parent) : QObject(parent), process_(process) {
    connect(&process_, &ProcessService::started, this, [this] {
        started_ = true;
        // Preparation establishes this only after the adapter has isolated progress.
        // Re-evaluate for every process; an ordinary launch must retain its policy.
        exit_.beginSession(process_.runtimeControls()["temporaryProgress"].toBool()
            ? AdventureSavePolicy::NoPersistentProgress : savePolicy_);
        if (!adventureId_.isEmpty()) emit adventureStarted(adventureId_);
        if (state_ == "stopping") { process_.stop(); return; }
        state_ = "running"; emit changed(); emit suspendRequested();
    });
    connect(&process_, &ProcessService::finished, this, [this](int code, bool crashed, const QString& error) {
        if (!active()) return;
        const bool failed = (started_ && process_.stopRequested()) || crashed || code != 0 || !error.isEmpty();
        // Completion consumers (including multiplayer restart) need the exact
        // finalization error before deciding whether another game may start.
        const auto resultError = !error.isEmpty() ? error : failed ? QString("The Adventure ended with an error. You can try again.") : QString();
        error_ = resultError;
        // A stop/kill is never evidence of a confirmed, graceful game exit.
        exit_.endSession(!failed && state_ != "stopping");
        if (started_ && !adventureId_.isEmpty()) emit adventureFinished(failed);
        started_ = false;
        restore(resultError);
    });
}
bool AdventureLaunchController::launch(const ProcessCommand& command, const QJsonObject& context, const QString& adventureId,
                                       AdventureSavePolicy savePolicy) {
    if (active() || process_.active()) return false;
    adventureId_ = adventureId; started_ = false; minimized_ = false;
    savePolicy_ = savePolicy;
    command_ = command; context_ = context; error_.clear(); state_ = "preparing";
    const auto token = ++request_; emit changed(); emit checkpointRequested(token, context_); return true;
}
void AdventureLaunchController::checkpointCompleted(quint64 request, const QString& error) {
    if (request != request_ || state_ != "preparing") return;
    if (!error.isEmpty()) { restore(error); return; }
    state_ = "starting"; emit changed();
    if (!process_.start(command_)) restore("This Adventure's launch setup isn't available.");
}
void AdventureLaunchController::cancel() {
    if (!active()) return;
    if (state_ == "preparing") { ++request_; restore({}); }
    else { state_ = "stopping"; emit changed(); process_.stop(); }
}
void AdventureLaunchController::restore(const QString& error) {
    minimized_ = false;
    error_ = error; state_ = error.isEmpty() ? "returned" : "failed";
    emit changed(); emit restoreRequested(context_);
}
bool AdventureLaunchController::setMinimized(bool value) {
    if(state_!="running" || !process_.active() || !process_.processId())return false;
    if(minimized_==value)return true;
    minimized_=value;emit changed();return true;
}
}
