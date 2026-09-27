#include "SaveLineage.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <memory>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif

namespace trainer {
namespace {
constexpr int RecordLimit = 16384, PayloadLimit = 4096;
const QByteArray Domain("TrainerOS/save-lineage/1\n");
QString digest(const QByteArray& b) { return QString::fromLatin1(QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex()); }
bool hashLike(const QString& s) { return s.size()==64 && QByteArray::fromHex(s.toLatin1()).toHex()==s.toLatin1(); }
QString problem() { return "Save history needs attention. Your save and backups have been kept."; }
QString ownerOf(const SaveTarget& t) { return t.lineageOwner.isEmpty()?t.backupOwner:t.lineageOwner; }
QString directory(const QString& root,const QString& owner) { return QDir(root).filePath("lineage/"+digest(owner.toUtf8())); }
bool syncDirectory(const QString& path) {
#ifdef Q_OS_UNIX
    const int fd=::open(QFile::encodeName(path).constData(),O_RDONLY|O_DIRECTORY);
    if(fd<0)return false;
    const bool ok=::fsync(fd)==0;::close(fd);return ok;
#else
    Q_UNUSED(path);return true;
#endif
}
bool safeDirectory(const QString& root,const QString& owner,bool create) {
    if(!QDir::isAbsolutePath(root) || owner.isEmpty() || owner.size()>128 || QFileInfo(root).isSymLink())return false;
    for(const auto& p:{root,QDir(root).filePath("lineage"),directory(root,owner)}) {
        if(QFileInfo(p).isSymLink() || (create && !QDir().mkpath(p)) || !QFileInfo(p).isDir())return false;
    }
    const auto dir=directory(root,owner);
    if(create && !QFile::setPermissions(dir,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner))return false;
    for(const auto& suffix:{"history.sqlite3","history.sqlite3-journal","history.sqlite3-wal","history.sqlite3-shm","identity.key","history.lock"})
        if(QFileInfo(QDir(dir).filePath(suffix)).isSymLink())return false;
    return true;
}
using Key = std::unique_ptr<EVP_PKEY,decltype(&EVP_PKEY_free)>;
using Context = std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>;
QByteArray publicKey(EVP_PKEY* key) {
    QByteArray result(32,0);size_t n=32;
    if(!key || EVP_PKEY_get_raw_public_key(key,reinterpret_cast<unsigned char*>(result.data()),&n)!=1 || n!=32)return {};
    return result;
}
QByteArray sign(EVP_PKEY* key,const QByteArray& record) {
    const auto data=Domain+record;QByteArray signature(64,0);size_t n=64;
    Context ctx(EVP_MD_CTX_new(),EVP_MD_CTX_free);
    if(!ctx || EVP_DigestSignInit(ctx.get(),nullptr,nullptr,nullptr,key)!=1
       || EVP_DigestSign(ctx.get(),reinterpret_cast<unsigned char*>(signature.data()),&n,
            reinterpret_cast<const unsigned char*>(data.constData()),data.size())!=1 || n!=64)return {};
    return signature;
}
bool signatureValid(const QByteArray& pub,const QByteArray& record,const QByteArray& signature) {
    if(pub.size()!=32 || signature.size()!=64)return false;
    Key key(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519,nullptr,reinterpret_cast<const unsigned char*>(pub.constData()),32),EVP_PKEY_free);
    Context ctx(EVP_MD_CTX_new(),EVP_MD_CTX_free);const auto data=Domain+record;
    return key && ctx && EVP_DigestVerifyInit(ctx.get(),nullptr,nullptr,nullptr,key.get())==1
        && EVP_DigestVerify(ctx.get(),reinterpret_cast<const unsigned char*>(signature.constData()),signature.size(),
            reinterpret_cast<const unsigned char*>(data.constData()),data.size())==1;
}
struct Database {
    QString name=QUuid::createUuid().toString();
    QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE",name);
    ~Database(){db.close();db={};QSqlDatabase::removeDatabase(name);}
    bool open(const QString& path,bool write) {
        db.setDatabaseName(path);
        db.setConnectOptions(write?"QSQLITE_BUSY_TIMEOUT=1000":"QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1000");
        if(!db.open())return false;
        QSqlQuery q(db);
        if(!q.exec("PRAGMA user_version") || !q.next())return false;
        const int version=q.value(0).toInt();q.finish();
        if(!write)return version==1;
        if(!q.exec("PRAGMA journal_mode=DELETE") || !q.exec("PRAGMA synchronous=EXTRA"))return false;
        if(version==1)return true;
        if(version!=0 || !q.exec("SELECT count(*) FROM sqlite_master") || !q.next() || q.value(0).toInt()!=0)return false;
        q.finish();
        if(!db.transaction())return false;
        if(!q.exec("CREATE TABLE identity(owner TEXT PRIMARY KEY NOT NULL,public_key BLOB NOT NULL)")
           || !q.exec("CREATE TABLE records(stream TEXT NOT NULL,sequence INTEGER NOT NULL,payload BLOB NOT NULL,signature BLOB NOT NULL,PRIMARY KEY(stream,sequence))")
           || !q.exec("CREATE TABLE heads(stream TEXT PRIMARY KEY NOT NULL,sequence INTEGER NOT NULL,hash TEXT NOT NULL)")
           || !q.exec("PRAGMA user_version=1") || !db.commit())return false;
        return true;
    }
};
SaveLineageProof load(QSqlDatabase& db,const QString& owner,const QString& stream) {
    SaveLineageProof proof;QSqlQuery q(db);
    q.prepare("SELECT public_key FROM identity WHERE owner=?");q.addBindValue(owner);
    if(!q.exec() || !q.next()){proof.error=problem();return proof;}
    proof.publicKey=q.value(0).toByteArray();q.finish();
    q.prepare("SELECT sequence,payload,signature FROM records WHERE stream=? ORDER BY sequence LIMIT ?");
    q.addBindValue(stream);q.addBindValue(RecordLimit+1);
    if(!q.exec()){proof.error=problem();return proof;}
    while(q.next()) {
        const auto bytes=q.value(1).toByteArray(),signature=q.value(2).toByteArray();
        if(proof.records.size()>=RecordLimit || q.value(0).toInt()!=proof.records.size()+1 || bytes.size()>PayloadLimit || signature.size()!=64){proof.error=problem();break;}
        proof.records.append(bytes);proof.signatures.append(signature);
    }
    q.finish();q.prepare("SELECT sequence,hash FROM heads WHERE stream=?");q.addBindValue(stream);
    if(!q.exec()){proof.error=problem();return proof;}
    if(q.next()) {
        if(proof.records.isEmpty() || q.value(0).toInt()!=proof.records.size() || q.value(1).toString()!=digest(proof.records.last()))proof.error=problem();
    } else if(!proof.records.isEmpty())proof.error=problem();
    return proof;
}
}

