#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSysInfo>
#include "features/crates/CratesController.h"
#include "fixtures/crates/CatalogueFixture.h"
using namespace trainer;
class CratesTests: public QObject {
    Q_OBJECT
private slots:
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
    void detailImagesVersionsAndScroll() {
        CratesController c;QVERIFY(c.loadCatalogue(cratesCatalogueFixture()));c.open(1,true);c.openProject(0);
        QSignalSpy catalogue(&c,&CratesController::catalogueChanged);
        c.dispatch(Action::Right);QCOMPARE(c.screenshotIndex(),1);c.selectScreenshot(99);QCOMPARE(c.screenshotIndex(),1);
        QSignalSpy scroll(&c,&CratesController::detailScrollRequested);c.dispatch(Action::Down);QCOMPARE(scroll.count(),1);
        c.dispatch(Action::LocalAction);QVERIFY(c.versionsOpen());c.dispatch(Action::Down);QCOMPARE(c.releaseIndex(),1);
        c.dispatch(Action::Back);QVERIFY(c.detailOpen());QVERIFY(!c.versionsOpen());
        c.dispatch(Action::Back);c.openProject(0);QCOMPARE(c.releaseIndex(),0);QCOMPARE(c.screenshotIndex(),0);
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
