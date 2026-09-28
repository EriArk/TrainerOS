#include "SaveBackupStorage.h"
#include "SaveLineage.h"
#include "LinkSaveStore.h"
#include "integrations/progress/EmeraldLink.h"
#include "platform/process/ProcessService.h"
#include <QDebug>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLockFile>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>
#ifdef Q_OS_UNIX
#include <unistd.h>
#include <fcntl.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#endif

namespace trainer {
namespace {
constexpr qint64 SaveLimit = 512 * 1024, BundleLimit = 768 * 1024;
constexpr int CopyLimit = 512;
QString hash(const QByteArray& bytes) { return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()); }
bool validHash(const QString& text) { static const QRegularExpression re("^[0-9a-f]{64}$"); return re.match(text).hasMatch(); }
bool validId(const QString& text) { return !QUuid(text).isNull() && QUuid(text).toString(QUuid::WithoutBraces)==text; }
bool syncFile(QFileDevice& file) {
    if (!file.flush()) return false;
#ifdef Q_OS_UNIX
    return ::fsync(file.handle()) == 0;
#elif defined(Q_OS_WIN)
    return ::_commit(file.handle()) == 0;
#else
    return false;
#endif
}
bool syncDirectory(const QString& path) {
#ifdef Q_OS_UNIX
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_DIRECTORY);
    if (fd < 0) return false;
    const bool ok = ::fsync(fd) == 0; ::close(fd); return ok;
#else
    Q_UNUSED(path); return true; // Development hosts; Linux is the delivery target.
#endif
}
QString shelf(const QString& root, const SaveTarget& target) {
    const auto key=target.backupOwner.isEmpty()?target.adventureId.toUtf8():
        QJsonDocument(QJsonObject{{"owner",target.backupOwner},{"adventure",target.adventureId}}).toJson(QJsonDocument::Compact);
    return QDir(root).filePath(hash(key));
}
struct CurrentSave { bool success=false, exists=false; QByteArray data; QString revision, parent; };
CurrentSave current(const SaveTarget& target) {
    if (!target.supported || target.adventureId.isEmpty() || !validHash(target.contentRevision)
        || target.contextRevision.isEmpty() || !QDir::isAbsolutePath(target.savePath)) return {};
    const QFileInfo info(target.savePath); const auto parent = info.dir().canonicalPath();
    if (parent.isEmpty() && !info.exists() && !info.isSymLink() && !target.backupOwner.isEmpty())
        return {true,false,{},hash((target.contextRevision+"\n"+target.savePath+"\nabsent").toUtf8()),{}};
    if (parent.isEmpty() || info.isSymLink() || (info.exists() && (!info.isFile() || !info.isReadable()))) return {};
    CurrentSave result; result.parent=parent; result.exists=info.exists();
    if (result.exists) {
        QFile f(target.savePath);
        if (!f.open(QIODevice::ReadOnly) || f.size()>SaveLimit) return {};
        result.data=f.read(SaveLimit+1);
        if (f.error()!=QFile::NoError || result.data.size()!=f.size() || result.data.size()>SaveLimit) return {};
    }
    result.revision=hash((target.contextRevision+"\n"+target.contentRevision+"\n"+parent+"/"+info.fileName()+"\n"
        +(result.exists?hash(result.data):QString("absent"))).toUtf8());
    result.success=true; return result;
}
struct Bundle { SaveBackup entry; QString adventure, content, owner; QByteArray data; };
Bundle readBundle(const QString& path) {
    Bundle result; result.entry.id=QFileInfo(path).completeBaseName();
    if (!validId(result.entry.id) || QFileInfo(path).isSymLink() || !QFileInfo(path).isFile()) return result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly) || f.size()>BundleLimit) return result;
    const auto bytes=f.read(BundleLimit+1); if (f.error()!=QFile::NoError || bytes.size()>BundleLimit) return result;
    result.entry.revision=hash(bytes);
    const auto json=QJsonDocument::fromJson(bytes).object();
    const auto encoded=json["data"].toString().toLatin1(); result.data=QByteArray::fromBase64(encoded);
    result.owner=json["owner"].toString();
    result.adventure=json["adventure"].toString(); result.content=json["contentSha256"].toString();
    result.entry.createdAt=QDateTime::fromString(json["createdAt"].toString(),Qt::ISODateWithMs);
    result.entry.hasSave=json["hasSave"].toBool(); result.entry.protection=json["protection"].toBool();
    result.entry.reason=json["reason"].toString();
    result.entry.bytes=result.data.size();
    result.entry.valid=json["version"].toDouble(-1)==1 && json["id"].toString()==result.entry.id
        && !result.adventure.isEmpty() && validHash(result.content) && result.entry.createdAt.isValid()
        && json["hasSave"].isBool() && json["protection"].isBool() && result.data.toBase64()==encoded
        && result.data.size()<=SaveLimit && json["sha256"].toString()==hash(result.data)
        && json["bytes"].toDouble(-1)==result.data.size()
        && (result.entry.hasSave || (result.data.isEmpty() && result.entry.protection));
    return result;
}
bool safeShelf(const QString& root, const QString& directory) {
    const auto base=QFileInfo(root).canonicalFilePath(), actual=QFileInfo(directory).canonicalFilePath();
    return !base.isEmpty() && !actual.isEmpty() && !QFileInfo(root).isSymLink() && !QFileInfo(directory).isSymLink()
        && QFileInfo(actual).dir().canonicalPath()==base;
}
QString writeBundle(const QString& root, const SaveTarget& target, const CurrentSave& save, bool protection, const QString& reason = {}) {
    if (!QDir::isAbsolutePath(root) || QFileInfo(root).isSymLink() || !QDir().mkpath(root)) return {};
    const auto directory=shelf(root,target);
    if (!QDir().mkpath(directory) || !safeShelf(root,directory)) return {};
    QDirIterator count(directory,{"*.tosbackup"},QDir::Files|QDir::NoSymLinks); int total=0;
    while(count.hasNext()) { count.next(); if(++total>=CopyLimit)return {}; }
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces), path=QDir(directory).filePath(id+".tosbackup");
    const QJsonObject object{{"version",1},{"id",id},{"adventure",target.adventureId},{"title",target.title},
        {"owner",target.backupOwner},{"contentSha256",target.contentRevision},{"createdAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"hasSave",save.exists},{"protection",protection},{"reason",reason},{"bytes",save.data.size()},
        {"sha256",hash(save.data)},{"data",QString::fromLatin1(save.data.toBase64())}};
    const auto bytes=QJsonDocument(object).toJson(QJsonDocument::Compact);
    QSaveFile file(path); file.setDirectWriteFallback(false);
    if (QFileInfo::exists(path) || !file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)
        || file.write(bytes)!=bytes.size() || !syncFile(file) || !safeShelf(root,directory) || !file.commit()
        || !syncDirectory(directory) || !syncDirectory(root) || !syncDirectory(QFileInfo(root).dir().absolutePath())) return {};
    const auto check=readBundle(path);
    return check.entry.valid && check.data==save.data && check.adventure==target.adventureId && check.owner==target.backupOwner ? id : QString();
}
QString problem(const SaveTarget& target) { return target.error.isEmpty()?"This Adventure has no verified save-backup setup yet.":target.error; }
void rememberMerchants(const QString& root,const SaveTarget& target,MerchantSnapshot& state) {
    if(!state.supported||state.lineage.isEmpty()||!QDir::isAbsolutePath(root)||QFileInfo(root).isSymLink())return;
    const auto directory=QDir(root).filePath("merchant-discovery");
    if(QFileInfo(directory).isSymLink()||!QDir().mkpath(directory))return;
    const auto identity=QJsonDocument(QJsonObject{{"owner",target.backupOwner},{"adventure",target.adventureId},
        {"content",target.contentRevision},{"path",target.savePath},{"lineage",state.lineage}}).toJson(QJsonDocument::Compact);
    const auto path=QDir(directory).filePath(hash(identity)+".json");if(QFileInfo(path).isSymLink())return;
    QFile old(path);QJsonArray seen;bool baseline=false;
    if(old.open(QIODevice::ReadOnly)&&old.size()<16384){const auto d=QJsonDocument::fromJson(old.readAll());baseline=d.isArray();seen=d.array();}
    old.close();
    QStringList fresh;
    for(const auto& merchant:state.merchants)if(merchant.discovered&&!seen.contains(merchant.id)){
        seen.append(merchant.id);if(baseline)fresh.append(merchant.name);
    }
    if(baseline&&fresh.isEmpty())return;
    QSaveFile file(path);file.setDirectWriteFallback(false);
    const auto bytes=QJsonDocument(seen).toJson(QJsonDocument::Compact);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())return;
    if(!fresh.isEmpty())state.discoveryNotice=fresh.size()==1?"New merchant discovered · "+fresh.first():QString("%1 new merchants discovered").arg(fresh.size());
}
}
QString observeSaveSession(ProcessCommand& command,const QString& root,const AdventureRegistration& record,const SaveTargetResolver& resolve) {
    if(command.settled || !QDir::isAbsolutePath(root) || QFileInfo(root).isSymLink() || !QDir().mkpath(root))return "Save session history is unavailable.";
    QLockFile lock(QDir(root).filePath("service.lock"));
    if(!lock.tryLock(0))return "Save session history is busy.";
    const auto target=resolve(record);const auto source=current(target);
    if(!source.success || target.lineageOwner.isEmpty())return "Save session history has no verified owner or source.";
    SaveLineageEdit history(root,target,record,source.revision,source.data,source.exists);
    if(!history.error().isEmpty())return history.error();
    const auto id=history.beginSession();
    if(id.isEmpty())return "Save session history could not be started.";
    command.settled=[root,record,resolve,target,id](const ProcessOutcome& outcome){
        QLockFile lock(QDir(root).filePath("service.lock"));
        if(!lock.tryLock(0)){qWarning("Save session history: return observation deferred.");return;}
        const auto afterTarget=resolve(record);const auto after=current(afterTarget);
        if(!after.success || afterTarget.contextRevision!=target.contextRevision
           || saveLineageStream(afterTarget)!=saveLineageStream(target)) {
            qWarning("Save session history: return source changed or unavailable.");return;
        }
        SaveLineageEdit history(root,afterTarget,record,after.revision,after.data,after.exists,id);
        if(!history.error().isEmpty() || !history.finishSession(outcome.started,outcome.exitCode,outcome.crashed,outcome.stopped).isEmpty())
            qWarning("Save session history: return could not be recorded.");
    };
    return {};
}
SaveBackupSnapshot inspectSaveBackups(const QString& root, const SaveTarget& target, const SaveHealer& healer, const MerchantReader& shops) {
    SaveBackupSnapshot result; result.supported=target.supported;
    if (!target.supported) { result.error=problem(target); return result; }
    const auto save=current(target);
    if (save.success) { result.hasSave=save.exists&&!save.data.isEmpty(); result.token=save.revision; }
    else result.error="The in-game save folder couldn't be read. Check storage and try again.";
    if (healer && result.hasSave) {
        const auto healing=healer(save.data,target.contentRevision);
        result.canHeal=!healing.data.isEmpty() && healing.error.isEmpty();
        result.needsHealing=result.canHeal && healing.data!=save.data;
        result.partyCount=healing.partyCount; result.healingError=healing.error;
    }
    if (shops && result.hasSave) {result.shops=shops(save.data,target.contentRevision);rememberMerchants(root,target,result.shops);}
    const auto directory=shelf(root,target);
    if (!QFileInfo::exists(directory)) return result;
    if (!safeShelf(root,directory)) { result.error="Your backup folder needs attention. Existing copies have been kept."; return result; }
    QDirIterator files(directory,{"*.tosbackup"},QDir::Files|QDir::NoSymLinks); int count=0;
    while(files.hasNext()) {
        files.next(); if(++count>CopyLimit) { result.error="Your backup shelf is full. Use maintenance mode to archive older copies."; break; }
        auto copy=readBundle(files.filePath());
        if(copy.adventure!=target.adventureId || copy.owner!=target.backupOwner || copy.content!=target.contentRevision)copy.entry.valid=false;
        result.copies.append(copy.entry);
    }
    std::sort(result.copies.begin(),result.copies.end(),[](const auto& a,const auto& b){return a.createdAt==b.createdAt?a.id>b.id:a.createdAt>b.createdAt;});
    return result;
}
bool saveWritesReadOnly(const QString& root) {
    if(!QDir::isAbsolutePath(root) || QFileInfo(root).isSymLink())return true;
    const auto path=QDir(root).filePath("save-policy.json");
    if(QFileInfo(path).isSymLink())return true;
    if(!QFileInfo::exists(path))return false;
    QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>1024)return true;
    const auto obj=QJsonDocument::fromJson(file.readAll()).object();
    return obj["version"].toInt()!=1 || !obj["readOnly"].isBool() || obj["readOnly"].toBool();
}
QString setSaveWritesReadOnly(const QString& root,bool enabled) {
    if(!QDir::isAbsolutePath(root)||QFileInfo(root).isSymLink()||!QDir().mkpath(root))return "Could not save the save policy.";
    QLockFile lock(QDir(root).filePath("service.lock"));
    if(!lock.tryLock(0))return "A save operation is running. Try again when it finishes.";
    const auto path=QDir(root).filePath("save-policy.json");
    if(QFileInfo(path).isSymLink())return "The save policy path needs attention.";
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"readOnly",enabled}}).toJson(QJsonDocument::Compact);
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)||!file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)
       ||file.write(bytes)!=bytes.size()||!syncFile(file)||!file.commit()||!syncDirectory(root))return "Could not save the save policy. Check storage and retry.";
    return {};
}
SaveBackupResult createSaveBackup(const QString& root, const AdventureRegistration& record, const QString& token, const SaveTargetResolver& resolve) {
    if (!QDir::isAbsolutePath(root) || QFileInfo(root).isSymLink() || !QDir().mkpath(root))return {false,false,"Couldn't open the backup folder."};
    QLockFile lock(QDir(root).filePath("service.lock")); if(!lock.tryLock(0))return {false,false,"Another backup operation is running. Try again shortly."};
    if(pendingLinkSave(root))return {false,false,"Finish the pending trade at Link Counter first."};
    const auto target=resolve(record); if(!target.supported)return {false,false,problem(target)};
    const auto save=current(target);
    if(!save.success || !save.exists || save.data.isEmpty() || token.isEmpty() || save.revision!=token)return {false,false,"The save changed or isn't available. Check again before making a copy."};
    if(current(resolve(record)).revision!=save.revision)return {false,false,"The save changed while being checked. Try again after returning from the Adventure."};
    const auto id=writeBundle(root,target,save,false);
    if(id.isEmpty())return {false,false,"Couldn't keep a verified backup. Check free space and backup-folder access. Your in-game save is unchanged."};
    return {true,false,"In-game save copied and verified.",inspectSaveBackups(root,resolve(record))};
}
SaveBackupResult restoreSaveBackup(const QString& root, const AdventureRegistration& record, const SaveBackup& selected,
        const QString& token, const SaveTargetResolver& resolve) {
    if(pendingLinkSave(root))return {false,false,"Finish the pending trade at Link Counter first."};
    if(!QDir::isAbsolutePath(root) || QFileInfo(root).isSymLink() || !validId(selected.id) || selected.revision.isEmpty())return {false,false,"Choose a saved copy again."};
    QLockFile lock(QDir(root).filePath("service.lock")); if(!lock.tryLock(0))return {false,false,"Another backup operation is running. Try again shortly."};
    if(saveWritesReadOnly(root))return {false,false,"Read-only saves is on. Change it in Settings to restore a save."};
    const auto target=resolve(record); if(!target.supported)return {false,false,problem(target)};
    const auto directory=shelf(root,target);
    if(!safeShelf(root,directory))return {false,false,"The backup folder changed. Existing saves have been kept."};
    const auto copy=readBundle(QDir(directory).filePath(selected.id+".tosbackup"));
    if(!copy.entry.valid || !copy.entry.hasSave || copy.data.isEmpty() || copy.entry.revision!=selected.revision || copy.adventure!=target.adventureId || copy.owner!=target.backupOwner || copy.content!=target.contentRevision)
        return {false,false,"This copy changed, is damaged, or belongs to different game content. It wasn't restored."};
    const auto save=current(target);
    if(!save.success || token.isEmpty() || save.revision!=token)return {false,false,"The current save changed. Check it again before confirming a restore."};
    const auto protection=writeBundle(root,target,save,true);
    if(protection.isEmpty())return {false,false,"Couldn't protect the current save first. Restore was cancelled; your save is unchanged."};
    SaveLineageEdit lineage(root,target,record,token,save.data,save.exists);
    if(!lineage.error().isEmpty())return {false,false,lineage.error()};
    if(current(resolve(record)).revision!=token)return {false,false,"The save changed during preparation. Restore was cancelled; the protection copy was kept."};
    QSaveFile file(target.savePath); file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly) || file.write(copy.data)!=copy.data.size() || !syncFile(file))
        return {false,false,"Couldn't prepare the restored save. Your current save and protection copy were kept."};
    if(current(resolve(record)).revision!=token) { file.cancelWriting(); return {false,false,"The save changed during preparation. Check again before restoring."}; }
    if(saveWritesReadOnly(root)){file.cancelWriting();return {false,false,"Read-only saves is on. The save was not changed."};}
    if(!file.commit())return {false,false,"Couldn't replace the save. The protection copy is available in Pokémon Center."};
    const bool synced=syncDirectory(save.parent);
    const auto after=current(target);
    if(!synced || !after.success || after.data!=copy.data)return {false,true,"The save was replaced, but storage verification failed. Keep the protection copy and check the storage device."};
    if(!lineage.finish(after.data,"restore",protection).isEmpty())
        return {false,true,"The save was restored, but its history could not be recorded. Keep the protection copy and check storage."};
    return {true,true,"Save restored. Open the Adventure normally to use it.",inspectSaveBackups(root,resolve(record))};
}
static SaveBackupResult applySaveEdit(const QString& root, const AdventureRegistration& record, const QString& token,
        const SaveTargetResolver& resolve, const std::function<MerchantWrite(const QByteArray&,const QString&)>& edit,
        const QString& reason, const SaveHealer& healer, const MerchantReader& shops) {
    if(pendingLinkSave(root))return {false,false,"Finish the pending trade at Link Counter first."};
    if (!edit) return {false,false,"This service is not available for this Adventure."};
    if (!QDir::isAbsolutePath(root) || QFileInfo(root).isSymLink() || !QDir().mkpath(root))
        return {false,false,"Couldn't open the backup folder. Your save is unchanged."};
    QLockFile lock(QDir(root).filePath("service.lock"));
    if (!lock.tryLock(0)) return {false,false,"Another save operation is running."};
    if(pendingLinkSave(root))return {false,false,"Finish the pending trade at Link Counter first."};
    if(saveWritesReadOnly(root))return {false,false,"Read-only saves is on. Change it in Settings to allow save changes."};
    const auto target=resolve(record); if (!target.supported) return {false,false,problem(target)};
    const auto save=current(target);
    if (!save.success || !save.exists || token.isEmpty() || save.revision!=token)
        return {false,false,"The save changed. Visit the Center again before confirming."};
    const auto treatment=edit(save.data,target.contentRevision);
    if (!treatment.error.isEmpty() || treatment.data.isEmpty()) return {false,false,treatment.error};
    if (treatment.data==save.data) return {true,false,treatment.message,inspectSaveBackups(root,target,healer,shops)};
    const auto protection=writeBundle(root,target,save,true,reason);
    if (protection.isEmpty())
        return {false,false,"Couldn't protect your save. The change was cancelled."};
    SaveLineageEdit lineage(root,target,record,token,save.data,save.exists);
    if(!lineage.error().isEmpty())return {false,false,lineage.error()};
    if (current(resolve(record)).revision!=token)
        return {false,false,"The save changed. The change was cancelled; your backup was kept."};
    QSaveFile file(target.savePath); file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileInfo(target.savePath).permissions())
        || file.write(treatment.data)!=treatment.data.size() || !syncFile(file))
        return {false,false,"Couldn't prepare the change. Your save and backup were kept."};
    if (current(resolve(record)).revision!=token) {
        file.cancelWriting(); return {false,false,"The save changed before confirmation. Please try again."};
    }
    if(saveWritesReadOnly(root)){file.cancelWriting();return {false,false,"Read-only saves is on. The save was not changed."};}
    if (!file.commit()) return {false,false,"Couldn't replace the save. Your backup is available in Center."};
    const bool synced=syncDirectory(save.parent);
    const auto after=current(target);
    if (!synced || !after.success || after.data!=treatment.data)
        return {false,true,"The change was written, but storage verification failed. Keep the backup and check storage."};
    if(!lineage.finish(after.data,reason,protection).isEmpty())
        return {false,true,"The change was saved, but its history could not be recorded. Keep the backup and check storage."};
    return {true,true,treatment.message,inspectSaveBackups(root,target,healer,shops)};
}
SaveBackupResult healSaveParty(const QString& root,const AdventureRegistration& record,const QString& token,
        const SaveTargetResolver& resolve,const SaveHealer& healer) {
    if(!healer)return {false,false,"Healing is not available for this Adventure."};
    return applySaveEdit(root,record,token,resolve,[healer](const QByteArray& bytes,const QString& hash){
        const auto r=healer(bytes,hash);return MerchantWrite{r.data,r.error,r.data==bytes?"Your Pokémon are already feeling great!":"Your Pokémon are back to full health!"};
    },"healing",healer,{});
}
SaveBackupResult purchaseSaveItems(const QString& root,const AdventureRegistration& record,const QString& token,
        const MerchantPurchase& request,const SaveTargetResolver& resolve,const MerchantBuyer& buyer,const MerchantReader& shops) {
    if(!buyer||!shops)return {false,false,"Purchases are unavailable for this Adventure."};
    return applySaveEdit(root,record,token,resolve,[buyer,request](const QByteArray& bytes,const QString& hash){return buyer(bytes,hash,request);},"purchase",{},shops);
}
SaveBackupResult moveSavePokemon(const QString& root,const AdventureRegistration& record,const QString& token,const PartyMove& request,const SaveTargetResolver& resolve,const PartyMover& mover) {
    if(!mover)return {false,false,"Moving is unavailable for this Adventure."};
    return applySaveEdit(root,record,token,resolve,[mover,request](const QByteArray& bytes,const QString& hash){
        const auto moved=mover(bytes,hash,request);return MerchantWrite{moved.data,moved.error,moved.message};
    },"movement",{},{});
}
void LocalSaveBackupService::movePokemon(const AdventureRegistration& r,const QString& token,const PartyMove& request,QObject* context,std::function<void(SaveBackupResult)> completed) {
    run([this,r,token,request]{return moveSavePokemon(root_,r,token,request,resolve_,mover_);},context,completed);
}
SaveBackupResult releaseSavePokemon(const QString& root,const AdventureRegistration& record,const QString& token,const PokemonRelease& request,const SaveTargetResolver& resolve,const PokemonReleaser& releaser) {
    if(!releaser)return {false,false,"Release is unavailable for this Adventure."};
    return applySaveEdit(root,record,token,resolve,[releaser,request](const QByteArray& bytes,const QString& hash){
        const auto result=releaser(bytes,hash,request);return MerchantWrite{result.data,result.error,result.message};
    },"release",{},{});
}
void LocalSaveBackupService::releasePokemon(const AdventureRegistration& r,const QString& token,const PokemonRelease& request,QObject* context,std::function<void(SaveBackupResult)> completed) {
    run([this,r,token,request]{return releaseSavePokemon(root_,r,token,request,resolve_,releaser_);},context,completed);
}
SaveBackupResult renameSaveBox(const QString& root,const AdventureRegistration& record,const QString& token,const BoxNameChange& request,const SaveTargetResolver& resolve,const BoxNameWriter& writer) {
    if(!writer)return {false,false,"Renaming boxes is unavailable for this Adventure."};
    return applySaveEdit(root,record,token,resolve,[writer,request](const QByteArray& bytes,const QString& hash){
        const auto result=writer(bytes,hash,request);return MerchantWrite{result.data,result.error,result.message};
    },"box-name",{},{});
}
void LocalSaveBackupService::renameBox(const AdventureRegistration& r,const QString& token,const BoxNameChange& request,QObject* context,std::function<void(SaveBackupResult)> completed) {
    run([this,r,token,request]{return renameSaveBox(root_,r,token,request,resolve_,boxNameWriter_);},context,completed);
}
SaveBackupResult changeSaveHeldItem(const QString& root,const AdventureRegistration& record,const QString& token,const HeldItemChange& request,const SaveTargetResolver& resolve,const HeldItemWriter& writer) {
    if(!writer)return {false,false,"Held items are unavailable for this Adventure."};
    return applySaveEdit(root,record,token,resolve,[writer,request](const QByteArray& bytes,const QString& hash){
        const auto result=writer(bytes,hash,request);return MerchantWrite{result.data,result.error,result.message};
    },"held-item",{},{});
}
void LocalSaveBackupService::changeHeldItem(const AdventureRegistration& r,const QString& token,const HeldItemChange& request,QObject* context,std::function<void(SaveBackupResult)> completed) {
    run([this,r,token,request]{return changeSaveHeldItem(root_,r,token,request,resolve_,heldItemWriter_);},context,completed);
}
namespace {
QString linkPath(const QString& root,const QString& id={}) {return QDir(root).filePath(id.isEmpty()?"link/active.json":"link/"+id+".json");}
QJsonObject linkError(const QString& text){return {{"error",text}};}
QJsonObject readLink(const QString& root,const QString& id={}) {
    if(!id.isEmpty() && !validId(id))return {};
    const auto path=linkPath(root,id);QFile f(path);
    if(QFileInfo(path).isSymLink() || QFileInfo(QDir(root).filePath("link")).isSymLink() || !f.open(QIODevice::ReadOnly) || f.size()>768*1024)return {};
    const auto value=QJsonDocument::fromJson(f.readAll()).object();
    if(value["version"].toInt()!=1 || !validId(value["id"].toString()))return {};
    return value;
}
bool writeLink(const QString& root,const QJsonObject& value,bool archive=false) {
    const auto directory=QDir(root).filePath("link");
    if(!QDir::isAbsolutePath(root) || QFileInfo(root).isSymLink() || QFileInfo(directory).isSymLink() || !QDir().mkpath(directory))return false;
    QSaveFile f(linkPath(root,archive?value["id"].toString():QString()));f.setDirectWriteFallback(false);
    const auto bytes=QJsonDocument(value).toJson(QJsonDocument::Compact);
    return f.open(QIODevice::WriteOnly) && f.setPermissions(QFile::ReadOwner|QFile::WriteOwner)
        && f.write(bytes)==bytes.size() && syncFile(f) && f.commit() && syncDirectory(directory);
}
QJsonObject publicLink(const QJsonObject& j) {
    QJsonObject out;
    for(const auto key:{"id","peer","proposal","before","after","peerAfter","stage","adventure","owner","kind"})out[key]=j[key];
    return out;
}
}
bool pendingLinkSave(const QString& root){const QFileInfo f(linkPath(root));return f.exists() || f.isSymLink();}
QJsonObject linkSaveStatus(const QString& root,const QString& id) {
    auto j=readLink(root);
    if(!id.isEmpty() && j["id"]!=id)j=readLink(root,id);
    if(j.isEmpty())return pendingLinkSave(root) && id.isEmpty()?linkError("The pending trade record needs recovery. Your save has been kept."):QJsonObject{};
    return publicLink(j);
}
QJsonObject inspectLinkPokemon(const QString& root,const AdventureRegistration& r,int slot,const SaveTargetResolver& resolve) {
    const auto target=resolve(r);const auto save=current(target);
    if(!save.success || !save.exists)return linkError("The ordinary save is unavailable.");
    const auto account=emeraldLinkAccount(save.data,target.contentRevision);
    if(account.contains("error"))return account;
    if(slot<0)return {{"save",hash(save.data)},{"account",account}};
    const auto offer=emeraldLinkOffer(save.data,target.contentRevision,slot);
    if(!offer.error.isEmpty())return linkError(offer.error);
    return {{"pokemon",offer.pokemon},{"save",hash(save.data)},{"slot",slot},{"account",account}};
}
QJsonObject prepareLinkSave(const QString& root,const AdventureRegistration& r,const QJsonObject& request,const SaveTargetResolver& resolve) {
    if(!QDir::isAbsolutePath(root) || QFileInfo(root).isSymLink() || !QDir().mkpath(root))return linkError("Cannot protect this trade.");
    QLockFile lock(QDir(root).filePath("service.lock"));if(!lock.tryLock(0))return linkError("Another save operation is running.");
    const auto id=request["id"].toString();
    if(!validId(id) || !validId(request["peer"].toString()) || !validHash(request["proposal"].toString()))return linkError("The trade proposal is invalid.");
    if(pendingLinkSave(root))return linkError("Finish the pending trade first.");
    if(!readLink(root,id).isEmpty())return linkError("This trade has already been completed.");
    if(saveWritesReadOnly(root))return linkError("Read-only saves is on. Change it in Settings before trading.");
    const auto target=resolve(r);const auto save=current(target);
    if(!save.success || !save.exists || hash(save.data)!=request["save"].toString())return linkError("The save changed. Choose your Pokemon again.");
    const auto kind=request["kind"].toString("trade");const bool sale=kind=="sale" || kind=="gift";
    if(!sale && kind!="trade")return linkError("Unsupported transfer.");
    const bool seller=request["seller"].toBool();
    if(!sale || seller) {
        const auto offered=emeraldLinkOffer(save.data,target.contentRevision,request["slot"].toInt(-1));
        if(!offered.error.isEmpty() || offered.pokemon!=request["outgoing"].toObject())return linkError("Your selected Pokemon changed.");
    }
    if(sale && (request["price"].toDouble(-1)!=request["price"].toInt(-2) || (kind=="gift" && request["price"].toInt()!=0)))return linkError("The price is invalid.");
    const auto result=sale?sellEmeraldPokemon(save.data,target.contentRevision,request["slot"].toInt(-1),hash(save.data),request["incoming"].toObject(),request["price"].toInt(-1),seller)
        :tradeEmeraldPokemon(save.data,target.contentRevision,request["slot"].toInt(-1),hash(save.data),request["incoming"].toObject());
    if(!result.error.isEmpty() || result.data.isEmpty())return linkError(result.error);
    const auto backup=writeBundle(root,target,save,true,"link-"+kind);if(backup.isEmpty())return linkError("Couldn't protect the current save.");
    if(current(resolve(r)).revision!=save.revision)return linkError("The save changed while preparing the trade.");
    QJsonObject journal{{"version",1},{"id",id},{"peer",request["peer"]},{"proposal",request["proposal"]},{"kind",kind},
        {"stage","prepared"},{"adventure",r.adventure.id},{"owner",target.backupOwner},
        {"content",target.contentRevision},{"context",target.contextRevision},{"path",target.savePath},
        {"token",save.revision},{"before",hash(save.data)},{"after",hash(result.data)},
        {"original",QString::fromLatin1(save.data.toBase64())},{"candidate",QString::fromLatin1(result.data.toBase64())},{"protection",backup}};
    if(!writeLink(root,journal))return linkError("Couldn't keep the prepared trade. The save is unchanged.");
    return publicLink(journal);
}
QJsonObject commitLinkSave(const QString& root,const AdventureRegistration& r,const QString& id,const QString& peerAfter,const SaveTargetResolver& resolve) {
    QLockFile lock(QDir(root).filePath("service.lock"));if(!lock.tryLock(0))return linkError("Another save operation is running.");
    auto j=readLink(root);if(j["id"]!=id || !validHash(peerAfter))return linkError("This trade cannot be recovered yet.");
    if(j.contains("peerAfter") && j["peerAfter"]!=peerAfter)return linkError("The partner's prepared save changed.");
    const auto target=resolve(r);const auto save=current(target);
    if(j["adventure"]!=r.adventure.id || j["owner"]!=target.backupOwner || j["content"]!=target.contentRevision || j["context"]!=target.contextRevision || j["path"]!=target.savePath || !save.success)
        return linkError("Return to the Trainer and Emerald Adventure used for this trade.");
    const auto before=QByteArray::fromBase64(j["original"].toString().toLatin1()),after=QByteArray::fromBase64(j["candidate"].toString().toLatin1());
    if(before.size()!=0x20000 || after.size()!=0x20000 || hash(before)!=j["before"] || hash(after)!=j["after"])
        return linkError("The protected trade copy could not be verified.");
    if(save.data!=before && save.data!=after)return linkError("The save changed outside this trade. Keep both consoles and protection copies for recovery.");
    if(j["stage"]=="prepared" && save.data!=before)return linkError("The prepared save changed outside this trade.");
    if(j["stage"]=="committed" && save.data==after)return publicLink(j);
    if(saveWritesReadOnly(root))return linkError("Read-only saves is on. Allow this pending trade in Settings to continue.");
    // Durable decision BEFORE replacement. Repeating this operation compares
    // both images, never applies the Pokemon swap twice.
    j["stage"]="commit";j["peerAfter"]=peerAfter;if(!writeLink(root,j))return linkError("Couldn't record the trade decision.");
    if(save.data==before) {
        SaveLineageEdit lineage(root,target,r,save.revision,before,true);
        if(!lineage.error().isEmpty())return linkError(lineage.error());
        QSaveFile f(target.savePath);f.setDirectWriteFallback(false);
        if(!f.open(QIODevice::WriteOnly) || !f.setPermissions(QFileInfo(target.savePath).permissions()) || f.write(after)!=after.size() || !syncFile(f))return linkError("Couldn't write this trade. Reconnect to continue.");
        if(current(resolve(r)).revision!=save.revision){f.cancelWriting();return linkError("The save changed during this trade.");}
        if(!f.commit() || !syncDirectory(save.parent) || current(resolve(r)).data!=after)return linkError("Trade readback failed. Keep the protection copies.");
        if(!lineage.finish(after,"link-"+j["kind"].toString("trade"),j["protection"].toString()).isEmpty())return linkError("Trade saved; its history needs recovery.");
    }
    j["stage"]="committed";if(!writeLink(root,j))return linkError("Trade saved; reconnect to finish its receipt.");
    return publicLink(j);
}
QJsonObject finishLinkSave(const QString& root,const QString& id,const QString& peerAfter) {
    QLockFile lock(QDir(root).filePath("service.lock"));if(!lock.tryLock(0))return linkError("Another save operation is running.");
    auto j=readLink(root);if(j["id"]!=id)return linkSaveStatus(root,id);
    if(j["stage"]!="committed" || j["peerAfter"]!=peerAfter)return linkError("Waiting for the partner's saved receipt.");
    j["stage"]="complete";
    if(!writeLink(root,j,true) || !QFile::remove(linkPath(root)) || !syncDirectory(QDir(root).filePath("link")))return linkError("Both trades are saved; reconnect to finish the receipt.");
    return publicLink(j);
}
QJsonObject abortPreparedLinkSave(const QString& root,const AdventureRegistration& r,const QString& id,const SaveTargetResolver& resolve) {
    QLockFile lock(QDir(root).filePath("service.lock"));if(!lock.tryLock(0))return linkError("Another save operation is running.");
    auto j=readLink(root);const auto target=resolve(r);const auto save=current(target);
    if(j["id"]!=id || j["stage"]!="prepared" || !save.success || j["owner"]!=target.backupOwner
        || j["path"]!=target.savePath || j["token"]!=save.revision || j["before"]!=hash(save.data))return linkError("This trade needs both consoles to recover. No rollback was attempted.");
    j["stage"]="cancelled";
    if(!writeLink(root,j,true) || !QFile::remove(linkPath(root)) || !syncDirectory(QDir(root).filePath("link")))return linkError("Couldn't finish cancelling the prepared trade.");
    return publicLink(j);
}
LocalSaveBackupService::LocalSaveBackupService(QString root, SaveTargetResolver resolve,
        std::function<bool(const AdventureRegistration&)> supports, QObject* parent)
    : SaveBackupService(parent),root_(std::move(root)),resolve_(std::move(resolve)),supports_(std::move(supports)),worker_(new QObject) {
    readOnly_=saveWritesReadOnly(root_);
    worker_->moveToThread(&thread_); connect(&thread_,&QThread::finished,worker_,&QObject::deleteLater); thread_.start();
}
LocalSaveBackupService::~LocalSaveBackupService(){thread_.quit();thread_.wait();}
void LocalSaveBackupService::linkOperation(const AdventureRegistration& r,const QString& operation,const QJsonObject& request,
        QObject* receiver,std::function<void(QJsonObject)> done) {
    if(busy_){done(linkError("A save operation is already running."));return;}
    busy_=true;emit busyChanged();
    QMetaObject::invokeMethod(worker_,[this,r,operation,request,guard=QPointer<QObject>(receiver),done]{
        QJsonObject result;
        if(operation=="inspect")result=inspectLinkPokemon(root_,r,request["slot"].toInt(-1),resolve_);
        else if(operation=="prepare")result=prepareLinkSave(root_,r,request,resolve_);
        else if(operation=="commit")result=commitLinkSave(root_,r,request["id"].toString(),request["peerAfter"].toString(),resolve_);
        else if(operation=="finish")result=finishLinkSave(root_,request["id"].toString(),request["peerAfter"].toString());
        else if(operation=="abort-prepared")result=abortPreparedLinkSave(root_,r,request["id"].toString(),resolve_);
        else if(operation=="status")result=linkSaveStatus(root_,request["id"].toString());
        else result=linkError("Unknown Link operation.");
        if(result.contains("error") && pendingLinkSave(root_))result["pending"]=linkSaveStatus(root_);
        QMetaObject::invokeMethod(this,[this,guard,done,result]{busy_=false;if(guard)done(result);emit busyChanged();},Qt::QueuedConnection);
    },Qt::QueuedConnection);
}
void LocalSaveBackupService::run(std::function<SaveBackupResult()> work,QObject* context,std::function<void(SaveBackupResult)> completed) {
    if(busy_){completed({false,false,"A save operation is already running."});return;}
    busy_=true;emit busyChanged();
    QMetaObject::invokeMethod(worker_,[this,work,guard=QPointer<QObject>(context),completed]{
        const auto result=work();
        QMetaObject::invokeMethod(this,[this,guard,completed,result]{busy_=false;if(guard)completed(result);if(!result.success)emit operationFailed();emit busyChanged();},Qt::QueuedConnection);
    },Qt::QueuedConnection);
}
void LocalSaveBackupService::setReadOnly(bool value,QObject* receiver,std::function<void(QString)> done) {
    run([this,value]{const auto error=setSaveWritesReadOnly(root_,value);return SaveBackupResult{error.isEmpty(),false,error};},receiver,
        [this,done](const SaveBackupResult& result){readOnly_=saveWritesReadOnly(root_);emit policyChanged();done(result.message);});
}
void LocalSaveBackupService::inspect(const AdventureRegistration& r,QObject* context,std::function<void(SaveBackupSnapshot)> completed) {
    run([this,r]{return SaveBackupResult{true,false,{},inspectSaveBackups(root_,resolve_(r),healer_,shops_)};},context,
        [completed](const SaveBackupResult& result){if(result.success)completed(result.snapshot);else {SaveBackupSnapshot error;error.error=result.message;completed(error);}});
}
void LocalSaveBackupService::create(const AdventureRegistration& r,const QString& token,QObject* context,std::function<void(SaveBackupResult)> completed) {
    run([this,r,token]{return createSaveBackup(root_,r,token,resolve_);},context,completed);
}
void LocalSaveBackupService::restore(const AdventureRegistration& r,const SaveBackup& selected,const QString& token,QObject* context,std::function<void(SaveBackupResult)> completed) {
    run([this,r,selected,token]{return restoreSaveBackup(root_,r,selected,token,resolve_);},context,completed);
}
void LocalSaveBackupService::heal(const AdventureRegistration& r,const QString& token,QObject* context,std::function<void(SaveBackupResult)> completed) {
    run([this,r,token]{return healSaveParty(root_,r,token,resolve_,healer_);},context,completed);
}
void LocalSaveBackupService::purchase(const AdventureRegistration& r,const QString& token,const MerchantPurchase& request,QObject* context,std::function<void(SaveBackupResult)> completed) {
    run([this,r,token,request]{return purchaseSaveItems(root_,r,token,request,resolve_,buyer_,shops_);},context,completed);
}

}