QString saveLineageStream(const SaveTarget& t) {
    if(ownerOf(t).isEmpty() || t.adventureId.isEmpty() || !hashLike(t.contentRevision) || !QDir::isAbsolutePath(t.savePath))return {};
    // No path leaves the host. Context/config revisions are signed per event,
    // rather than silently creating a new stream whenever a setting changes.
    return digest(QJsonDocument(QJsonObject{{"owner",ownerOf(t)},{"adventure",t.adventureId},
        {"build",t.contentRevision},{"namespace",QDir::cleanPath(t.savePath)}}).toJson(QJsonDocument::Compact));
}
SaveLineageProof readSaveLineage(const QString& root,const SaveTarget& target) {
    SaveLineageProof proof;
    if(ownerOf(target).isEmpty())return proof;
    const auto dir=directory(root,ownerOf(target));
    if(!QFileInfo::exists(dir) && !QFileInfo(dir).isSymLink())return proof;
    if(!safeDirectory(root,ownerOf(target),false)){proof.error=problem();return proof;}
    Database d;
    if(!d.open(QDir(dir).filePath("history.sqlite3"),false)){proof.error=problem();return proof;}
    return load(d.db,ownerOf(target),saveLineageStream(target));
}
SaveLineageStatus verifySaveLineage(const SaveLineageProof& proof,const QByteArray& expectedKey,
        const QString& owner,const QString& stream,const QString& currentHash) {
    SaveLineageStatus status;
    auto broken=[&]{status.state=LineageState::Broken;status.error=problem();return status;};
    if(!proof.error.isEmpty() || proof.records.size()!=proof.signatures.size() || proof.records.size()>RecordLimit)return broken();
    if(proof.records.isEmpty())return status;
    if(expectedKey.size()!=32 || expectedKey!=proof.publicKey || !hashLike(stream) || owner.isEmpty())return broken();
    QString lastHash,lastContext,adventure,build,pendingSession;
    for(int i=0;i<proof.records.size();++i) {
        const auto& bytes=proof.records[i];
        if(bytes.size()>PayloadLimit || !signatureValid(expectedKey,bytes,proof.signatures[i]))return broken();
        const auto o=QJsonDocument::fromJson(bytes).object();
        const auto op=o["operation"].toString(),save=o["saveSha256"].toString();
        const bool session=op.startsWith("session-");
        const bool begin=op=="session-start", completed=op=="session-completed";
        const bool interrupted=QStringList{"session-interrupted","session-failed","session-cancelled"}.contains(op);
        const bool observation=op=="imported" || op=="external-observation" || interrupted;
        const auto sessionId=o["session"].toString();
        if(o["version"].toInt()!=1 || o["sequence"].toInt()!=i+1 || o["owner"].toString()!=owner
           || o["stream"].toString()!=stream || o["previousEntry"].toString()!=status.head
           || o["unknownOrigin"]!=QJsonValue(true) || o["assurance"].toString()!="software-local"
           || (!hashLike(save) && save!="absent") || !hashLike(o["sourceToken"].toString())
           || o["context"].toString().isEmpty()
           || !QStringList{"legacy","trainer"}.contains(o["namespaceKind"].toString())
           || o["writer"].toString()!=(session?"traineros-session/1":observation?"save-observer/1":"traineros-protected-"+op+"/1")
           || !QDateTime::fromString(o["observedAt"].toString(),Qt::ISODateWithMs).isValid())return broken();
        if(i==0) {
            if(op!="imported")return broken();
            adventure=o["adventure"].toString();build=o["build"].toString();
            if(adventure.isEmpty() || !hashLike(build))return broken();
        } else if(o["adventure"].toString()!=adventure || o["build"].toString()!=build || op=="imported")return broken();
        if(session) {
            if(QUuid(sessionId).isNull() || (!begin && !completed && !interrupted))return broken();
            if(begin) {
                if(!pendingSession.isEmpty() || i==0 || save!=lastHash)return broken();
                pendingSession=sessionId;
            } else {
                if(pendingSession!=sessionId)return broken();
                pendingSession.clear();
                if(op!="session-interrupted") {
                    if(!o["started"].isBool() || !o["crashed"].isBool() || !o["stopped"].isBool() || !o["exitCode"].isDouble())return broken();
                    const bool clean=o["started"].toBool() && o["exitCode"].toInt()==0 && !o["crashed"].toBool() && !o["stopped"].toBool();
                    if(completed!=clean || (op=="session-cancelled" && (o["started"].toBool() || !o["stopped"].toBool())))return broken();
                }
            }
        } else if(!pendingSession.isEmpty())return broken();
        if(observation) {
            if(!o["stateParent"].toString().isEmpty() || !o["beforeSha256"].toString().isEmpty())return broken();
            status.state=op=="imported"?LineageState::Imported:LineageState::Changed;
        } else {
            if(i==0 || (!hashLike(save) && !(session && save=="absent"))
               || (!session && !QStringList{"healing","purchase","movement","release","held-item","box-name","restore"}.contains(op))
               || o["stateParent"].toString()!=status.head || o["beforeSha256"].toString()!=lastHash
               || o["context"].toString()!=lastContext || (!session && o["protection"].toString().isEmpty()))return broken();
            status.state=begin?LineageState::Running:LineageState::Managed;
        }
        lastHash=save;lastContext=o["context"].toString();status.head=digest(bytes);status.saveHash=save;++status.count;
    }
    if(status.saveHash!=currentHash)status.state=LineageState::Changed;
    return status;
}

