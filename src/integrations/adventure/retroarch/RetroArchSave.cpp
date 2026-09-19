#include "RetroArchSave.h"
#include "RetroArchConfiguration.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace trainer {
using namespace retroarch;
QString prepareRetroArchLaunch(ProcessCommand& command, const AdventureRegistration& record,
        const RetroArchInstallation& installation, const std::atomic_bool& cancelled) {
    if (cancelled) return "Opening was cancelled.";
    const auto settings = readSettings(installation.configFile);
    if (!supportedConfiguration(record, installation, settings))
        return "This Adventure's play setup needs verification before opening. Your saves are unchanged.";
    // One bounded adapter-owned config instead of a new state folder per run.
    // Never replace an existing file with different content (or a symlink).
    const auto path = QFileInfo(installation.configFile).dir().filePath("traineros-ordinary-v1.cfg");
    if (!safePath(path)) return "Couldn't prepare the Adventure. Your saves are unchanged.";
    const QByteArray bytes = "# TrainerOS ordinary launch v1\n"
        "savestate_auto_save = \"false\"\nsavestate_auto_load = \"false\"\n"
        "savestate_thumbnail_enable = \"false\"\nconfig_save_on_exit = \"false\"\n"
        "auto_overrides_enable = \"false\"\n";
    QFile config(path);
    if (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink()) {
        if (config.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            if (config.write(bytes) != bytes.size() || !config.flush())
                return "Couldn't prepare the Adventure. Check storage and try again.";
            config.close();
        }
    }
    if (QFileInfo(path).isSymLink() || !config.open(QIODevice::ReadOnly)
        || config.read(bytes.size() + 1) != bytes || config.error() != QFile::NoError)
        return "The Adventure's launch settings changed. Check play setup before opening.";
    if (cancelled) return "Opening was cancelled.";
    if (command.arguments.isEmpty() || command.arguments.last() != record.contentPath)
        return "The Adventure's play setup changed.";
    command.arguments.removeLast();
    command.arguments << "--appendconfig" << path << record.contentPath;
    return {};
}

SaveTarget resolveRetroArchSave(const AdventureRegistration& r, const RetroArchInstallation& installation) {
    SaveTarget target; target.adventureId=r.adventure.id; target.title=r.adventure.title;
    if (!installation.saveBackups || r.adventure.collectionOnly || r.adventure.adapterId!="retroarch"
        || r.integrationConfig["core"].toString()!="mgba" || QFileInfo(r.contentPath).suffix().toLower()!="gba") return target;
#ifdef Q_OS_LINUX
    QDirIterator processes("/proc",QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks);
    while(processes.hasNext()) {
        processes.next(); bool number=false; processes.fileName().toUInt(&number); if(!number)continue;
        QFile comm(QDir(processes.filePath()).filePath("comm"));
        if(comm.open(QIODevice::ReadOnly) && comm.read(128).trimmed()=="retroarch") {
            target.error="Close the running Adventure before checking or changing saves."; return target;
        }
    }
#endif
    const auto settings=readSettings(installation.configFile);
    if(!supportedConfiguration(r,installation,settings)) { target.error="This save layout needs verification before backups can be used."; return target; }
    for(const auto& key : {QString("savefiles_in_content_dir"),QString("sort_savefiles_enable"),QString("sort_savefiles_by_content_enable")})
        if(settings.value(key)!="true" && settings.value(key)!="false") { target.error="This save layout needs verification before backups can be used."; return target; }
    const QFileInfo content(r.contentPath);
    QString directory=enabled(settings,"savefiles_in_content_dir")?content.absolutePath():configuredPath(settings,"savefile_directory");
    if(!safePath(directory))return target;
    if(enabled(settings,"sort_savefiles_by_content_enable"))directory=QDir(directory).filePath(content.dir().dirName());
    if(enabled(settings,"sort_savefiles_enable"))directory=QDir(directory).filePath("mGBA");
    const std::atomic_bool cancelled{false};
    target.contentRevision=fileDigest(r.contentPath,64*1024*1024,cancelled);
    QJsonArray parts{QString("mgba-sram-v1"), installation.program,
        QJsonArray::fromStringList(installation.prefixArguments), r.integrationConfig};
    const auto paths = contextFiles(r, installation, settings);
    for (int n = 0; n < paths.size(); ++n) {
        const auto& path = paths[n];
        const bool exists = QFileInfo::exists(path);
        const auto hash = exists ? fileDigest(path, n == 3 ? 64 * 1024 * 1024 : 128 * 1024 * 1024, cancelled) : QString("absent");
        if (!safePath(path) || hash.isEmpty() || (n < 4 && !exists)) {
            target.error = "Game content or play setup couldn't be verified. Check storage and try again."; return target;
        }
        parts.append(path); parts.append(hash);
    }
    target.contextRevision = QString::fromLatin1(QCryptographicHash::hash(
        QJsonDocument(parts).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    if(target.contentRevision.isEmpty() || target.contextRevision.isEmpty()) { target.error="Game content or play setup couldn't be verified. Check storage and try again."; return target; }
    target.savePath=QDir(directory).filePath(content.completeBaseName()+".srm");
    target.supported=true; return target;
}

}
