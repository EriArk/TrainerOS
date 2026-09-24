#include "RetroArchSave.h"
#include "RetroArchConfiguration.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace trainer {
using namespace retroarch;
namespace {
QString digestId(const QString& id) {
    return QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(),QCryptographicHash::Sha256).toHex());
}
bool ownedDirectory(const QString& base,const QString& path,bool create) {
    if(!safePath(base) || !safePath(path) || QFileInfo(base).isSymLink())return false;
    const auto relative=QDir(base).relativeFilePath(path);
    if(relative.startsWith("..") || QDir::isAbsolutePath(relative))return false;
    QString current=base;
    for(const auto& component:relative.split('/')) {
        current=QDir(current).filePath(component);
        if(QFileInfo(current).isSymLink() || (QFileInfo::exists(current) && !QFileInfo(current).isDir()))return false;
        if(create && !QDir().mkpath(current))return false;
    }
    return true;
}
}

QString prepareRetroArchLaunch(ProcessCommand& command, const AdventureRegistration& record,
        const RetroArchInstallation& installation, const std::atomic_bool& cancelled) {
    if (cancelled) return "Opening was cancelled.";
    const auto settings = readSettings(installation.configFile);
    if (!supportedConfiguration(record, installation, settings))
        return "This Adventure's play setup needs verification before opening. Your saves are unchanged.";
    SaveTarget target;
    const auto owner=installation.saves?installation.saves->owner():std::optional<RetroArchSaveOwner>{};
    if(installation.saves) {
        if(!owner)return "Open your Trainer before opening this Adventure.";
        target=resolveRetroArchSave(record,installation);
        if(!target.supported || !target.error.isEmpty())return target.error.isEmpty()?"This Adventure's save setup needs verification.":target.error;
        if(!owner->legacy && !ownedDirectory(owner->directory,QFileInfo(target.savePath).absolutePath(),true))
            return "Couldn't open your save folder. Check storage and try again.";
    }
    // One bounded adapter-owned config instead of a new state folder per run.
    // Never replace an existing file with different content (or a symlink).
    auto path = owner && !owner->legacy
        ? QFileInfo(target.savePath).dir().filePath("traineros-owner-v1.cfg")
        : QFileInfo(installation.configFile).dir().filePath("traineros-ordinary-v1.cfg");
    if (!safePath(path)) return "Couldn't prepare the Adventure. Your saves are unchanged.";
    QByteArray bytes = "# TrainerOS ordinary launch v1\n"
        "savestate_auto_save = \"false\"\nsavestate_auto_load = \"false\"\n"
        "savestate_thumbnail_enable = \"false\"\nconfig_save_on_exit = \"false\"\n"
        "auto_overrides_enable = \"false\"\n";
    if(owner && !owner->legacy) {
        bytes += "savefile_directory = \"" + QFileInfo(target.savePath).absolutePath().toUtf8() + "\"\n"
            "savefiles_in_content_dir = \"false\"\nsort_savefiles_enable = \"false\"\n"
            "sort_savefiles_by_content_enable = \"false\"\n";
    } else if(record.integrationConfig.contains("librarySaveBase")) {
        const auto base=record.integrationConfig["librarySaveBase"].toString();
        if(!safePath(base))return "The game's saved location needs to be checked.";
        bytes += "savefile_directory = \""+base.toUtf8()+"\"\nsavefiles_in_content_dir = \"false\"\n"
            "sort_savefiles_by_content_enable = \"false\"\nsort_savefiles_enable = \""
            +QByteArray(record.integrationConfig["librarySaveSortCore"].toBool()?"true":"false")+"\"\n";
        path=QFileInfo(installation.configFile).dir().filePath("traineros-move-"+digestId(record.adventure.id).left(16)+"-"
            +QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex().left(16))+".cfg");
    }
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
    const auto owner=installation.saves?installation.saves->owner():std::optional<RetroArchSaveOwner>{};
    if(installation.saves && !owner) { target.error="Open your Trainer before checking saves.";return target; }
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
