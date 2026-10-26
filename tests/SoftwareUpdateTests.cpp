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
    void downloadSurvivesRestartButMustBeVerifiedAgain(){
        QTemporaryDir root;const auto source=softwareUpdateFixture(root.path(),UPDATE_PYTHON);
        {
            SoftwareUpdateController flow;flow.configure(source);flow.check();QTRY_VERIFY(!flow.busy());
            flow.activate(3);QTRY_VERIFY(flow.progress()>0);flow.command("pause");QTRY_VERIFY(!flow.busy());
        }
        SoftwareUpdateController resumed;resumed.configure(source);QCOMPARE(resumed.status(),"paused");
        QCOMPARE(resumed.version(),"fixture-7");QVERIFY(!resumed.lastChecked().isEmpty());
        QVERIFY(!resumed.actions()[8].toMap()["enabled"].toBool());
        resumed.command("resume");QTRY_VERIFY_WITH_TIMEOUT(!resumed.busy(),7000);QCOMPARE(resumed.status(),"prepared");
        auto installed=source;installed.installedSequence=7;
        SoftwareUpdateController current;current.configure(installed);QVERIFY(current.tasks().isEmpty());
    }
    void maintenanceRequiresConfirmationAndRechecksRuntimeGuard(){
        QTemporaryDir root;auto source=softwareUpdateFixture(root.path(),UPDATE_PYTHON);
        source.manager=source.helper;source.installationRoot=root.path();source.rollbackAvailable=true;
        SoftwareUpdateController flow;flow.configure(source);QSignalSpy restart(&flow,&SoftwareUpdateController::restartRequested);
        QString blocked="Finish game";flow.maintenanceGuard=[&]{return blocked;};
        flow.activate(9);QVERIFY(!flow.busy());QVERIFY(flow.message().contains("previous compatible"));
        flow.dispatch(Action::Back);QVERIFY(!flow.busy());QCOMPARE(restart.size(),0);
        flow.activate(9);flow.activate(9);QVERIFY(!flow.busy());QCOMPARE(flow.message(),blocked);
        blocked.clear();flow.activate(9);flow.activate(9);QVERIFY(flow.transitionPending());
        flow.command("cancel-all");QVERIFY(flow.busy()); // Downloads cannot cancel a maintenance request.
        blocked="New save operation";QTRY_VERIFY(!flow.busy());QCOMPARE(restart.size(),0);QCOMPARE(flow.message(),blocked);
        blocked.clear();flow.activate(9);flow.activate(9);QTRY_VERIFY(!flow.busy());QCOMPARE(restart.size(),1);
        QVERIFY(flow.transitionPending());flow.activate(0);QCOMPARE(flow.channel(),"beta");
        flow.cancelRestart();QVERIFY(!flow.transitionPending());QVERIFY(flow.message().contains("cancelled"));
    }
    void managedApplyAuditAndCacheUseTheirOwnCommands(){
        QTemporaryDir root;auto source=softwareUpdateFixture(root.path(),UPDATE_PYTHON);
        source.manager=source.helper;source.installationRoot=root.path();
        SoftwareUpdateController flow;flow.configure(source);flow.maintenanceGuard=[] {return QString();};
        flow.check();QTRY_VERIFY(!flow.busy());flow.activate(3);QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),7000);
        QCOMPARE(flow.status(),"prepared");QVERIFY(flow.actions()[8].toMap()["enabled"].toBool());
        QSignalSpy restart(&flow,&SoftwareUpdateController::restartRequested);
        flow.activate(8);QVERIFY(!flow.busy());flow.activate(8);QTRY_VERIFY(!flow.busy());QCOMPARE(restart.size(),1);
        flow.cancelRestart();flow.activate(11);QTRY_VERIFY(!flow.busy());QCOMPARE(flow.status(),"audit-ready");
        flow.activate(12);QTRY_VERIFY(!flow.busy());QCOMPARE(flow.status(),"cache-cleared");QVERIFY(flow.tasks().isEmpty());
    }
    void noSpaceHasActionableMessage(){
        QTemporaryDir root;SoftwareUpdateController flow;flow.configure(softwareUpdateFixture(root.path(),UPDATE_PYTHON));flow.check();QTRY_VERIFY(!flow.busy());
        softwareUpdateMode(root.path(),"space");flow.activate(3);QTRY_VERIFY(!flow.busy());QCOMPARE(flow.status(),"error");QVERIFY(flow.message().contains("Free some space"));
    }
};
QTEST_GUILESS_MAIN(SoftwareUpdateTests)
#include "SoftwareUpdateTests.moc"
