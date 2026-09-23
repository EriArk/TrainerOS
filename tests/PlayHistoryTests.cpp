#include "LegacyStoreFixture.h"
#include "core/storage/LocalStateStore.h"
#include "features/home/PlayHistoryController.h"
#include "features/home/ExitImage.h"
#include <QPainter>
#include <QBuffer>
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
    QString saveMedia(LocalStateStore& store, const PlaySession& value, const std::optional<ExitMediaSource>& source,
                      const std::optional<ExitCapture>& capture = {}) {
        bool done = false; QString error;
        store.saveSessionMediaAsync(value, source, capture, this, [&](const QString& e) { error = e; done = true; });
        QElapsedTimer wait; wait.start();
        while (!done && wait.elapsed() < 3000) QTest::qWait(10);
        return done ? error : "Timed out";
    }
    void profile(LocalStateStore& store) {
        bool done = false; bool ok = false;
        store.saveAsync({"owner", "Fixture", "compass", {}, QDateTime::currentDateTimeUtc()}, this,
            [&](ProfileWriteResult result) { ok = result.success; done = true; });
        QTRY_VERIFY(done); QVERIFY(ok);
    }
private slots:
    void multiverseExitMediaKeepsItsDomainAcrossRestart() {
        QTemporaryDir dir;const auto path=content(dir);const auto now=QDateTime::currentDateTimeUtc();
        QImage frame(64,36,QImage::Format_RGB32);frame.fill(Qt::blue);
        {
            LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());profile(store);
            AdventureRegistration r;r.adventure.id="general";r.adventure.title="General fixture";
            r.adventure.domain="multiverse";r.adventure.platformId="gba";r.adventure.adapterId="unconfigured";r.contentPath=path;
            bool done=false;store.saveAdventureAsync(r,this,[&](auto result){QVERIFY(result.success);done=true;});QTRY_VERIFY(done);
            ExitMediaSource source{"owner","multiverse",*store.registration("general")};
            PlaySession value{"general-exit","general",now,{},{},PlaySessionOutcome::Running};
            QVERIFY(saveMedia(store,value,source).isEmpty());
            value.outcome=PlaySessionOutcome::Returned;value.endedAt=now.addSecs(3);value.elapsedSeconds=3;
            QVERIFY(saveMedia(store,value,source,ExitCapture{frame,now}).isEmpty());
            QVERIFY(store.exitMedia("general"));QCOMPARE(store.exitMedia("general")->domain,"multiverse");
            QVERIFY(store.home().activeAdventureId.isEmpty());
        }
        LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
        QVERIFY(store.exitMedia("general"));QCOMPARE(store.exitMedia("general")->domain,"multiverse");
        QVERIFY(!store.exitImage("general-exit").isNull());QVERIFY(store.home().activeAdventureId.isEmpty());
    }
    void exitPicturesFillFromContentWithoutDestroyingCapture() {
        QImage frame(960, 540, QImage::Format_RGB32); frame.fill(Qt::black);
        QPainter painter(&frame); painter.fillRect(75, 0, 810, 540, QColor("#54ada6")); painter.end();
        QCOMPARE(frameExitImage(frame).size(), QSize(810, 540));
        QCOMPARE(frame.size(), QSize(960, 540));
        QCOMPARE(frame.pixelColor(0, 200), QColor(Qt::black));
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
        QVERIFY(frame.save(&buffer, "JPEG", 85));
        const auto cropped = frameExitImage(QImage::fromData(bytes));
        QVERIFY(cropped.width() >= 810 && cropped.width() <= 814);
        QVERIFY(cropped.pixelColor(0, 200).value() > 24);
        // A dark scene, asymmetric shadows and full-frame media must not zoom.
        frame.fill(Qt::black); QCOMPARE(frameExitImage(frame), frame);
        frame.fill(QColor("#54ada6")); QCOMPARE(frameExitImage(frame), frame);
        painter.begin(&frame); painter.fillRect(0, 0, 75, 540, Qt::black); painter.end();
        QCOMPARE(frameExitImage(frame), frame);
        frame.fill(Qt::black); painter.begin(&frame);
        painter.fillRect(0, 60, 960, 420, QColor("#ddb467")); painter.end();
        QCOMPARE(frameExitImage(frame).size(), QSize(960, 420));
    }
    void exitPictureSurvivesRestartAndRejectsForeignOrChangedSources() {
        QTemporaryDir dir; const auto path = content(dir);
        QImage frame(64, 36, QImage::Format_RGB32); frame.fill(qRgb(20, 110, 80));
        const auto now = QDateTime::currentDateTimeUtc();
        {
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
            addAdventure(store, path); profile(store);
            ExitMediaSource source{"owner", "pokemon", *store.registration("journey")};
            PlaySession value{"clean", "journey", now, {}, {}, PlaySessionOutcome::Running};
            QVERIFY(saveMedia(store, value, source).isEmpty()); QVERIFY(!store.exitMedia("journey"));
            value.outcome = PlaySessionOutcome::Returned; value.endedAt = now.addSecs(5); value.elapsedSeconds = 5;
            QVERIFY(saveMedia(store, value, source, ExitCapture{frame, now}).isEmpty());
            QVERIFY(store.exitMedia("journey")); QCOMPARE(store.exitMedia("journey")->sessionId, "clean");
            QCOMPARE(store.exitMedia("journey")->buildSha256.size(), 64);
            QVERIFY(!store.exitImage("clean").isNull()); QVERIFY(store.exitImage("foreign").isNull());
            // Uncaptured/crashed newer sessions cannot replace the good image.
            value = {"crashed", "journey", now, {}, {}, PlaySessionOutcome::Running};
            QVERIFY(saveMedia(store, value, source).isEmpty());
            value.outcome = PlaySessionOutcome::Failed; value.endedAt = now; value.elapsedSeconds = 1;
            QVERIFY(!saveMedia(store, value, source, ExitCapture{frame, now}).isEmpty());
            QCOMPARE(store.exitMedia("journey")->sessionId, "clean");
            QCOMPARE(store.recentSessions().first().outcome, PlaySessionOutcome::Failed);
            auto foreign = source; foreign.trainerId = "another-owner";
            value = {"foreign", "journey", now, {}, {}, PlaySessionOutcome::Running};
            QVERIFY(saveMedia(store, value, foreign).isEmpty());
            value.outcome = PlaySessionOutcome::Returned; value.endedAt = now; value.elapsedSeconds = 1;
            QVERIFY(!saveMedia(store, value, foreign, ExitCapture{frame, now}).isEmpty());
            QCOMPARE(store.exitMedia("journey")->sessionId, "clean");
        }
        {
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
            QVERIFY(store.exitMedia("journey")); QCOMPARE(store.exitMedia("journey")->sessionId, "clean");
            QVERIFY(!store.exitImage("clean").isNull());
            auto record = *store.registration("journey"); record.adventure.title = "Changed edition";
            bool done = false;
            store.saveAdventureAsync(record, this, [&](LibraryWriteResult result) { done = result.success; });
            QTRY_VERIFY(done); QVERIFY(!store.exitMedia("journey")); QVERIFY(store.exitImage("clean").isNull());
        }
        LocalStateStore reopened(dir.path()); reopened.open(); QTRY_VERIFY(reopened.ready());
        QVERIFY(!reopened.exitMedia("journey")); QCOMPARE(*reopened.recordedSeconds("journey"), 7);
    }
    void mediaWriteFailureKeepsPriorImageAndCommitsRealReturn() {
        QTemporaryDir dir; const auto path = content(dir);
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready()); addAdventure(store, path); profile(store);
        ExitMediaSource source{"owner", "pokemon", *store.registration("journey")};
        const auto now = QDateTime::currentDateTimeUtc();
        QImage frame(16, 16, QImage::Format_RGB32); frame.fill(Qt::green);
        auto round = [&](const QString& id) {
            PlaySession value{id, "journey", now, {}, {}, PlaySessionOutcome::Running};
            auto error = saveMedia(store, value, source);
            if (!error.isEmpty()) return error;
            value.outcome = PlaySessionOutcome::Returned; value.endedAt = now; value.elapsedSeconds = 4;
            return saveMedia(store, value, source, ExitCapture{frame, now});
        };
        QVERIFY(round("first").isEmpty());
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "media-failure"); db.setDatabaseName(dir.filePath("traineros.sqlite3")); QVERIFY(db.open());
            { QSqlQuery q(db); QVERIFY(q.exec("CREATE TRIGGER refuse_media BEFORE UPDATE ON exit_media BEGIN SELECT RAISE(ABORT,'fixture'); END")); }
            db.close();
        }
        QSqlDatabase::removeDatabase("media-failure");
        QVERIFY(!round("second").isEmpty());
        QCOMPARE(store.recentSessions().first().id, "second"); QCOMPARE(store.recentSessions().first().outcome, PlaySessionOutcome::Returned);
        QCOMPARE(*store.recordedSeconds("journey"), 8); QCOMPARE(store.exitMedia("journey")->sessionId, "first");
    }
    void controllerPublishesOnlyConfirmedCleanProcessExit() {
        QTemporaryDir dir; const auto path = content(dir);
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready()); addAdventure(store, path); profile(store);
        ProcessService process; AdventureLaunchController launch(process); PlayHistoryController history(launch, store);
        history.setMediaSource([&](const QString& id) -> std::optional<ExitMediaSource> { return ExitMediaSource{"owner", "pokemon", *store.registration(id)}; });
        connect(&launch, &AdventureLaunchController::checkpointRequested, this, [&](quint64 token, const QJsonObject&) { launch.checkpointCompleted(token, {}); });
        const auto probe = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "trainer_process_probe.exe"
