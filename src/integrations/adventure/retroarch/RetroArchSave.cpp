#include "RetroArchSave.h"
#include "RetroArchSavePaths.h"
#include "platform/storage/SaveBackupStorage.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace trainer {
using namespace retroarch;
QString prepareGenericRetroArchLaunch(ProcessCommand& command,const AdventureRegistration& record,
        const RetroArchInstallation& installation,const std::atomic_bool& cancelled) {
    if(cancelled)return "Opening was cancelled.";
    const QFileInfo content(record.contentPath),program(installation.program);
    if(!content.isFile() || !content.isReadable() || content.size()<=0)
        return "The game file is missing or unreadable. Check its folder and refresh the library.";
    const QFileInfo core(installation.cores.value(record.integrationConfig["core"].toString()));
    if(!program.isFile() || !program.isExecutable() || !core.isFile() || !core.isReadable() || core.size()<=0)
        return "The emulator is unavailable. Restore it before opening the game.";
    const auto settings=readSettings(installation.configFile);
    if(settings.isEmpty())return "Couldn't read this game's play settings.";
    // Generic ordinary play is independent of exact-build save providers and RA
    // mode. Keep emulator saves in place; only suppress automatic savestates.
    const QByteArray bytes="# TrainerOS generic ordinary launch v1\n"
        "savestate_auto_save = \"false\"\nsavestate_auto_load = \"false\"\n"
        "config_save_on_exit = \"false\"\nauto_overrides_enable = \"false\"\n";
    const auto path=QFileInfo(installation.configFile).dir().filePath("traineros-generic-ordinary-v1.cfg");
    if(!safePath(path))return "Couldn't prepare the game's launch settings.";
    QFile config(path);
    if(!QFileInfo::exists(path) && !QFileInfo(path).isSymLink() && config.open(QIODevice::WriteOnly|QIODevice::NewOnly)) {
        if(config.write(bytes)!=bytes.size() || !config.flush())return "Couldn't prepare the game's launch settings.";
        config.close();
    }
    if(QFileInfo(path).isSymLink() || !config.open(QIODevice::ReadOnly) || config.read(bytes.size()+1)!=bytes)
        return "The game's launch settings changed. Try opening it again.";
    if(cancelled)return "Opening was cancelled.";
    if(command.arguments.isEmpty() || command.arguments.last()!=record.contentPath)return "The game's launch route changed.";
    command.arguments.removeLast();command.arguments << "--appendconfig" << path << record.contentPath;
    return {};
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
        return "The Adventure's launch settings changed. Try opening it again.";
    if (cancelled) return "Opening was cancelled.";
    if (command.arguments.isEmpty() || command.arguments.last() != record.contentPath)
        return "The Adventure's play setup changed.";
    command.arguments.removeLast();
    command.arguments << "--appendconfig" << path << record.contentPath;
    if(owner && target.supported && !installation.lineageRoot.isEmpty()) {
        const auto error=observeSaveSession(command,installation.lineageRoot,record,
            [installation](const AdventureRegistration& r){return resolveRetroArchSave(r,installation);});
        if(!error.isEmpty())qWarning("Save session history unavailable; ordinary play continues.");
    }
    return {};
}


}
