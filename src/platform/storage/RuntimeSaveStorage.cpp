#include "RuntimeSaveStorage.h"
#include "LinkSaveStore.h"
#include "SaveLineage.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QUuid>
#ifdef Q_OS_UNIX
#include <unistd.h>
#include <fcntl.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#endif

namespace trainer {
namespace {
constexpr qint64 Limit=64*1024*1024;
QString hash(const QByteArray& bytes) {return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
bool sync(QFileDevice& file) {
    if(!file.flush())return false;
#ifdef Q_OS_UNIX
    return ::fsync(file.handle())==0;
#elif defined(Q_OS_WIN)
    return ::_commit(file.handle())==0;
#else
    return false;
#endif
}
bool syncDirectory(const QString& path) {
#ifdef Q_OS_UNIX
    const int fd=::open(QFile::encodeName(path).constData(),O_RDONLY|O_DIRECTORY);
    if(fd<0)return false;
    const bool ok=::fsync(fd)==0;::close(fd);return ok;
#else
    Q_UNUSED(path);return true;
#endif
}
bool rootReady(const QString& root) {
    return QDir::isAbsolutePath(root)&&!QFileInfo(root).isSymLink()&&QDir().mkpath(root)
        &&!QFileInfo(root).canonicalFilePath().isEmpty();
}
RuntimeSaveSeed read(const SaveTarget& target) {
    RuntimeSaveSeed seed;seed.target=target;
    seed.error="Couldn't verify your game save. The original file was kept.";
    const QFileInfo info(target.savePath);
    const auto parent=info.dir().canonicalPath();
    if(!target.supported||!target.error.isEmpty()||target.adventureId.isEmpty()
       ||target.contentRevision.size()!=64||target.contextRevision.isEmpty()
       ||!info.isAbsolute()||parent.isEmpty()||info.isSymLink())return seed;
    seed.existed=info.exists();
    if(seed.existed) {
        if(!info.isFile()||info.size()<1||info.size()>Limit)return seed;
        QFile file(target.savePath);
        if(!file.open(QIODevice::ReadOnly))return seed;
        seed.bytes=file.read(Limit+1);
        if(file.error()!=QFile::NoError||seed.bytes.size()!=info.size())return seed;
    }
    const QJsonObject identity{{"adventure",target.adventureId},{"content",target.contentRevision},
        {"context",target.contextRevision},{"owner",target.backupOwner},{"lineageOwner",target.lineageOwner},
        {"path",QDir(parent).filePath(info.fileName())},{"exists",seed.existed},{"sha256",hash(seed.bytes)}};
    seed.token=hash(QJsonDocument(identity).toJson(QJsonDocument::Compact));seed.error.clear();return seed;
}
bool writeNew(const QString& path,const QByteArray& bytes) {
    if(QFileInfo::exists(path)||QFileInfo(path).isSymLink())return false;
    QSaveFile file(path);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)||!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)
       ||file.write(bytes)!=bytes.size()||!sync(file)||!file.commit())return false;
    QFile check(path);
    return check.open(QIODevice::ReadOnly)&&check.size()==bytes.size()&&hash(check.read(Limit+1))==hash(bytes);
}
QString protect(const QString& root,const RuntimeSaveSeed& seed,const QByteArray& after) {
    const auto parent=QDir(root).filePath("runtime-copies");
    if(QFileInfo(parent).isSymLink()||!QDir().mkpath(parent)
       ||QFileInfo(parent).dir().canonicalPath()!=QFileInfo(root).canonicalFilePath())return {};
    const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces),folder=QDir(parent).filePath(id);
    if(!QDir().mkdir(folder)||!QFile::setPermissions(folder,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner))return {};
    if((seed.existed&&!writeNew(QDir(folder).filePath("before.sav"),seed.bytes))
       ||!writeNew(QDir(folder).filePath("returned.sav"),after))return {};
    const QJsonObject manifest{{"version",1},{"adventure",seed.target.adventureId},{"content",seed.target.contentRevision},
        {"owner",seed.target.backupOwner},{"sourceToken",seed.token},{"existed",seed.existed},
        {"beforeBytes",seed.bytes.size()},{"beforeSha256",hash(seed.bytes)},
        {"returnedBytes",after.size()},{"returnedSha256",hash(after)}};
    if(!writeNew(QDir(folder).filePath("receipt.json"),QJsonDocument(manifest).toJson(QJsonDocument::Compact))
       ||!syncDirectory(folder)||!syncDirectory(parent)||!syncDirectory(root)
       ||!syncDirectory(QFileInfo(root).dir().absolutePath()))return {};
    return "runtime-copies/"+id;
}
}
RuntimeSaveSeed captureRuntimeSave(const QString& root,const AdventureRegistration& record,const SaveTargetResolver& resolve) {
    RuntimeSaveSeed failed;failed.error="Another save operation is running, or save storage is unavailable.";
    if(!resolve||!rootReady(root))return failed;
    QLockFile lock(QDir(root).filePath("service.lock"));
    if(!lock.tryLock(0)||pendingLinkSave(root))return failed;
    auto seed=read(resolve(record));
    if(seed.error.isEmpty()&&read(resolve(record)).token!=seed.token)seed.error="Your save changed. Invite again after saving.";
    return seed;
}
QString returnRuntimeSave(const QString& root,const AdventureRegistration& record,const RuntimeSaveSeed& seed,
                         const QByteArray& after,const SaveTargetResolver& resolve) {
    const QString kept="Your original save was kept. The multiplayer copy remains in the session folder.";
    if(!resolve||!seed.error.isEmpty()||seed.token.isEmpty()||after.isEmpty()||after.size()>Limit||!rootReady(root))return kept;
    QLockFile lock(QDir(root).filePath("service.lock"));
    if(!lock.tryLock(0)||pendingLinkSave(root))return kept;
    if(seed.existed&&after==seed.bytes)return {};
    if(saveWritesReadOnly(root))return "Read-only saves is on. Your multiplayer copy remains in the session folder.";
    const auto current=read(resolve(record));
    if(!current.error.isEmpty()||current.token!=seed.token||(seed.existed&&after.size()!=seed.bytes.size()))return kept;
    const auto protection=protect(root,seed,after);
    if(protection.isEmpty())return "Couldn't keep a verified protection copy. Your original save and session folder were kept.";
    SaveLineageEdit lineage(root,current.target,record,seed.token,seed.bytes,seed.existed);
    if(!lineage.error().isEmpty())return kept;
    QSaveFile file(current.target.savePath);file.setDirectWriteFallback(false);
    if(!file.open(QIODevice::WriteOnly)||!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)
       ||file.write(after)!=after.size()||!sync(file))return kept;
    if(read(resolve(record)).token!=seed.token||saveWritesReadOnly(root)){file.cancelWriting();return kept;}
    if(!file.commit())return kept;
    const auto verified=read(resolve(record));
    if(!syncDirectory(QFileInfo(current.target.savePath).dir().canonicalPath())||!verified.error.isEmpty()||verified.bytes!=after)
        return "The save was replaced, but verification failed. Keep the protected copies and check storage.";
    if(!lineage.finish(after,"runtime-return",protection).isEmpty())
        return "The save was returned, but its history could not be recorded. Protected copies were kept.";
    return {};
}
}
