#include "core/navigation/AdventureLaunchController.h"
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace trainer;
using Phase = AdventureExitController::Phase;
namespace {
struct RunningAdventure {
    QTemporaryDir directory;
    ProcessService process;
    AdventureLaunchController launch{process};
    QSignalSpy started{&process, &ProcessService::started};
    QSignalSpy returned{&launch, &AdventureLaunchController::restoreRequested};
    QSignalSpy captures{&launch.exitController(), &AdventureExitController::captureRequested};
    QSignalSpy prompts{&launch.exitController(), &AdventureExitController::confirmationRequested};
    QSignalSpy closes{&launch.exitController(), &AdventureExitController::gracefulExitRequested};
    QSignalSpy resumes{&launch.exitController(), &AdventureExitController::returnToGameRequested};
    QSignalSpy completed{&launch.exitController(), &AdventureExitController::completed};
    QJsonObject context{{"page", "worlds"}, {"route", "detail"}, {"selected", "fixture"}};
    RunningAdventure() {
        QObject::connect(&launch, &AdventureLaunchController::checkpointRequested, &launch,
            [this](quint64 token, const QJsonObject&) { launch.checkpointCompleted(token, {}); });
    }
    AdventureExitController& exit() { return launch.exitController(); }
    QString path(const QString& name) const { return directory.filePath(name); }
    bool command(const QByteArray& data) {
        QFile file(path("command"));
        if (!file.open(QIODevice::WriteOnly)) return false;
        return file.write(data) == data.size();
    }
    QByteArray pid() const {
        QFile file(path("pid"));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }
    bool start(AdventureSavePolicy policy = AdventureSavePolicy::Unknown) {
        const auto probe = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "trainer_process_probe.exe"
#else
            "trainer_process_probe"
#endif
        );
        return launch.launch({probe, {"controlled", path("command"), path("pid")}, {}}, context, "fixture", policy);
    }
    quint64 attempt() const { return captures.last()[0].toULongLong(); }
    ~RunningAdventure() {
        command("exit");
        QElapsedTimer deadline; deadline.start();
        while (process.active() && deadline.elapsed() < 1000) QTest::qWait(10);
    }
};
QImage frame(QRgb color = qRgb(31, 120, 83)) {
    QImage value(64, 36, QImage::Format_RGB32); value.fill(color); return value;
}
}
class AdventureExitTests final : public QObject {
    Q_OBJECT
private slots:
    void cleanExitBeforeCloseDispatchIsNotConfirmedCompletion() {
        AdventureExitController exit;
        QSignalSpy completed(&exit, &AdventureExitController::completed);
        QSignalSpy closes(&exit, &AdventureExitController::gracefulExitRequested);
        QSignalSpy captures(&exit, &AdventureExitController::captureRequested);
        exit.beginSession(AdventureSavePolicy::Unknown); exit.setAvailable(true);
        connect(&exit, &AdventureExitController::changed, &exit, [&] {
            // A presenter/platform observer sees the child disappear before
            // its graceful-close provider has even received a request.
            if (exit.phase() == Phase::Closing) exit.endSession(true);
        });
        QVERIFY(exit.requestExit());
        exit.captureCompleted(captures.first()[0].toULongLong(), frame());
        QVERIFY(exit.confirm()); QVERIFY(completed.isEmpty()); QVERIFY(closes.isEmpty());
    }
    void manualAndUnknownRequireConfirmation_data() {
        QTest::addColumn<bool>("manual");
        QTest::newRow("unknown") << false;
        QTest::newRow("manual") << true;
    }
    void manualAndUnknownRequireConfirmation() {
        QFETCH(bool, manual);
        RunningAdventure game;
        QVERIFY(!game.exit().requestExit());
        QVERIFY(game.start(manual ? AdventureSavePolicy::ManualConfirm : AdventureSavePolicy::Unknown));
        QTRY_VERIFY(!game.pid().isEmpty()); const auto pid = game.pid();
        QVERIFY(!game.exit().requestExit()); // Capability defaults off, even during a game.
        game.exit().setAvailable(true);
        QVERIFY(game.exit().requestExit()); const auto first = game.attempt();
        QVERIFY(!game.exit().requestExit()); QVERIFY(!game.exit().confirm());
        QVERIFY(game.prompts.isEmpty()); QVERIFY(game.closes.isEmpty());
        game.exit().captureCompleted(first, frame());
        QCOMPARE(game.prompts.size(), 1); QCOMPARE(game.exit().phase(), Phase::Confirming);
        QVERIFY(game.process.active()); QVERIFY(game.completed.isEmpty());
        QVERIFY(game.exit().cancel()); QCOMPARE(game.resumes.size(), 1);
        QCOMPARE(game.pid(), pid); QCOMPARE(game.started.size(), 1);
        QVERIFY(game.returned.isEmpty()); QVERIFY(game.completed.isEmpty()); QVERIFY(game.closes.isEmpty());
        QVERIFY(game.exit().requestExit()); const auto second = game.attempt();
        QVERIFY(second > first);
        game.exit().captureCompleted(first, frame()); // Late result from the cancelled request.
        QCOMPARE(game.exit().phase(), Phase::Capturing); QCOMPARE(game.prompts.size(), 1);
        const auto latest = frame(qRgb(190, 60, 90));
        game.exit().captureCompleted(second, latest);
        QVERIFY(game.exit().confirm()); QVERIFY(!game.exit().confirm());
        QVERIFY(!game.exit().cancel()); QCOMPARE(game.closes.size(), 1);
        QVERIFY(game.completed.isEmpty()); QVERIFY(game.process.active());
        QVERIFY(game.command("exit")); QTRY_COMPARE(game.returned.size(), 1);
        QCOMPARE(game.returned.first()[0].toJsonObject(), game.context);
        QCOMPARE(game.completed.size(), 1); QCOMPARE(game.completed.first()[0].toULongLong(), second);
        QCOMPARE(qvariant_cast<QImage>(game.completed.first()[1]), latest);
        QVERIFY(game.completed.first()[2].toBool()); QCOMPARE(game.started.size(), 1);
        QVERIFY(!game.exit().requestExit());
    }
    void verifiedAutosaveCapturesBeforeClosingWithoutClaimingConfirmation() {
        RunningAdventure game; QVERIFY(game.start(AdventureSavePolicy::VerifiedAutosave));
        QTRY_VERIFY(!game.pid().isEmpty()); game.exit().setAvailable(true);
        QVERIFY(game.exit().requestExit()); QVERIFY(game.closes.isEmpty());
        game.exit().captureCompleted(game.attempt(), frame());
        QVERIFY(game.prompts.isEmpty()); QCOMPARE(game.closes.size(), 1);
        QVERIFY(game.command("exit")); QTRY_COMPARE(game.completed.size(), 1);
        QVERIFY(!game.completed.first()[2].toBool());
    }
    void captureFailureAndTimeoutKeepExitAndCancellationUsable_data() {
        QTest::addColumn<bool>("timeout");
        QTest::newRow("explicit-failure") << false;
        QTest::newRow("deadline") << true;
    }
    void captureFailureAndTimeoutKeepExitAndCancellationUsable() {
        QFETCH(bool, timeout);
        RunningAdventure game; QVERIFY(game.start()); QTRY_VERIFY(!game.pid().isEmpty());
        game.exit().setAvailable(true); QVERIFY(game.exit().requestExit());
        if (!timeout) game.exit().captureCompleted(game.attempt(), frame(), "Capture unavailable");
        QTRY_COMPARE_WITH_TIMEOUT(game.exit().phase(), Phase::Confirming, 3500);
        QVERIFY(!game.exit().captureError().isEmpty());
        // A valid-looking but late frame cannot replace a reported failure.
        game.exit().captureCompleted(game.attempt(), frame());
        QVERIFY(game.exit().confirm()); QVERIFY(game.command("exit"));
        QTRY_COMPARE(game.completed.size(), 1);
        QVERIFY(qvariant_cast<QImage>(game.completed.first()[1]).isNull());
    }
    void cancellationWhileCapturingAndLostCapabilityDoNotStopGame() {
        RunningAdventure game; QVERIFY(game.start()); QTRY_VERIFY(!game.pid().isEmpty());
        game.exit().setAvailable(true); QVERIFY(game.exit().requestExit()); const auto first = game.attempt();
        QVERIFY(game.exit().cancel()); game.exit().captureCompleted(first, frame());
        QCOMPARE(game.exit().phase(), Phase::Idle); QVERIFY(game.prompts.isEmpty());
        QVERIFY(game.exit().requestExit());
        game.exit().captureCompleted(game.attempt(), {}, "Unavailable");
        game.exit().setAvailable(false);
        QCOMPARE(game.resumes.size(), 2); QCOMPARE(game.exit().phase(), Phase::Idle);
        QVERIFY(game.process.active()); QVERIFY(game.closes.isEmpty()); QVERIFY(game.completed.isEmpty());
    }
    void newSessionRejectsOldCaptureAndDoesNotInheritAutosavePolicy() {
        RunningAdventure game; QVERIFY(game.start(AdventureSavePolicy::VerifiedAutosave));
        QTRY_VERIFY(!game.pid().isEmpty()); game.exit().setAvailable(true);
        QVERIFY(game.exit().requestExit()); const auto oldToken = game.attempt();
        QVERIFY(game.command("exit")); QTRY_COMPARE(game.returned.size(), 1);
        QVERIFY(QFile::remove(game.path("command"))); QVERIFY(QFile::remove(game.path("pid")));
        QVERIFY(game.start()); QTRY_VERIFY(!game.pid().isEmpty());
        QVERIFY(!game.exit().available()); QVERIFY(!game.exit().requestExit());
        game.exit().setAvailable(true);
        QVERIFY(game.exit().requestExit()); QVERIFY(game.attempt() > oldToken);
        game.exit().captureCompleted(oldToken, frame());
        QCOMPARE(game.exit().phase(), Phase::Capturing); QVERIFY(game.closes.isEmpty());
        game.exit().captureCompleted(game.attempt(), frame());
        QCOMPARE(game.exit().phase(), Phase::Confirming); QCOMPARE(game.prompts.size(), 1);
        QVERIFY(game.exit().cancel()); QCOMPARE(game.started.size(), 2);
    }
    void failedCloseRequiresFreshCaptureAndConfirmation() {
        RunningAdventure game; QVERIFY(game.start()); QTRY_VERIFY(!game.pid().isEmpty());
        game.exit().setAvailable(true); QVERIFY(game.exit().requestExit()); const auto first = game.attempt();
        game.exit().captureCompleted(first, frame()); QVERIFY(game.exit().confirm());
        game.exit().gracefulExitFailed(first, "Owned window no longer available");
        QCOMPARE(game.resumes.size(), 1); QVERIFY(game.process.active()); QVERIFY(game.completed.isEmpty());
        QVERIFY(game.exit().requestExit());
        game.exit().gracefulExitFailed(first, "Stale failure");
        QCOMPARE(game.exit().phase(), Phase::Capturing);
        game.exit().captureCompleted(game.attempt(), frame()); QCOMPARE(game.prompts.size(), 2);
        QVERIFY(game.exit().cancel()); QVERIFY(game.command("exit"));
        QTRY_COMPARE(game.returned.size(), 1); QVERIFY(game.completed.isEmpty());
    }
    void interruptedOrUnconfirmedExitNeverPublishesFrame_data() {
        QTest::addColumn<int>("stage"); QTest::addColumn<QByteArray>("outcome");
        QTest::newRow("during-capture") << 0 << QByteArray("exit");
        QTest::newRow("during-question") << 1 << QByteArray("exit");
        QTest::newRow("crash-after-confirm") << 2 << QByteArray("crash");
        QTest::newRow("error-after-confirm") << 2 << QByteArray("error");
        QTest::newRow("force-after-confirm") << 2 << QByteArray("force");
    }
    void interruptedOrUnconfirmedExitNeverPublishesFrame() {
        QFETCH(int, stage); QFETCH(QByteArray, outcome);
        RunningAdventure game; QVERIFY(game.start()); QTRY_VERIFY(!game.pid().isEmpty());
        game.exit().setAvailable(true); QVERIFY(game.exit().requestExit()); const auto token = game.attempt();
        if (stage > 0) game.exit().captureCompleted(token, frame());
        if (stage > 1) QVERIFY(game.exit().confirm());
        if (outcome == "force") game.launch.cancel(); else QVERIFY(game.command(outcome));
        QTRY_COMPARE_WITH_TIMEOUT(game.returned.size(), 1, 4000);
        QCOMPARE(game.launch.state(), outcome == "exit" ? "returned" : "failed");
        QVERIFY(game.completed.isEmpty()); QCOMPARE(game.exit().phase(), Phase::Idle);
        game.exit().captureCompleted(token, frame()); game.exit().endSession(true);
        QVERIFY(game.completed.isEmpty()); QCOMPARE(game.returned.size(), 1);
    }
};
QTEST_GUILESS_MAIN(AdventureExitTests)
#include "AdventureExitTests.moc"
