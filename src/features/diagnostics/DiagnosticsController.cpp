#include "DiagnosticsController.h"
#include <algorithm>

namespace trainer {
namespace {
QString actionName(Action action) {
    const QStringList names{"Up", "Down", "Left", "Right", "Confirm (A)", "Back (B)", "System (Start)", "Previous section (L1)", "Next section (R1)", "Continue / filter (Y)", "Secondary (X)"};
    return names.value(int(action), "Unknown action");
}
}
void DiagnosticsController::configure(ControllerInput* input, DiagnosticsService* service) {
    if (input_) disconnect(input_, nullptr, this, nullptr);
    if (service_) disconnect(service_, nullptr, this, nullptr);
    input_ = input; service_ = service;
    if (input_) {
        connect(input_, &ControllerInput::sampled, this, &DiagnosticsController::sample);
        connect(input_, &ControllerInput::observedAction, this, [this](Action action, bool controller) {
            if (!recording_ || !controller) return; // Keyboard never satisfies a controller check.
            lastAction_ = actionName(action);
            actions_.append(lastAction_);
            while (actions_.size() > 24) actions_.removeFirst();
            emit changed();
        });
    }
    if (service_) connect(service_, &DiagnosticsService::runtimeChanged, this, [this] {
        if (recording_) { runtime_ = service_->runtime(); emit changed(); }
    });
    if (service_) connect(service_, &DiagnosticsService::completed, this, [this](const QString& error, const QString&) {
        status_ = error.isEmpty() ? "Report saved locally in the diagnostics folder." : error;
        emit changed(); emit messageRequested(status_);
    });
}
void DiagnosticsController::begin() {
    focus_ = 0;
    if (!recording_) { recording_ = true; reset(); }
    if (service_) runtime_ = service_->runtime();
    sample(); emit changed();
}
void DiagnosticsController::sample() {
    if (!recording_ || !input_) return;
    auto next = input_->sample();
    if (!next.connected) { next.name = sample_.name; next.guid = sample_.guid; next.mapping = sample_.mapping; }
    if (next.connected != sample_.connected) next.connected ? ++connections_ : ++disconnections_;
    // A different mapping starts a new set of button/range observations.
    if (next.connected && (next.guid != sample_.guid || next.mapping != sample_.mapping)) {
        seen_.fill(false); minimum_.fill(0); maximum_.fill(0); axesSeen_.fill(false); actions_ = {};
        lastAction_ = "No controller action recorded";
    }
    const bool changedReadings = next.connected != sample_.connected || next.enabled != sample_.enabled
        || next.awaitingNeutral != sample_.awaitingNeutral || next.name != sample_.name
        || next.buttons != sample_.buttons || next.axes != sample_.axes || next.unmappedDevices != sample_.unmappedDevices;
    sample_ = next;
    if (sample_.connected) {
        for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) seen_[i] = seen_[i] || sample_.buttons[i];
        for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) {
            minimum_[i] = axesSeen_[i] ? std::min(minimum_[i], sample_.axes[i]) : sample_.axes[i];
            maximum_[i] = axesSeen_[i] ? std::max(maximum_[i], sample_.axes[i]) : sample_.axes[i]; axesSeen_[i] = true;
        }
    }
    if (changedReadings) emit changed();
}
void DiagnosticsController::reset() {
    seen_.fill(false); minimum_.fill(0); maximum_.fill(0); axesSeen_.fill(false); actions_ = {};
    connections_ = disconnections_ = 0; lastAction_ = "No controller action recorded";
    status_ = "Checks reset. Button marks mean a signal was observed, not that its physical label is correct.";
    emit changed();
}
QVariantList DiagnosticsController::buttons() const {
    const std::pair<SDL_GameControllerButton, QString> shown[] = {
        {SDL_CONTROLLER_BUTTON_DPAD_UP,"Up"}, {SDL_CONTROLLER_BUTTON_DPAD_DOWN,"Down"}, {SDL_CONTROLLER_BUTTON_DPAD_LEFT,"Left"},
        {SDL_CONTROLLER_BUTTON_DPAD_RIGHT,"Right"}, {ControllerInput::ConfirmButton,"A"}, {ControllerInput::BackButton,"B"},
        {SDL_CONTROLLER_BUTTON_X,"X"}, {SDL_CONTROLLER_BUTTON_Y,"Y"}, {SDL_CONTROLLER_BUTTON_LEFTSHOULDER,"L1"},
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,"R1"}, {SDL_CONTROLLER_BUTTON_START,"Start"}, {SDL_CONTROLLER_BUTTON_BACK,"Select"},
        {SDL_CONTROLLER_BUTTON_LEFTSTICK,"L3"}, {SDL_CONTROLLER_BUTTON_RIGHTSTICK,"R3"}, {SDL_CONTROLLER_BUTTON_GUIDE,"Guide"}};
    QVariantList result;
    for (const auto& [id, name] : shown) result.append(QVariantMap{{"label", name}, {"held", sample_.buttons[id]}, {"seen", seen_[id]}});
    return result;
}
QVariantList DiagnosticsController::axes() const {
    QVariantList result;
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) result.append(QVariantMap{{"value", sample_.axes[i] / 32768.0},
        {"minimum", minimum_[i] / 32768.0}, {"maximum", maximum_[i] / 32768.0}});
    return result;
}
QString DiagnosticsController::connection() const {
    if (!input_ || !sample_.initialized) return "Controller input is unavailable";
    if (sample_.connected) return sample_.name.isEmpty() ? "Controller connected" : sample_.name;
    if (!sample_.unmappedDevices.isEmpty()) return "Device detected without a controller mapping";
    return "Connect a controller to begin";
}
QString DiagnosticsController::gate() const {
    if (!sample_.connected) return "No mapped controller";
    if (!sample_.enabled) return "Shell input paused while inactive";
    return sample_.awaitingNeutral ? "Release buttons and center the left stick" : "Shell input ready";
}
QStringList DiagnosticsController::runtimeLines() const {
    if (runtime_.isEmpty()) return {"Display information is unavailable"};
    const auto window = runtime_["window"].toObject(), screen = runtime_["qtReportedScreen"].toObject();
    QStringList lines{runtime_["os"].toString() + " · " + runtime_["cpuArchitecture"].toString(),
        "Qt " + runtime_["qtVersion"].toString() + " · SDL " + runtime_["sdlVersion"].toString(),
        "Display backend: " + runtime_["qpaPlatform"].toString()};
    if (!window.isEmpty()) lines << QString("Window: %1 × %2 · DPR %3").arg(window["width"].toInt()).arg(window["height"].toInt()).arg(window["devicePixelRatio"].toDouble(), 0, 'f', 2);
    if (!screen.isEmpty()) lines << QString("Qt screen: %1 × %2 · %3 Hz").arg(screen["width"].toInt()).arg(screen["height"].toInt()).arg(screen["refreshRate"].toDouble(), 0, 'f', 1);
    return lines;
}
QJsonObject DiagnosticsController::observations() const {
    QJsonArray buttons, axes;
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) buttons.append(QJsonObject{{"sdlButton", i},
        {"name", QString::fromLatin1(SDL_GameControllerGetStringForButton(SDL_GameControllerButton(i)))},
        {"held", sample_.buttons[i]}, {"observed", seen_[i]}});
    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i) axes.append(QJsonObject{{"sdlAxis", i},
        {"name", QString::fromLatin1(SDL_GameControllerGetStringForAxis(SDL_GameControllerAxis(i)))},
        {"rawValue", sample_.connected ? QJsonValue(sample_.axes[i]) : QJsonValue()},
        {"minimum", axesSeen_[i] ? QJsonValue(minimum_[i]) : QJsonValue()},
        {"maximum", axesSeen_[i] ? QJsonValue(maximum_[i]) : QJsonValue()}});
    return {{"connected", sample_.connected}, {"name", sample_.name}, {"guid", sample_.guid}, {"sdlMapping", sample_.mapping},
        {"unmappedDevices", QJsonArray::fromStringList(sample_.unmappedDevices)}, {"gate", gate()}, {"buttons", buttons}, {"axes", axes},
        {"recentControllerActions", actions_}, {"connectionsObserved", connections_}, {"disconnectionsObserved", disconnections_},
        {"normalization", QJsonObject{{"stickEngage", ControllerInput::StickEngage}, {"stickRelease", ControllerInput::StickRelease},
            {"repeatDelayMs", ControllerInput::RepeatDelayMs}, {"repeatIntervalMs", ControllerInput::RepeatIntervalMs}}},
        {"readings", "SDL-mapped state before TrainerOS filtering; action history excludes keyboard input"}};
}
void DiagnosticsController::activate(int index) {
    if (index < 0 || index > 3) return;
    focus_ = index;
    if (index == 3) { emit closeRequested(); return; }
    if (index == 0 && service_) { runtime_ = service_->runtime(); status_ = "Display readings refreshed. L1/R1 and Start keep their normal actions."; }
    if (index == 1 && service_ && !saving()) { service_->save(observations()); status_ = "Saving report…"; }
    if (index == 2) reset();
    if (!service_ && index < 2) status_ = "The diagnostic report service isn't available in this fixture.";
    emit changed();
}
void DiagnosticsController::dispatch(Action action) {
    if (action == Action::Back) { emit closeRequested(); return; }
    if (action == Action::Confirm) { activate(focus_); return; }
    if (action == Action::Left) focus_ = std::max(0, focus_ - 1);
    if (action == Action::Right) focus_ = std::min(3, focus_ + 1);
    emit changed();
}
}
