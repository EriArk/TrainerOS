#include "RetroArchAdapter.h"
#include "RetroArchAppearance.h"
#include "RetroArchNetplay.h"
#include "RetroArchHandheldLink.h"
#include "core/repository/CollectionRepository.h"
#include "RetroArchSave.h"
#include "RetroArchDisc.h"
#include "core/repository/RomPlatforms.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QGuiApplication>
#include <QScreen>

namespace trainer {
namespace {
const QHash<QString, QStringList> extensions{
    {"mgba", {"gba"}}, {"gambatte", {"gb", "gbc"}},
    {"parallel_n64", {"z64", "n64", "v64"}}, {"pokemini", {"min"}},
    {"fceumm", {"nes"}}, {"snes9x", {"sfc", "smc", "zip"}},
    {"genesis_plus_gx", {"md", "gen", "smd", "bin", "zip", "chd", "cue"}},
    {"picodrive", {"32x"}}, {"mednafen_ngp", {"ngp", "ngc"}},
    {"mednafen_pce_fast", {"pce"}}, {"neocd", {"cue", "chd"}}
};
bool contentRoute(const QString& platform, const QString& core, const QString& extension) {
    if (retroarch::discPlatform(platform)) return extension == "chd" || extension == "cue";
    // Compressed GBA saves need a separately verified owned-save route.
    if (core == "mgba") return extension == "gba";
    if (platform.isEmpty()) return extensions.value(core).contains(extension);
    return romContentSupported(platform, core, extension);
}
bool cartridgeRoute(const QString& core) {
    return QStringList{"snes9x", "genesis_plus_gx", "picodrive", "mednafen_ngp", "mednafen_pce_fast"}.contains(core);
}
}
RetroArchInstallation RetroArchInstallation::load(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536) return {};
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    return fromJson(document.object());
}
RetroArchInstallation RetroArchInstallation::fromJson(const QJsonObject& object) {
    if (object.value("version").toInt() != 1) return {};
    RetroArchInstallation result;
    result.program = object.value("program").toString();
    result.configFile = object.value("configFile").toString();
    const auto coresDirectory = object.value("coresDirectory").toString();
    const QFileInfo program(result.program), config(result.configFile);
    if (!program.isAbsolute() || !program.isFile() || !program.isExecutable()
            || !config.isAbsolute() || !config.isFile() || !config.isReadable()
            || !QDir::isAbsolutePath(coresDirectory)) return {};
    if (!object.value("prefixArguments").isArray()) return {};
    const auto arguments = object.value("prefixArguments").toArray();
    if (arguments.size() > 32) return {};
    for (const auto& value : arguments) {
        if (!value.isString() || value.toString().contains(QChar::Null)) return {};
        result.prefixArguments.append(value.toString());
    }
    for (const auto& route : romPlatforms()) {
        if(route.core.isEmpty())continue;
        const auto path = QDir(coresDirectory).absoluteFilePath(route.core + "_libretro.so");
        const QFileInfo core(path);
        if (core.isFile() && core.isReadable()) result.cores.insert(route.core, path);
    }
    // Network-only companions; never replace ordinary Gambatte/mGBA routes.
    for(const auto& name:{QString("gpsp"),QString("DoubleCherryGB")}) {
        const auto path=QDir(coresDirectory).absoluteFilePath(name+"_libretro.so");
        if(QFileInfo(path).isFile()&&QFileInfo(path).isReadable())result.cores.insert(name,path);
    }
    if (QFileInfo(object.value("runtimeFile").toString()).isAbsolute())
        result.runtimeFile = object.value("runtimeFile").toString();
    if (object.value("resumeProtocol").toString() == "mgba-entry-v1") {
        const auto directory = object.value("resumeDirectory").toString();
        const auto runtime = object.value("runtimeFile").toString();
        if (QDir::isAbsolutePath(directory) && QFileInfo(runtime).isAbsolute()) {
            result.resumeDirectory = directory; result.runtimeFile = runtime;
        }
    }
    result.saveBackups = object.value("backupProtocol").toString() == "mgba-sram-v1";
    const auto firmware = object.value("discFirmware").toObject();
    for (const auto& platform : {QString("segacd"), QString("neogeocd")}) {
        result.discFirmware.insert(platform, firmware.value(platform).toObject());
        const std::atomic_bool cancel{false};
        if (retroarch::verifiedDiscFirmware(result, platform, cancel)) result.readyDiscPlatforms.insert(platform);
    }
    return result;
}
RetroArchAdapter::RetroArchAdapter(LibraryRepository& repository, RetroArchInstallation installation)
    : repository_(repository), installation_(std::move(installation)) {}
