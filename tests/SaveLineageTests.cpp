#include "platform/storage/SaveLineage.h"
#include "platform/storage/SaveBackupStorage.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

using namespace trainer;
namespace {
QString sha(const QByteArray& b){return QString::fromLatin1(QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex());}
void write(const QString& path,const QByteArray& data){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(data),data.size());}
QByteArray read(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
struct Fixture {
    QTemporaryDir temp;
    QString root=temp.filePath("backups"),path=temp.filePath("save.srm");
    AdventureRegistration record;
    SaveTarget target;
    SaveTargetResolver resolve=[this](const AdventureRegistration&){return target;};
    Fixture(){record.adventure.id="adventure";record.adventure.adapterId="retroarch";record.revision=4;
        target={"adventure","Example",path,sha("rom"),sha("context"),{},true,"trainer-one"};write(path,"ORIGINAL PRIVATE SAVE");}
    SaveBackupSnapshot inspect(){return inspectSaveBackups(root,target);}
    SaveBackupResult change(const QByteArray& suffix="!") {
        return changeSaveHeldItem(root,record,inspect().token,{},resolve,[suffix](const QByteArray& b,const QString&,const HeldItemChange&){return HeldItemResult{b+suffix,{},"Changed"};});
    }
    SaveLineageProof proof(){return readSaveLineage(root,target);}
    SaveLineageStatus status(){const auto p=proof();return verifySaveLineage(p,p.publicKey,target.backupOwner,saveLineageStream(target),sha(read(path)));}
    QString directory(){return QDir(root).filePath("lineage/"+sha(target.backupOwner.toUtf8()));}
    void sql(const QString& sql) {
        const auto id=QUuid::createUuid().toString();
        {auto db=QSqlDatabase::addDatabase("QSQLITE",id);db.setDatabaseName(QDir(directory()).filePath("history.sqlite3"));QVERIFY(db.open());QSqlQuery q(db);QVERIFY2(q.exec(sql),qPrintable(sql));}
        QSqlDatabase::removeDatabase(id);
    }
};
QJsonObject entry(const SaveLineageProof& p,int i){return QJsonDocument::fromJson(p.records[i]).object();}
}
class SaveLineageTests final:public QObject {
    Q_OBJECT
private slots:
    void protectedEditRestoreAndStablePrivateIdentity() {
        Fixture f;QCOMPARE(f.status().state,LineageState::Untracked);
        const auto result=f.change();QVERIFY2(result.success,qPrintable(result.message));
        const auto proof=f.proof();QCOMPARE(proof.publicKey.size(),32);QCOMPARE(proof.records.size(),2);
        QCOMPARE(entry(proof,0)["operation"].toString(),"imported");QCOMPARE(entry(proof,1)["operation"].toString(),"held-item");
        QCOMPARE(f.status().state,LineageState::Managed);
        QCOMPARE(entry(proof,1)["beforeSha256"].toString(),sha("ORIGINAL PRIVATE SAVE"));
        QCOMPARE(entry(proof,1)["registrationRevision"].toInt(),4);
        for(const auto& payload:proof.records) {
            QVERIFY(!payload.contains(f.path.toUtf8()));QVERIFY(!payload.contains("ORIGINAL PRIVATE SAVE"));
            QCOMPARE(QJsonDocument::fromJson(payload).object()["unknownOrigin"].toBool(),true);
        }
        const auto key=read(QDir(f.directory()).filePath("identity.key"));QCOMPARE(key.size(),32);QVERIFY(key!=proof.publicKey);
        const auto restored=restoreSaveBackup(f.root,f.record,result.snapshot.copies[0],result.snapshot.token,f.resolve);
        QVERIFY2(restored.success,qPrintable(restored.message));QCOMPARE(read(f.path),QByteArray("ORIGINAL PRIVATE SAVE"));
        QCOMPARE(f.proof().publicKey,proof.publicKey);QCOMPARE(read(QDir(f.directory()).filePath("identity.key")),key);
        QCOMPARE(f.proof().records.mid(0,2),proof.records);QCOMPARE(entry(f.proof(),2)["operation"].toString(),"restore");
        QCOMPARE(f.status().count,3);QCOMPARE(f.status().state,LineageState::Managed);
    }
    void externalChangeBreaksStateParentButPreservesEarlierAudit() {
        Fixture f;QVERIFY(f.change().success);const auto original=f.proof();
        write(f.path,"EXTERNAL SAVE");QCOMPARE(f.status().state,LineageState::Changed);
        QVERIFY(f.change().success);auto proof=f.proof();QCOMPARE(proof.records.size(),4);
        QCOMPARE(proof.records.mid(0,2),original.records);
        QCOMPARE(entry(proof,2)["operation"].toString(),"external-observation");
        QVERIFY(entry(proof,2)["stateParent"].toString().isEmpty());
        QCOMPARE(entry(proof,2)["previousEntry"].toString(),sha(original.records.last()));
        QCOMPARE(entry(proof,3)["stateParent"].toString(),sha(proof.records[2]));
        // Config changes must also cut continuity even if save bytes are equal.
        f.target.contextRevision=sha("other config");QVERIFY(f.change().success);
        QCOMPARE(entry(f.proof(),4)["operation"].toString(),"external-observation");
    }
    void interruptedPreparationHasNoSuccessfulSuccessor() {
        Fixture f;const auto token=f.inspect().token;
        {SaveLineageEdit prepared(f.root,f.target,f.record,token,read(f.path),true);QVERIFY(prepared.error().isEmpty());}
        QCOMPARE(f.proof().records.size(),1);QCOMPARE(f.status().state,LineageState::Imported);
        // Model a crash after save replacement but before provenance finish.
        write(f.path,"CHANGED BEFORE CRASH");QCOMPARE(f.status().state,LineageState::Changed);
        QVERIFY(f.change().success);QCOMPARE(f.proof().records.size(),3);
        QCOMPARE(entry(f.proof(),1)["operation"].toString(),"external-observation");
    }
    void failedStaleReadOnlyAndNoOpWritesAddNoSuccess() {
        Fixture f;QVERIFY(setSaveWritesReadOnly(f.root,true).isEmpty());QVERIFY(!f.change().success);QVERIFY(f.proof().records.isEmpty());
        QVERIFY(setSaveWritesReadOnly(f.root,false).isEmpty());
        const HeldItemWriter noChange=[](const QByteArray& b,const QString&,const HeldItemChange&){return HeldItemResult{b,{},"No change"};};
        QVERIFY(changeSaveHeldItem(f.root,f.record,f.inspect().token,{},f.resolve,noChange).success);QVERIFY(f.proof().records.isEmpty());
        const HeldItemWriter race=[&](const QByteArray& b,const QString&,const HeldItemChange&){write(f.path,"GAME WRITE");return HeldItemResult{b+"!",{},"Change"};};
        QVERIFY(!changeSaveHeldItem(f.root,f.record,f.inspect().token,{},f.resolve,race).success);
        QCOMPARE(read(f.path),QByteArray("GAME WRITE"));QCOMPARE(f.proof().records.size(),1);
        QCOMPARE(entry(f.proof(),0)["operation"].toString(),"imported");
    }
    void independentProofRejectsSubstitutionReorderAndTampering() {
        Fixture f;QVERIFY(f.change().success);const auto p=f.proof();
        auto verify=[&](const SaveLineageProof& v,const QByteArray& key,const QString& owner,const QString& stream){return verifySaveLineage(v,key,owner,stream,sha(read(f.path))).state;};
        const auto stream=saveLineageStream(f.target);
        QCOMPARE(verify(p,QByteArray(32,'x'),f.target.backupOwner,stream),LineageState::Broken);
        QCOMPARE(verify(p,p.publicKey,"other trainer",stream),LineageState::Broken);
        QCOMPARE(verify(p,p.publicKey,f.target.backupOwner,sha("other stream")),LineageState::Broken);
        auto bad=p;bad.records[1][10]^=1;QCOMPARE(verify(bad,p.publicKey,f.target.backupOwner,stream),LineageState::Broken);
        bad=p;bad.signatures[1][0]^=1;QCOMPARE(verify(bad,p.publicKey,f.target.backupOwner,stream),LineageState::Broken);
        bad=p;bad.records.swapItemsAt(0,1);bad.signatures.swapItemsAt(0,1);QCOMPARE(verify(bad,p.publicKey,f.target.backupOwner,stream),LineageState::Broken);
        // A separately pinned key can verify the proof with no private key or save bytes.
        QCOMPARE(verify(p,p.publicKey,f.target.backupOwner,stream),LineageState::Managed);
    }
    void missingOrReplacedKeyNeverRegeneratesIdentity() {
        Fixture f;QVERIFY(f.change().success);const auto before=read(f.path),pub=f.proof().publicKey;
        const auto key=QDir(f.directory()).filePath("identity.key");QVERIFY(QFile::rename(key,key+".kept"));
        QVERIFY(!f.change().success);QVERIFY(!QFileInfo::exists(key));QCOMPARE(read(f.path),before);QCOMPARE(f.proof().publicKey,pub);
        write(key,QByteArray(32,'x'));QVERIFY(!f.change().success);QCOMPARE(read(f.path),before);
    }
    void databaseCorruptionAndTailDeletionAreDetected() {
        Fixture f;QVERIFY(f.change().success);const auto before=read(f.path);
        f.sql("DELETE FROM records WHERE sequence=2");QCOMPARE(f.status().state,LineageState::Broken);
        QVERIFY(!f.change().success);QCOMPARE(read(f.path),before);
    }
    void historyFailureAfterSaveCommitReportsChangedAndDoesNotForgeSuccess() {
        Fixture f;QVERIFY(f.change().success);const auto count=f.proof().records.size();
        f.sql("CREATE TRIGGER fail_history BEFORE INSERT ON records BEGIN SELECT RAISE(ABORT,'fixture'); END");
        const auto result=f.change();QVERIFY(!result.success);QVERIFY(result.restored);
        QCOMPARE(read(f.path),QByteArray("ORIGINAL PRIVATE SAVE!!"));QCOMPARE(f.proof().records.size(),count);
        QCOMPARE(f.status().state,LineageState::Changed);QVERIFY(!result.snapshot.hasSave);QVERIFY(!f.inspect().copies.isEmpty());
        f.sql("DROP TRIGGER fail_history");QVERIFY(f.change().success);
        QCOMPARE(entry(f.proof(),count)["operation"].toString(),"external-observation");
    }
    void trainersAndNamespacesRemainIndependentAndLegacyStaysUnassigned() {
        Fixture f;QVERIFY(f.change().success);const auto first=f.proof();
        f.target.backupOwner="trainer-two";QVERIFY(f.change().success);QVERIFY(f.proof().publicKey!=first.publicKey);
        f.target.backupOwner="trainer-one";f.target.adventureId="other-adventure";f.record.adventure.id="other-adventure";
        QVERIFY(f.change().success);QCOMPARE(f.proof().publicKey,first.publicKey);QCOMPARE(f.proof().records.size(),2);
        f.target.backupOwner.clear();QVERIFY(f.change().success);QVERIFY(f.proof().records.isEmpty());
    }
    void concurrentHistoryWriterIsRefused() {
        Fixture f;SaveLineageEdit first(f.root,f.target,f.record,f.inspect().token,read(f.path),true);QVERIFY(first.error().isEmpty());
        SaveLineageEdit second(f.root,f.target,f.record,f.inspect().token,read(f.path),true);QVERIFY(!second.error().isEmpty());
    }
    void knownLegacyTrainerUsesHistoryWithoutMovingHisBackupShelf() {
        Fixture f;f.target.lineageOwner=f.target.backupOwner;f.target.backupOwner.clear();
        const auto result=f.change();QVERIFY2(result.success,qPrintable(result.message));
        const auto proof=f.proof();QCOMPARE(proof.records.size(),2);
        QCOMPARE(entry(proof,1)["owner"].toString(),"trainer-one");
        QCOMPARE(entry(proof,1)["namespaceKind"].toString(),"legacy");
        const auto legacyCopy=QDir(f.root).filePath(sha(f.record.adventure.id.toUtf8())+"/"+result.snapshot.copies[0].id+".tosbackup");
        QVERIFY(QFileInfo::exists(legacyCopy));
        QVERIFY(restoreSaveBackup(f.root,f.record,result.snapshot.copies[0],result.snapshot.token,f.resolve).success);
        QCOMPARE(read(f.path),QByteArray("ORIGINAL PRIVATE SAVE"));
    }
};
QTEST_GUILESS_MAIN(SaveLineageTests)
#include "SaveLineageTests.moc"
