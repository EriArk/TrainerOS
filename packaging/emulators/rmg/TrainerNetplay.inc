// TrainerOS bridge for RMG 4777ed3f; GPL-3.0-only, as the surrounding RMG code.
// No networking protocol here: launch the already admitted upstream session.
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QHostAddress>
#include <cstdio>

bool UserInterface::MainWindow::OpenTrainerNetplay(QString file, QString sessionPath)
{
    QFile sessionFile(sessionPath);
    if (!sessionFile.open(QIODevice::ReadOnly) || sessionFile.size() > 4096)
        return false;
    const auto session = QJsonDocument::fromJson(sessionFile.readAll()).object();
    const auto address = session["address"].toString();
    const int port = session["port"].toInt();
    const int controller = session["controller"].toInt();
    const auto renderer = session["renderer"].toString();
    if (session["protocol"] != 1 || QHostAddress(address).isNull() || port < 1 || port > 65535
        || controller < 1 || controller > 4 || (renderer != "vulkan" && renderer != "opengl"))
        return false;

    // One local controller feeds the native network player selected by RMG.
    // Reuse upstream's SDL gamepad mapping, not a TrainerOS button protocol.
    QFile profiles(QString::fromStdString(CoreGetSharedDataDirectory().string()) + "/InputProfileDB.json");
    if (!profiles.open(QIODevice::ReadOnly) || profiles.size() > 4 * 1024 * 1024)
        return false;
    QJsonObject mapping;
    for (const auto& value : QJsonDocument::fromJson(profiles.readAll()).array()) {
        const auto candidate = value.toObject();
        if (candidate["DeviceName"].toArray().contains("fallback_profile")) {
            mapping = candidate;
            break;
        }
    }
    if (mapping.isEmpty()) return false;
    const std::string section = "Rosalie's Mupen GUI - Input Plugin Profile 0";
    const auto copy = [&](SettingsID id, const char* key) {
        return mapping[key].isString() && CoreSettingsSetValue(id, section, mapping[key].toString().toStdString());
    };
#define TRAINER_INPUT(setting, key) \
    if (!copy(SettingsID::Input_##setting##_InputType, #key "_InputType") \
        || !copy(SettingsID::Input_##setting##_Name, #key "_Name") \
        || !copy(SettingsID::Input_##setting##_Data, #key "_Data") \
        || !copy(SettingsID::Input_##setting##_ExtraData, #key "_ExtraData")) return false;
    TRAINER_INPUT(A, A)
    TRAINER_INPUT(B, B)
    TRAINER_INPUT(Start, Start)
    TRAINER_INPUT(DpadUp, DpadUp)
    TRAINER_INPUT(DpadDown, DpadDown)
    TRAINER_INPUT(DpadLeft, DpadLeft)
    TRAINER_INPUT(DpadRight, DpadRight)
    TRAINER_INPUT(CButtonUp, CButtonUp)
    TRAINER_INPUT(CButtonDown, CButtonDown)
    TRAINER_INPUT(CButtonLeft, CButtonLeft)
    TRAINER_INPUT(CButtonRight, CButtonRight)
    TRAINER_INPUT(LeftShoulder, LeftTrigger)
    TRAINER_INPUT(RightShoulder, RightTrigger)
    TRAINER_INPUT(ZTrigger, ZTrigger)
    TRAINER_INPUT(AnalogStickUp, AnalogStickUp)
    TRAINER_INPUT(AnalogStickDown, AnalogStickDown)
    TRAINER_INPUT(AnalogStickLeft, AnalogStickLeft)
    TRAINER_INPUT(AnalogStickRight, AnalogStickRight)
#undef TRAINER_INPUT
    if (!CoreSettingsSetValue(SettingsID::Input_PluggedIn, section, true)
        || !CoreSettingsSetValue(SettingsID::Input_DeviceType, section, 2) // upstream Automatic
        || !CoreSettingsSetValue(SettingsID::Input_Deadzone, section, 15)
        || !CoreSettingsSetValue(SettingsID::Input_Sensitivity, section, 100)
        || !CoreSettingsSetValue(SettingsID::Input_Pak, section, 0) // private memory pak
        || !CoreSettingsSetValue(SettingsID::GUI_ConfirmExitWhileInGame, false)
        || !CoreSettingsSetValue(SettingsID::Core_GFX_Plugin, std::string(renderer == "vulkan"
            ? "mupen64plus-video-parallel.so" : "mupen64plus-video-GLideN64.so"))
        || !CoreSettingsSetValue(SettingsID::Core_RSP_Plugin, std::string(renderer == "vulkan"
            ? "mupen64plus-rsp-parallel.so" : "mupen64plus-rsp-hle.so"))
        || !CoreApplyPluginSettings()) return false;
    this->ui_LaunchInFullscreen = true;
    this->ui_QuitAfterEmulation = true;
    this->updateUI(true, false);
    this->launchEmulationThread(file, address, port, controller);
    return true;
}
