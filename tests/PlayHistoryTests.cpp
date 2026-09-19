#include "core/storage/LocalStateStore.h"
#include "features/home/PlayHistoryController.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSqlQuery>

using namespace trainer;
class PlayHistoryTests final : public QObject {
    Q_OBJECT
    void addAdventure(LocalStateStore& store, const QString& path, const QString& id = "journey") {
        AdventureRegistration r; r.adventure.id = id; r.adventure.title = "History fixture";
        r.adventure.worldId = "hoenn"; r.adventure.adapterId = "unconfigured"; r.contentPath = path;
        bool done = false; QString error;
        store.saveAdventureAsync(r, this, [&](LibraryWriteResult result) { done = true; error = result.error; });
        QTRY_VERIFY(done); QVERIFY2(error.isEmpty(), qPrintable(error));
    }
    QString save(LocalStateStore& store, const PlaySession& value) {
        bool done = false; QString error;
        store.saveSessionAsync(value, this, [&](const QString& e) { error = e; done = true; });
        QElapsedTimer wait; wait.start();
        while (!done && wait.elapsed() < 3000) QTest::qWait(10);
        return done ? error : "Timed out";
    }
    QString content(QTemporaryDir& dir) {
        const auto path = dir.filePath("original.bin"); QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return {};
        f.write("Content-free history fixture"); return path;
    }
private slots:
    void durableIdentityTotalsAndInterruptedRecovery() {
        QTemporaryDir dir; const auto path = content(dir);
        const auto time = QDateTime::fromString("2026-09-01T10:00:00.000Z", Qt::ISODateWithMs);
        {
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
            addAdventure(store, path); addAdventure(store, path, "other");
            PlaySession first{"first", "journey", time, {}, {}, PlaySessionOutcome::Running};
            QVERIFY(save(store, first).isEmpty()); QVERIFY(!store.recordedSeconds("journey"));
            auto wrong = first; wrong.adventureId = "other"; wrong.endedAt = time.addSecs(120); wrong.elapsedSeconds = 120; wrong.outcome = PlaySessionOutcome::Returned;
            QVERIFY(!save(store, wrong).isEmpty());
            first.endedAt = time.addSecs(120); first.elapsedSeconds = 120; first.outcome = PlaySessionOutcome::Returned;
            QVERIFY(save(store, first).isEmpty()); QVERIFY(!save(store, first).isEmpty()); // No double counting.
            PlaySession second{"second", "journey", time.addSecs(200), {}, {}, PlaySessionOutcome::Running};
            QVERIFY(save(store, second).isEmpty());
            // A wall-clock adjustment does not invalidate monotonic elapsed time.
            second.endedAt = time.addSecs(-20); second.elapsedSeconds = 80; second.outcome = PlaySessionOutcome::Returned;
            QVERIFY(save(store, second).isEmpty());
            QCOMPARE(store.recentSessions().size(), 1); QCOMPARE(*store.recordedSeconds("journey"), 200);
            PlaySession open{"open", "other", time.addSecs(-500), {}, {}, PlaySessionOutcome::Running};
            QVERIFY(save(store, open).isEmpty()); QCOMPARE(store.home().activeAdventureId, "other");
            QVERIFY(!store.home().badges); QVERIFY(!store.home().caught);
        }
        LocalStateStore reopened(dir.path()); reopened.open(); QTRY_VERIFY(reopened.ready());
        QCOMPARE(reopened.recentSessions().size(), 2);
        const auto interrupted = reopened.recentSessions().first();
        QCOMPARE(interrupted.id, "open"); QCOMPARE(interrupted.outcome, PlaySessionOutcome::Interrupted);
        QVERIFY(!interrupted.endedAt.isValid()); QVERIFY(!interrupted.elapsedSeconds);
        QCOMPARE(*reopened.recordedSeconds("journey"), 200); QVERIFY(!reopened.recordedSeconds("other"));
        QFile original(path); QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), "Content-free history fixture");
    }
    void realProcessesRecordOnlyStartedChildren() {
        QTemporaryDir dir; const auto path = content(dir);
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready()); addAdventure(store, path);
        ProcessService process; AdventureLaunchController launch(process); PlayHistoryController history(launch, store);
        QSignalSpy returned(&launch, &AdventureLaunchController::restoreRequested);
        QSignalSpy failures(&history, &PlayHistoryController::writeFailed);
        QString checkpointError;
        connect(&launch, &AdventureLaunchController::checkpointRequested, this, [&](quint64 token, const QJsonObject&) { launch.checkpointCompleted(token, checkpointError); });
        const auto probe = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "trainer_process_probe.exe"
