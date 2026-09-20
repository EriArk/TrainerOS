#include "ControllerInput.h"
#include <QKeyEvent>
#include <QDebug>
#include <cmath>
#include <algorithm>

namespace trainer {
ControllerInput::ControllerInput(QObject* parent, SDL_JoystickID preferredDevice)
    : QObject(parent), preferredDevice_(preferredDevice) {
    SDL_SetMainReady(); // Qt owns the application entry point, including on Windows.
    // SDL's default handlers convert SIGTERM/SIGINT into SDL_QUIT, but Qt owns
    // our event loop. Preserve OS termination instead of swallowing service stop.
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    initialized_ = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0;
    if (!initialized_) qWarning() << "Controller input unavailable:" << SDL_GetError();
    clock_.start();
    connect(&timer_, &QTimer::timeout, this, &ControllerInput::poll);
    timer_.start(16);
}
ControllerInput::~ControllerInput() {
    if (controller_) SDL_GameControllerClose(controller_);
    if (initialized_) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}
void ControllerInput::setEnabled(bool enabled) {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    awaitingNeutral_ = true;
    heldDirection_.reset();
    previous_.fill(false);
    triggers_.fill(false);
    sample_.enabled = enabled_; sample_.awaitingNeutral = true;
    emit sampled();
}
void ControllerInput::deliver(Action semantic, bool fromController) {
    if (semantic == Action::Confirm) emit confirmPressed();
    emit observedAction(semantic, fromController);
    emit action(semantic);
}
std::optional<Action> ControllerInput::direction(float x, float y,
        const std::array<bool, SDL_CONTROLLER_BUTTON_MAX>& b) {
    if (b[SDL_CONTROLLER_BUTTON_DPAD_UP]) return Action::Up;
    if (b[SDL_CONTROLLER_BUTTON_DPAD_DOWN]) return Action::Down;
    if (b[SDL_CONTROLLER_BUTTON_DPAD_LEFT]) return Action::Left;
    if (b[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]) return Action::Right;
    const float threshold = heldDirection_ ? StickRelease : StickEngage;
    if (std::max(std::abs(x), std::abs(y)) < threshold) return {};
    if (std::abs(x) > std::abs(y)) return x < 0 ? Action::Left : Action::Right;
    return y < 0 ? Action::Up : Action::Down;
}
void ControllerInput::poll() {
    sample_.initialized = initialized_;
    if (!initialized_) return;
    SDL_GameControllerUpdate();
    if (controller_ && !SDL_GameControllerGetAttached(controller_)) {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
        awaitingNeutral_ = true;
        heldDirection_.reset();
        previous_.fill(false);
        triggers_.fill(false);
        sample_.name.clear(); sample_.guid.clear(); sample_.mapping.clear();
        emit connectionChanged();
    }
    sample_.unmappedDevices.clear();
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
        if (!SDL_IsGameController(i)) sample_.unmappedDevices.append(QString::fromUtf8(SDL_JoystickNameForIndex(i)));
    if (!controller_) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (preferredDevice_ != -1 && SDL_JoystickGetDeviceInstanceID(i) != preferredDevice_) continue;
            if (!SDL_IsGameController(i)) continue;
            controller_ = SDL_GameControllerOpen(i);
            if (controller_) {
                sample_.name = QString::fromUtf8(SDL_GameControllerName(controller_));
                char guid[33]{};
                SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(controller_)), guid, sizeof(guid));
                sample_.guid = QString::fromLatin1(guid);
                char* mapping = SDL_GameControllerMapping(controller_);
                sample_.mapping = QString::fromUtf8(mapping); SDL_free(mapping);
                emit connectionChanged(); break;
            }
        }
    }
    // We poll state; drain only controller/joystick events owned by this source.
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_JOYAXISMOTION, SDL_CONTROLLERDEVICEREMAPPED);
    sample_.connected = connected(); sample_.enabled = enabled_; sample_.awaitingNeutral = awaitingNeutral_;
    sample_.buttons.fill(false); sample_.axes.fill(0);
    if (controller_) {
        for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
            sample_.buttons[i] = SDL_GameControllerGetButton(controller_, static_cast<SDL_GameControllerButton>(i));
        for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i)
            sample_.axes[i] = SDL_GameControllerGetAxis(controller_, static_cast<SDL_GameControllerAxis>(i));
    }
    if (!controller_ || !enabled_) { emit sampled(); return; }
    std::array<bool, SDL_CONTROLLER_BUTTON_MAX> buttons{};
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
        buttons[i] = SDL_GameControllerGetButton(controller_, static_cast<SDL_GameControllerButton>(i));
    const float x = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTX) / 32768.0f;
    const float y = SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTY) / 32768.0f;
    const std::array<float, 2> triggers{
        sample_.axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] / 32768.0f,
        sample_.axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] / 32768.0f};
    if (awaitingNeutral_) {
        const bool anyButton = std::any_of(buttons.begin(), buttons.end(), [](bool b) { return b; });
        if (anyButton || std::abs(x) >= StickRelease || std::abs(y) >= StickRelease
                || triggers[0] >= StickRelease || triggers[1] >= StickRelease) { emit sampled(); return; }
        awaitingNeutral_ = false;
    }
    sample_.awaitingNeutral = awaitingNeutral_; emit sampled();
    const std::pair<SDL_GameControllerButton, Action> bindings[] = {
        {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Action::PreviousPage},
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Action::NextPage},
        {SDL_CONTROLLER_BUTTON_START, Action::SystemMenu},
        {SDL_CONTROLLER_BUTTON_GUIDE, Action::Home},
        {SDL_CONTROLLER_BUTTON_BACK, Action::LocalAction},
        {BackButton, Action::Back},
        {SDL_CONTROLLER_BUTTON_X, Action::ToggleContinue},
        {SDL_CONTROLLER_BUTTON_Y, Action::Secondary},
        {ConfirmButton, Action::Confirm}
    };
    for (const auto& [button, semantic] : bindings)
        if (buttons[button] && !previous_[button]) deliver(semantic, true);
    // Triggers are analog axes. Hysteresis and edges avoid repeated page flips
    // while held, and the shared neutral gate protects return from an Adventure.
    for (int i = 0; i < 2; ++i) {
        const bool pressed = triggers[i] >= (triggers_[i] ? StickRelease : StickEngage);
        if (pressed && !triggers_[i]) deliver(i == 0 ? Action::PreviousFace : Action::NextFace, true);
        triggers_[i] = pressed;
    }
    const auto current = direction(x, y, buttons);
    const auto now = clock_.elapsed();
    if (current != heldDirection_) {
        heldDirection_ = current;
        if (current) deliver(*current, true);
        nextRepeat_ = now + RepeatDelayMs;
    } else if (current && now >= nextRepeat_) {
        deliver(*current, true);
        nextRepeat_ = now + RepeatIntervalMs;
    }
    previous_ = buttons;
}
bool ControllerInput::eventFilter(QObject*, QEvent* event) {
    if (event->type() != QEvent::KeyPress) return false;
    const auto* key = static_cast<QKeyEvent*>(event);
    std::optional<Action> semantic;
    switch (key->key()) {
    case Qt::Key_Up: semantic = Action::Up; break;
    case Qt::Key_Down: semantic = Action::Down; break;
    case Qt::Key_Left: semantic = Action::Left; break;
    case Qt::Key_Right: semantic = Action::Right; break;
    case Qt::Key_Return: case Qt::Key_Enter: semantic = Action::Confirm; break;
    case Qt::Key_Escape: case Qt::Key_Backspace: semantic = Action::Back; break;
    case Qt::Key_F1: semantic = Action::SystemMenu; break;
    case Qt::Key_Home: semantic = Action::Home; break;
    case Qt::Key_Q: semantic = Action::PreviousPage; break;
    case Qt::Key_E: semantic = Action::NextPage; break;
    case Qt::Key_Z: semantic = Action::PreviousFace; break;
    case Qt::Key_C: semantic = Action::NextFace; break;
    case Qt::Key_Tab: semantic = Action::LocalAction; break;
    case Qt::Key_Y: semantic = Action::ToggleContinue; break;
    case Qt::Key_X: semantic = Action::Secondary; break;
    default: return false;
    }
    const bool directional = *semantic <= Action::Right;
    if (enabled_ && (!key->isAutoRepeat() || directional)) deliver(*semantic, false);
    return true;
}
}