QString RetroArchAdapter::setupIssue(const AdventureRegistration& record) const {
    const auto core = romCore(record.adventure.platformId);
    if (core.isEmpty()) return {};
    if (installation_.program.isEmpty())
        return "The emulator configuration is missing or unreadable. Restore it in Desktop Mode.";
    if (!installation_.cores.contains(core))
        return "The emulator for this platform is missing. Restore it in Desktop Mode.";
    if (!contentRoute(record.adventure.platformId, core, QFileInfo(record.contentPath).suffix().toLower()))
        return "This file format cannot be opened by this platform's emulator.";
    if (retroarch::discPlatform(record.adventure.platformId) && !installation_.readyDiscPlatforms.contains(record.adventure.platformId))
        return "This platform's BIOS files are missing or have not been verified. Check its BIOS files in Desktop Mode.";
    return {};
}
QString RetroArchAdapter::verifyInstallation(const AdventureRegistration& record) const {
    const auto core=record.integrationConfig.value("core").toString();
    if(core.isEmpty() || core!=romCore(record.adventure.platformId) || !installation_.cores.contains(core))return setupIssue(record);
    const QFileInfo program(installation_.program),config(installation_.configFile),coreFile(installation_.cores.value(core));
    if(!program.isFile() || !program.isExecutable() || !config.isFile() || !config.isReadable() || !coreFile.isFile() || !coreFile.isReadable())
        return "Emulator files are missing or unreadable. Restore play setup in Desktop Mode, then check again.";
    if(!installation_.runtimeFile.isEmpty() && (!QFileInfo(installation_.runtimeFile).isFile() || !QFileInfo(installation_.runtimeFile).isExecutable()))
        return "The selected emulator is unavailable. Restore it in Desktop Mode, then check again.";
    if(retroarch::discPlatform(record.adventure.platformId)) {
        const std::atomic_bool cancel{false};
        if(!retroarch::verifiedDiscFirmware(installation_,record.adventure.platformId,cancel))
            return "This platform's BIOS files changed or are missing. Restore the verified files before playing.";
    }
    if(core=="mgba" && installation_.saves && !installation_.saveBackups)
        return "This platform's save setup needs verification before opening.";
    return {};
}
void RetroArchAdapter::prepareInstallation(AdventureRegistration& record) const {
    auto& a = record.adventure;
    if (a.adapterId != "unconfigured" && a.adapterId != id()) return;
    const auto extension = QFileInfo(record.contentPath).suffix().toLower();
    if (a.platformId.isEmpty()) {
        if (extension == "gb" || extension == "gbc" || extension == "gba") a.platformId = extension;
        else if (QStringList{"z64", "n64", "v64"}.contains(extension)) a.platformId = "n64";
        else if (extension == "min") a.platformId = "pokemini";
        else if (extension == "nes") a.platformId = "nes";
    }
    const auto core = romCore(a.platformId);
    if (!installation_.program.isEmpty() && installation_.cores.contains(core) && contentRoute(a.platformId, core, extension)
        && (!retroarch::discPlatform(a.platformId) || installation_.readyDiscPlatforms.contains(a.platformId))) {
        a.adapterId = id(); record.integrationConfig.insert("core", core);
    } else if (a.adapterId == id()) { a.adapterId = "unconfigured"; record.integrationConfig.remove("core"); }
}

