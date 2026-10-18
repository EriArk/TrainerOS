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
    {"cemu", "info.cemu.Cemu", "Cemu", {"Cemu", "cemu"}, {"wiiu"}},
    {"duckstation", "org.duckstation.DuckStation", "duckstation-qt", {"duckstation-qt", "DuckStation", "duckstation"}, {"psx"}},
    {"rpcs3", {}, "rpcs3", {"rpcs3", "RPCS3"}, {"ps3"}},
    {"eden", {}, "eden", {"eden"}, {"switch"}},
    {"melonds", "net.kuribo64.melonDS", "melonDS", {"melonDS"}, {"nds"}},
    {"azahar", "org.azahar_emu.Azahar", "azahar", {"azahar"}, {"n3ds"}},
    {"vita3k", {}, "vita3k", {"vita3k"}, {"psvita"}},
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
    return QDir(base).filePath(route.id == "dolphin" ? "dolphin-emu" : route.id == "azahar" ? "azahar-emu" : route.id == "melonds" ? "melonDS" : route.id == "vita3k" ? "Vita3K" : route.id);
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
    if (route.id == "rpcs3") {
        // TrainerOS already confirms Exit; leave existing desktop preferences alone.
        if (!seed(QDir(configDir).filePath("GuiConfigs/CurrentSettings.ini"),
                  "[main_window]\ninfoBoxEnabledWelcome=false\nconfirmationBoxExitGame=false\n"))
            notices << "rpcs3: initial window settings unavailable";
        if (!seed(QDir(configDir).filePath("config.yml"),
                  "Video:\n  Renderer: Vulkan\n  Shader Mode: Async Recompiler (multi-threaded)\n"))
            notices << "rpcs3: initial renderer settings unavailable";
    } else if (route.id == "eden") {
        if (env.switchBindings.isEmpty()) return;
        QByteArray bytes = "[Controls]\nplayer_0_connected=true\nplayer_0_connected\\default=false\n";
        for (auto it=env.switchBindings.cbegin(); it!=env.switchBindings.cend(); ++it)
            bytes += "player_0_" + it.key().toUtf8() + "=\"" + it.value().toUtf8()
                + "\"\nplayer_0_" + it.key().toUtf8() + "\\default=false\n";
        if (!seed(QDir(configDir).filePath("qt-config.ini"),bytes)) notices << "eden: initial controls unavailable";
    } else if (route.id == "azahar") {
        const auto config = QDir(configDir).filePath("qt-config.ini");
        QByteArray bytes="[UI]\nfirstStart=false\nfirstStart\\default=false\nconfirmClose=false\nconfirmClose\\default=false\n"
            "[Renderer]\ngraphics_api=2\ngraphics_api\\default=false\nresolution_factor=1\nresolution_factor\\default=false\n"
            "[Layout]\nlayout_option=3\nlayout_option\\default=false\n"
            "[Controls]\nprofiles\\size=1\nprofiles\\1\\name=TrainerOS\nprofiles\\1\\input_maptype=2\n";
        const QMap<QByteArray,int> buttons{{"a",SDL_CONTROLLER_BUTTON_B},{"b",SDL_CONTROLLER_BUTTON_A},
            {"x",SDL_CONTROLLER_BUTTON_Y},{"y",SDL_CONTROLLER_BUTTON_X},{"l",SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
            {"r",SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},{"start",SDL_CONTROLLER_BUTTON_START},{"select",SDL_CONTROLLER_BUTTON_BACK},
            {"up",SDL_CONTROLLER_BUTTON_DPAD_UP},{"down",SDL_CONTROLLER_BUTTON_DPAD_DOWN},
            {"left",SDL_CONTROLLER_BUTTON_DPAD_LEFT},{"right",SDL_CONTROLLER_BUTTON_DPAD_RIGHT}};
        for(auto it=buttons.cbegin();it!=buttons.cend();++it)bytes+="profiles\\1\\button_"+it.key()+"=\"api:controller,button:"
            +QByteArray::number(it.value())+",engine:sdl,maptype:all,port:0\"\nprofiles\\1\\button_"+it.key()+"\\default=false\n";
        bytes+="profiles\\1\\circle_pad=\"api:controller,axis_x:0,axis_y:1,engine:sdl,maptype:all,port:0\"\nprofiles\\1\\circle_pad\\default=false\n"
            "profiles\\1\\c_stick=\"api:controller,axis_x:2,axis_y:3,engine:sdl,maptype:all,port:0\"\nprofiles\\1\\c_stick\\default=false\n"
            "profiles\\1\\button_zl=\"api:controller,axis:4,direction:+,engine:sdl,maptype:all,port:0,threshold:0.5\"\nprofiles\\1\\button_zl\\default=false\n"
            "profiles\\1\\button_zr=\"api:controller,axis:5,direction:+,engine:sdl,maptype:all,port:0,threshold:0.5\"\nprofiles\\1\\button_zr\\default=false\n";
        if(!seed(config,bytes))notices<<"azahar: initial controls unavailable";
    } else if (route.id == "melonds") {
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
        if(route.id=="duckstation" && !env.executables.contains(route.names.first())) {
            const auto image=QDir(env.home).filePath("Applications/DuckStation.AppImage");
            if(executable(image))env.executables.insert(route.names.first(),image);
        }
    }
    const bool sdlReady = readController && SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0;
    if (sdlReady && SDL_NumJoysticks() > 0 && SDL_IsGameController(0)) {
        if (auto* pad = SDL_GameControllerOpen(0)) {
            env.controllerName = QString::fromUtf8(SDL_GameControllerName(pad));
            char guid[33]{};
            auto switchGuid=SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(pad));
            // Eden ignores SDL's controller-name CRC when identifying a device.
            switchGuid.data[2]=switchGuid.data[3]=0;
            SDL_JoystickGetGUIDString(switchGuid,guid,sizeof(guid));
            const QString prefix = "engine:sdl,port:0,guid:" + QString::fromLatin1(guid);
            const auto bind = [&](const QString& key, SDL_GameControllerButtonBind binding) {
                QString value;
                if (binding.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON)
                    value = ",button:" + QString::number(binding.value.button);
                else if (binding.bindType == SDL_CONTROLLER_BINDTYPE_AXIS)
                    value = ",axis:" + QString::number(binding.value.axis) + ",threshold:0.5,invert:+";
                else if (binding.bindType == SDL_CONTROLLER_BINDTYPE_HAT) {
                    const QHash<int,QString> directions{{SDL_HAT_UP,"up"},{SDL_HAT_DOWN,"down"},{SDL_HAT_LEFT,"left"},{SDL_HAT_RIGHT,"right"}};
                    const auto direction = directions.value(binding.value.hat.hat_mask);
                    if (!direction.isEmpty()) value = ",hat:" + QString::number(binding.value.hat.hat) + ",direction:" + direction;
                }
                if (!value.isEmpty()) env.switchBindings.insert("button_"+key,prefix+value);
            };
            const QList<QPair<QString,SDL_GameControllerButton>> switchButtons{
                {"a",SDL_CONTROLLER_BUTTON_B},{"b",SDL_CONTROLLER_BUTTON_A},
                {"x",SDL_CONTROLLER_BUTTON_Y},{"y",SDL_CONTROLLER_BUTTON_X},
                {"l",SDL_CONTROLLER_BUTTON_LEFTSHOULDER},{"r",SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
                {"plus",SDL_CONTROLLER_BUTTON_START},{"minus",SDL_CONTROLLER_BUTTON_BACK},
                {"lstick",SDL_CONTROLLER_BUTTON_LEFTSTICK},{"rstick",SDL_CONTROLLER_BUTTON_RIGHTSTICK},
                {"dup",SDL_CONTROLLER_BUTTON_DPAD_UP},{"ddown",SDL_CONTROLLER_BUTTON_DPAD_DOWN},
                {"dleft",SDL_CONTROLLER_BUTTON_DPAD_LEFT},{"dright",SDL_CONTROLLER_BUTTON_DPAD_RIGHT}};
            for (const auto& [key,button] : switchButtons) bind(key,SDL_GameControllerGetBindForButton(pad,button));
            bind("zl",SDL_GameControllerGetBindForAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERLEFT));
            bind("zr",SDL_GameControllerGetBindForAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
            for (int stick=0; stick<2; ++stick) {
                const auto x=SDL_GameControllerGetBindForAxis(pad,stick==0?SDL_CONTROLLER_AXIS_LEFTX:SDL_CONTROLLER_AXIS_RIGHTX);
                const auto y=SDL_GameControllerGetBindForAxis(pad,stick==0?SDL_CONTROLLER_AXIS_LEFTY:SDL_CONTROLLER_AXIS_RIGHTY);
                if (x.bindType==SDL_CONTROLLER_BINDTYPE_AXIS && y.bindType==SDL_CONTROLLER_BINDTYPE_AXIS)
                    env.switchBindings.insert(stick==0?"lstick":"rstick",prefix+",axis_x:"+QString::number(x.value.axis)
                        +",axis_y:"+QString::number(y.value.axis)+",deadzone:0.15,range:1.0");
            }
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
        // The graphical Armada session has a deliberately short PATH. A
        // selected per-user launcher must still be discoverable there, while
        // explicit profiles and binaries already found on PATH keep priority.
        if (program.isEmpty()) for (const auto& name : route.names) {
            const auto local = QStandardPaths::findExecutable(name,
                {QDir(env.home).filePath(".local/bin")});
            if (executable(local)) { program = local; break; }
        }
        QJsonArray arguments;
        QString runtimeFile = program;
        const bool flatpak = program.isEmpty() && executable(env.executables.value("flatpak")) && executable(runtime(route, env));
        if (flatpak) { program = env.executables.value("flatpak"); runtimeFile = runtime(route, env); arguments = {"run", route.app}; }
        if (program.isEmpty()) continue;
        if(route.id=="vita3k") {
            // A .psvita link is an installed-title reference, not a native CLI
            // image argument. Only discover the maintained launcher here.
            const auto launcher=QFileInfo(program).canonicalFilePath();
            const auto manifest=readProfile(QFileInfo(launcher).dir().filePath("../manifest.json"));
            if(manifest["protocol"]!=1||manifest["source"]!="46602447c7f84d53790b44e1869ab5105178bf16"||manifest["settings"]!="native-lan-two-owned-v1")continue;
        }
        if ((route.id == "melonds" || route.id == "dolphin" || route.id == "azahar" || route.id == "vita3k" || route.id == "cemu" || route.id == "duckstation" || route.id == "rpcs3" || route.id == "eden") && executable(env.executables.value("python3"))) {
            const QStringList helpers{QDir(env.home).filePath(".local/libexec/traineros/controller-bridge.py"),
                "/var/opt/traineros/integrations/controller-bridge.py"};
            for (const auto& helper : helpers) if (QFileInfo(helper).isFile()) {
                QJsonArray wrapped{helper};
                if (route.id == "melonds" || route.id == "azahar" || route.id == "vita3k" || route.id == "cemu") wrapped.append("--pointer");
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
        } else {
            controllerDefaults(route, configDir, env, result.notices);
            if (route.id == "rpcs3" && !env.controllerName.isEmpty()) {
                // Separate named input profile: keep the user's global mapping
                // (often Steam Virtual Gamepad) intact outside TrainerOS.
                auto device=QJsonDocument(QJsonArray{env.controllerName+" 1"}).toJson(QJsonDocument::Compact);
                device=device.mid(1,device.size()-2); // JSON string is also a YAML quoted scalar.
                QByteArray bytes="Player 1 Input:\n  Handler: SDL\n  Device: "+device+"\n  Config:\n"
                    "    Left Stick Left: LS X-\n    Left Stick Right: LS X+\n    Left Stick Up: LS Y+\n    Left Stick Down: LS Y-\n"
                    "    Right Stick Left: RS X-\n    Right Stick Right: RS X+\n    Right Stick Up: RS Y+\n    Right Stick Down: RS Y-\n"
                    "    Cross: South\n    Circle: East\n    Square: West\n    Triangle: North\n"
                    "    Up: Up\n    Down: Down\n    Left: Left\n    Right: Right\n"
                    "    Start: Start\n    Select: Back\n    PS Button: Guide\n"
                    "    L1: LB\n    R1: RB\n    L2: LT\n    R2: RT\n    L3: LS\n    R3: RS\n";
                for (int player=2;player<=7;++player)
                    bytes+="Player "+QByteArray::number(player)+" Input:\n  Handler: \"Null\"\n  Device: \"Null\"\n";
                if (seed(QDir(configDir).filePath("input_configs/global/TrainerOS.yml"),bytes)) {
                    arguments.append("--input-config"); arguments.append("TrainerOS");
                    profile["prefixArguments"]=arguments;
                } else result.notices << "rpcs3: initial controls unavailable";
            }
            if (route.id == "melonds" && QFileInfo(configDir + "/melonDS.toml").isFile()) {
                profile["configFile"] = configDir + "/melonDS.toml";
                profile["backupProtocol"] = "melonds-sav-v1";
            }
        }
        // Persist the selected runtime family once. Installing a different
        // emulator later must not silently move existing games to different saves.
        if (!seed(discoveredPath,QJsonDocument(profile).toJson())) { result.notices << route.id + ": could not retain discovered route"; continue; }
        result.profiles.insert(route.id, profile);
    }
    return result;
}
}
