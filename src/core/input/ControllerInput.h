#pragma once
#include "Action.h"
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <SDL.h>
#include <array>
#include <optional>
#include <QStringList>

namespace trainer {
// SDL-mapped readings, before TrainerOS dead zones and foreground gating.
// These are observations, not a claim about the handheld's printed labels.
struct ControllerSample {
    bool initialized = false, connected = false, enabled = true, awaitingNeutral = true;
    QString name, guid, mapping;
    QStringList unmappedDevices;
    std::array<bool, SDL_CONTROLLER_BUTTON_MAX> buttons{};
    std::array<int, SDL_CONTROLLER_AXIS_MAX> axes{};
};
class ControllerInput final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
public:
    static constexpr float StickEngage = 0.55f, StickRelease = 0.35f;
    static constexpr int RepeatDelayMs = 350, RepeatIntervalMs = 100;
    explicit ControllerInput(QObject* parent = nullptr, SDL_JoystickID preferredDevice = -1);
    ~ControllerInput() override;
    bool connected() const { return controller_ != nullptr; }
    bool initialized() const { return initialized_; }
    const ControllerSample& sample() const { return sample_; }
    void setEnabled(bool enabled);
    void poll();
    bool eventFilter(QObject*, QEvent*) override;
signals:
    void action(trainer::Action action);
    void connectionChanged();
    void sampled();
    void observedAction(trainer::Action action, bool fromController);
private:
    void deliver(Action action, bool fromController);
    std::optional<Action> direction(float x, float y, const std::array<bool, SDL_CONTROLLER_BUTTON_MAX>& buttons);
    bool initialized_ = false;
    bool enabled_ = true;
    bool awaitingNeutral_ = true;
    SDL_GameController* controller_ = nullptr;
    SDL_JoystickID preferredDevice_ = -1;
    QTimer timer_;
    QElapsedTimer clock_;
    std::array<bool, SDL_CONTROLLER_BUTTON_MAX> previous_{};
    std::optional<Action> heldDirection_;
    qint64 nextRepeat_ = 0;
    ControllerSample sample_;
};
}
