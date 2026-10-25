#pragma once
#include "Action.h"
#include "ControllerProfile.h"
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
    Q_PROPERTY(bool pointerMode READ pointerMode WRITE setPointerMode)
    Q_PROPERTY(bool nativeKeyboard MEMBER nativeKeyboard_)
    Q_PROPERTY(bool browserMode MEMBER browserMode_)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(QString family READ family NOTIFY profileChanged)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY profileChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY profileChanged)
    Q_PROPERTY(QString profileError READ profileError NOTIFY profileChanged)
    Q_PROPERTY(QVariantMap live READ live NOTIFY sampled)
    Q_PROPERTY(QString captureMode READ captureMode NOTIFY captureChanged)
    Q_PROPERTY(QString captureStatus READ captureStatus NOTIFY captureChanged)
public:
    // Switch-style shell: printed A at the right confirms; printed B below
    // goes back. SDL names these positions B/east and A/south respectively.
    static constexpr auto ConfirmButton = SDL_CONTROLLER_BUTTON_B;
    static constexpr auto BackButton = SDL_CONTROLLER_BUTTON_A;
    static constexpr float StickEngage = 0.55f, StickRelease = 0.35f;
    static constexpr int RepeatDelayMs = 350, RepeatIntervalMs = 100;
    explicit ControllerInput(QObject* parent = nullptr, SDL_JoystickID preferredDevice = -1);
    ~ControllerInput() override;
    bool connected() const { return controller_ != nullptr; }
    bool initialized() const { return initialized_; }
    QString family() const;
    QString deviceName() const { return physicalName_.isEmpty()?sample_.name:physicalName_; }
    QString profileError() const { return profileError_; }
    QVariantMap profile() const { return profile_.json(); }
    Q_INVOKABLE bool setProfile(const QVariantMap&);
    Q_INVOKABLE void resetProfile() { setProfile(ControllerProfile{}.json()); }
    Q_INVOKABLE QString glyph(const QString& semanticHint) const;
    QVariantMap live() const;
    QString captureMode() const {return captureMode_;}
    QString captureStatus() const {return captureStatus_;}
    Q_INVOKABLE void beginCapture(const QString& mode);
    Q_INVOKABLE void cancelCapture();
    Q_INVOKABLE void advanceCalibration();
    int confirmButton() const;
    int backButton() const { return confirmButton()==SDL_CONTROLLER_BUTTON_A?SDL_CONTROLLER_BUTTON_B:SDL_CONTROLLER_BUTTON_A; }
    const ControllerSample& sample() const { return sample_; }
    QString kernelDeviceName() const;
    void setEnabled(bool enabled);
    void setHoldConfirmEnabled(bool enabled);
    bool pointerMode() const{return pointerMode_;}
    void setPointerMode(bool enabled){pointerMode_=enabled;}
    void setTextEntryEnabled(bool enabled) { textEntryEnabled_=enabled; }
    void requireNeutral();
    void poll();
    bool eventFilter(QObject*, QEvent*) override;
signals:
    // Presentation-only cue, emitted before Confirm changes the focused page.
    void confirmPressed();
    void action(trainer::Action action);
    void textEntered(QString text);
    void textBackspace();
    void textSubmitted();
    void connectionChanged();
    void profileChanged();
    void sampled();
    void captureChanged();
    void buttonCaptured(QString button);
    void keyCaptured(QString chord);
    void pointerMoved(qreal dx,qreal dy);
    void pageScrolled(qreal dx,qreal dy);
    void observedAction(trainer::Action action, bool fromController);
private:
    void deliver(Action action, bool fromController);
    std::optional<Action> direction(float x, float y, const std::array<bool, SDL_CONTROLLER_BUTTON_MAX>& buttons);
    bool pointerMode_=false;
    bool nativeKeyboard_=false;
    bool browserMode_=false;
    bool holdConfirmEnabled_ = false, confirmPending_ = false;
    qint64 confirmStarted_ = 0;
    bool initialized_ = false;
    bool enabled_ = true;
    bool textEntryEnabled_ = false;
    bool awaitingNeutral_ = true;
    SDL_GameController* controller_ = nullptr;
    SDL_JoystickID preferredDevice_ = -1;
    QTimer timer_;
    QElapsedTimer clock_;
    std::array<bool, SDL_CONTROLLER_BUTTON_MAX> previous_{};
    std::array<bool, 2> triggers_{};
    std::optional<Action> heldDirection_;
    qint64 nextRepeat_ = 0;
    qint64 nextScroll_ = 0;
    ControllerSample sample_;
    ControllerProfile profile_;
    QString detectedFamily_="generic", profileError_, physicalName_, profileKey_;
    void loadProfile();
    void publishShellMapping();
    QString captureMode_,captureStatus_;
    bool captureArmed_=false;
    int calibrationSamples_=0;
    std::array<double,6> calibrationCenter_{},calibrationMin_{},calibrationMax_{};
    std::array<bool,SDL_CONTROLLER_BUTTON_MAX> capturePrevious_{};
    bool pollCapture();
};
}
