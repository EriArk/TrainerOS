#include "StandaloneAdapter.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>

namespace trainer {
namespace {
QStringList allowedPlatforms(const QString& id) {
    if (id == "ppsspp") return {"psp"};
    if (id == "armsx2") return {"ps2"};
    if (id == "melonds") return {"nds"};
    if (id == "dolphin") return {"gc", "wii"};
    return {};
}
QStringList extensions(const QString& platform) {
    if (platform == "psp") return {"iso", "cso", "pbp", "chd"};
    if (platform == "ps2") return {"iso", "chd", "cso", "bin", "gz"};
    if (platform == "nds") return {"nds"};
    if (platform == "gc") return {"iso", "gcm", "rvz"};
    if (platform == "wii") return {"iso", "rvz", "wbfs", "wad"};
    return {};
}
bool executable(const QString& path) {
    const QFileInfo file(path); return file.isAbsolute() && file.isFile() && file.isExecutable();
}
}
StandaloneInstallation StandaloneInstallation::load(const QString& filename, const QString& adapterId) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 65536 || allowedPlatforms(adapterId).isEmpty()) return {};
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    return fromJson(document.object(), adapterId);
}
StandaloneInstallation StandaloneInstallation::fromJson(const QJsonObject& object, const QString& adapterId) {
    if (allowedPlatforms(adapterId).isEmpty()) return {};
    if (object["version"].toInt() != 1 || object["adapter"].toString() != adapterId) return {};
    StandaloneInstallation result;
    result.program = object["program"].toString(); result.runtimeFile = object["runtimeFile"].toString();
    if (!executable(result.program) || !executable(result.runtimeFile)) return {};
    if (!object["prefixArguments"].isArray() || !object["validatedPlatforms"].isArray()) return {};
    const auto arguments = object["prefixArguments"].toArray();
    if (arguments.size() > 32) return {};
    for (const auto& value : arguments) {
        if (!value.isString() || value.toString().contains(QChar::Null) || value.toString().size() > 4096) return {};
        result.prefixArguments.append(value.toString());
    }
    const auto enabled = object["validatedPlatforms"].toArray();
    if (enabled.isEmpty() || enabled.size() > 2) return {};
    for (const auto& value : enabled) {
        if (!value.isString() || !allowedPlatforms(adapterId).contains(value.toString())) return {};
        result.platforms.append(value.toString());
    }
    result.platforms.removeDuplicates();
    result.configFile = object["configFile"].toString();
    result.melonDsSaveBackups = adapterId == "melonds" && object["backupProtocol"].toString() == "melonds-sav-v1"
        && QFileInfo(result.configFile).isAbsolute() && QFileInfo(result.configFile).isFile();
    return result;
}
StandaloneAdapter::StandaloneAdapter(QString id, LibraryRepository& library, StandaloneInstallation installation)
    : id_(std::move(id)), library_(library), installation_(std::move(installation)) {}
bool StandaloneAdapter::supports(const QString& platform, const QString& path) const {
    return !installation_.program.isEmpty() && allowedPlatforms(id_).contains(platform)
        && installation_.platforms.contains(platform) && extensions(platform).contains(QFileInfo(path).suffix().toLower());
}
QString StandaloneAdapter::setupIssue(const AdventureRegistration& record) const {
    const auto platform = record.adventure.platformId;
    if (!allowedPlatforms(id_).contains(platform)) return {};
    if (installation_.program.isEmpty() || !installation_.platforms.contains(platform)) {
        // An unconfigured title may have a prepared core route instead.
        if (record.adventure.adapterId != id_) return {};
        return "The selected emulator setup is unavailable. Restore it in Desktop Mode, then reopen TrainerOS.";
    }
    if (!extensions(platform).contains(QFileInfo(record.contentPath).suffix().toLower()))
        return "This file format cannot be opened by this emulator.";
    return {};
}
QString StandaloneAdapter::verifyInstallation(const AdventureRegistration& record) const {
    if(!supports(record.adventure.platformId,record.contentPath))return setupIssue(record);
    if(!executable(installation_.program) || !executable(installation_.runtimeFile))
        return "The selected emulator is unavailable. Restore it in Desktop Mode, then check again.";
    return {};
}
void StandaloneAdapter::prepareInstallation(AdventureRegistration& record) const {
    auto& adventure = record.adventure;
    if (adventure.adapterId != "unconfigured" && adventure.adapterId != id_) return;
    if (adventure.platformId.isEmpty() && QFileInfo(record.contentPath).suffix().toLower() == "nds") adventure.platformId = "nds";
    if (supports(adventure.platformId, record.contentPath)) adventure.adapterId = id_;
    else if (adventure.adapterId == id_) adventure.adapterId = "unconfigured";
}
std::optional<ProcessCommand> StandaloneAdapter::command(const Adventure& adventure) const {
    if (adventure.collectionOnly || adventure.adapterId != id_) return {};
    const auto record = library_.registration(adventure.id);
    if (!record || record->adventure.adapterId != id_ || !QDir::isAbsolutePath(record->contentPath)
        || !supports(record->adventure.platformId, record->contentPath)) return {};
    auto arguments = installation_.prefixArguments;
    if (id_ == "melonds") arguments << "-f" << record->contentPath;
    else if (id_ == "ppsspp") arguments << "--fullscreen" << "--pause-menu-exit" << record->contentPath;
    else if (id_ == "armsx2") arguments << "-batch" << "-fullscreen" << "--" << record->contentPath;
    else if (id_ == "dolphin") arguments << "-b" << "-C" << "Dolphin.Display.Fullscreen=True"
        << "-C" << "Dolphin.Interface.ConfirmStop=False" << "-e" << record->contentPath;
    else return {};
    ProcessCommand result{installation_.program, arguments, {}};
    result.prepare = [path = record->contentPath, program = installation_.program, runtime = installation_.runtimeFile]
        (ProcessCommand&, const std::atomic_bool& cancel) -> QString {
        if (cancel) return "Opening cancelled.";
        const QFileInfo content(path);
        if (!content.isFile() || !content.isReadable() || content.size() <= 0) return "The Adventure file is missing or unreadable. Link its current file in Worlds.";
        if (!executable(program) || !executable(runtime)) return "This Adventure's play setup changed. Check it in Desktop Mode.";
        return {};
    };
    return result;
}
AdventureCapabilities StandaloneAdapter::capabilities(const Adventure& adventure) const { return {command(adventure).has_value(), false, false}; }
AdventureResult StandaloneAdapter::launch(const Adventure& adventure) {
    const auto invocation = command(adventure);
    if (!invocation) return {false, "This Adventure needs play setup. Its library record has been kept."};
    if (!requestLaunch || !requestLaunch(*invocation, adventure.id)) return {false, "An Adventure is already opening. Try again after returning."};
    return {true, {}, true};
}
AdventureResult StandaloneAdapter::resume(const Adventure&, const ResumePoint&) {
    return {false, "Open this Adventure normally and choose your save inside it. Direct resume is not configured."};
}
}