std::optional<ProcessCommand> RetroArchAdapter::command(const Adventure& adventure) const {
    // Only in-memory metadata and path syntax here: querying capabilities while
    // drawing a page must not touch a missing/slow removable filesystem.
    if (adventure.collectionOnly || adventure.adapterId != id() || installation_.program.isEmpty()) return {};
    const auto record = repository_.registration(adventure.id);
    if (!record || record->adventure.adapterId != id() || !QDir::isAbsolutePath(record->contentPath)) return {};
    const auto core = record->integrationConfig.value("core").toString();
    if (!installation_.cores.contains(core)
            || (!record->adventure.platformId.isEmpty() && romCore(record->adventure.platformId) != core)
            || !contentRoute(record->adventure.platformId, core, QFileInfo(record->contentPath).suffix().toLower())
            || (retroarch::discPlatform(record->adventure.platformId) && !installation_.readyDiscPlatforms.contains(record->adventure.platformId))) return {};
    auto arguments = installation_.prefixArguments;
    arguments << "--fullscreen" << "--config" << installation_.configFile
              << "--libretro" << installation_.cores.value(core) << record->contentPath;
    ProcessCommand result{installation_.program, arguments, {}};
    result.inspectOutput=[](const QByteArray& bytes)->QString {
        const auto output=bytes.toLower();
        if(output.contains("failed to load content") || output.contains("failed to load libretro core")
            || output.contains("failed to open libretro core"))
            return "Couldn't open this game. Check its file and required BIOS.";
        return {};
    };
    return result;
}
AdventureCapabilities RetroArchAdapter::capabilities(const Adventure& adventure) const {
    return {command(adventure).has_value(), false, false};
}
AdventureResult RetroArchAdapter::launch(const Adventure& adventure) {
    return launchConfigured(adventure,{});
}
AdventureResult RetroArchAdapter::launchNetplay(const Adventure& adventure,const retroarch::NetplayRequest& request) {
    const auto record=repository_.registration(adventure.id);
    if(!record)return {false,"This game is no longer in your library."};
    return launchConfigured(adventure,[record=*record,installation=installation_,request](ProcessCommand& cmd,const std::atomic_bool& cancel){
        return retroarch::prepareNetplay(cmd,record,installation,request,cancel);
    });
}
AdventureResult RetroArchAdapter::launchConfigured(const Adventure& adventure,std::function<QString(ProcessCommand&, const std::atomic_bool&)> extra) {
    auto invocation = command(adventure);
    if (!invocation) return {false, "This game's file or emulator is unavailable."};
    const auto registration = repository_.registration(adventure.id);
    if(registration && registration->integrationConfig["core"].toString()=="mgba" && installation_.saves && !installation_.saveBackups)
        return {false,"This Adventure's save setup needs verification before opening."};
    if (registration && registration->integrationConfig["core"].toString() == "mgba"
        && (installation_.saveBackups || !installation_.resumeDirectory.isEmpty())) {
        const auto record = *repository_.registration(adventure.id);
        invocation->prepare = [record, installation = installation_](ProcessCommand& cmd, const std::atomic_bool& cancel) {
            return prepareRetroArchLaunch(cmd, record, installation, cancel);
        };
    } else if (registration && (cartridgeRoute(registration->integrationConfig["core"].toString())
          || retroarch::discPlatform(registration->adventure.platformId) || registration->integrationConfig.contains("librarySaveBase"))) {
        // These cartridge routes retain the emulator's existing ordinary-save
        // directories. Only mGBA currently owns verified per-Trainer namespaces.
        auto installation = installation_;
        installation.saves.reset();
        invocation->prepare = [record = *registration, installation](ProcessCommand& cmd, const std::atomic_bool& cancel) {
            if (retroarch::discPlatform(record.adventure.platformId)) {
                if (!retroarch::verifiedDiscFirmware(installation, record.adventure.platformId, cancel))
                    return QString("This system's BIOS files changed or are missing. Restore them before opening the game.");
                const auto error = retroarch::validateDiscContent(record.contentPath, cancel);
                if (!error.isEmpty()) return error;
            }
            return prepareRetroArchLaunch(cmd, record, installation, cancel);
        };
    } else if (registration) {
        invocation->prepare = [record=*registration, installation=installation_](ProcessCommand& cmd,const std::atomic_bool& cancel) {
            return prepareGenericRetroArchLaunch(cmd,record,installation,cancel);
        };
    }
    const auto prepare=invocation->prepare;const auto id=adventure.id;const auto config=installation_.configFile;
    const auto screen=qobject_cast<QGuiApplication*>(QCoreApplication::instance())?QGuiApplication::primaryScreen():nullptr;
    const auto display=screen?screen->size()*screen->devicePixelRatio():QSize();
    retroarch::BezelGame bezelGame{adventure.platformId,registration?registration->contentPath:QString(),{}};
    if(adventure.kind!=AdventureKind::RomHack&&!adventure.catalogueId.isEmpty())
        for(const auto& entry:collectionCatalogue(true))if(entry.catalogueId==adventure.catalogueId&&entry.platformId==adventure.platformId){bezelGame.catalogueTitle=entry.title;break;}
    invocation->prepare=[prepare,registration,installation=installation_,id,config,runtime=installation_.runtimeFile,display,bezelGame,extra](ProcessCommand& cmd,const std::atomic_bool& cancel){
        const auto recovery=registration?retroarch::recoverHandheldReturn(*registration,installation):QString();
        if(!recovery.isEmpty())return recovery;
        const auto error=prepare?prepare(cmd,cancel):QString();
        if(!error.isEmpty()||cancel)return error;
        const auto appearance=retroarch::prepareAppearance(cmd,id,config,runtime,display,bezelGame);
        return appearance.isEmpty()&&extra?extra(cmd,cancel):appearance;
    };
    if (!requestLaunch) return {false, "Game launch is unavailable in this session."};
    return requestLaunch(*invocation, adventure.id);
}
AdventureResult RetroArchAdapter::resume(const Adventure& adventure, const ResumePoint& point) {
    Q_UNUSED(adventure); Q_UNUSED(point);
    return {false, "Open this Adventure normally to use its in-game save."};
}
}