struct SaveLineageEdit::Impl {
    Database database;
    std::unique_ptr<QLockFile> lock;
    Key key{nullptr,EVP_PKEY_free};
    SaveTarget target;AdventureRegistration record;
    QString stream,sourceToken,beforeHash,head,error,sessionId,sessionBefore;
    int sequence=0;
    bool enabled=false,finished=false;
    bool append(const QString& save,const QString& operation,const QString& protection,bool observation,const QJsonObject& extra = {}) {
        if(sequence>=RecordLimit){error=problem();return false;}
        QJsonObject object{{"version",1},{"sequence",sequence+1},{"owner",ownerOf(target)},
            {"adventure",target.adventureId},{"build",target.contentRevision},{"stream",stream},
            {"namespaceKind",target.backupOwner.isEmpty()?"legacy":"trainer"},
            {"context",target.contextRevision},{"sourceToken",sourceToken},{"registrationRevision",record.revision},
            {"runtime",record.adventure.adapterId},{"writer",observation?"save-observer/1":"traineros-protected-"+operation+"/1"},
            {"operation",operation},{"protection",protection},{"saveSha256",save},
            {"beforeSha256",observation?QString():beforeHash},{"previousEntry",head},
            {"stateParent",observation?QString():head},{"unknownOrigin",true},{"assurance","software-local"},
            {"observedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        for(auto it=extra.begin();it!=extra.end();++it)object.insert(it.key(),it.value());
        const auto bytes=QJsonDocument(object).toJson(QJsonDocument::Compact),signature=sign(key.get(),bytes);
        if(bytes.size()>PayloadLimit || !signatureValid(publicKey(key.get()),bytes,signature) || !database.db.transaction()){error=problem();return false;}
        QSqlQuery q(database.db);q.prepare("INSERT INTO records(stream,sequence,payload,signature) VALUES(?,?,?,?)");
        q.addBindValue(stream);q.addBindValue(sequence+1);q.addBindValue(bytes);q.addBindValue(signature);
        if(!q.exec()){database.db.rollback();error=problem();return false;}
        q.prepare("INSERT INTO heads(stream,sequence,hash) VALUES(?,?,?) ON CONFLICT(stream) DO UPDATE SET sequence=excluded.sequence,hash=excluded.hash");
        q.addBindValue(stream);q.addBindValue(sequence+1);q.addBindValue(digest(bytes));
        if(!q.exec() || !database.db.commit()){database.db.rollback();error=problem();return false;}
        head=digest(bytes);++sequence;return true;
    }
};

SaveLineageEdit::SaveLineageEdit(const QString& root,const SaveTarget& target,const AdventureRegistration& record,
        const QString& token,const QByteArray& before,bool existed,const QString& endingSession):impl_(std::make_unique<Impl>()) {
    auto& p=*impl_;p.target=target;p.record=record;p.sourceToken=token;p.beforeHash=existed?digest(before):"absent";
    // Unbound routes have no proven Trainer. Never infer one from a save or PIN.
    const auto owner=ownerOf(target);
    if(owner.isEmpty())return;
    p.enabled=true;p.error=problem();p.stream=saveLineageStream(target);
    if(p.stream.isEmpty() || !hashLike(token) || !safeDirectory(root,owner,true))return;
    const auto dir=directory(root,owner);
    p.lock=std::make_unique<QLockFile>(QDir(dir).filePath("history.lock"));
    if(!p.lock->tryLock(0) || !safeDirectory(root,owner,false))return;
    const auto path=QDir(dir).filePath("history.sqlite3");
    if(!p.database.open(path,true) || !QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner))return;
    QSqlQuery q(p.database.db);q.prepare("SELECT owner,public_key FROM identity");
    if(!q.exec())return;
    QByteArray knownPublic;
    if(q.next()){if(q.value(0).toString()!=owner)return;knownPublic=q.value(1).toByteArray();if(knownPublic.size()!=32 || q.next())return;}
    q.finish();
    const auto keyPath=QDir(dir).filePath("identity.key");QFile secret(keyPath);
    if(secret.exists()) {
        if(!secret.open(QIODevice::ReadOnly) || secret.size()!=32)return;
        auto raw=secret.readAll();secret.close();
        p.key.reset(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519,nullptr,reinterpret_cast<const unsigned char*>(raw.constData()),raw.size()));
        OPENSSL_cleanse(raw.data(),raw.size());
    } else {
        // Missing keys are never silently replaced when an identity already exists.
        if(!knownPublic.isEmpty())return;
        p.key.reset(EVP_PKEY_Q_keygen(nullptr,nullptr,"ED25519"));
        QByteArray raw(32,0);size_t length=32;
        if(!p.key || EVP_PKEY_get_raw_private_key(p.key.get(),reinterpret_cast<unsigned char*>(raw.data()),&length)!=1 || length!=32){OPENSSL_cleanse(raw.data(),raw.size());return;}
        QSaveFile output(keyPath);output.setDirectWriteFallback(false);
        bool ok=output.open(QIODevice::WriteOnly) && output.setPermissions(QFile::ReadOwner|QFile::WriteOwner) && output.write(raw)==raw.size() && output.flush();
        OPENSSL_cleanse(raw.data(),raw.size());
#ifdef Q_OS_UNIX
        if(ok)ok=::fsync(output.handle())==0;
#endif
        if(!ok || !output.commit() || !syncDirectory(dir))return;
    }
    const auto pub=publicKey(p.key.get());
    if(pub.size()!=32 || (!knownPublic.isEmpty() && knownPublic!=pub))return;
    if(knownPublic.isEmpty()) {
        if(!q.exec("SELECT count(*) FROM records") || !q.next() || q.value(0).toInt()!=0)return;
        q.finish();q.prepare("INSERT INTO identity(owner,public_key) VALUES(?,?)");q.addBindValue(owner);q.addBindValue(pub);
        if(!q.exec())return;
    }
    if(!syncDirectory(dir) || !syncDirectory(QFileInfo(dir).absolutePath()) || !syncDirectory(root))return;
    const auto proof=load(p.database.db,owner,p.stream);
    const auto status=verifySaveLineage(proof,pub,owner,p.stream,p.beforeHash);
    if(status.state==LineageState::Broken)return;
    p.head=status.head;p.sequence=status.count;p.error.clear();
    const bool changedContext=!proof.records.isEmpty() && QJsonDocument::fromJson(proof.records.last()).object()["context"].toString()!=target.contextRevision;
    const auto last=proof.records.isEmpty()?QJsonObject():QJsonDocument::fromJson(proof.records.last()).object();
    const bool pending=last["operation"].toString()=="session-start";
    if(!endingSession.isEmpty()) {
        if(!pending || last["session"].toString()!=endingSession || changedContext){p.error=problem();return;}
        p.sessionId=endingSession;p.sessionBefore=status.saveHash;
    } else if(pending) {
        p.append(p.beforeHash,"session-interrupted",{},true,{{"session",last["session"]},{"writer","traineros-session/1"}});
    } else if(!p.sequence)p.append(p.beforeHash,"imported",{},true);
    else if(status.saveHash!=p.beforeHash || changedContext)p.append(p.beforeHash,"external-observation",{},true);
    if(p.sequence>=RecordLimit)p.error=problem(); // Reserve the successor before touching the save.
}
SaveLineageEdit::~SaveLineageEdit()=default;
QString SaveLineageEdit::error() const { return impl_->error; }
QString SaveLineageEdit::finish(const QByteArray& after,const QString& operation,const QString& protectionId) {
    auto& p=*impl_;
    if(!p.enabled)return {};
    if(!p.error.isEmpty() || p.finished || protectionId.isEmpty()
       || !QStringList{"healing","purchase","movement","release","held-item","box-name","restore"}.contains(operation))return problem();
    p.finished=true;
    return p.append(digest(after),operation,protectionId,false)?QString():problem();
}
}

namespace trainer {
QString SaveLineageEdit::beginSession() {
    auto& p=*impl_;
    if(!p.enabled || !p.error.isEmpty() || p.finished || !p.sessionId.isEmpty())return {};
    if(p.sequence+2>RecordLimit){p.error=problem();return {};}
    p.sessionId=QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.finished=true;
    return p.append(p.beforeHash,"session-start",{},false,{{"session",p.sessionId},{"writer","traineros-session/1"}})?p.sessionId:QString();
}
QString SaveLineageEdit::finishSession(bool started,int exitCode,bool crashed,bool stopped) {
    auto& p=*impl_;
    if(!p.enabled || !p.error.isEmpty() || p.finished || p.sessionId.isEmpty())return problem();
    p.finished=true;
    const bool clean=started && exitCode==0 && !crashed && !stopped;
    const auto operation=clean?"session-completed":!started && stopped?"session-cancelled":"session-failed";
    const auto after=p.beforeHash;p.beforeHash=p.sessionBefore;
    return p.append(after,operation,{},!clean,{{"session",p.sessionId},{"writer","traineros-session/1"},
        {"started",started},{"exitCode",exitCode},{"crashed",crashed},{"stopped",stopped}})?QString():problem();
}
}