#else
            "trainer_process_probe"
#endif
        );
        checkpointError = "Cannot checkpoint";
        QVERIFY(launch.launch({probe, {}, {}}, {}, "journey")); QTRY_COMPARE(returned.size(), 1);
        QVERIFY(store.recentSessions().isEmpty()); checkpointError.clear();
        QVERIFY(launch.launch({probe + ".missing", {}, {}}, {}, "journey")); QTRY_COMPARE(returned.size(), 2);
        QVERIFY(store.recentSessions().isEmpty());
        QVERIFY(launch.launch({probe, {"wait"}, {}}, {}, "journey"));
        QTRY_COMPARE(launch.state(), "running"); QTRY_COMPARE(store.recentSessions().size(), 1);
        QVERIFY(!launch.launch({probe, {}, {}}, {}, "other"));
        QTest::qWait(1100); launch.cancel(); QTRY_COMPARE(returned.size(), 3); QTRY_COMPARE(store.pending(), 0);
        QCOMPARE(store.recentSessions().first().adventureId, "journey");
        QCOMPARE(store.recentSessions().first().outcome, PlaySessionOutcome::Failed);
        QVERIFY(store.recentSessions().first().elapsedSeconds && *store.recentSessions().first().elapsedSeconds >= 1);
        const auto oldId = store.recentSessions().first().id;
        QVERIFY(launch.launch({probe, {}, {}}, {}, "journey")); QTRY_COMPARE(returned.size(), 4); QTRY_COMPARE(store.pending(), 0);
        QCOMPARE(store.recentSessions().first().outcome, PlaySessionOutcome::Returned);
        QVERIFY(store.recentSessions().first().id != oldId);
        QVERIFY(launch.launch({probe, {"crash"}, {}}, {}, "journey")); QTRY_COMPARE(returned.size(), 5); QTRY_COMPARE(store.pending(), 0);
        QCOMPARE(store.recentSessions().first().outcome, PlaySessionOutcome::Failed);
        QVERIFY(history.error().isEmpty()); QCOMPARE(failures.size(), 0);
    }
    void schemaThreeMigrationPreservesLibrary() {
        QTemporaryDir dir; const auto path = content(dir);
        { LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready()); addAdventure(store, path); }
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "history-migration"); db.setDatabaseName(dir.filePath("traineros.sqlite3")); QVERIFY(db.open());
            { QSqlQuery q(db); QVERIFY(q.exec("DROP TABLE play_sessions")); QVERIFY(q.exec("DROP TABLE hall_of_fame")); QVERIFY(q.exec("DROP TABLE pokedex_records")); QVERIFY(q.exec("PRAGMA user_version=3")); }
            db.close();
        }
        QSqlDatabase::removeDatabase("history-migration");
        LocalStateStore reopened(dir.path()); reopened.open(); QTRY_VERIFY(reopened.ready());
        QVERIFY(reopened.registration("journey")); QCOMPARE(reopened.registration("journey")->contentPath, path);
        QVERIFY(reopened.recentSessions().isEmpty());
        PlaySession invalid{"bad", "absent", QDateTime::currentDateTimeUtc(), {}, {}, PlaySessionOutcome::Running};
        QVERIFY(!save(reopened, invalid).isEmpty()); QVERIFY(reopened.recentSessions().isEmpty());
    }
};
QTEST_GUILESS_MAIN(PlayHistoryTests)
#include "PlayHistoryTests.moc"
