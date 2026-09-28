#include "EmulatorDiscovery.h"
#include "integrations/adventure/retroarch/RetroArchConfiguration.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <SDL.h>

namespace trainer {
namespace {
struct Route { QString id, app, binary; QStringList names, platforms; };
const QList<Route> routes{
    {"ppsspp", "org.ppsspp.PPSSPP", "PPSSPPSDL", {"PPSSPPSDL", "ppsspp"}, {"psp"}},
    {"armsx2", {}, "ARMSX2", {"ARMSX2", "armsx2"}, {"ps2"}},
    {"melonds", "net.kuribo64.melonDS", "melonDS", {"melonDS"}, {"nds"}},
    {"dolphin", "org.DolphinEmu.dolphin-emu", "dolphin-emu", {"dolphin-emu"}, {"gc", "wii"}},
    {"retroarch", "org.libretro.RetroArch", "retroarch", {"retroarch"}, {}}
};
bool executable(const QString& path) {
    const QFileInfo f(path); return f.isAbsolute() && f.isFile() && f.isExecutable();
}
bool exists(const QString& path) { return QFileInfo::exists(path) || QFileInfo(path).isSymLink(); }
QJsonObject readProfile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size() > 65536) return {};
    const auto document = QJsonDocument::fromJson(f.readAll());
    return document.isObject() ? document.object() : QJsonObject{};
}
// Fresh defaults only. Existing preferences, including malformed files and
// symlinks, belong to their owner and are never overwritten by discovery.
bool seed(const QString& path, const QByteArray& bytes) {
    if (exists(path)) return QFileInfo(path).isFile();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return false;
    if (file.write(bytes) == bytes.size() && file.flush()) return true;
    file.close(); file.remove(); return false;
}
QString runtime(const Route& route, const EmulatorEnvironment& env) {
    const auto location = env.flatpaks.value(route.app);
    return location.isEmpty() ? QString() : QDir(location).filePath("files/bin/" + route.binary);
}
QString configDirectory(const Route& route, bool flatpak, const EmulatorEnvironment& env) {
    const auto base = flatpak ? QDir(env.home).filePath(".var/app/" + route.app + "/config") : env.configHome;
    return QDir(base).filePath(route.id == "dolphin" ? "dolphin-emu" : route.id == "melonds" ? "melonDS" : route.id);
}
QByteArray dsDefaults(const EmulatorEnvironment& env) {
    QByteArray bytes = "# TrainerOS initial controller defaults; future user edits are preserved.\n"
        "[Emu]\nDirectBoot = true\nExternalBIOSEnable = false\n"
        "[Instance0]\nJoystickID = 0\n[Instance0.Joystick]\n";
    const QStringList keys{"A", "B", "X", "Y", "L", "R", "Start", "Select", "Up", "Down", "Left", "Right"};
    for (const auto& key : keys) bytes += key.toUtf8() + " = " + QByteArray::number(env.controllerButtons.value(key, -1)) + "\n";
    bytes += "[Instance0.Window0]\nScreenLayout = 2\n";
    return bytes;
}
QByteArray dolphinDefaults(const EmulatorEnvironment& env) {
    return "# TrainerOS initial SDL controller; never replaced on update.\n[GCPad1]\nDevice = SDL/0/"
        + env.controllerName.toUtf8() + "\n"
        "Buttons/A = `Button E`\nButtons/B = `Button S`\nButtons/X = `Button N`\nButtons/Y = `Button W`\n"
        "Buttons/Z = `Shoulder R`\nButtons/Start = `Start`\n"
        "Main Stick/Up = `Left Y+`\nMain Stick/Down = `Left Y-`\nMain Stick/Left = `Left X-`\nMain Stick/Right = `Left X+`\n"
        "C-Stick/Up = `Right Y+`\nC-Stick/Down = `Right Y-`\nC-Stick/Left = `Right X-`\nC-Stick/Right = `Right X+`\n"
        "Triggers/L = `Trigger L` > 0.8\nTriggers/R = `Trigger R` > 0.8\n"
        "Triggers/L-Analog = `Trigger L`\nTriggers/R-Analog = `Trigger R`\n"
        "D-Pad/Up = `Pad N`\nD-Pad/Down = `Pad S`\nD-Pad/Left = `Pad W`\nD-Pad/Right = `Pad E`\n";
}
void controllerDefaults(const Route& route, const QString& configDir, const EmulatorEnvironment& env, QStringList& notices) {
    if (route.id == "melonds") {
        const auto config = QDir(configDir).filePath("melonDS.toml");
        if (env.controllerButtons.size() == 12 && !seed(config, dsDefaults(env))) notices << "melonds: initial controls unavailable";
        if (!exists(config)) notices << "melonds: controller defaults need a connected SDL controller";
    } else if (route.id == "dolphin") {
        const auto config = QDir(configDir).filePath("GCPadNew.ini");
        if (!env.controllerName.isEmpty() && !env.controllerName.contains('\n') && !seed(config, dolphinDefaults(env))) notices << "dolphin: initial controls unavailable";
        if (!exists(config)) notices << "dolphin: GameCube controls need a connected SDL controller";
    }
}
}
EmulatorEnvironment installedEmulators(const QString& stateDirectory, const QString& libraryRoot, bool readController) {
    EmulatorEnvironment env;
    env.home = QDir::homePath(); env.configHome = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    env.stateDirectory = stateDirectory; env.libraryRoot = libraryRoot;
#ifdef Q_OS_LINUX
    env.nativeCoreDirectories = {"/usr/lib/libretro", "/usr/lib64/libretro", "/usr/lib/aarch64-linux-gnu/libretro"};
    for (const auto& route : routes) for (const auto& name : route.names) {
        const auto path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty()) env.executables.insert(name, path);
    }
    env.executables.insert("python3", QStandardPaths::findExecutable("python3"));
    const auto flatpak = QStandardPaths::findExecutable("flatpak");
    if (!flatpak.isEmpty()) {
        env.executables.insert("flatpak", flatpak);
        QElapsedTimer deadline; deadline.start();
        for (const auto& route : routes) {
            if (route.app.isEmpty() || deadline.elapsed() >= 2000) continue;
            QProcess process; process.start(flatpak, {"info", "--show-location", route.app});
            if (!process.waitForFinished(qMax(1, 2000 - int(deadline.elapsed())))) { process.kill(); process.waitForFinished(100); continue; }
            if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) continue;
            auto location = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
            if (!QDir::isAbsolutePath(location) || location.contains('\n')) continue;
            // Keep Flatpak's stable active link where available. No hash-specific
            // runtime identity survives an upstream package update.
            const auto active = QFileInfo(location).dir().filePath("active");
            if (QFileInfo(active).canonicalFilePath() == QFileInfo(location).canonicalFilePath()) location = active;
            env.flatpaks.insert(route.app, location);
        }
    }
    for (const auto& route : routes) {
        const auto appImage = QDir(env.home).filePath("Applications/" + route.binary + ".AppImage");
        if (!env.executables.contains(route.names.first()) && executable(appImage)) env.executables.insert(route.names.first(), appImage);
    }
    const bool sdlReady = readController && SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0;
    if (sdlReady && SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
        if (auto* pad = SDL_GameControllerOpen(0)) {
            env.controllerName = QString::fromUtf8(SDL_GameControllerName(pad));
            const QList<QPair<QString, SDL_GameControllerButton>> buttons{
                {"A",SDL_CONTROLLER_BUTTON_B},{"B",SDL_CONTROLLER_BUTTON_A},{"X",SDL_CONTROLLER_BUTTON_Y},{"Y",SDL_CONTROLLER_BUTTON_X},
                {"L",SDL_CONTROLLER_BUTTON_LEFTSHOULDER},{"R",SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
                {"Start",SDL_CONTROLLER_BUTTON_START},{"Select",SDL_CONTROLLER_BUTTON_BACK},
                {"Up",SDL_CONTROLLER_BUTTON_DPAD_UP},{"Down",SDL_CONTROLLER_BUTTON_DPAD_DOWN},
                {"Left",SDL_CONTROLLER_BUTTON_DPAD_LEFT},{"Right",SDL_CONTROLLER_BUTTON_DPAD_RIGHT}};
            for (const auto& [name, key] : buttons) {
                const auto binding = SDL_GameControllerGetBindForButton(pad, key);
                if (binding.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON) env.controllerButtons.insert(name, binding.value.button);
                else if (binding.bindType == SDL_CONTROLLER_BINDTYPE_HAT)
                    env.controllerButtons.insert(name, 0x100 | (binding.value.hat.hat << 4) | binding.value.hat.hat_mask);
            }
            SDL_GameControllerClose(pad);
        }
    }
    if (sdlReady) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
#endif
    return env;
}
EmulatorDiscovery prepareEmulators(const EmulatorEnvironment& env) {
    EmulatorDiscovery result;
    if (env.stateDirectory.isEmpty()) return result;
    for (const auto& route : routes) {
        const auto explicitPath = QDir(env.stateDirectory).filePath("integrations/" + route.id + ".json");
        const auto discoveredPath = QDir(env.stateDirectory).filePath("integrations/discovered/" + route.id + ".json");
        const bool supplied = exists(explicitPath), previous = exists(discoveredPath);
        auto profile = readProfile(supplied ? explicitPath : discoveredPath);
        if (supplied || previous) {
            if (profile["version"].toInt() != 1) { result.notices << route.id + ": existing integration is unreadable; kept unchanged"; continue; }
            // Repair only the declared Flatpak runtime, never its launcher,
            // wrapper, arguments, config/save locations or selected emulator.
            const auto arguments = profile["prefixArguments"].toArray().toVariantList();
            const auto oldRuntime = profile["runtimeFile"].toString();
            const auto currentRuntime = runtime(route, env);
            const auto flatpakIndex = arguments.indexOf(route.app);
            if (!route.app.isEmpty() && flatpakIndex > 0 && arguments[flatpakIndex - 1] == "run"
                && oldRuntime.contains("/app/" + route.app + "/") && oldRuntime.endsWith("/files/bin/" + route.binary)
                && executable(currentRuntime)) profile["runtimeFile"] = currentRuntime;
            if (!supplied && QDir::isAbsolutePath(profile["initialConfigDirectory"].toString()))
                controllerDefaults(route, profile["initialConfigDirectory"].toString(), env, result.notices);
            result.profiles.insert(route.id, profile);
            continue;
        }
        QString program;
        for (const auto& name : route.names) if (executable(env.executables.value(name))) { program = env.executables.value(name); break; }
        QJsonArray arguments;
        QString runtimeFile = program;
        const bool flatpak = program.isEmpty() && executable(env.executables.value("flatpak")) && executable(runtime(route, env));
        if (flatpak) { program = env.executables.value("flatpak"); runtimeFile = runtime(route, env); arguments = {"run", route.app}; }
        if (program.isEmpty()) continue;
        if ((route.id == "melonds" || route.id == "dolphin") && executable(env.executables.value("python3"))) {
            const QStringList helpers{QDir(env.home).filePath(".local/libexec/traineros/controller-bridge.py"),
                "/var/opt/traineros/integrations/controller-bridge.py"};
            for (const auto& helper : helpers) if (QFileInfo(helper).isFile()) {
                QJsonArray wrapped{helper};
                if (route.id == "melonds") wrapped.append("--pointer");
                wrapped.append("--"); wrapped.append(program);
                for (const auto& value : arguments) wrapped.append(value);
                arguments = wrapped; program = env.executables.value("python3"); break;
            }
        }
        profile = {{"version",1},{"adapter",route.id},{"program",program},{"runtimeFile",runtimeFile},
            {"prefixArguments",arguments},{"validatedPlatforms",QJsonArray::fromStringList(route.platforms)}};
        const auto configDir = configDirectory(route, flatpak, env);
        profile["initialConfigDirectory"] = configDir;
        if (route.id == "retroarch") {
            const auto config = QDir(configDir).filePath("retroarch.cfg");
            const auto saves = QDir(configDir).filePath("saves");
            const auto bios = QFileInfo(env.libraryRoot).dir().filePath("bios");
            if (!retroarch::safePath(saves) || !retroarch::safePath(bios)) continue;
            const auto bytes = QByteArray("# TrainerOS initial defaults; preserve user edits.\n")
                + "input_driver = \"sdl2\"\ninput_joypad_driver = \"sdl2\"\ninput_autodetect_enable = \"true\"\n"
                + "video_fullscreen = \"true\"\nconfig_save_on_exit = \"false\"\n"
                + "savestate_auto_load = \"false\"\nsavestate_auto_save = \"false\"\n"
                + "savefile_directory = \"" + saves.toUtf8() + "\"\nsystem_directory = \"" + bios.toUtf8() + "\"\n"
                + "savefiles_in_content_dir = \"false\"\nsort_savefiles_enable = \"false\"\nsort_savefiles_by_content_enable = \"false\"\n";
            const bool fresh = !exists(config);
            if (fresh && !QDir().mkpath(saves)) { result.notices << "retroarch: could not prepare save directory"; continue; }
            if (!seed(config, bytes)) { result.notices << "retroarch: could not prepare initial settings"; continue; }
            const auto settings = retroarch::readSettings(config);
            auto cores = retroarch::configuredPath(settings,"libretro_directory",QDir(configDir).filePath("cores"));
            if (!flatpak && QDir(cores).entryList({"*_libretro.so"},QDir::Files).isEmpty()) {
                for (const auto& candidate : env.nativeCoreDirectories)
                    if (!QDir(candidate).entryList({"*_libretro.so"},QDir::Files).isEmpty()) { cores = candidate; break; }
            }
            profile["configFile"] = config; profile["coresDirectory"] = cores;
            profile["backupProtocol"] = "mgba-sram-v1"; // Existing runtime checks still gate save ownership.
        } else controllerDefaults(route, configDir, env, result.notices);
        // Persist the selected runtime family once. Installing a different
        // emulator later must not silently move existing games to different saves.
        if (!seed(discoveredPath,QJsonDocument(profile).toJson())) { result.notices << route.id + ": could not retain discovered route"; continue; }
        result.profiles.insert(route.id, profile);
    }
    return result;
}
}
