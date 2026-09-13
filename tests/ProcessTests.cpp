#include "core/navigation/AdventureLaunchController.h"
#include "core/storage/LocalStateStore.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

using namespace trainer;
class ProcessTests final : public QObject {
    Q_OBJECT
    QString probe() const {
        return QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "trainer_process_probe.exe"
#else
            "trainer_process_probe"
#endif
        );
    }
private slots:
    void preparationIsAsyncCancellableAndCannotStartLate() {
        ProcessService process;
        QSignalSpy started(&process, &ProcessService::started), done(&process, &ProcessService::finished);
        auto entered = std::make_shared<std::atomic_bool>(false);
        ProcessCommand command{probe(), {}, {}};
        command.prepare = [entered](ProcessCommand&, const std::atomic_bool& cancelled) {
            *entered = true;
            while (!cancelled) QThread::msleep(5);
            return QString();
        };
        QVERIFY(process.start(command));
        QTRY_VERIFY(*entered); QVERIFY(started.isEmpty());
        process.stop(); QCOMPARE(done.size(), 1);
        QVERIFY(process.start({probe(), {}, {}})); QTRY_COMPARE(done.size(), 2);
        QCOMPARE(started.size(), 1); QCOMPARE(done.last()[0].toInt(), 0);
        QTest::qWait(50); QCOMPARE(started.size(), 1);
    }
    void preparationFailureNeverStartsAChild() {
        ProcessService process;
        QSignalSpy started(&process, &ProcessService::started), done(&process, &ProcessService::finished);
        ProcessCommand command{probe(), {}, {}};
        command.prepare = [](ProcessCommand&, const std::atomic_bool&) { return QString("Selected state changed"); };
        QVERIFY(process.start(command)); QTRY_COMPARE(done.size(), 1);
        QVERIFY(started.isEmpty()); QCOMPARE(done.first()[2].toString(), "Selected state changed");
    }
    void adapterOutputFailureStopsOnlyItsOwnedChild() {
        ProcessService process;
        QSignalSpy done(&process, &ProcessService::finished);
        ProcessCommand command{probe(), {"output"}, {}};
        command.inspectOutput = [](const QByteArray& output) {
            return output.contains("original failure fixture") ? QString("Couldn't restore this moment") : QString();
        };
        QVERIFY(process.start(command)); QTRY_COMPARE(done.size(), 1);
        QCOMPARE(done.first()[2].toString(), "Couldn't restore this moment");
        QVERIFY(!process.active());
    }
    void checkpointsBeforeStartingAndRestores() {
        QTemporaryDir dir; LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
        ProcessService process; AdventureLaunchController launch(process);
        const QJsonObject context{{"version", 1}, {"page", "worlds"}, {"worlds", QJsonObject{{"world", "hoenn"}, {"route", "detail"}}}};
        QSignalSpy suspended(&launch, &AdventureLaunchController::suspendRequested), restored(&launch, &AdventureLaunchController::restoreRequested);
        connect(&launch, &AdventureLaunchController::checkpointRequested, this, [&](quint64 token, const QJsonObject& state) {
            QVERIFY(!process.active());
            store.saveNavigation(state, &launch, [&, token](const QString& error) { launch.checkpointCompleted(token, error); });
        });
        connect(&process, &ProcessService::started, this, [&] { QCOMPARE(store.navigation(), context); });
        QVERIFY(launch.launch({probe(), {}, dir.path()}, context)); QVERIFY(!launch.launch({probe(), {}, {}}, {}));
        QCOMPARE(launch.state(), "preparing"); QTRY_COMPARE(restored.size(), 1); QCOMPARE(suspended.size(), 1);
        QCOMPARE(launch.state(), "returned"); QCOMPARE(restored.first().first().toJsonObject(), context);
        QVERIFY(launch.launch({probe() + ".missing", {}, {}}, context)); QTRY_COMPARE(restored.size(), 2);
        QCOMPARE(launch.state(), "failed"); QVERIFY(!launch.error().isEmpty()); QCOMPARE(suspended.size(), 1);
        QVERIFY(launch.launch({probe(), {"error"}, {}}, context)); QTRY_COMPARE(restored.size(), 3); QCOMPARE(launch.state(), "failed");
    }
    void failedOrCancelledCheckpointNeverStarts() {
        ProcessService process; AdventureLaunchController launch(process);
        QSignalSpy checkpoint(&launch, &AdventureLaunchController::checkpointRequested), started(&process, &ProcessService::started);
        QVERIFY(launch.launch({probe(), {}, {}}, {})); const auto token = checkpoint.last()[0].toULongLong();
        launch.checkpointCompleted(token, "Storage unavailable"); QCOMPARE(launch.state(), "failed"); QVERIFY(started.isEmpty());
        QVERIFY(launch.launch({probe(), {}, {}}, {})); const auto cancelled = checkpoint.last()[0].toULongLong();
        launch.cancel(); launch.checkpointCompleted(cancelled, {}); QTest::qWait(150); QVERIFY(started.isEmpty()); QCOMPARE(launch.state(), "returned");
    }
    void actualCrashAndStopRestoreExactlyOnce() {
        ProcessService process; AdventureLaunchController launch(process);
        connect(&launch, &AdventureLaunchController::checkpointRequested, &launch, [&](quint64 token, const QJsonObject&) { launch.checkpointCompleted(token, {}); });
        QSignalSpy restored(&launch, &AdventureLaunchController::restoreRequested);
        QVERIFY(launch.launch({probe(), {"crash"}, {}}, {})); QTRY_COMPARE_WITH_TIMEOUT(restored.size(), 1, 6000);
        QCOMPARE(launch.state(), "failed"); QVERIFY(!process.active());
        QVERIFY(launch.launch({probe(), {"wait"}, {}}, {})); QTRY_COMPARE(launch.state(), "running");
        launch.cancel(); QTRY_COMPARE_WITH_TIMEOUT(restored.size(), 2, 4000); QCOMPARE(launch.state(), "returned");
        QTest::qWait(50); QCOMPARE(restored.size(), 2);
    }
    void argumentsAreLiteralWithoutShellInterpretation() {
        QTemporaryDir dir; const auto path = dir.path() + "/echo result.txt";
        const QString literal = "space ; & `text` $(echo danger) ' \" %PATH% — 42";
        ProcessService process; QSignalSpy done(&process, &ProcessService::finished);
        QVERIFY(process.start({probe(), {"echo", path, literal}, dir.path()})); QTRY_COMPARE(done.size(), 1);
        QCOMPARE(done.first()[0].toInt(), 0); QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(QString::fromUtf8(file.readAll()), literal);
    }
};
QTEST_GUILESS_MAIN(ProcessTests)
#include "ProcessTests.moc"
