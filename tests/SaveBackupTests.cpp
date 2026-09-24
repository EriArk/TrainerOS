#include "platform/storage/SaveBackupStorage.h"
#include "integrations/adventure/retroarch/RetroArchSave.h"
#include "integrations/adventure/standalone/MelonDsSave.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include "core/storage/SessionState.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QLockFile>
#include <QSemaphore>
#include <QTimer>
#include <QProcess>

using namespace trainer;
namespace {
void write(const QString& path,const QByteArray& data) { QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(data),data.size()); }
QByteArray read(const QString& path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};return file.readAll();}
QString sha(const QByteArray& data){return QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex());}
struct Fixture {
    QTemporaryDir dir;
    QString root=dir.filePath("backups"),path=dir.filePath("saves/fixture.srm");
    AdventureRegistration record;
    SaveTarget target;
    SaveTargetResolver resolve=[this](const AdventureRegistration&){return target;};
    Fixture(){record.adventure.id="fixture";record.adventure.title="Original save fixture";record.adventure.worldId="hoenn";target={"fixture","Original save fixture",path,sha("ROM"),sha("context"),{},true};write(path,"FIRST SAVE");}
    SaveBackupSnapshot inspect(){return inspectSaveBackups(root,target);}
    SaveBackup backup(){const auto result=createSaveBackup(root,record,inspect().token,resolve);return result.success?result.snapshot.copies.first():SaveBackup{};}
    QString copyPath(const SaveBackup& b){return QDir(root).filePath(sha(record.adventure.id.toUtf8())+"/"+b.id+".tosbackup");}
};
}
class SaveBackupTests final : public QObject {
    Q_OBJECT
private slots:
    void shoppingWritesOneProtectedSaveAndDiscoveryIsScoped() {
        Fixture f;bool unlocked=false;
        const MerchantReader reader=[&](const QByteArray&,const QString&){MerchantSnapshot s;s.supported=true;s.lineage="trainer-id";if(unlocked){Merchant m;m.discovered=true;m.id="one";m.name="Oldale";s.merchants.append(m);}return s;};
        const MerchantBuyer buyer=[](const QByteArray&,const QString&,const MerchantPurchase& r){return r.quantity==2?MerchantWrite{"PURCHASED",{},"Bought"}:MerchantWrite{{},"Invalid quantity",{}};};
        auto before=inspectSaveBackups(f.root,f.target,{},reader);QVERIFY(before.shops.discoveryNotice.isEmpty());
        unlocked=true;auto discovered=inspectSaveBackups(f.root,f.target,{},reader);QVERIFY(discovered.shops.discoveryNotice.contains("Oldale"));
        QVERIFY(inspectSaveBackups(f.root,f.target,{},reader).shops.discoveryNotice.isEmpty());
        auto other=f.target;other.backupOwner="other-owner";QVERIFY(inspectSaveBackups(f.root,other,{},reader).shops.discoveryNotice.isEmpty());
        QVERIFY(!purchaseSaveItems(f.root,f.record,before.token,{"one",13,0},f.resolve,buyer,reader).success);QCOMPARE(read(f.path),"FIRST SAVE");
        const auto bought=purchaseSaveItems(f.root,f.record,before.token,{"one",13,2},f.resolve,buyer,reader);
        QVERIFY(bought.success);QCOMPARE(read(f.path),"PURCHASED");QCOMPARE(bought.snapshot.copies.size(),1);QCOMPARE(bought.snapshot.copies[0].reason,"purchase");
        QVERIFY(!purchaseSaveItems(f.root,f.record,before.token,{"one",13,2},f.resolve,buyer,reader).success);
        QVERIFY(restoreSaveBackup(f.root,f.record,bought.snapshot.copies[0],bought.snapshot.token,f.resolve).success);QCOMPARE(read(f.path),"FIRST SAVE");
    }
    void healingIsProtectedAndUndoRestoresExactOriginal() {
        Fixture f;
        const SaveHealer healer=[](const QByteArray&,const QString&){return SaveHealing{"HEALED SAVE",{},3};};
        auto snapshot=inspectSaveBackups(f.root,f.target,healer);QVERIFY(snapshot.canHeal);QVERIFY(snapshot.needsHealing);
        const auto done=healSaveParty(f.root,f.record,snapshot.token,f.resolve,healer);
        QVERIFY2(done.success,qPrintable(done.message));QVERIFY(done.restored);QCOMPARE(read(f.path),"HEALED SAVE");
        QCOMPARE(done.snapshot.copies.size(),1);QVERIFY(!done.snapshot.needsHealing);
        const auto protection=done.snapshot.copies.first();QCOMPARE(protection.reason,"healing");QVERIFY(protection.protection);
        const auto twice=healSaveParty(f.root,f.record,done.snapshot.token,f.resolve,healer);
        QVERIFY(twice.success);QVERIFY(!twice.restored);QCOMPARE(twice.snapshot.copies.size(),1);
        const auto undo=restoreSaveBackup(f.root,f.record,protection,twice.snapshot.token,f.resolve);
        QVERIFY(undo.success);QCOMPARE(read(f.path),"FIRST SAVE");
    }
    void healingRejectsChangedTargetsAndFailedProtection() {
        Fixture f;const auto token=f.inspect().token;
        const SaveHealer healer=[](const QByteArray&,const QString&){return SaveHealing{"HEALED SAVE",{},1};};
        write(f.path,"NEW SAVE");QVERIFY(!healSaveParty(f.root,f.record,token,f.resolve,healer).success);QCOMPARE(read(f.path),"NEW SAVE");
        auto fresh=f.inspect().token;
        auto calls=0;const auto changed=[&](const AdventureRegistration&){auto t=f.target;if(++calls>1)t.contextRevision="another-owner";return t;};
        QVERIFY(!healSaveParty(f.root,f.record,fresh,changed,healer).success);QCOMPARE(read(f.path),"NEW SAVE");
        const SaveHealer reject=[](const QByteArray&,const QString&){return SaveHealing{{},"Unsupported team"};};
        QVERIFY(!healSaveParty(f.root,f.record,fresh,f.resolve,reject).success);QCOMPARE(read(f.path),"NEW SAVE");
        const auto blocked=f.dir.filePath("blocked");write(blocked,"NOT A FOLDER");
        QVERIFY(!healSaveParty(blocked,f.record,fresh,f.resolve,healer).success);QCOMPARE(read(f.path),"NEW SAVE");
        QLockFile lock(QDir(f.root).filePath("service.lock"));QVERIFY(lock.tryLock(0));
        QVERIFY(!healSaveParty(f.root,f.record,fresh,f.resolve,healer).success);QCOMPARE(read(f.path),"NEW SAVE");
    }
    void melonDsSaveRequiresVerifiedSingleInstanceLayout() {
        QTemporaryDir dir; AdventureRegistration record;
        record.adventure.id = "ds-fixture"; record.adventure.adapterId = "melonds"; record.adventure.platformId = "nds";
        record.contentPath = dir.filePath("roms/original.v2.nds"); write(record.contentPath, "ORIGINAL DS FIXTURE");
        StandaloneInstallation installation;
        installation.program = QCoreApplication::applicationFilePath(); installation.runtimeFile = dir.filePath("runtime");
        installation.configFile = dir.filePath("melonDS.toml"); installation.platforms = {"nds"}; installation.melonDsSaveBackups = true;
        write(installation.runtimeFile, "runtime fixture");
        const QByteArray config = "[Instance0]\nSaveFilePath = \"" + dir.filePath("saves").toUtf8()
            + "\"\n[Savestate]\nRelocSRAM = false\n[Emu]\nConsoleType = 0\n";
        write(installation.configFile, config);
        const auto target = resolveMelonDsSave(record, installation);
        QVERIFY2(target.supported, qPrintable(target.error));
        QCOMPARE(target.savePath, dir.filePath("saves/original.v2.sav"));
        QCOMPARE(target.contentRevision, sha("ORIGINAL DS FIXTURE"));
        const QList<QByteArray> invalid{
            QByteArray(config).replace("RelocSRAM = false", "RelocSRAM = true"),
            QByteArray(config).replace("ConsoleType = 0", "ConsoleType = 1"),
            QByteArray(config).replace("RelocSRAM = false", "RelocSRAM = false\nRelocSRAM = true"),
            QByteArray(config).replace("RelocSRAM = false", ""),
            QByteArray(config).replace(dir.filePath("saves").toUtf8(), "relative"),
            config + "[Instance1]\nSaveFilePath = \"/somewhere\"\n",
            config + "[Savestate]\nRelocSRAM = false\n"};
        for (const auto& bytes : invalid) { write(installation.configFile, bytes); QVERIFY(!resolveMelonDsSave(record, installation).supported); }
        write(installation.configFile, config); installation.melonDsSaveBackups = false;
        QVERIFY(!resolveMelonDsSave(record, installation).supported); installation.melonDsSaveBackups = true;
        record.adventure.platformId = "dsi"; QVERIFY(!resolveMelonDsSave(record, installation).supported); record.adventure.platformId = "nds";
        const auto resolve = [&](const AdventureRegistration& r) { return resolveMelonDsSave(r, installation); };
        const auto root = dir.filePath("backups"); write(target.savePath, "DS SAVE ONE");
        auto snapshot = inspectSaveBackups(root, resolve(record));
        const auto backup = createSaveBackup(root, record, snapshot.token, resolve); QVERIFY(backup.success);
        write(target.savePath, "DS SAVE TWO"); snapshot = inspectSaveBackups(root, resolve(record));
        const auto restored = restoreSaveBackup(root, record, backup.snapshot.copies.first(), snapshot.token, resolve);
        QVERIFY2(restored.success, qPrintable(restored.message)); QCOMPARE(read(target.savePath), "DS SAVE ONE");
        QCOMPARE(restored.snapshot.copies.size(), 2);
#ifdef Q_OS_LINUX
        const auto executable = dir.filePath("melonDS.AppImage"); QVERIFY(QFile::copy("/bin/sleep", executable));
        QProcess process; process.start(executable, {"30"}); QVERIFY(process.waitForStarted());
        const auto running = resolve(record); process.kill(); process.waitForFinished();
        QVERIFY(!running.supported); QVERIFY(running.error.contains("Close the running"));
#endif
    }
    void restoreProtectsCurrentBytesAndAllowsUndo() {
        Fixture f; const auto first=f.backup(); QVERIFY(first.valid); QCOMPARE(read(f.path),QByteArray("FIRST SAVE"));
        write(f.path,"SECOND SAVE");const auto second=f.inspect();
        auto result=restoreSaveBackup(f.root,f.record,first,second.token,f.resolve);
        QVERIFY2(result.success,qPrintable(result.message));QVERIFY(result.restored);QCOMPARE(read(f.path),QByteArray("FIRST SAVE"));QCOMPARE(result.snapshot.copies.size(),2);
        SaveBackup protection;for(const auto& copy:result.snapshot.copies)if(copy.protection)protection=copy;
        QVERIFY(protection.valid&&protection.hasSave);
        result=restoreSaveBackup(f.root,f.record,protection,result.snapshot.token,f.resolve);
        QVERIFY(result.success);QCOMPARE(read(f.path),QByteArray("SECOND SAVE"));QCOMPARE(result.snapshot.copies.size(),3);
    }
    void staleCurrentCorruptCopyAndDifferentContentNeverReplace() {
        Fixture f;const auto first=f.backup();QVERIFY(first.valid);const auto token=f.inspect().token;
        write(f.path,"KEEP THIS SAVE");
        QVERIFY(!restoreSaveBackup(f.root,f.record,first,token,f.resolve).success);QCOMPARE(f.inspect().copies.size(),1);
        auto changed=first;changed.revision="different";QVERIFY(!restoreSaveBackup(f.root,f.record,changed,f.inspect().token,f.resolve).success);
        f.target.contentRevision=sha("different ROM");QVERIFY(!restoreSaveBackup(f.root,f.record,first,f.inspect().token,f.resolve).success);f.target.contentRevision=sha("ROM");
        write(f.copyPath(first),read(f.copyPath(first))+"broken");
        QVERIFY(!f.inspect().copies.first().valid);QVERIFY(!restoreSaveBackup(f.root,f.record,first,f.inspect().token,f.resolve).success);
        QCOMPARE(read(f.path),QByteArray("KEEP THIS SAVE"));
    }
    void missingSaveCanBeRecoveredWithoutInventingDeletionRestore() {
        Fixture f;const auto first=f.backup();QVERIFY(QFile::remove(f.path));const auto missing=f.inspect();QVERIFY(!missing.hasSave);QVERIFY(!missing.token.isEmpty());
        QVERIFY(!createSaveBackup(f.root,f.record,missing.token,f.resolve).success);
        const auto result=restoreSaveBackup(f.root,f.record,first,missing.token,f.resolve);QVERIFY(result.success);QCOMPARE(read(f.path),QByteArray("FIRST SAVE"));
        for(const auto& copy:result.snapshot.copies)if(!copy.hasSave){QVERIFY(copy.protection);QVERIFY(!restoreSaveBackup(f.root,f.record,copy,result.snapshot.token,f.resolve).success);}
        QCOMPARE(read(f.path),QByteArray("FIRST SAVE"));
    }
    void emptyDamagedSaveIsProtectedBeforeRecovery() {
        Fixture f;const auto first=f.backup();write(f.path,{});const auto empty=f.inspect();
        QVERIFY(!empty.hasSave);QVERIFY(!empty.token.isEmpty());
        const auto result=restoreSaveBackup(f.root,f.record,first,empty.token,f.resolve);QVERIFY(result.success);QCOMPARE(read(f.path),QByteArray("FIRST SAVE"));
        for(const auto& copy:result.snapshot.copies)if(copy.protection){QVERIFY(copy.valid&&copy.hasSave);QCOMPARE(copy.bytes,0);QVERIFY(!restoreSaveBackup(f.root,f.record,copy,result.snapshot.token,f.resolve).success);}
    }
    void failedProtectionAndConcurrentOperationLeaveSaveUntouched() {
        Fixture f;const auto first=f.backup();write(f.path,"CURRENT");const auto token=f.inspect().token;
        { QLockFile lock(QDir(f.root).filePath("service.lock"));QVERIFY(lock.tryLock(0));QVERIFY(!restoreSaveBackup(f.root,f.record,first,token,f.resolve).success); }
        const auto folder=QFileInfo(f.copyPath(first)).absolutePath();
        for(int i=0;i<511;++i)write(QDir(folder).filePath(QString::number(i)+".tosbackup"),"unreadable fixture");
        const auto result=restoreSaveBackup(f.root,f.record,first,token,f.resolve);QVERIFY(!result.success);QVERIFY(!result.restored);QCOMPARE(read(f.path),QByteArray("CURRENT"));
    }
    void changedTargetDuringPreparationCancelsReplacement() {
        Fixture f;const auto first=f.backup();write(f.path,"CURRENT");const auto token=f.inspect().token;int calls=0;
        const auto resolve=[&](const AdventureRegistration&){auto target=f.target;if(++calls>1)target.contextRevision="changed";return target;};
        const auto result=restoreSaveBackup(f.root,f.record,first,token,resolve);QVERIFY(!result.success);QCOMPARE(read(f.path),QByteArray("CURRENT"));QCOMPARE(f.inspect().copies.size(),2);
    }
    void linksAndMissingFoldersAreRejected() {
        Fixture f;auto target=f.target;target.savePath=f.dir.filePath("absent/save.srm");QVERIFY(inspectSaveBackups(f.root,target).token.isEmpty());
#ifdef Q_OS_UNIX
        const auto linked=f.dir.filePath("linked.srm");QVERIFY(QFile::link(f.path,linked));target.savePath=linked;QVERIFY(inspectSaveBackups(f.root,target).token.isEmpty());
        QVERIFY(QDir().mkpath(f.dir.filePath("other")));QVERIFY(QFile::link(f.dir.filePath("other"),f.root));
        QVERIFY(!createSaveBackup(f.root,f.record,f.inspect().token,f.resolve).success);QVERIFY(!QFileInfo::exists(f.dir.filePath("other/service.lock")));
#endif
    }
    void workerKeepsControllerResponsiveAndExitWaitsForCompletion() {
        Fixture f;QSemaphore entered,release;
        LocalSaveBackupService service(f.root,[&](const AdventureRegistration& r){entered.release();release.acquire();return f.resolve(r);},[](const AdventureRegistration&){return true;});
        struct ReleaseOnExit { QSemaphore& semaphore; ~ReleaseOnExit(){semaphore.release();} } unblock{release};
        MockLibraryRepository library;MockTrainerRepository profile;MockAdventureAdapter adapter;DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profile,adapter,platform,dex,dex,archive,achievements);SessionState session(shell,nullptr);
        connect(&service,&SaveBackupService::busyChanged,&session,[&]{session.setServiceActive(service.busy());});
        QSignalSpy exit(&session,&SessionState::exitReady);bool completed=false;
        service.inspect(f.record,this,[&](const SaveBackupSnapshot& snapshot){QVERIFY(snapshot.hasSave);completed=true;});
        QTRY_VERIFY(entered.available()>0);QVERIFY(service.busy());session.requestExit();QVERIFY(session.blocked());QCOMPARE(exit.size(),0);
        int ticks=0;QTimer timer;connect(&timer,&QTimer::timeout,this,[&]{++ticks;});timer.start(5);QTest::qWait(30);QVERIFY(ticks>0);
        release.release();QTRY_VERIFY(completed);QTRY_COMPARE(exit.size(),1);QVERIFY(!service.busy());
        session.cancelPendingExit();session.setServiceActive(true);session.requestExit();session.cancelPendingExit();session.setServiceActive(false);QCOMPARE(exit.size(),1);
    }
    void trainerSaveRoutesPreserveLegacyAndSeparateLaunchReadsAndBackups() {
        QTemporaryDir dir;AdventureRegistration r;r.adventure.id="same-adventure";
        r.adventure.adapterId="retroarch";r.integrationConfig={{"core","mgba"}};
        r.contentPath=dir.filePath("roms/game.gba");write(r.contentPath,"ROM");
        RetroArchInstallation i;i.program=QCoreApplication::applicationFilePath();i.saveBackups=true;
        i.configFile=dir.filePath("retroarch.cfg");i.runtimeFile=dir.filePath("runtime");i.cores={{"mgba",dir.filePath("core")}};
        write(i.runtimeFile,"RUNTIME");write(i.cores["mgba"],"CORE");
        const QByteArray config="savefile_directory = \""+dir.filePath("legacy").toUtf8()+"\"\nsavefiles_in_content_dir = \"false\"\nsort_savefiles_enable = \"true\"\nsort_savefiles_by_content_enable = \"true\"\nauto_overrides_enable = \"false\"\n";
        write(i.configFile,config);
        const auto legacy=resolveRetroArchSave(r,i);QVERIFY(legacy.supported);write(legacy.savePath,"ORIGINAL PLAYER");
        const auto root=dir.filePath("backups");const auto legacyResolve=[&](const auto& a){return resolveRetroArchSave(a,i);};
        const auto saved=createSaveBackup(root,r,inspectSaveBackups(root,legacy).token,legacyResolve);QVERIFY(saved.success);
        i.saves=std::make_shared<RetroArchSaveSession>();
        QVERIFY(!resolveRetroArchSave(r,i).supported); // No pre-unlock fallback to the shared save.
        const auto data=dir.filePath("data");QVERIFY(i.saves->bind({"original",data,true}));
        const auto original=resolveRetroArchSave(r,i);QCOMPARE(original.savePath,legacy.savePath);
        QCOMPARE(original.contextRevision,legacy.contextRevision);QCOMPARE(inspectSaveBackups(root,original).copies.size(),1);
        QVERIFY(!i.saves->bind({"child",data,false})); // A worker session cannot be rebound across owners.
        auto child=i;child.saves=std::make_shared<RetroArchSaveSession>();QVERIFY(child.saves->bind({"child",data,false}));
        const auto target=resolveRetroArchSave(r,child);QVERIFY2(target.supported,qPrintable(target.error));
        QVERIFY(target.savePath!=legacy.savePath);QVERIFY(!QFileInfo::exists(target.savePath));
        QVERIFY(inspectSaveBackups(root,target).copies.isEmpty());QVERIFY(inspectSaveBackups(root,target).error.isEmpty());
        ProcessCommand cmd{i.program,{r.contentPath},{}};std::atomic_bool cancelled{false};
        QVERIFY2(prepareRetroArchLaunch(cmd,r,child,cancelled).isEmpty(),"owned launch");
        const auto launchConfig=read(cmd.arguments[cmd.arguments.size()-2]);
        QVERIFY(launchConfig.contains(QFileInfo(target.savePath).absolutePath().toUtf8()));
        QVERIFY(launchConfig.contains("sort_savefiles_enable = \"false\""));
        QVERIFY(launchConfig.contains("savefiles_in_content_dir = \"false\""));
        QCOMPARE(read(legacy.savePath),QByteArray("ORIGINAL PLAYER"));QCOMPARE(read(i.configFile),config);
        write(target.savePath,"CHILD SAVE");const auto childResolve=[&](const auto& a){return resolveRetroArchSave(a,child);};
        auto snap=inspectSaveBackups(root,target);QVERIFY(snap.hasSave);QVERIFY(snap.copies.isEmpty());
        QVERIFY(!restoreSaveBackup(root,r,saved.snapshot.copies.first(),snap.token,childResolve).success);
        const auto copy=createSaveBackup(root,r,snap.token,childResolve);QVERIFY(copy.success);
        write(target.savePath,"CHILD LATER");snap=inspectSaveBackups(root,target);
        QVERIFY(restoreSaveBackup(root,r,copy.snapshot.copies.first(),snap.token,childResolve).success);
        QCOMPARE(read(target.savePath),QByteArray("CHILD SAVE"));QCOMPARE(read(legacy.savePath),QByteArray("ORIGINAL PLAYER"));
        auto second=child;second.saves=std::make_shared<RetroArchSaveSession>();QVERIFY(second.saves->bind({"second",data,false}));
        const auto other=resolveRetroArchSave(r,second);QVERIFY(other.supported);QVERIFY(other.savePath!=target.savePath);
        QVERIFY(inspectSaveBackups(root,other).copies.isEmpty());
        auto another=r;another.adventure.id="different-record";QVERIFY(resolveRetroArchSave(another,child).savePath!=target.savePath);
        write(r.contentPath,"DIFFERENT BUILD");QVERIFY(resolveRetroArchSave(r,child).savePath!=target.savePath);write(r.contentPath,"ROM");
        for(const auto& flag:QStringList{"--save", "-s"+legacy.savePath, "--save="+legacy.savePath, "--appendconfig=other.cfg"}) { child.prefixArguments={flag};QVERIFY(!resolveRetroArchSave(r,child).supported); }
        child.prefixArguments.clear();
        write(QFileInfo(target.savePath).dir().filePath("traineros-owner-v1.cfg"),"OTHER CONFIG");
        cmd.arguments={r.contentPath};QVERIFY(!prepareRetroArchLaunch(cmd,r,child,cancelled).isEmpty());
        QCOMPARE(read(legacy.savePath),QByteArray("ORIGINAL PLAYER"));
#ifdef Q_OS_UNIX
        QVERIFY(QFile::remove(target.savePath));QVERIFY(QFile::link(legacy.savePath,target.savePath));
        QVERIFY(!resolveRetroArchSave(r,child).supported); // Never follow another player's save.
#endif
    }
    void verifiedRetroArchLayoutIsExplicitAndRejectsAmbiguity() {
        QTemporaryDir dir;AdventureRegistration record;record.adventure.id="fixture";record.adventure.adapterId="retroarch";record.contentPath=dir.filePath("content/original.gba");record.integrationConfig={{"core","mgba"}};
        RetroArchInstallation installation;installation.program=QCoreApplication::applicationFilePath();installation.configFile=dir.filePath("retroarch.cfg");installation.cores={{"mgba",dir.filePath("mgba.so")}};installation.runtimeFile=dir.filePath("runtime");installation.resumeDirectory=dir.filePath("moments");installation.saveBackups=true;
        write(record.contentPath,"ROM");write(installation.cores["mgba"],"core");write(installation.runtimeFile,"runtime");
        const auto config="savefile_directory = \""+dir.filePath("saves").toUtf8()+"\"\nsavefiles_in_content_dir = \"false\"\nsort_savefiles_enable = \"true\"\nsort_savefiles_by_content_enable = \"true\"\nauto_overrides_enable = \"false\"\n";
        write(installation.configFile,config);const auto target=resolveRetroArchSave(record,installation);QVERIFY2(target.supported,qPrintable(target.error));QCOMPARE(target.savePath,dir.filePath("saves/content/mGBA/original.srm"));QCOMPARE(target.contentRevision,sha("ROM"));
        // Retiring the legacy state directory leaves ordinary-save identity stable.
        installation.resumeDirectory.clear();
        const auto independent = resolveRetroArchSave(record, installation);
        QVERIFY(independent.supported); QCOMPARE(independent.savePath, target.savePath);
        QCOMPARE(independent.contextRevision, target.contextRevision);
        write(target.savePath, "ORDINARY SAVE");
        const auto shelf = dir.filePath("backups");
        auto oldTarget = target; oldTarget.contextRevision = sha("old entry-state setup");
        const auto oldResolver = [&](const AdventureRegistration&) { return oldTarget; };
        auto oldSnapshot = inspectSaveBackups(shelf, oldTarget);
        const auto oldBackup = createSaveBackup(shelf, record, oldSnapshot.token, oldResolver);
        QVERIFY(oldBackup.success);
        const auto resolver = [&](const AdventureRegistration& r) { return resolveRetroArchSave(r, installation); };
        auto newSnapshot = inspectSaveBackups(shelf, resolver(record));
        QVERIFY(newSnapshot.copies.first().valid); // Existing bundles bind to content, not the ephemeral context token.
        QVERIFY(!createSaveBackup(shelf, record, oldSnapshot.token, resolver).success);
        write(target.savePath, "NEWER SAVE");
        newSnapshot = inspectSaveBackups(shelf, resolver(record));
        QVERIFY(restoreSaveBackup(shelf, record, oldBackup.snapshot.copies.first(), newSnapshot.token, resolver).success);
        QCOMPARE(read(target.savePath), QByteArray("ORDINARY SAVE"));
        write(installation.runtimeFile, "changed runtime");
        QVERIFY(resolveRetroArchSave(record, installation).contextRevision != independent.contextRevision);
        QVERIFY(QFile::remove(installation.runtimeFile));
        QVERIFY(!resolveRetroArchSave(record, installation).supported);
        write(installation.runtimeFile, "runtime");
        installation.saveBackups=false;QVERIFY(!resolveRetroArchSave(record,installation).supported);installation.saveBackups=true;
        write(installation.configFile,config+"sort_savefiles_enable = \"false\"\n");QVERIFY(!resolveRetroArchSave(record,installation).supported);
        write(installation.configFile,config);record.integrationConfig={{"core","gambatte"}};QVERIFY(!resolveRetroArchSave(record,installation).supported);
#ifdef Q_OS_LINUX
        record.integrationConfig={{"core","mgba"}};const auto executable=dir.filePath("retroarch");QVERIFY(QFile::copy("/bin/sleep",executable));
        QProcess process;process.start(executable,{"30"});QVERIFY(process.waitForStarted());
        const auto running=resolveRetroArchSave(record,installation);process.kill();process.waitForFinished();
        QVERIFY(!running.supported);QVERIFY(running.error.contains("Close the running"));
#endif
    }
};
QTEST_GUILESS_MAIN(SaveBackupTests)
#include "SaveBackupTests.moc"
