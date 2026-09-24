#include "SaveBackupStorage.h"
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
SaveBackupResult createSaveBackup(const QString& root, const AdventureRegistration& record, const QString& token, const SaveTargetResolver& resolve) {
    if (!QDir::isAbsolutePath(root) || QFileInfo(root).isSymLink() || !QDir().mkpath(root))return {false,false,"Couldn't open the backup folder."};
    QLockFile lock(QDir(root).filePath("service.lock")); if(!lock.tryLock(0))return {false,false,"Another backup operation is running. Try again shortly."};
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
    if(!QDir::isAbsolutePath(root) || QFileInfo(root).isSymLink() || !validId(selected.id) || selected.revision.isEmpty())return {false,false,"Choose a saved copy again."};
    QLockFile lock(QDir(root).filePath("service.lock")); if(!lock.tryLock(0))return {false,false,"Another backup operation is running. Try again shortly."};
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
    if(current(resolve(record)).revision!=token)return {false,false,"The save changed during preparation. Restore was cancelled; the protection copy was kept."};
    QSaveFile file(target.savePath); file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly) || file.write(copy.data)!=copy.data.size() || !syncFile(file))
        return {false,false,"Couldn't prepare the restored save. Your current save and protection copy were kept."};
    if(current(resolve(record)).revision!=token) { file.cancelWriting(); return {false,false,"The save changed during preparation. Check again before restoring."}; }
    if(!file.commit())return {false,false,"Couldn't replace the save. The protection copy is available in Pokémon Center."};
    const bool synced=syncDirectory(save.parent);
    const auto after=current(target);
    if(!synced || !after.success || after.data!=copy.data)return {false,true,"The save was replaced, but storage verification failed. Keep the protection copy and check the storage device."};
    return {true,true,"Save restored. Open the Adventure normally to use it.",inspectSaveBackups(root,resolve(record))};
}
static SaveBackupResult applySaveEdit(const QString& root, const AdventureRegistration& record, const QString& token,
        const SaveTargetResolver& resolve, const std::function<MerchantWrite(const QByteArray&,const QString&)>& edit,
        const QString& reason, const SaveHealer& healer, const MerchantReader& shops) {
    if (!edit) return {false,false,"This service is not available for this Adventure."};
    if (!QDir::isAbsolutePath(root) || QFileInfo(root).isSymLink() || !QDir().mkpath(root))
        return {false,false,"Couldn't open the backup folder. Your save is unchanged."};
    QLockFile lock(QDir(root).filePath("service.lock"));
    if (!lock.tryLock(0)) return {false,false,"Another save operation is running."};
    const auto target=resolve(record); if (!target.supported) return {false,false,problem(target)};
    const auto save=current(target);
    if (!save.success || !save.exists || token.isEmpty() || save.revision!=token)
        return {false,false,"The save changed. Visit the Center again before confirming."};
    const auto treatment=edit(save.data,target.contentRevision);
    if (!treatment.error.isEmpty() || treatment.data.isEmpty()) return {false,false,treatment.error};
    if (treatment.data==save.data) return {true,false,treatment.message,inspectSaveBackups(root,target,healer,shops)};
    if (writeBundle(root,target,save,true,reason).isEmpty())
        return {false,false,"Couldn't protect your save. The change was cancelled."};
    if (current(resolve(record)).revision!=token)
        return {false,false,"The save changed. The change was cancelled; your backup was kept."};
    QSaveFile file(target.savePath); file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileInfo(target.savePath).permissions())
        || file.write(treatment.data)!=treatment.data.size() || !syncFile(file))
        return {false,false,"Couldn't prepare the change. Your save and backup were kept."};
    if (current(resolve(record)).revision!=token) {
        file.cancelWriting(); return {false,false,"The save changed before confirmation. Please try again."};
    }
    if (!file.commit()) return {false,false,"Couldn't replace the save. Your backup is available in Center."};
    const bool synced=syncDirectory(save.parent);
    const auto after=current(target);
    if (!synced || !after.success || after.data!=treatment.data)
        return {false,true,"The change was written, but storage verification failed. Keep the backup and check storage."};
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
LocalSaveBackupService::LocalSaveBackupService(QString root, SaveTargetResolver resolve,
        std::function<bool(const AdventureRegistration&)> supports, QObject* parent)
    : SaveBackupService(parent),root_(std::move(root)),resolve_(std::move(resolve)),supports_(std::move(supports)),worker_(new QObject) {
    worker_->moveToThread(&thread_); connect(&thread_,&QThread::finished,worker_,&QObject::deleteLater); thread_.start();
}
LocalSaveBackupService::~LocalSaveBackupService(){thread_.quit();thread_.wait();}
void LocalSaveBackupService::run(std::function<SaveBackupResult()> work,QObject* context,std::function<void(SaveBackupResult)> completed) {
    if(busy_){completed({false,false,"A save operation is already running."});return;}
    busy_=true;emit busyChanged();
    QMetaObject::invokeMethod(worker_,[this,work,guard=QPointer<QObject>(context),completed]{
        const auto result=work();
        QMetaObject::invokeMethod(this,[this,guard,completed,result]{busy_=false;if(guard)completed(result);if(!result.success)emit operationFailed();emit busyChanged();},Qt::QueuedConnection);
    },Qt::QueuedConnection);
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
