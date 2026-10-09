#include "features/adventure/RuntimeMultiplayer.h"
#include "features/adventure/AdventureExitPresentation.h"
#include "features/social/SocialController.h"
#include "core/navigation/AdventureLaunchController.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QPromise>

namespace trainer {
class RuntimeMultiplayerTests final : public QObject {
    Q_OBJECT
    QTemporaryDir settings_;
    struct Library final : LibraryRepository {
        std::function<void()> inspect;
        QList<AdventureRegistration> records;
        QList<AdventureRegistration> registrations() const override { return records; }
        QList<World> worlds() const override { return {}; }
        QList<Adventure> adventures() const override { return {}; }
        QList<ResumePoint> resumePoints() const override { return {}; }
        HomeSnapshot home() const override { return {}; }
        std::optional<AdventureRegistration> registration(const QString&) const override {
            if(inspect)inspect();return {};
        }
    };
    struct Fixture {
        QTemporaryDir dir;
        Library library;
        RetroArchAdapter retro{library,{}};
        StandaloneAdapter psp{"ppsspp",library,{}},dolphin{"dolphin",library,{}};
        SocialController social;
        ProcessService process;
        AdventureLaunchController lifecycle{process};
        AdventureExitPresentation view{lifecycle.exitController()};
        RuntimeMultiplayer runtime{library,retro,psp,dolphin,social,process,lifecycle,view};
        Fixture() {
            QObject::connect(&lifecycle,&AdventureLaunchController::checkpointRequested,&lifecycle,
                [this](quint64 token,const QJsonObject&){lifecycle.checkpointCompleted(token,{});});
            QObject::connect(&view,&AdventureExitPresentation::menuCaptureRequested,&view,
                [this](quint64 token){view.menuCaptureCompleted(token,{});});
            QObject::connect(&lifecycle.exitController(),&AdventureExitController::captureRequested,&view,
                [this](quint64 token){lifecycle.exitController().captureCompleted(token,{},"No test frame");});
            QObject::connect(&lifecycle.exitController(),&AdventureExitController::gracefulExitRequested,&view,
                [this](quint64){QFile f(dir.filePath("control"));if(f.open(QIODevice::WriteOnly))f.write("exit");});
        }
        bool start(bool finalizerFails=false) {
#ifdef Q_OS_WIN
            const auto name="trainer_process_probe.exe";
#else
            const auto name="trainer_process_probe";
#endif
            ProcessCommand command{QDir(QCoreApplication::applicationDirPath()).filePath(name),
                {"controlled",dir.filePath("control"),dir.filePath("pid")},{}};
            if(finalizerFails)command.finalize=[](const ProcessOutcome&){return QString("Save recovery required");};
            return lifecycle.launch(command,{},"ordinary",AdventureSavePolicy::ManualConfirm);
        }
    };
    void requestRestart(Fixture& f) {
        f.lifecycle.exitController().setAvailable(true);
        QVERIFY(f.view.requestMenu());
        f.runtime.active_=true;f.runtime.host_=true;
        f.runtime.game_="missing-after-close";
        f.runtime.descriptor_={{"transport","netpacket"}};
        f.runtime.deadline_=QDateTime::currentSecsSinceEpoch()+1;
        f.runtime.timer_.start();
        f.runtime.prepareHost();
        QCOMPARE(f.lifecycle.exitController().phase(),AdventureExitController::Phase::Confirming);
    }
private slots:
    void verifiedGameIsAvailableWhileOtherGamesAreStillScanning() {
        Fixture f;
        AdventureRegistration first;first.adventure.id="verified";first.adventure.platformId="nes";
        first.integrationConfig["core"]="fceumm";
        auto pending=first;pending.adventure.id="not-yet-checked";
        f.library.records={first,pending};
        // Keep the worker unfinished to exercise result delivery rather than
        // relying on the relative speed of small and large files on this host.
        QPromise<RuntimeMultiplayer::ScannedGame> scan;
        scan.start();f.runtime.scan_.setFuture(scan.future());
        f.runtime.refresh({},"test-trainer",true);
        QVERIFY(f.runtime.onlineGames().isEmpty());
        const auto profile=retroarch::netplayProfile("nes","fceumm",QString(64,'a'));
        QVERIFY(!profile.isEmpty());
        scan.addResult({first.adventure.id,profile});
        QTRY_COMPARE(f.runtime.onlineGames(),QStringList{"verified"});
        QVERIFY(f.runtime.scan_.isRunning());
        f.runtime.refresh({},"test-trainer",false);
        QVERIFY(f.runtime.onlineGames().isEmpty());
        scan.addResult({pending.adventure.id,profile});
        QCoreApplication::processEvents();
        QVERIFY(f.runtime.onlineGames().isEmpty());
        scan.finish();QTRY_VERIFY(!f.runtime.scan_.isRunning());
    }
    void initTestCase() {
        QVERIFY(settings_.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_.path());
        QCoreApplication::setOrganizationName("TrainerOS test");
        QCoreApplication::setApplicationName("Runtime transition");
    }
    void savePromptRetainsOwnedGameAndCancelDoesNotLaunch() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        const auto pid=f.process.processId();requestRestart(f);
        QVERIFY(!f.runtime.timer_.isActive());QCOMPARE(f.runtime.deadline_,0);
        QVERIFY(f.runtime.restarting_);QCOMPARE(f.process.processId(),pid);
        QSignalSpy notice(&f.runtime,&RuntimeMultiplayer::notice);
        QVERIFY(f.lifecycle.exitController().cancel());
        QVERIFY(!f.runtime.active_);QVERIFY(!f.runtime.launchPending_);
        QCOMPARE(f.process.processId(),pid);QVERIFY(f.lifecycle.active());
        QVERIFY(!notice.isEmpty());
        QFile finish(f.dir.filePath("control"));QVERIFY(finish.open(QIODevice::WriteOnly));finish.write("exit");finish.close();
        QTRY_VERIFY(!f.process.active());
    }
    void successfulCloseStartsANewConnectionBudget() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");requestRestart(f);
        qint64 connectionDeadline=0;bool timerRunning=false;
        f.library.inspect=[&]{connectionDeadline=f.runtime.deadline_;timerRunning=f.runtime.timer_.isActive();};
        QVERIFY(f.lifecycle.exitController().confirm());
        QTRY_VERIFY(connectionDeadline>0);
        QVERIFY(timerRunning);QVERIFY(connectionDeadline>=QDateTime::currentSecsSinceEpoch()+70);
        QVERIFY(!f.process.active());
    }
    void failedSaveFinalizationPreventsMultiplayerLaunch() {
        Fixture f;QVERIFY(f.start(true));QTRY_COMPARE(f.lifecycle.state(),"running");requestRestart(f);
        int launches=0;f.library.inspect=[&]{++launches;};
        QSignalSpy notice(&f.runtime,&RuntimeMultiplayer::notice);
        QVERIFY(f.lifecycle.exitController().confirm());QTRY_VERIFY(!f.lifecycle.active());
        QCoreApplication::processEvents();
        QCOMPARE(launches,0);QVERIFY(!f.runtime.active_);QVERIFY(!f.runtime.timer_.isActive());
        QVERIFY(!notice.isEmpty());QCOMPARE(notice.last()[0].toString(),"Save recovery required");
    }
    void queuedLaunchCannotAttachToAReplacementInvitation() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");requestRestart(f);
        int launches=0;f.library.inspect=[&]{++launches;};
        connect(&f.lifecycle,&AdventureLaunchController::adventureFinished,&f.runtime,[&]{
            // A new invitation can arrive before the old zero-delay callback.
            f.runtime.fail("Cancelled");f.runtime.active_=true;
            ++f.runtime.sessionGeneration_;f.runtime.game_="replacement";
        });
        QVERIFY(f.lifecycle.exitController().confirm());QTRY_VERIFY(!f.lifecycle.active());
        QCoreApplication::processEvents();
        QCOMPARE(launches,0);QVERIFY(f.runtime.active_);QCOMPARE(f.runtime.game_,"replacement");
    }
};
}
QTEST_GUILESS_MAIN(trainer::RuntimeMultiplayerTests)
#include "RuntimeMultiplayerTests.moc"
