#include "ControllerInput.h"
#include "RemoteGamePad.h"
#include <QKeyEvent>
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <QSettings>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace trainer {
QString ControllerInput::family() const { return profile_.family=="auto"?detectedFamily_:profile_.family; }
int ControllerInput::confirmButton() const {
    const bool south=profile_.confirm=="south" || (profile_.confirm=="auto" && QStringList{"xbox","playstation","steam"}.contains(family()));
    return south?SDL_CONTROLLER_BUTTON_A:SDL_CONTROLLER_BUTTON_B;
}
QString ControllerInput::glyph(const QString& hint) const {
    QString position=hint;
    if(hint=="A")position=confirmButton()==SDL_CONTROLLER_BUTTON_A?"south":"east";
    if(hint=="B")position=backButton()==SDL_CONTROLLER_BUTTON_A?"south":"east";
    if(hint=="X")position="north";
    if(hint=="Y")position="west";
    const QStringList positions{"south","east","west","north"};
    const int index=positions.indexOf(position);
    if(index>=0) {
        const QStringList labels=family()=="nintendo"?QStringList{"B","A","Y","X"}
            :family()=="playstation"?QStringList{"×","○","□","△"}
            :family()=="generic"?QStringList{"↓","→","←","↑"}:QStringList{"A","B","X","Y"};
        return labels[index];
    }
    if(family()=="xbox" || family()=="steam") {
        if(hint=="L1")return "LB";
        if(hint=="R1")return "RB";
        if(hint=="L2")return "LT";
        if(hint=="R2")return "RT";
        if(hint=="Start")return "Menu";
        if(hint=="Select")return "View";
    }
    if(family()=="playstation") {if(hint=="Start")return "Options";
        if(hint=="Select")return "Share";}
    if(family()=="nintendo") {if(hint=="L2")return "ZL";
        if(hint=="R2")return "ZR";}
    return hint;
}
void ControllerInput::loadProfile() {
    physicalName_.clear();profileKey_=sample_.guid;
#ifdef Q_OS_LINUX
    // InputPlumber presents a virtual pad. Resolve its physical source before
    // choosing printed labels; an Xbox-shaped virtual target is not an Xbox pad.
    const char* raw=SDL_GameControllerPath(controller_);
    const auto node=QFileInfo(QString::fromUtf8(raw?raw:"")).fileName();
    if(QFileInfo("/sys/class/input/"+node).canonicalFilePath().contains("/devices/virtual/")) {
        QProcess query;
        query.start("busctl",{"--system","get-property","org.shadowblip.InputPlumber","/org/shadowblip/InputPlumber/CompositeDevice0","org.shadowblip.Input.CompositeDevice","SourceDevicePaths"});
        if(query.waitForFinished(250)&&query.exitCode()==0) {
            const auto output=QString::fromUtf8(query.readAllStandardOutput());
            auto matches=QRegularExpression("/dev/input/event[0-9]+").globalMatch(output);
            while(matches.hasNext()) {
                const auto base="/sys/class/input/"+QFileInfo(matches.next().captured()).fileName()+"/device/";
                QFile nameFile(base+"name");if(!nameFile.open(QIODevice::ReadOnly))continue;
                const auto candidate=QString::fromUtf8(nameFile.readAll()).trimmed();
                if(candidate=="Retroid Pocket Gamepad"||candidate=="AYN Odin2 Gamepad") {
                    physicalName_=candidate;profileKey_="physical-"+QString::fromLatin1(QCryptographicHash::hash(candidate.toUtf8(),QCryptographicHash::Sha256).toHex());break;
                }
            }
        } else {query.kill();query.waitForFinished(100);}
    }
#endif
    const auto name=(physicalName_+" "+kernelDeviceName()+" "+sample_.name).toLower();
    const auto type=controller_?SDL_GameControllerGetType(controller_):SDL_CONTROLLER_TYPE_UNKNOWN;
    detectedFamily_=name.contains("retroid")||name.contains("odin")||name.contains("nintendo")||name.contains("switch")?"nintendo"
        :name.contains("steam")?"steam"
        :type==SDL_CONTROLLER_TYPE_PS3||type==SDL_CONTROLLER_TYPE_PS4||type==SDL_CONTROLLER_TYPE_PS5?"playstation"
        :type==SDL_CONTROLLER_TYPE_XBOX360||type==SDL_CONTROLLER_TYPE_XBOXONE?"xbox":"generic";
    profile_=ControllerProfile::read(QSettings().value("controllers/"+profileKey_+"/profile").toMap());
    profileError_.clear();requireNeutral();publishShellMapping();emit profileChanged();
}
void ControllerInput::publishShellMapping() {
#ifdef Q_OS_LINUX
    const auto directory=QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if(directory.isEmpty())return;
    QSaveFile file(directory+"/traineros-shell-input.json");
    if(!file.open(QIODevice::WriteOnly))return;
    const auto data=QJsonDocument(QJsonObject{{"version",1},{"device",physicalName_.isEmpty()?kernelDeviceName():physicalName_},
        {"southConfirms",confirmButton()==SDL_CONTROLLER_BUTTON_A},{"invertX",profile_.invertX},{"invertY",profile_.invertY}}).toJson(QJsonDocument::Compact);
    if(file.write(data)==data.size())file.commit();
#endif
}
bool ControllerInput::setProfile(const QVariantMap& value) {
    if(!connected() || sample_.guid.isEmpty()) {profileError_="Connect a controller before saving its settings.";emit profileChanged();return false;}
    const auto candidate=ControllerProfile::read(value);
    QSettings settings;settings.setValue("controllers/"+profileKey_+"/profile",candidate.json());settings.sync();
    if(settings.status()!=QSettings::NoError){profileError_="Could not save controller settings.";emit profileChanged();return false;}
    profile_=candidate;profileError_.clear();requireNeutral();publishShellMapping();emit profileChanged();return true;
}
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
QString ControllerInput::kernelDeviceName() const {
    if(!controller_||!SDL_GameControllerGetAttached(controller_))return {};
    const auto id=SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_));
    for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_JoystickGetDeviceInstanceID(i)==id&&!isRemoteGamePad(i))
        return QString::fromStdString(kernelGamePadName(i));
    return {};
}
void ControllerInput::setHoldConfirmEnabled(bool enabled) {
    if(holdConfirmEnabled_==enabled)return;
    holdConfirmEnabled_=enabled;confirmPending_=false;
}
void ControllerInput::setEnabled(bool enabled) {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    confirmPending_=false;
    awaitingNeutral_ = true;
    heldDirection_.reset();
    previous_.fill(false);
    triggers_.fill(false);
    sample_.enabled = enabled_; sample_.awaitingNeutral = true;
    emit sampled();
}
void ControllerInput::deliver(Action semantic, bool fromController) {
    // A layer change can occur in the middle of this same sampled packet.
    if (fromController && awaitingNeutral_) return;
    if(semantic!=Action::Confirm)confirmPending_=false;
    if (semantic == Action::Confirm) emit confirmPressed();
    emit observedAction(semantic, fromController);
    emit action(semantic);
}
void ControllerInput::requireNeutral() {
    awaitingNeutral_ = true; confirmPending_ = false;
    heldDirection_.reset(); previous_.fill(false); triggers_.fill(false);
    sample_.awaitingNeutral = true;
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
        confirmPending_=false;
        awaitingNeutral_ = true;
        heldDirection_.reset();
        previous_.fill(false);
        triggers_.fill(false);
        sample_.name.clear(); sample_.guid.clear(); sample_.mapping.clear();
        profile_=ControllerProfile{};physicalName_.clear();profileKey_.clear();detectedFamily_="generic";publishShellMapping();emit profileChanged();
        emit connectionChanged();
    }
    sample_.unmappedDevices.clear();
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
        if (!isRemoteGamePad(i) && !SDL_IsGameController(i)) sample_.unmappedDevices.append(QString::fromUtf8(SDL_JoystickNameForIndex(i)));
    if (!controller_) {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            // Remote ports belong to the emulator, never to shell navigation.
            if (isRemoteGamePad(i)) continue;
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
                loadProfile();
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
    const auto normalized=[this](int axis,bool invert) {
        const float v=sample_.axes[axis]/32768.0f, dz=profile_.deadzone/100.0f;
        if(std::abs(v)<=dz)return 0.0f;
        // Keep the established 20% profile's navigation thresholds unchanged,
        // while making a larger dead zone require a larger physical deflection.
        const float filtered=std::copysign(.2f+(std::abs(v)-dz)*.8f/(1.0f-dz),v);
        return invert?-filtered:filtered;
    };
    const float x = normalized(SDL_CONTROLLER_AXIS_LEFTX,profile_.invertX);
    const float y = normalized(SDL_CONTROLLER_AXIS_LEFTY,profile_.invertY);
    const std::array<float, 2> triggers{
        sample_.axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT] / 32768.0f,
        sample_.axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] / 32768.0f};
    if (awaitingNeutral_) {
        const bool anyButton = std::any_of(buttons.begin(), buttons.end(), [](bool b) { return b; });
        if (anyButton || std::abs(x) >= StickRelease || std::abs(y) >= StickRelease
                || std::abs(sample_.axes[SDL_CONTROLLER_AXIS_RIGHTX] / 32768.0f) >= StickRelease
                || std::abs(sample_.axes[SDL_CONTROLLER_AXIS_RIGHTY] / 32768.0f) >= StickRelease
                || triggers[0] >= StickRelease || triggers[1] >= StickRelease) { emit sampled(); return; }
        awaitingNeutral_ = false;
    }
    sample_.awaitingNeutral = awaitingNeutral_; emit sampled();
    const std::pair<int, Action> bindings[] = {
        {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Action::PreviousPage},
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Action::NextPage},
        {SDL_CONTROLLER_BUTTON_START, Action::SystemMenu},
        {SDL_CONTROLLER_BUTTON_GUIDE, Action::Home},
        {SDL_CONTROLLER_BUTTON_BACK, Action::LocalAction},
        {SDL_CONTROLLER_BUTTON_RIGHTSTICK, Action::ChangeView},
        {SDL_CONTROLLER_BUTTON_LEFTSTICK, Action::ToggleFavorite},
        {backButton(), Action::Back},
        {SDL_CONTROLLER_BUTTON_X, Action::ToggleContinue},
        {SDL_CONTROLLER_BUTTON_Y, Action::Secondary},
        {confirmButton(), Action::Confirm}
    };
    const bool wasHoldEnabled=holdConfirmEnabled_;
    bool competing=false;
    for (const auto& [button, semantic] : bindings)
        if (buttons[button] && !previous_[button]) {
            if(semantic==Action::Confirm && wasHoldEnabled) {
                if(!competing && holdConfirmEnabled_){confirmPending_=true;confirmStarted_=clock_.elapsed();}
            } else {competing=true;deliver(semantic,true);}
        }
    // Triggers are analog axes. Hysteresis and edges avoid repeated page flips
    // while held, and the shared neutral gate protects return from an Adventure.
    for (int i = 0; i < 2; ++i) {
        const bool pressed = triggers[i] >= (triggers_[i] ? StickRelease : StickEngage);
        if (pressed && !triggers_[i]) deliver(i == 0 ? Action::PreviousFace : Action::NextFace, true);
        triggers_[i] = pressed;
    }
    if(pointerMode_ && (std::abs(x)>.2f || std::abs(y)>.2f))emit pointerMoved(std::abs(x)>.2f?x*7:0,std::abs(y)>.2f?y*7:0);
    const auto current = direction(pointerMode_?0:x, pointerMode_?0:y, buttons);
    const auto now = clock_.elapsed();
    const auto scroll=normalized(SDL_CONTROLLER_AXIS_RIGHTY,false);
    if(browserMode_) {
        const auto horizontal=normalized(SDL_CONTROLLER_AXIS_RIGHTX,false);
        if(std::abs(horizontal)>.2f||std::abs(scroll)>.2f)emit pageScrolled(std::abs(horizontal)>.2f?-horizontal*8:0,std::abs(scroll)>.2f?-scroll*8:0);
    }
    if(std::abs(scroll)<StickRelease)nextScroll_=0;
    else if(!browserMode_ && std::abs(scroll)>=StickEngage && now>=nextScroll_) {
        deliver(scroll>0?Action::ScrollDown:Action::ScrollUp,true);
        nextScroll_=now+100; // Bounded cadence, independent of D-pad focus/repeat.
    }
    if (current != heldDirection_) {
        heldDirection_ = current;
        if (current) deliver(*current, true);
        nextRepeat_ = now + RepeatDelayMs;
    } else if (current && now >= nextRepeat_) {
        deliver(*current, true);
        nextRepeat_ = now + RepeatIntervalMs;
    }
    if(confirmPending_ && holdConfirmEnabled_) {
        if(!buttons[confirmButton()]) {confirmPending_=false;deliver(Action::Confirm,true);}
        else if(now-confirmStarted_>=600) {confirmPending_=false;deliver(Action::ContextMenu,true);}
    }
    previous_ = buttons;
}
bool ControllerInput::eventFilter(QObject*, QEvent* event) {
    if (event->type() != QEvent::KeyPress) return false;
    const auto* key = static_cast<QKeyEvent*>(event);
    if(nativeKeyboard_&&!textEntryEnabled_&&key->key()!=Qt::Key_F1&&key->key()!=Qt::Key_Escape)return false;
    if(textEntryEnabled_&&enabled_) {
        if(key->key()==Qt::Key_Backspace){emit textBackspace();return true;}
        if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter){if(!key->isAutoRepeat())emit textSubmitted();return true;}
        if(!(key->modifiers()&(Qt::ControlModifier|Qt::AltModifier|Qt::MetaModifier))&&!key->text().isEmpty()&&key->text().front().isPrint()) {
            emit textEntered(key->text());return true;
        }
    }
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
    case Qt::Key_Menu: semantic = Action::ContextMenu; break;
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
