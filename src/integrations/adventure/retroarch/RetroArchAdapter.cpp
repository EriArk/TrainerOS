#include "RetroArchAdapter.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

namespace trainer {
namespace {
const QHash<QString, QStringList> extensions{
    {"mgba", {"gba"}}, {"gambatte", {"gb", "gbc"}},
    {"parallel_n64", {"z64", "n64", "v64"}}
};
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
    return result;
}
RetroArchAdapter::RetroArchAdapter(LibraryRepository& repository, RetroArchInstallation installation)
    : repository_(repository), installation_(std::move(installation)) {}

std::optional<ProcessCommand> RetroArchAdapter::command(const Adventure& adventure) const {
    // Only in-memory metadata and path syntax here: querying capabilities while
    // drawing a page must not touch a missing/slow removable filesystem.
    if (adventure.adapterId != id() || installation_.program.isEmpty()) return {};
    const auto record = repository_.registration(adventure.id);
    if (!record || record->adventure.adapterId != id() || !QDir::isAbsolutePath(record->contentPath)) return {};
    const auto core = record->integrationConfig.value("core").toString();
    if (!installation_.cores.contains(core)
            || !extensions.value(core).contains(QFileInfo(record->contentPath).suffix().toLower())) return {};
    auto arguments = installation_.prefixArguments;
    arguments << "--fullscreen" << "--config" << installation_.configFile
              << "--libretro" << installation_.cores.value(core) << record->contentPath;
    return ProcessCommand{installation_.program, arguments, {}};
}
AdventureCapabilities RetroArchAdapter::capabilities(const Adventure& adventure) const {
    return {command(adventure).has_value(), false, false};
}
AdventureResult RetroArchAdapter::launch(const Adventure& adventure) {
    const auto invocation = command(adventure);
    if (!invocation) return {false, "This Adventure needs play setup. Its library record has been kept."};
    if (!requestLaunch || !requestLaunch(*invocation)) return {false, "An Adventure is already opening. Try again after returning."};
    return {true, {}, true};
}
AdventureResult RetroArchAdapter::resume(const Adventure&, const ResumePoint&) {
    return {false, "Open the Adventure and choose your save there. Direct resume is not configured."};
}
}
