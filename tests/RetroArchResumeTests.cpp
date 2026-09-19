#include "integrations/adventure/retroarch/RetroArchResume.h"
#include "core/repository/ResumeLibraryRepository.h"
#include "core/storage/LocalStateStore.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QDirIterator>

using namespace trainer;
namespace {
void write(const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write(bytes), bytes.size());
}
struct Fixture {
    QTemporaryDir dir;
    AdventureRegistration record;
    RetroArchInstallation installation;
    QString state;
    std::atomic_bool cancelled{false};
    Fixture() {
        record.adventure.id = "original-adventure"; record.adventure.adapterId = "retroarch";
        record.adventure.platformId = "gba"; record.contentPath = dir.filePath("content/original.gba");
        record.integrationConfig = {{"core", "mgba"}};
        installation.program = QCoreApplication::applicationFilePath();
        installation.configFile = dir.filePath("retroarch.cfg");
        installation.cores = {{"mgba", dir.filePath("cores/mgba.so")}};
        installation.runtimeFile = dir.filePath("runtime"); installation.resumeDirectory = dir.filePath("moments");
        write(record.contentPath, "original content fixture");
        write(installation.cores["mgba"], "original core fixture"); write(installation.runtimeFile, "original runtime fixture");
        write(installation.configFile, "savestate_directory = \"" + dir.filePath("states").toUtf8() + "\"\ncore_options_path = \"\"\n");
        state = dir.filePath("states/original.state"); write(state, "original state fixture");
    }
    RetroArchResumeSnapshot scan(const QList<ResumePoint>& previous = {}) {
        return scanRetroArchMoments({record}, installation, previous, cancelled);
    }
};
}
class RetroArchResumeTests final : public QObject {
    Q_OBJECT
private slots:
    void queuedRefreshDiscardsOldResultWithoutLosingRequestedAdventure() {
        Fixture f; LocalStateStore store(f.dir.filePath("database")); store.open(); QTRY_VERIFY(store.ready());
        f.record.adventure.worldId = "hoenn"; f.record.adventure.title = "Original fixture";
        int writes = 0;
        store.saveAdventureAsync(f.record, this, [&](LibraryWriteResult r) { if (r.success) ++writes; });
        QTRY_COMPARE(writes, 1);
        auto other = f.record; other.adventure.id = "second-original";
        store.saveAdventureAsync(other, this, [&](LibraryWriteResult r) { if (r.success) ++writes; });
        QTRY_COMPARE(writes, 2);
        RetroArchResumeProvider provider(store, f.installation);
        int updates = 0; QList<ResumePoint> points;
        connect(&provider, &RetroArchResumeProvider::updated, this, [&](const QList<ResumePoint>& snapshot) { ++updates; points = snapshot; });
        provider.refresh(f.record.adventure.id); provider.refresh(other.adventure.id);
        QTRY_COMPARE(updates, 1); QCOMPARE(points.size(), 2);
        QVERIFY(points[0].adventureId != points[1].adventureId);
        QTest::qWait(50); QCOMPARE(updates, 1);
    }
    void discoveryPinsBytesAndKeepsSourceTime() {
        Fixture f;
        const auto first = f.scan(); QCOMPARE(first.points.size(), 1);
        const auto point = first.points.first(); QCOMPARE(point.availability, ResumeAvailability::Exact);
        QVERIFY(point.source.complete()); QCOMPARE(point.savedAt, QFileInfo(f.state).lastModified().toUTC());
        QVERIFY(point.previewKey.isEmpty());
        write(f.state, "different state bytes!");
        QFile state(f.state); QVERIFY(state.open(QIODevice::ReadWrite)); QVERIFY(state.setFileTime(point.savedAt, QFileDevice::FileModificationTime)); state.close();
        const auto second = f.scan(first.points); QCOMPARE(second.points.size(), 1);
        QCOMPARE(second.points[0].id, point.id); QVERIFY(second.points[0].source.revision != point.source.revision);
        QCOMPARE(second.points[0].savedAt, point.savedAt);
        QCOMPARE(second.points[0].source.integrationRevision, point.source.integrationRevision);
        QVERIFY(QFile::remove(f.state));
        const auto missing = f.scan(second.points); QCOMPARE(missing.points.size(), 1);
        QCOMPARE(missing.points[0].availability, ResumeAvailability::Missing);
        QCOMPARE(missing.points[0].source, second.points[0].source);
    }
    void coreContentAndOverridesInvalidateOrLimitExactResume() {
        Fixture f; const auto first = f.scan().points.first();
        write(f.record.contentPath, "changed original content");
        const auto second = f.scan().points.first(); QVERIFY(second.source.integrationRevision != first.source.integrationRevision);
        write(f.installation.cores["mgba"], "changed original core");
        const auto third = f.scan().points.first(); QVERIFY(third.source.integrationRevision != second.source.integrationRevision);
        write(f.dir.filePath("config/mGBA/mGBA.cfg"), "unverified_override = \"true\"\n");
        QCOMPARE(f.scan().points.first().availability, ResumeAvailability::LaunchOnly);
        write(f.installation.configFile, "#include \"external.cfg\"\n");
        const auto unchecked = f.scan({third}); QCOMPARE(unchecked.points.first().availability, ResumeAvailability::Stale);
    }
    void thumbnailsBelongToTheObservedRevisionAndAreOptional() {
        Fixture f;
        QImage image(240, 160, QImage::Format_RGB32); image.fill(QColor("#327c84")); QVERIFY(image.save(f.state + ".png"));
        auto first = f.scan(); QVERIFY(!first.points[0].previewKey.isEmpty()); QCOMPARE(first.previews.size(), 1);
        const auto key = first.points[0].previewKey;
        write(f.state, "different original state");
        auto second = f.scan(); QVERIFY(second.points[0].previewKey != key);
        QVERIFY(QFile::remove(f.state + ".png"));
        second = f.scan(); QVERIFY(second.points[0].previewKey.isEmpty());
        QCOMPARE(second.points[0].availability, ResumeAvailability::Exact);
    }
    void replacedOrForeignMomentNeverReachesValidationProcess() {
        Fixture f; auto point = f.scan().points.first();
        ProcessCommand cmd{f.installation.program, {"--config", f.installation.configFile, f.record.contentPath}, {}};
        const auto args = cmd.arguments;
        write(f.state, "replacement state fixture");
        QVERIFY(!prepareRetroArchResume(cmd, f.record, point, f.installation, f.cancelled).isEmpty());
        QCOMPARE(cmd.arguments, args);
        point.adventureId = "someone-else";
        QVERIFY(!prepareRetroArchResume(cmd, f.record, point, f.installation, f.cancelled).isEmpty());
        QCOMPARE(cmd.arguments, args);
    }
    void normalLaunchDisablesStateAutosavesAndPreservesNativeSavePaths() {
        Fixture f;
        auto command = [&] {
            ProcessCommand result{f.installation.program, {"--config", f.installation.configFile, f.record.contentPath}, {}};
            const auto error = prepareRetroArchResume(result, f.record, {}, f.installation, f.cancelled);
            if (!error.isEmpty()) qWarning() << error;
            return result;
        };
        const auto first = command(), second = command();
        QVERIFY(first.arguments.contains("--appendconfig")); QVERIFY(first.arguments != second.arguments);
        const auto config = first.arguments[first.arguments.indexOf("--appendconfig") + 1];
        QFile file(config); QVERIFY(file.open(QIODevice::ReadOnly)); const auto bytes = file.readAll();
        QVERIFY(!bytes.contains("savefile_directory")); QVERIFY(bytes.contains("savestate_auto_save = \"false\""));
        QVERIFY(bytes.contains("savestate_auto_load = \"false\"")); QVERIFY(bytes.contains("savestate_thumbnail_enable = \"false\""));
        QVERIFY(!first.arguments.contains("--entryslot"));
        QFile original(f.state); QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), QByteArray("original state fixture"));
    }
    void discoveryIsBoundedAndDoesNotTreatEntryCopiesAsNewStates() {
        Fixture f;
        for (int n = 1; n < 20; ++n) write(f.state + QString::number(n), "another original fixture");
        write(f.state + "0.entry", "not a new state"); write(f.state + ".png", "not an image");
        const auto snapshot = f.scan(); QCOMPARE(snapshot.points.size(), 12);
        for (const auto& point : snapshot.points) QVERIFY(!point.adapterPayload["path"].toString().endsWith(".entry"));
    }
    void validationOnlyDirectoriesNeverBecomeGameplayCards() {
        Fixture f;
        const auto identity = QString::fromLatin1(QCryptographicHash::hash(f.record.adventure.id.toUtf8(), QCryptographicHash::Sha256).toHex());
        const auto folder = QDir(f.installation.resumeDirectory).filePath(identity + "/unlaunched");
        write(QDir(folder).filePath("original.state.auto"), "not a real gameplay moment");
        QCOMPARE(f.scan().points.size(), 1); // Only the real native state.
        write(QDir(folder).filePath("launch.cfg"), "original launch marker");
        QCOMPARE(f.scan().points.size(), 2);
    }
    void validatorConfigurationCannotEnableExitSaveThroughDuplicateKeys() {
        Fixture f;
        f.installation.program = QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "trainer_process_probe.exe"
#else
            "trainer_process_probe"
#endif
        );
        const auto point = f.scan().points.first();
        ProcessCommand command{f.installation.program, {f.record.contentPath}, {}};
        QVERIFY(!prepareRetroArchResume(command, f.record, point, f.installation, f.cancelled).isEmpty());
        QDirIterator files(f.installation.resumeDirectory, {"check.cfg"}, QDir::Files, QDirIterator::Subdirectories);
        QVERIFY(files.hasNext()); QFile cfg(files.next()); QVERIFY(cfg.open(QIODevice::ReadOnly));
        const auto bytes = cfg.readAll(); QSet<QByteArray> keys;
        for (const auto& line : bytes.split('\n')) {
            if (!line.contains('=')) continue;
            const auto key = line.left(line.indexOf('=')).trimmed(); QVERIFY(!keys.contains(key)); keys.insert(key);
        }
        QVERIFY(bytes.contains("savestate_auto_save = \"false\""));
        QVERIFY(bytes.contains("savestate_thumbnail_enable = \"false\""));
        QVERIFY(!files.hasNext());
    }
};
QTEST_GUILESS_MAIN(RetroArchResumeTests)
#include "RetroArchResumeTests.moc"
