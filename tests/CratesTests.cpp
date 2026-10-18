#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "features/crates/CratesController.h"
#include "fixtures/crates/CatalogueFixture.h"
using namespace trainer;
class CratesTests: public QObject {
    Q_OBJECT
private slots:
    void activeWorkerCancellationIsNotFailure() {
#ifndef Q_OS_LINUX
        QSKIP("Production worker runs on Linux");
#else
        QTemporaryDir temp;QVERIFY(temp.isValid());
        const auto registry=temp.path()+"/registry.json",helper=temp.path()+"/worker.py";
        QFile f(registry);QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"packages\":[{\"id\":\"fixture\",\"title\":\"Fixture\",\"version\":\"1\"}]}");f.close();
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
        f.write("{\"packages\":[{\"id\":\"2048\",\"title\":\"2048\",\"version\":\"1\",\"kind\":\"game\"}]}");f.close();
        CratesController c;c.installer()->replacementGuard=[]{return QString("Game is running");};
        c.installer()->configure("/not-executed",registry,temp.path()+"/store");
        c.open(0,true);c.openProject(0);QCOMPARE(c.primaryLabel(),QString("Download"));QVERIFY(c.primaryEnabled());
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
