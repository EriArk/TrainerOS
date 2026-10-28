#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSysInfo>
#include <QSaveFile>
#include <QSettings>
#include "features/crates/CratesController.h"
#include "fixtures/crates/CatalogueFixture.h"
using namespace trainer;
class CratesTests: public QObject {
    Q_OBJECT
    QTemporaryDir settingsDirectory_;
private slots:
    void initTestCase(){
        QVERIFY(settingsDirectory_.isValid());QCoreApplication::setOrganizationName("TrainerOSCratesTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDirectory_.path());
    }
    void switchingShellSectionsKeepsCrateAndProjectButDismissesActions(){
        CratesController c;QVERIFY(c.loadCatalogue(cratesCatalogueFixture()));
        c.open(0,false);c.leave(true);QVERIFY(c.opened());
        c.openProject(0);QVERIFY(c.detailOpen());const auto selected=c.project();
        c.showActions();c.leave(true);c.enter();QVERIFY(c.detailOpen());QCOMPARE(c.project(),selected);
        c.dispatch(Action::Back);QVERIFY(!c.detailOpen());QVERIFY(c.opened());
        c.openProject(0);c.leave();QVERIFY(!c.detailOpen());
    }
    void enabledUpdatesCheckInBackgroundWithoutOpeningCrates(){
#ifndef Q_OS_UNIX
        QSKIP("Real catalogue helper uses the Linux Python interpreter");
#else
        QTemporaryDir temp;QVERIFY(temp.isValid());QDir root(temp.path());QVERIFY(root.mkpath("active"));
        const auto write=[&](QString path,QByteArray data){QFile f(root.filePath(path));return f.open(QIODevice::WriteOnly)&&f.write(data)==data.size();};
        QVERIFY(write("active/preview-0.json","{\"id\":\"preview-0\",\"version\":\"1\"}"));
        QVERIFY(write("helper.py","import sys\nprint(open(sys.argv[2]+'/response.json').read())\n"));
        auto arch=QSysInfo::currentCpuArchitecture();if(arch=="arm64")arch="aarch64";
        CratesController c;
        const QJsonObject response{{"crates",QJsonArray::fromVariantList(c.crates())},
            {"catalogue",QJsonDocument::fromJson(cratesCatalogueFixture()).object()},
            {"packages",QJsonArray{QJsonObject{{"id","preview-0"},{"version","2"},{"architecture",arch},{"compatibility","supported"}}}}};
        QVERIFY(write("response.json",QJsonDocument(response).toJson()));
        c.installer()->replacementGuard=[]{return QString("Running session");};
        c.installer()->configure("/not-executed",{},temp.path());
        c.configureCatalogue(root.filePath("helper.py"),temp.path());
        QVERIFY(!c.automaticUpdates());QVERIFY(c.installer()->tasks().isEmpty());
        QSignalSpy downloads(&c,&CratesController::downloadsRequested);
        c.setAutomaticUpdates(true);
        QTRY_COMPARE(c.installer()->tasks().size(),1);QVERIFY(!c.opened());QCOMPARE(downloads.count(),0);
        QCOMPARE(c.installer()->tasks()[0].toMap()["state"].toString(),QString("queued"));
        QVERIFY(QSettings().value("crates/automaticUpdates").toBool());
        {CratesController restored;restored.configureCatalogue({},temp.path());QVERIFY(restored.automaticUpdates());}
        c.setAutomaticUpdates(false);
        QCOMPARE(c.installer()->tasks()[0].toMap()["state"].toString(),QString("cancelled"));
        {CratesController restored;restored.configureCatalogue({},temp.path());QVERIFY(!restored.automaticUpdates());}
#endif
    }
    void updatePrimaryAndAutomaticQueueRespectInstalledState() {
        QTemporaryDir temp;QVERIFY(temp.isValid());QDir root(temp.path());QVERIFY(root.mkpath("active"));
        const auto write=[&](QString id,QString version){QSaveFile f(root.filePath("active/"+id+".json"));
            if(!f.open(QIODevice::WriteOnly))return false;
            f.write(QJsonDocument(QJsonObject{{"id",id},{"version",version},{"kind","game"}}).toJson());return f.commit();};
        QVERIFY(write("preview-0","1.0"));QVERIFY(write("preview-1","9.0"));
        QVERIFY(write("preview-2","1.0"));QVERIFY(write("preview-3","1.0"));
        auto arch=QSysInfo::currentCpuArchitecture();if(arch=="arm64")arch="aarch64";
        const auto record=[&](QString id,QString compatibility){return QVariantMap{{"id",id},{"version","2.0"},
            {"architecture",arch},{"compatibility",compatibility},{"sha256",QString(64,'b')}};};
        CratesController c;QVERIFY(c.loadCatalogue(cratesCatalogueFixture()));
        auto* jobs=c.installer();jobs->replacementGuard=[]{return QString("Game is running");};
        jobs->configure("/not-executed",{},temp.path());
        auto withdrawn=record("preview-3","supported");withdrawn["withdrawn"]=true;
        jobs->useCatalogue({record("preview-0","supported"),record("preview-1","supported"),
            record("preview-2","untested"),withdrawn,record("not-installed","supported")},{});
        c.open(1,true);c.openProject(0);QCOMPARE(c.primaryLabel(),QString("Update"));
        for(const auto& action:c.actions())QVERIFY(action.toMap()["id"]!="versions");
        QSignalSpy launch(&c,&CratesController::launchRequested);c.primary();
        QCOMPARE(launch.count(),0);QCOMPARE(jobs->tasks().size(),1);
        QCOMPARE(c.primaryLabel(),QString("View progress"));
        jobs->command("preview-0","cancel");jobs->queueAutomaticUpdates();
        QCOMPARE(jobs->tasks().size(),1);QCOMPARE(jobs->tasks()[0].toMap()["state"].toString(),QString("cancelled"));
        // A new release may be scheduled; cancelled, untested, withdrawn,
        // uninstalled and newer local packages are never silently replaced.
        auto next=record("preview-0","supported");next["version"]="3.0";
        jobs->useCatalogue({next,record("preview-1","supported"),record("preview-2","untested"),withdrawn},{});
        jobs->setOnline(false);jobs->queueAutomaticUpdates();
        QCOMPARE(jobs->tasks()[0].toMap()["state"].toString(),QString("cancelled"));
        jobs->setOnline(true);jobs->queueAutomaticUpdates();jobs->queueAutomaticUpdates();
        QCOMPARE(jobs->tasks().size(),1);QCOMPARE(jobs->tasks()[0].toMap()["version"].toString(),QString("3.0"));
        QVERIFY(jobs->tasks()[0].toMap()["automatic"].toBool());
        QCOMPARE(jobs->tasks()[0].toMap()["state"].toString(),QString("queued"));
        jobs->command("preview-0","pause");jobs->queueAutomaticUpdates();
        QCOMPARE(jobs->tasks()[0].toMap()["state"].toString(),QString("paused"));
        jobs->stopAutomaticUpdates();QCOMPARE(jobs->tasks()[0].toMap()["state"].toString(),QString("cancelled"));
        jobs->queueAutomaticUpdates();QCOMPARE(jobs->tasks()[0].toMap()["state"].toString(),QString("queued"));
        jobs->command("preview-0","cancel");
        // Activation returns the primary action to Play, with no version picker.
        QVERIFY(write("preview-0","3.0"));QTRY_COMPARE(c.primaryLabel(),QString("Play"));
        c.primary();QCOMPARE(launch.count(),1);
        c.closeDetail();c.openProject(1);QCOMPARE(c.primaryLabel(),QString("Play"));
        QVERIFY(!jobs->state("preview-1")["updateAvailable"].toBool());
        const auto count=jobs->tasks().size();
        c.activateAction(0);QCOMPARE(jobs->tasks().size(),count); // No hidden destructive action.
        c.dispatch(Action::ContextMenu);QVERIFY(c.removePrompt());QCOMPARE(c.actionIndex(),1);
        c.dispatch(Action::Confirm);QVERIFY(!c.actionsOpen());QCOMPARE(jobs->tasks().size(),count);
        c.showActions();c.dispatch(Action::Back);QVERIFY(!c.removePrompt());QVERIFY(c.detailOpen());
        c.showActions();c.dispatch(Action::Up);c.dispatch(Action::Confirm);
        QCOMPARE(jobs->state("preview-1")["job"].toMap()["operation"].toString(),QString("remove"));
        QVERIFY(!c.canRemove());c.showActions();QVERIFY(!c.actionsOpen());
    }
    void restartSettlesCommittedOperationsAndPreservesQueue() {
        QTemporaryDir temp;QDir root(temp.path());QVERIFY(root.mkpath("active"));
        const auto write=[&](QString path,QByteArray bytes){QFile f(root.filePath(path));return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();};
        const QString sha(64,'a');QVariantList tasks;
        for(const auto& id:{"installed","old","removed","retained","queued","paused"}){
            tasks<<QVariantMap{{"id",id},{"version","2"},{"sha256",sha},{"operation",QString(id)=="removed"||QString(id)=="retained"?"remove":"install"},
                {"state",QString(id)=="queued"?"queued":QString(id)=="paused"?"paused":"running"},{"terminal",false}};
        }
        for(const auto& id:{"installed","old","retained"})
            QVERIFY(write(QString("active/")+id+".json",QJsonDocument(QJsonObject{{"id",id},{"version",QString(id)=="installed"?"2":"1"},{"sha256",sha}}).toJson()));
        QVERIFY(write("jobs.json",QJsonDocument(QJsonArray::fromVariantList(tasks)).toJson()));
        CratesInstaller jobs;jobs.setOnline(false);jobs.configure({}, {},temp.path());
        const auto state=[&](QString id){return jobs.state(id)["job"].toMap()["state"].toString();};
        QCOMPARE(state("installed"),"done");QCOMPARE(state("removed"),"done");
        QCOMPARE(state("old"),"failed");QCOMPARE(state("retained"),"failed");
        QCOMPARE(state("queued"),"queued");QCOMPARE(state("paused"),"paused");
    }
    void startupRecoveryRunsOfflineAndWaitsForLease() {
#ifndef Q_OS_LINUX
        QSKIP("Production worker requires Linux flock");
#else
        const auto helper=QFINDTESTDATA("../packaging/crates/installer.py");QVERIFY(!helper.isEmpty());
        QTemporaryDir temp;QDir root(temp.path());QVERIFY(root.mkpath("active"));QVERIFY(root.mkpath("transactions"));QVERIFY(root.mkpath("locks"));
        const auto write=[&](QString path,QJsonDocument doc){QFile f(root.filePath(path));return f.open(QIODevice::WriteOnly)&&f.write(doc.toJson())>0;};
        const QString sha(64,'a'),object="fixture-"+sha;
        QVERIFY(root.mkpath("objects/"+object));
        QVERIFY(write("active/fixture.json",QJsonDocument(QJsonObject{{"id","fixture"},{"version","2"},{"kind","game"},{"sha256",sha},{"object",object}})));
        QVERIFY(write("transactions/fixture.json",QJsonDocument(QJsonObject{{"operation","install"},{"newObject",object}})));
        QVERIFY(write("jobs.json",QJsonDocument(QJsonArray{QJsonObject{{"id","fixture"},{"operation","install"},{"version","2"},{"sha256",sha},{"state","running"},{"terminal",false}}})));
        QProcess lease;lease.start("/usr/bin/python3",{"-c","import fcntl,sys,time\nf=open(sys.argv[1],'a');fcntl.flock(f,fcntl.LOCK_SH);print('locked',flush=True);time.sleep(20)",root.filePath("locks/fixture")});
        QVERIFY(lease.waitForReadyRead());QCOMPARE(lease.readLine().trimmed(),QByteArray("locked"));
        CratesInstaller jobs;jobs.setOnline(false);jobs.configure(helper,"missing-offline-registry",temp.path());
        QTest::qWait(350);QCOMPARE(jobs.state("fixture")["job"].toMap()["state"].toString(),"recovering");
        QVERIFY(QFileInfo::exists(root.filePath("transactions/fixture.json")));
        lease.terminate();QVERIFY(lease.waitForFinished());
        QTRY_COMPARE(jobs.state("fixture")["job"].toMap()["state"].toString(),"done");
        QVERIFY(!QFileInfo::exists(root.filePath("transactions/fixture.json")));
        QCOMPARE(jobs.state("fixture")["installedVersion"].toString(),"2");
#endif
    }
    void failedRemovalRecoveryCanBeRetriedWithoutInstalledRecordOrNetwork() {
#ifndef Q_OS_LINUX
        QSKIP("Production worker requires Linux");
#else
        const auto helper=QFINDTESTDATA("../packaging/crates/installer.py");QVERIFY(!helper.isEmpty());
        QTemporaryDir temp;QDir root(temp.path());QVERIFY(root.mkpath("transactions"));
        const auto write=[&](QString path,QByteArray bytes){QFile f(root.filePath(path));return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();};
        QVERIFY(write("transactions/fixture.json","{\"operation\":\"invalid\"}"));
        QVERIFY(write("jobs.json","[{\"id\":\"fixture\",\"operation\":\"remove\",\"state\":\"running\",\"terminal\":false}]"));
        CratesInstaller jobs;jobs.setOnline(false);jobs.configure(helper,{},temp.path());
        QTRY_COMPARE(jobs.state("fixture")["job"].toMap()["state"].toString(),"failed");
        QVERIFY(jobs.installed().isEmpty());
        QVERIFY(write("transactions/fixture.json","{\"operation\":\"remove\"}"));
        jobs.command("fixture","retry");
        QTRY_COMPARE(jobs.state("fixture")["job"].toMap()["state"].toString(),"done");
        QVERIFY(!QFileInfo::exists(root.filePath("transactions/fixture.json")));
#endif
    }
    void automaticUpdateFailureSurvivesRestart() {
        QTemporaryDir temp;QVERIFY(temp.isValid());QDir root(temp.path());QVERIFY(root.mkpath("active"));
        QFile receipt(root.filePath("active/game.json"));QVERIFY(receipt.open(QIODevice::WriteOnly));
        receipt.write("{\"id\":\"game\",\"version\":\"1\"}");receipt.close();
        auto arch=QSysInfo::currentCpuArchitecture();if(arch=="arm64")arch="aarch64";
        const QVariantMap release{{"id","game"},{"version","2"},{"architecture",arch},{"compatibility","supported"},{"sha256",QString(64,'c')}};
        {CratesInstaller jobs;jobs.configure("/not-executed",{},temp.path());jobs.useCatalogue({release},{});
            jobs.queueAutomaticUpdates();QTRY_VERIFY(jobs.tasks().value(0).toMap()["terminal"].toBool());
            QCOMPARE(jobs.tasks()[0].toMap()["state"].toString(),QString("failed"));}
        CratesInstaller restored;restored.configure("/not-executed",{},temp.path());restored.useCatalogue({release},{});
        restored.queueAutomaticUpdates();QCOMPARE(restored.tasks().size(),1);
        QCOMPARE(restored.tasks()[0].toMap()["state"].toString(),QString("failed"));
    }
    void queuePriorityPauseAndPersistentOrder() {
        QTemporaryDir temp;QVERIFY(temp.isValid());
        {CratesInstaller jobs;jobs.configure("/not-executed",{},temp.path());jobs.replacementGuard=[]{return QString("Held for queue test");};
            QVariantList records;auto arch=QSysInfo::currentCpuArchitecture();if(arch=="arm64")arch="aarch64";
            for(const auto& id:{"one","two","three"})records<<QVariantMap{{"id",id},{"title",id},{"architecture",arch},{"version","1"}};
            jobs.useCatalogue(records,{});for(const auto& id:{"one","two","three"})jobs.request(id);
            jobs.moveBefore("three","one");QCOMPARE(jobs.tasks()[0].toMap()["id"].toString(),"three");
            jobs.command("two","first");QCOMPARE(jobs.tasks()[0].toMap()["id"].toString(),"two");
            jobs.command("two","pause");QCOMPARE(jobs.tasks()[0].toMap()["state"].toString(),"paused");
            jobs.command("three","cancel");jobs.moveBefore("one","three"); // Terminal targets cannot reorder.
            QCOMPARE(jobs.tasks()[1].toMap()["id"].toString(),"three");
            jobs.command("one","pause");
        }
        CratesInstaller restored;restored.configure("/not-executed",{},temp.path());
        QCOMPARE(restored.tasks()[0].toMap()["id"].toString(),"two");QCOMPARE(restored.tasks()[0].toMap()["state"].toString(),"paused");
        QVERIFY(!restored.tasks()[0].toMap()["terminal"].toBool());
    }
    void externalAtomicUpgradeRefreshesLobbyInventory() {
        QTemporaryDir temp;QVERIFY(temp.isValid());QDir root(temp.path());
        CratesInstaller installer;installer.configure({}, {},temp.path());
        QSignalSpy updated(&installer,&CratesInstaller::inventoryChanged);
        auto activate=[&](QString project,QString protocol,QChar digit){
            const QString sha(64,digit),object=project+"-"+sha;
            if(!root.mkpath("objects/"+object)||!root.mkpath("active"))return false;
            QFile descriptor(root.filePath("objects/"+object+"/traineros-activity.json"));
            if(!descriptor.open(QIODevice::WriteOnly))return false;
            descriptor.write(QJsonDocument(QJsonObject{{"schema",1},{"app","org.test."+project},{"protocol",protocol},{"seats",2}}).toJson());descriptor.close();
            QSaveFile receipt(root.filePath("active/"+project+".json"));
            if(!receipt.open(QIODevice::WriteOnly))return false;
            receipt.write(QJsonDocument(QJsonObject{{"id",project},{"kind","game"},{"title",project},{"sha256",sha},{"object",object}}).toJson());
            return receipt.commit();
        };
        QVERIFY(activate("golf","game.5",'a'));
        QTRY_COMPARE(installer.activities().size(),1);
        QCOMPARE(installer.activities()[0].toMap()["protocol"].toString(),QString("game.5"));
        QVERIFY(activate("golf","game.6",'b'));
        QTRY_COMPARE(installer.activities()[0].toMap()["protocol"].toString(),QString("game.6"));
        QVERIFY(activate("arcade","arcade.2",'c'));
        QTRY_COMPARE(installer.activities().size(),2);
        QVERIFY(QFile::remove(root.filePath("active/golf.json")));
        QTRY_COMPARE(installer.activities().size(),1);
        QCOMPARE(installer.activities()[0].toMap()["app"].toString(),QString("org.test.arcade"));
        QVERIFY(updated.size()>=4);
        const auto count=updated.size();QFile unrelated(root.filePath("unrelated.txt"));
        QVERIFY(unrelated.open(QIODevice::WriteOnly));unrelated.write("unchanged inventory");unrelated.close();
        QTest::qWait(250);QCOMPARE(updated.size(),count);
    }
    void installedActivityDoesNotRequireRunningGame() {
        QTemporaryDir temp;QVERIFY(temp.isValid());QDir root(temp.path());
        QVERIFY(root.mkpath("active"));
        const QString sha(64,'a'),object="fixture-"+sha;
        QVERIFY(root.mkpath("objects/"+object));
        auto write=[&](QString name,QJsonObject data){QFile f(root.filePath(name));if(!f.open(QIODevice::WriteOnly))return false;return f.write(QJsonDocument(data).toJson())>0;};
        const QJsonObject receipt{{"id","fixture"},{"kind","game"},{"title","Fixture"},{"sha256",sha},{"object",object}};
        QVERIFY(write("active/fixture.json",receipt));
        const QJsonObject activity{{"schema",1},{"app","org.test.game"},{"protocol","game.1"},{"seats",2}};
        QVERIFY(write("objects/"+object+"/traineros-activity.json",activity));
        CratesInstaller installer;installer.configure({}, {},temp.path());
        QCOMPARE(installer.activities().size(),1);
        QCOMPARE(installer.activities()[0].toMap()["local"].toString(),QString("crates:fixture"));
        auto group=activity;group["seats"]=16;
        QVERIFY(write("objects/"+object+"/traineros-activity.json",group));
        QCOMPARE(installer.activities()[0].toMap()["seats"].toInt(),16);
        for(QJsonValue seats:{QJsonValue(1),QJsonValue(17),QJsonValue(2.5),QJsonValue("16")}){
            group["seats"]=seats;QVERIFY(write("objects/"+object+"/traineros-activity.json",group));QVERIFY(installer.activities().isEmpty());
        }
        auto invalid=activity;invalid["protocol"]="bad protocol";
        QVERIFY(write("objects/"+object+"/traineros-activity.json",invalid));QVERIFY(installer.activities().isEmpty());
        QVERIFY(write("objects/"+object+"/traineros-activity.json",activity));
        auto escaped=receipt;escaped["object"]="../outside";QVERIFY(write("active/fixture.json",escaped));
        installer.configure({}, {},temp.path());QVERIFY(installer.activities().isEmpty());
    }
    void packageArchitectureSelection() {
        QTemporaryDir temp;QVERIFY(temp.isValid());
        const auto cpu=QSysInfo::currentCpuArchitecture();
        const auto arch=cpu=="arm64"?QString("aarch64"):cpu;
        const auto foreign=arch=="aarch64"?QString("x86_64"):QString("aarch64");
        const QVariantMap local{{"id","fixture"},{"title","Local build"},{"version","1"},{"architecture",arch}};
        auto other=local;other["architecture"]=foreign;other["title"]="Wrong build";
        auto newer=other;newer["version"]="2";
        CratesInstaller jobs;jobs.configure("/not-executed",temp.path()+"/registry.json",temp.path());
        jobs.replacementGuard=[]{return QString("Test holds queue");};
        jobs.useCatalogue({newer,other,local},{});
        const auto state=jobs.state("fixture");
        QVERIFY(state["canInstall"].toBool());QCOMPARE(state["availableVersion"].toString(),QString("1"));
        QCOMPARE(state["packageReleases"].toList().size(),1);
        jobs.request("fixture","2");QVERIFY(jobs.tasks().isEmpty());
        jobs.request("fixture","1");QCOMPARE(jobs.tasks().size(),1);
        QCOMPARE(jobs.tasks()[0].toMap()["title"].toString(),QString("Local build"));
        jobs.command("fixture","cancel");
        auto withdrawn=local;withdrawn["withdrawn"]=true;
        jobs.useCatalogue({other,withdrawn},{});
        QVERIFY(!jobs.state("fixture")["canInstall"].toBool());
        QVERIFY(jobs.state("fixture")["packageReleases"].toList().isEmpty());
    }
    void catalogueOnlySyncsOnExplicitEntry() {
#ifndef Q_OS_LINUX
        QSKIP("Host catalogue worker runs on Linux");
#else
        QTemporaryDir temp;QVERIFY(temp.isValid());const auto root=temp.path();
        QFile source(":/crates/catalogue.json");QVERIFY(source.open(QIODevice::ReadOnly));
        QFile crates(":/crates/showcase.json");QVERIFY(crates.open(QIODevice::ReadOnly));
        const QJsonObject response{{"catalogue",QJsonDocument::fromJson(source.readAll()).object()},
            {"crates",QJsonDocument::fromJson(crates.readAll()).array()},{"packages",QJsonArray{}},{"result","snapshot"},{"revision",1}};
        QFile payload(root+"/response.json");QVERIFY(payload.open(QIODevice::WriteOnly));payload.write(QJsonDocument(response).toJson());payload.close();
        QFile cache(root+"/current.json");QVERIFY(cache.open(QIODevice::WriteOnly));cache.write("{}");cache.close();
        QFile helper(root+"/worker.py");QVERIFY(helper.open(QIODevice::WriteOnly));
        helper.write("import sys,pathlib\np=pathlib.Path(sys.argv[2])\nif sys.argv[1]=='sync':\n with (p/'calls').open('a') as f:f.write('request\\n')\nprint((p/'response.json').read_text())\n");helper.close();
        CratesController c;c.configureCatalogue(helper.fileName(),root);QTest::qWait(120);
        QVERIFY(!QFile::exists(root+"/calls")); // Cache load/OS navigation restore is not entry.
        c.enter();c.enter();QTRY_VERIFY(QFile::exists(root+"/calls"));QTRY_VERIFY(c.syncStatus().isEmpty());
        QFile calls(root+"/calls");QVERIFY(calls.open(QIODevice::ReadOnly));QCOMPARE(calls.readAll(),QByteArray("request\n"));calls.close();
        QVERIFY(c.installer()->tasks().isEmpty());
        c.leave();c.enter();QTRY_VERIFY(c.syncStatus().isEmpty());
        QVERIFY(calls.open(QIODevice::ReadOnly));QCOMPARE(calls.readAll(),QByteArray("request\nrequest\n"));
#endif
    }
    void activeWorkerCancellationIsNotFailure() {
#ifndef Q_OS_LINUX
        QSKIP("Production worker runs on Linux");
#else
        QTemporaryDir temp;QVERIFY(temp.isValid());
        const auto registry=temp.path()+"/registry.json",helper=temp.path()+"/worker.py";
        QFile f(registry);QVERIFY(f.open(QIODevice::WriteOnly));
        auto cpu=QSysInfo::currentCpuArchitecture();if(cpu=="arm64")cpu="aarch64";
        f.write(("{\"packages\":[{\"id\":\"fixture\",\"title\":\"Fixture\",\"version\":\"1\",\"architecture\":\""+cpu+"\"}]}").toUtf8());f.close();
        f.setFileName(helper);QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("import signal,time,sys\ndef stop(*_):\n print('{\"error\":\"Cancelled\"}',flush=True)\n sys.exit(1)\nsignal.signal(signal.SIGTERM,stop)\nprint('{\"stage\":\"Downloading\"}',flush=True)\ntime.sleep(20)\n");f.close();
        CratesInstaller jobs;jobs.configure(helper,registry,temp.path()+"/store");jobs.request("fixture");
        QTRY_COMPARE(jobs.tasks()[0].toMap()["detail"].toString(),QString("Downloading"));
        jobs.command("fixture","cancel");
        QTRY_VERIFY(jobs.tasks()[0].toMap()["terminal"].toBool());
        QCOMPARE(jobs.tasks()[0].toMap()["state"].toString(),QString("cancelled"));
        QCOMPARE(jobs.tasks()[0].toMap()["actions"].toList()[0].toMap()["id"].toString(),QString("retry"));
#endif
    }
    void promotedPackageYieldsActiveWorkerAndResumesItLater() {
#ifndef Q_OS_LINUX
        QSKIP("Production pause signals require Linux");
#else
        QTemporaryDir temp;const auto helper=temp.path()+"/worker.py";
        QFile f(helper);QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("import signal,sys,time\nsignal.signal(signal.SIGUSR1,lambda *_:sys.exit(75))\nsignal.signal(signal.SIGTERM,lambda *_:sys.exit(1))\nprint('{\"stage\":\"Downloading\",\"received\":128,\"total\":4096}',flush=True)\ntime.sleep(20)\n");f.close();
        CratesInstaller jobs;jobs.configure(helper,{},temp.path()+"/store");
        auto arch=QSysInfo::currentCpuArchitecture();if(arch=="arm64")arch="aarch64";QVariantList records;
        for(const auto& id:{"one","two","three"})records<<QVariantMap{{"id",id},{"title",id},{"version","1"},{"architecture",arch}};
        jobs.useCatalogue(records,{});jobs.request("one");
        const auto state=[&](QString id){return jobs.state(id)["job"].toMap();};
        QTRY_COMPARE(state("one")["stage"].toString(),"Downloading");jobs.request("two");jobs.request("three");
        jobs.command("three","first");jobs.command({},"pause-all");
        QTRY_COMPARE(state("one")["state"].toString(),"paused");
        QCOMPARE(state("three")["state"].toString(),"paused");
        jobs.command("one","resume");QTRY_COMPARE(state("one")["stage"].toString(),"Downloading");
        jobs.command("three","first");QTRY_COMPARE(state("three")["stage"].toString(),"Downloading");
        QCOMPARE(state("one")["state"].toString(),"queued");QVERIFY(!state("one")["terminal"].toBool());
        jobs.command({},"pause-all");QTRY_COMPARE(state("three")["state"].toString(),"paused");
        for(const auto& id:{"one","two","three"})QCOMPARE(state(id)["state"].toString(),"paused");
        jobs.command("one","resume");QTRY_COMPARE(state("one")["state"].toString(),"running");
        jobs.command({},"cancel-all");QTRY_VERIFY(state("one")["terminal"].toBool());
#endif
    }
    void packageActionsAndDeduplicatedQueue() {
        QTemporaryDir temp;QVERIFY(temp.isValid());
        const auto registry=temp.path()+"/registry.json";QFile f(registry);QVERIFY(f.open(QIODevice::WriteOnly));
        auto cpu=QSysInfo::currentCpuArchitecture();if(cpu=="arm64")cpu="aarch64";
        f.write(("{\"packages\":[{\"id\":\"2048\",\"title\":\"2048\",\"version\":\"1\",\"kind\":\"game\",\"architecture\":\""+cpu+"\"}]}").toUtf8());f.close();
        CratesController c;c.installer()->replacementGuard=[]{return QString("Game is running");};
        c.installer()->configure("/not-executed",registry,temp.path()+"/store");
        c.open(0,true);
        for(int i=0;i<c.projectCount();++i){c.selectProject(i);if(c.project()["id"]=="2048")break;}
        c.openProject(c.projectIndex());QCOMPARE(c.primaryLabel(),QString("Download"));QVERIFY(c.primaryEnabled());
        QSignalSpy downloads(&c,&CratesController::downloadsRequested);c.primary();c.primary();
        QCOMPARE(c.installer()->tasks().size(),1);QCOMPARE(c.primaryLabel(),QString("View progress"));
        QCOMPARE(c.installer()->tasks()[0].toMap()["detail"].toString(),QString("Game is running"));
        c.installer()->command("2048","cancel");QVERIFY(!c.project()["busy"].toBool());
        c.installer()->command("2048","retry");QCOMPARE(c.installer()->tasks().size(),1);
        c.installer()->command({},"cancel-all");QVERIFY(c.installer()->tasks()[0].toMap()["terminal"].toBool());
        c.closeDetail();c.openProject(1);QVERIFY(!c.primaryEnabled());QCOMPARE(c.primaryLabel(),QString("Package coming soon"));
    }
    void authoredIdentity() {
        CratesController c;QCOMPARE(c.crates().size(),6);
        QSet<QString> ids;
        for(const auto& row:c.crates()){const auto v=row.toMap();QVERIFY(!v["title"].toString().isEmpty());ids.insert(v["id"].toString());}
        QCOMPARE(ids.size(),6);QCOMPARE(c.current()["id"].toString(),QString("whats-new"));
    }
    void directionalRowsAndEdges() {
        CratesController c;c.dispatch(Action::Right);QCOMPARE(c.focusIndex(),1);
        c.dispatch(Action::Right);QCOMPARE(c.focusIndex(),1);
        c.dispatch(Action::Down);QCOMPARE(c.focusIndex(),4);
        c.dispatch(Action::Right);QCOMPARE(c.focusIndex(),5);
        c.dispatch(Action::Up);QCOMPARE(c.focusIndex(),1);
        c.select(2);c.dispatch(Action::Left);QCOMPARE(c.focusIndex(),2);
        c.select(-1);c.open(99);QCOMPARE(c.focusIndex(),2);QVERIFY(!c.opening());
    }
    void backAndSkipRestoreFocus() {
        CratesController c;c.open(4);QVERIFY(c.opening());
        c.dispatch(Action::Confirm);QVERIFY(c.opened());QVERIFY(!c.opening());
        c.dispatch(Action::Right);QCOMPARE(c.focusIndex(),4);
        QVERIFY(c.dispatch(Action::Back));QVERIFY(!c.opened());QCOMPARE(c.focusIndex(),4);
        QVERIFY(!c.dispatch(Action::Back));
    }
    void interruptedOpenDoesNotNavigateLater() {
        CratesController c;c.open(3);c.leave();QTest::qWait(260);
        QVERIFY(!c.opened());QVERIFY(!c.opening());QCOMPARE(c.focusIndex(),3);
        c.open(3);c.back();QTest::qWait(260);QVERIFY(!c.opened());
    }
    void reducedMotionAndRouteRetention() {
        CratesController c;c.setProperty("reducedMotion",true);c.dispatch(Action::Confirm);
        QVERIFY(c.opened());QVERIFY(!c.opening());c.leave();QVERIFY(c.opened());
        c.back();c.open(5);QVERIFY(c.opened());QCOMPARE(c.focusIndex(),5);
    }
    void cataloguePagingAndPopupReturn() {
        CratesController c;QVERIFY(c.loadCatalogue(cratesCatalogueFixture()));c.open(1,true);
        QCOMPARE(c.projectCount(),8);QCOMPARE(c.projects().size(),6);QCOMPARE(c.pageCount(),2);
        c.selectProject(5);c.dispatch(Action::Right);QCOMPARE(c.page(),1);QCOMPARE(c.projects().size(),2);
        c.dispatch(Action::Down);QCOMPARE(c.projectIndex(),7);
        c.dispatch(Action::Confirm);QVERIFY(c.detailOpen());QCOMPARE(c.project()["id"].toString(),QString("preview-7"));
        c.dispatch(Action::Back);QVERIFY(!c.detailOpen());QCOMPARE(c.page(),1);QCOMPARE(c.projectIndex(),7);
        c.turnPage(-1);QCOMPARE(c.projectIndex(),1);c.turnPage(-1);QCOMPARE(c.projectIndex(),1);
        c.selectProject(0);c.dispatch(Action::Up);c.dispatch(Action::Left);QCOMPARE(c.projectIndex(),0);
    }
    void sharedIdentityAndPerCrateFocus() {
        CratesController c;QVERIFY(c.loadCatalogue(cratesCatalogueFixture()));c.open(1,true);
        c.selectProject(5);const auto project=c.project();c.back();c.open(0,true);c.selectProject(1);
        QCOMPARE(c.project(),project);c.back();c.open(1,true);QCOMPARE(c.projectIndex(),5);
        c.openProject(5);c.leave();QVERIFY(c.opened());QVERIFY(!c.detailOpen());QCOMPARE(c.projectIndex(),5);
    }
    void detailImagesAndScrollWithoutVersionPicker() {
        CratesController c;QVERIFY(c.loadCatalogue(cratesCatalogueFixture()));c.open(1,true);c.openProject(0);
        QSignalSpy catalogue(&c,&CratesController::catalogueChanged);
        c.dispatch(Action::Right);QCOMPARE(c.screenshotIndex(),1);c.selectScreenshot(99);QCOMPARE(c.screenshotIndex(),1);
        QSignalSpy scroll(&c,&CratesController::detailScrollRequested);c.dispatch(Action::Down);QCOMPARE(scroll.count(),1);
        c.dispatch(Action::LocalAction);QVERIFY(c.detailOpen());QVERIFY(c.actions().isEmpty());
        c.dispatch(Action::Back);QVERIFY(!c.detailOpen());c.openProject(0);QCOMPARE(c.screenshotIndex(),0);
        QCOMPARE(catalogue.count(),0); // Detail navigation must not rebuild the six image delegates.
    }
    void invalidCatalogueKeepsCurrentData() {
        CratesController c;QVERIFY(c.loadCatalogue(cratesCatalogueFixture()));c.open(1,true);c.selectProject(7);
        QVERIFY(!c.loadCatalogue("not json"));QCOMPARE(c.projectCount(),8);QCOMPARE(c.projectIndex(),7);
        auto doc=QJsonDocument::fromJson(cratesCatalogueFixture()).object();auto rows=doc["projects"].toArray();
        rows.append(rows[0]);doc["projects"]=rows;QVERIFY(!c.loadCatalogue(QJsonDocument(doc).toJson()));
        doc=QJsonDocument::fromJson(cratesCatalogueFixture()).object();doc["contents"]=QJsonObject{{"workshop",QJsonArray{"missing"}}};
        QVERIFY(!c.loadCatalogue(QJsonDocument(doc).toJson()));QCOMPARE(c.projectCount(),8);
        QVERIFY(!c.loadCatalogue(QByteArray(3*1024*1024,' ')));
    }
    void metadataHasNoExecutableOrRemoteCapabilities() {
        auto doc=QJsonDocument::fromJson(cratesCatalogueFixture()).object();auto rows=doc["projects"].toArray();auto row=rows[0].toObject();
        row["hero"]="file:///private/secret";row["screenshots"]=QJsonArray{"https://tracker.example/a.png","qrc:/crates/../../secret"};
        row["command"]="dangerous";row["installed"]=true;row["rating"]=5;rows[0]=row;doc["projects"]=rows;
        CratesController c;QVERIFY(c.loadCatalogue(QJsonDocument(doc).toJson()));c.open(1,true);
        QVERIFY(c.project()["hero"].toString().isEmpty());QVERIFY(c.project()["screenshots"].toList().isEmpty());
        QVERIFY(!c.project().contains("command"));QVERIFY(!c.project().contains("installed"));QVERIFY(!c.project().contains("rating"));
        c.back();c.open(3,true);QCOMPARE(c.projectCount(),0);c.dispatch(Action::Confirm);QVERIFY(!c.detailOpen());
    }
};
QTEST_GUILESS_MAIN(CratesTests)
#include "CratesTests.moc"
