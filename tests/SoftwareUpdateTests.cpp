#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>
#include "SoftwareUpdateFixture.h"
#include "features/downloads/DownloadsController.h"
using namespace trainer;
class SoftwareUpdateTests : public QObject {
    Q_OBJECT
    QTemporaryDir settings_;
private slots:
    void initTestCase(){QCoreApplication::setOrganizationName("TrainerTest");QCoreApplication::setApplicationName("SoftwareUpdates");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_.path());}
    void init(){QSettings().clear();}
    void unconfiguredDoesNotCheckOrPretendInstalled(){
        SoftwareUpdateController flow;QCOMPARE(flow.status(),"unconfigured");QCOMPARE(flow.installedVersion(),"Development build");
        flow.check();flow.activate(3);QVERIFY(!flow.busy());QVERIFY(flow.tasks().isEmpty());
        QVERIFY(!flow.actions()[2].toMap()["enabled"].toBool());
    }
    void metadataCheckAndExplicitPreparation(){
        QTemporaryDir root;SoftwareUpdateController flow;flow.configure(softwareUpdateFixture(root.path(),UPDATE_PYTHON));
        flow.check();QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),5000);QCOMPARE(flow.status(),"available");QVERIFY(flow.tasks().isEmpty());
        QFile log(root.path()+"/requests");QVERIFY(log.open(QIODevice::ReadOnly|QIODevice::Text));QCOMPARE(log.readAll(),"check\n");log.close();
        flow.activate(3);QVERIFY(flow.busy());QVERIFY(!flow.tasks().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),7000);QCOMPARE(flow.status(),"prepared");QCOMPARE(flow.installedVersion(),"fixture-6");
        QVERIFY(!flow.actions()[3].toMap()["enabled"].toBool());QVERIFY(flow.message().contains("not available"));
        QCOMPARE(flow.tasks()[0].toMap()["state"].toString(),"done");
    }
    void pauseResumeCancelAndNoOverlap(){
        QTemporaryDir root;SoftwareUpdateController flow;flow.configure(softwareUpdateFixture(root.path(),UPDATE_PYTHON));
        flow.check();QTRY_VERIFY(!flow.busy());flow.activate(3);QTRY_VERIFY(flow.progress()>0);
        flow.activate(0);QCOMPARE(flow.channel(),"beta");flow.command("pause");QTRY_VERIFY(!flow.busy());QCOMPARE(flow.status(),"paused");
        flow.check(true);QVERIFY(!flow.busy());flow.command("resume");QTRY_VERIFY(flow.progress()>0);
        flow.command("cancel");QTRY_VERIFY(!flow.busy());QCOMPARE(flow.status(),"cancelled");
        QCOMPARE(flow.installedVersion(),"fixture-6");
    }
    void backgroundNeverPreparesAndCanBeDisabled(){
        QTemporaryDir root;SoftwareUpdateController flow;flow.configure(softwareUpdateFixture(root.path(),UPDATE_PYTHON));
        QSignalSpy notice(&flow,&SoftwareUpdateController::updateAvailable);
        flow.check(true);QTRY_VERIFY(!flow.busy());QCOMPARE(notice.size(),1);QVERIFY(flow.tasks().isEmpty());QVERIFY(!flow.notification().isEmpty());
        flow.dismissNotification();QVERIFY(flow.notification().isEmpty());
        flow.check(true);QTRY_VERIFY(!flow.busy());QCOMPARE(notice.size(),1);
        flow.activate(1);flow.check(true);QVERIFY(!flow.busy());
        SoftwareUpdateController reopened;reopened.configure(softwareUpdateFixture(root.path(),UPDATE_PYTHON));reopened.check(true);QVERIFY(!reopened.busy());
    }
    void protocolFailuresAreNotSuccess_data(){QTest::addColumn<QByteArray>("mode");for(const auto mode:{"error","empty","invalid"})QTest::newRow(mode)<<QByteArray(mode);}
    void protocolFailuresAreNotSuccess(){
        QFETCH(QByteArray,mode);QTemporaryDir root;SoftwareUpdateController flow;flow.configure(softwareUpdateFixture(root.path(),UPDATE_PYTHON));softwareUpdateMode(root.path(),mode);
        flow.check();QTRY_VERIFY(!flow.busy());QCOMPARE(flow.status(),"error");QVERIFY(flow.lastChecked().isEmpty());QVERIFY(flow.tasks().isEmpty());
    }
    void preparedPhaseAndFalseInstallationCannotClaimSuccess_data(){QTest::addColumn<QByteArray>("mode");QTest::newRow("phase then crash")<<QByteArray("crash-after-phase");QTest::newRow("claims installed")<<QByteArray("lie-installed");}
    void preparedPhaseAndFalseInstallationCannotClaimSuccess(){
        QFETCH(QByteArray,mode);QTemporaryDir root;SoftwareUpdateController flow;flow.configure(softwareUpdateFixture(root.path(),UPDATE_PYTHON));flow.check();QTRY_VERIFY(!flow.busy());
        softwareUpdateMode(root.path(),mode);flow.activate(3);QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),7000);QCOMPARE(flow.status(),"error");QCOMPARE(flow.installedVersion(),"fixture-6");
        flow.check(true);QVERIFY(!flow.busy()); // Background checks cannot erase a failed user job.
    }
    void incompatibilityStopsDownload(){
        QTemporaryDir root;SoftwareUpdateController flow;flow.configure(softwareUpdateFixture(root.path(),UPDATE_PYTHON));softwareUpdateMode(root.path(),"blocked");
        flow.check();QTRY_VERIFY(!flow.busy());QCOMPARE(flow.status(),"blocked");flow.activate(3);QVERIFY(!flow.busy());QVERIFY(flow.tasks().isEmpty());
    }
    void channelSwitchInvalidatesCandidate(){
        QTemporaryDir root;SoftwareUpdateController flow;flow.configure(softwareUpdateFixture(root.path(),UPDATE_PYTHON));flow.check();QTRY_VERIFY(!flow.busy());
        flow.activate(0);QCOMPARE(flow.channel(),"preview");QVERIFY(flow.version().isEmpty());QVERIFY(!flow.actions()[3].toMap()["enabled"].toBool());
        SoftwareUpdateController reopened;QCOMPARE(reopened.channel(),"preview");
    }
    void downloadRouting(){
        QTemporaryDir root;SoftwareUpdateController controller;auto* flow=&controller;flow->configure(softwareUpdateFixture(root.path(),UPDATE_PYTHON));
        DownloadsController downloads;connect(flow,&SoftwareUpdateController::changed,&downloads,[&]{downloads.publish("traineros-software",flow->tasks());});
        connect(&downloads,&DownloadsController::commandRequested,flow,[&](QString,QString,QString command){flow->command(command);});
        flow->check();QTRY_VERIFY(!flow->busy());flow->activate(3);QTRY_VERIFY(flow->progress()>0);
        downloads.controlAll("pause-all");QTRY_VERIFY(!flow->busy());QCOMPARE(flow->status(),"paused");
        downloads.controlAll("cancel-all");QCOMPARE(flow->status(),"cancelled");
    }
    void failedStartAllowsRetry(){
        QTemporaryDir root;SoftwareUpdateController flow;auto source=softwareUpdateFixture(root.path(),"/nonexistent/python");flow.configure(source);flow.check();
        QTRY_VERIFY(!flow.busy());QCOMPARE(flow.status(),"error");QVERIFY(flow.actions()[2].toMap()["enabled"].toBool());
    }
    void noSpaceHasActionableMessage(){
        QTemporaryDir root;SoftwareUpdateController flow;flow.configure(softwareUpdateFixture(root.path(),UPDATE_PYTHON));flow.check();QTRY_VERIFY(!flow.busy());
        softwareUpdateMode(root.path(),"space");flow.activate(3);QTRY_VERIFY(!flow.busy());QCOMPARE(flow.status(),"error");QVERIFY(flow.message().contains("Free some space"));
    }
};
QTEST_GUILESS_MAIN(SoftwareUpdateTests)
#include "SoftwareUpdateTests.moc"
