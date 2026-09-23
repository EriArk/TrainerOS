#include "RetroArchAdapter.h"
#include "RetroArchSave.h"
#include "RetroArchDisc.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

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
const QHash<QString, QString> coreForPlatform{
    {"gb", "gambatte"}, {"gbc", "gambatte"}, {"gba", "mgba"},
    {"n64", "parallel_n64"}, {"pokemini", "pokemini"}, {"nes", "fceumm"},
    {"snes", "snes9x"}, {"megadrive", "genesis_plus_gx"},
    {"sega32x", "picodrive"}, {"ngpc", "mednafen_ngp"}, {"pcengine", "mednafen_pce_fast"},
    {"segacd", "genesis_plus_gx"}, {"neogeocd", "neocd"}
};
bool contentRoute(const QString& platform, const QString& core, const QString& extension) {
    if (retroarch::discPlatform(platform)) return extension == "chd" || extension == "cue";
    if (extension == "chd" || extension == "cue") return false;
    return extensions.value(core).contains(extension);
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
    const auto object = document.object();
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
    for (auto it = extensions.cbegin(); it != extensions.cend(); ++it) {
        const auto path = QDir(coresDirectory).absoluteFilePath(it.key() + "_libretro.so");
        const QFileInfo core(path);
        if (core.isFile() && core.isReadable()) result.cores.insert(it.key(), path);
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
    const auto core = coreForPlatform.value(a.platformId);
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
            || (!record->adventure.platformId.isEmpty() && coreForPlatform.value(record->adventure.platformId) != core)
            || !contentRoute(record->adventure.platformId, core, QFileInfo(record->contentPath).suffix().toLower())
            || (retroarch::discPlatform(record->adventure.platformId) && !installation_.readyDiscPlatforms.contains(record->adventure.platformId))) return {};
    auto arguments = installation_.prefixArguments;
    arguments << "--fullscreen" << "--config" << installation_.configFile
              << "--libretro" << installation_.cores.value(core) << record->contentPath;
    return ProcessCommand{installation_.program, arguments, {}};
}
AdventureCapabilities RetroArchAdapter::capabilities(const Adventure& adventure) const {
    return {command(adventure).has_value(), false, false};
}
AdventureResult RetroArchAdapter::launch(const Adventure& adventure) {
    auto invocation = command(adventure);
    if (!invocation) return {false, "This Adventure needs play setup. Its library record has been kept."};
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
        || retroarch::discPlatform(registration->adventure.platformId))) {
        // These cartridge routes retain the emulator's existing ordinary-save
        // directories. Only mGBA currently owns verified per-Trainer namespaces.
        auto installation = installation_;
        installation.saves.reset();
        invocation->prepare = [record = *registration, installation](ProcessCommand& cmd, const std::atomic_bool& cancel) {
            if (retroarch::discPlatform(record.adventure.platformId)) {
                if (!retroarch::verifiedDiscFirmware(installation, record.adventure.platformId, cancel))
                    return QString("This system's BIOS files changed or are missing. Check play setup before opening.");
                const auto error = retroarch::validateDiscContent(record.contentPath, cancel);
                if (!error.isEmpty()) return error;
            }
            return prepareRetroArchLaunch(cmd, record, installation, cancel);
        };
    }
    if (!requestLaunch || !requestLaunch(*invocation, adventure.id)) return {false, "An Adventure is already opening. Try again after returning."};
    return {true, {}, true};
}
AdventureResult RetroArchAdapter::resume(const Adventure& adventure, const ResumePoint& point) {
    Q_UNUSED(adventure); Q_UNUSED(point);
    return {false, "Open this Adventure normally to use its in-game save."};
}
}