#else
            "trainer_process_probe"
#endif
        );
        QVERIFY(launch.launch({probe, {"controlled", dir.filePath("command"), dir.filePath("pid")}, {}}, {}, "journey"));
        QTRY_COMPARE(launch.state(), "running"); QTRY_COMPARE(store.pending(), 0);
        auto& exit = launch.exitController(); exit.setAvailable(true);
        QImage frame(48, 32, QImage::Format_RGB32); frame.fill(Qt::yellow);
        QVERIFY(exit.requestExit()); exit.captureCompleted(exit.attempt(), frame); QVERIFY(exit.cancel());
        QVERIFY(process.active()); QVERIFY(!store.exitMedia("journey"));
        QVERIFY(exit.requestExit()); exit.captureCompleted(exit.attempt(), frame); QVERIFY(exit.confirm());
        QVERIFY(!store.exitMedia("journey"));
        QFile command(dir.filePath("command")); QVERIFY(command.open(QIODevice::WriteOnly)); command.write("exit"); command.close();
        QTRY_VERIFY(!launch.active()); QTRY_COMPARE(store.pending(), 0);
        QVERIFY(store.exitMedia("journey")); QCOMPARE(store.exitMedia("journey")->sessionId, store.recentSessions().first().id);
        QVERIFY(history.error().isEmpty());
    }
    void damagedImageOrReplacedBuildNeverAppearsAfterRestart_data() {
        QTest::addColumn<bool>("replaceContent");
        QTest::newRow("corrupt-image") << false;
        QTest::newRow("same-size-and-time-different-build") << true;
    }
    void damagedImageOrReplacedBuildNeverAppearsAfterRestart() {
        QFETCH(bool, replaceContent);
        QTemporaryDir dir; const auto path = content(dir);
        const auto now = QDateTime::currentDateTimeUtc();
        {
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready()); addAdventure(store, path); profile(store);
            const ExitMediaSource source{"owner", "pokemon", *store.registration("journey")};
            PlaySession value{"clean", "journey", now, {}, {}, PlaySessionOutcome::Running};
            QVERIFY(saveMedia(store, value, source).isEmpty());
            value.outcome = PlaySessionOutcome::Returned; value.endedAt = now; value.elapsedSeconds = 1;
            QImage frame(24, 24, QImage::Format_RGB32); frame.fill(Qt::red);
            QVERIFY(saveMedia(store, value, source, ExitCapture{frame, now}).isEmpty()); QVERIFY(store.exitMedia("journey"));
        }
        if (replaceContent) {
            const QFileInfo original(path); const auto size = original.size(); const auto modified = original.lastModified();
            QFile file(path); QVERIFY(file.open(QIODevice::ReadWrite)); QCOMPARE(file.write(QByteArray(size, 'x')), size);
            QVERIFY(file.flush()); QVERIFY(file.setFileTime(modified, QFileDevice::FileModificationTime)); file.close();
            QCOMPARE(QFileInfo(path).size(), size); QCOMPARE(QFileInfo(path).lastModified(), modified);
        } else {
            {
                auto db = QSqlDatabase::addDatabase("QSQLITE", "media-damage"); db.setDatabaseName(dir.filePath("traineros.sqlite3")); QVERIFY(db.open());
                { QSqlQuery q(db); QVERIFY(q.exec("UPDATE exit_media SET jpeg=X'0001'")); } db.close();
            }
            QSqlDatabase::removeDatabase("media-damage");
        }
        LocalStateStore reopened(dir.path()); reopened.open(); QTRY_VERIFY(reopened.ready());
        QVERIFY(!reopened.exitMedia("journey")); QCOMPARE(reopened.recentSessions().first().outcome, PlaySessionOutcome::Returned);
        QCOMPARE(*reopened.recordedSeconds("journey"), 1); QVERIFY(reopened.registration("journey"));
    }
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
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "history-migration"); db.setDatabaseName(dir.filePath("traineros.sqlite3")); QVERIFY(db.open());
            QVERIFY(createLegacyStore(db,3));
            QSqlQuery q(db);
            q.prepare("INSERT INTO adventures(id,world_id,title,kind,description,content_path,adapter_id,config,revision) VALUES('journey','hoenn','History fixture',0,'',?,'unconfigured','{}',1)");
            q.addBindValue(path);QVERIFY(q.exec());q.finish();
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
