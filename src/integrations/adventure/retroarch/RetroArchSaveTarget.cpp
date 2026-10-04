#include "RetroArchSave.h"
#include "RetroArchSavePaths.h"
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace trainer {
using namespace retroarch;
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
    const auto owner=installation.saves?installation.saves->owner():std::optional<RetroArchSaveOwner>{};
    if(installation.saves && !owner) { target.error="Open your Trainer before checking saves.";return target; }
    if(owner)target.lineageOwner=owner->id;
    if(owner && !owner->legacy) {
        for(const auto& arg:installation.prefixArguments)
            if(arg.startsWith("-s") || arg.startsWith("--save") || arg.startsWith("--appendconfig") || arg.startsWith("-c") || arg.startsWith("--config")) {
                target.error="This Adventure's launch settings need verification.";return target;
            }
    }
    const auto settings=readSettings(installation.configFile);
    if(!supportedConfiguration(r,installation,settings)) { target.error="This save layout needs verification before backups can be used."; return target; }
    for(const auto& key : {QString("savefiles_in_content_dir"),QString("sort_savefiles_enable"),QString("sort_savefiles_by_content_enable")})
        if(settings.value(key)!="true" && settings.value(key)!="false") { target.error="This save layout needs verification before backups can be used."; return target; }
    const QFileInfo content(r.contentPath);
    QString directory=enabled(settings,"savefiles_in_content_dir")?content.absolutePath():configuredPath(settings,"savefile_directory");
    if(!safePath(directory))return target;
    if(enabled(settings,"sort_savefiles_by_content_enable"))directory=QDir(directory).filePath(content.dir().dirName());
    if(enabled(settings,"sort_savefiles_enable"))directory=QDir(directory).filePath("mGBA");
    if(r.integrationConfig.contains("librarySaveBase")) {
        directory=r.integrationConfig["librarySaveBase"].toString();
        if(!safePath(directory))return target;
        if(r.integrationConfig["librarySaveSortCore"].toBool())directory=QDir(directory).filePath("mGBA");
    }
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
    if(owner && !owner->legacy) {
        directory=QDir(owner->directory).filePath("trainers/"+digestId(owner->id)+"/saves/mgba/"
            +digestId(r.adventure.id)+"/"+target.contentRevision);
        if(r.adventure.id.isEmpty() || !ownedDirectory(owner->directory,directory,false)) {
            target.error="Your save folder needs attention. Existing saves have been kept.";return target;
        }
        target.backupOwner=owner->id;
        target.contextRevision=digestId(target.contextRevision+"\ntrainer-save-v1\n"+owner->id+"\n"+directory);
    }
    target.savePath=QDir(directory).filePath(content.completeBaseName()+".srm");
    if(owner && !owner->legacy) {
        for(const auto& suffix:QStringList{".srm",".rtc"})
            if(QFileInfo(QDir(directory).filePath(content.completeBaseName()+suffix)).isSymLink()) {
                target.error="Your save file needs attention. Existing saves have been kept.";return target;
            }
    }
    target.supported=true; return target;
}

}
