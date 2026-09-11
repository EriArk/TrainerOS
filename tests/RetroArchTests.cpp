#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "core/navigation/AdventureLaunchController.h"
#include "core/storage/LocalStateStore.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>

using namespace trainer;
class RetroArchTests final : public QObject {
    Q_OBJECT
    static void touch(const QString& path) { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("original test fixture"); }
    static QString probe() {
        return QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "trainer_process_probe.exe"
#else
            "trainer_process_probe"
#endif
        );
    }
private slots:
    void fileAttachmentPreparesOnlyAnInstalledMatchingCore() {
        MockLibraryRepository repository;
        RetroArchAdapter adapter(repository, {probe(), {}, "settings.cfg", {{"mgba", "mgba_libretro.so"}, {"gambatte", "gambatte_libretro.so"}, {"pokemini", "pokemini_libretro.so"}}});
        AdventureRegistration record; record.adventure.adapterId = "unconfigured"; record.contentPath = "/games/test.gba";
        adapter.prepareInstallation(record);
        QCOMPARE(record.adventure.platformId, "gba"); QCOMPARE(record.adventure.adapterId, "retroarch"); QCOMPARE(record.integrationConfig["core"].toString(), "mgba");
        record.contentPath = "/games/wrong.nds"; adapter.prepareInstallation(record);
        QCOMPARE(record.adventure.adapterId, "unconfigured"); QVERIFY(!record.integrationConfig.contains("core"));
        record.adventure.platformId = "pokemini"; record.contentPath = "/games/party.min"; adapter.prepareInstallation(record);
        QCOMPARE(record.adventure.adapterId, "retroarch"); QCOMPARE(record.integrationConfig["core"].toString(), "pokemini");
        record.adventure.platformId = "n64"; record.contentPath = "/games/stadium.z64"; adapter.prepareInstallation(record);
        QCOMPARE(record.adventure.adapterId, "unconfigured"); // Core not installed.
        record.adventure.adapterId = "another-adapter"; record.integrationConfig = {{"preserve", true}};
        adapter.prepareInstallation(record); QCOMPARE(record.adventure.adapterId, "another-adapter"); QVERIFY(record.integrationConfig["preserve"].toBool());
    }
    void launchUsesCommittedMetadataAndLiteralArguments() {
        QTemporaryDir dir;
        const auto content = dir.filePath("original ; $(unsafe) ' quoted adventure.gba"); touch(content);
        const auto core = dir.filePath("mgba_libretro.so"); touch(core);
        const auto config = dir.filePath("frontend settings.cfg"); touch(config);
        const auto resultFile = dir.filePath("arguments.json");
        LocalStateStore store(dir.filePath("data")); store.open(); QTRY_VERIFY(store.ready());
        AdventureRegistration record; record.adventure.id = "real-adapter-fixture";
        record.adventure.title = "Original test Adventure"; record.adventure.worldId = "hoenn";
        record.adventure.adapterId = "retroarch"; record.contentPath = content;
        record.integrationConfig = {{"core", "mgba"}};
        bool saved = false;
        store.saveAdventureAsync(record, this, [&](LibraryWriteResult r) { saved = r.success; }); QTRY_VERIFY(saved);
        RetroArchInstallation installation{probe(), {"arguments", resultFile}, config, {{"mgba", core}}};
        RetroArchAdapter adapter(store, installation);
        QVERIFY(adapter.capabilities(record.adventure).launch);
        QVERIFY(!adapter.capabilities(record.adventure).directResume);
        ProcessService process; AdventureLaunchController lifecycle(process);
        const QJsonObject context{{"page", "worlds"}};
        connect(&lifecycle, &AdventureLaunchController::checkpointRequested, &store,
                [&](quint64 token, const QJsonObject& state) {
            store.saveNavigation(state, &lifecycle, [&, token](const QString& error) { lifecycle.checkpointCompleted(token, error); });
        });
        adapter.requestLaunch = [&](const ProcessCommand& command) { return lifecycle.launch(command, context); };
        QSignalSpy restored(&lifecycle, &AdventureLaunchController::restoreRequested);
        const auto accepted = adapter.launch(record.adventure); QVERIFY(accepted.success && accepted.inProgress);
        QVERIFY(!adapter.launch(record.adventure).success); // No second child while checkpointing.
        QTRY_COMPARE(restored.size(), 1); QCOMPARE(lifecycle.state(), "returned"); QCOMPARE(store.navigation(), context);
        QFile output(resultFile); QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(output.readAll()).array(), QJsonArray::fromStringList(
            {"--fullscreen", "--config", config, "--libretro", core, content}));
        // Removing the media doesn't block metadata-only browsing. The external
        // program owns file loading and a failed child restores the shell.
        QVERIFY(QFile::remove(content)); QVERIFY(adapter.capabilities(record.adventure).launch);
        auto foreign = record.adventure; foreign.adapterId = "another-adapter";
        QVERIFY(!adapter.capabilities(foreign).launch); QVERIFY(!adapter.launch(foreign).success);
        foreign = record.adventure; foreign.id = "missing"; QVERIFY(!adapter.capabilities(foreign).launch);
        QVERIFY(!adapter.resume(record.adventure, {}).success);
        installation.cores.clear(); RetroArchAdapter missingCore(store, installation);
        QVERIFY(!missingCore.capabilities(record.adventure).launch);
    }
    void installationConfigurationRejectsIncompleteOrInvalidData() {
        QTemporaryDir dir; const auto path = dir.filePath("installation.json");
        const auto config = dir.filePath("retroarch.cfg"); touch(config);
        touch(dir.filePath("mgba_libretro.so"));
        QJsonObject settings{{"version", 1}, {"program", probe()}, {"prefixArguments", QJsonArray{"run", "a literal argument"}},
            {"configFile", config}, {"coresDirectory", dir.path()}};
        const auto write = [&] { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(QJsonDocument(settings).toJson()); };
        write(); auto installation = RetroArchInstallation::load(path);
        QCOMPARE(installation.program, probe()); QCOMPARE(installation.cores.size(), 1);
        QVERIFY(installation.cores.contains("mgba"));
        settings["prefixArguments"] = QJsonArray{42}; write(); QVERIFY(RetroArchInstallation::load(path).program.isEmpty());
        settings["prefixArguments"] = QJsonArray{};
        settings["program"] = "relative-path"; write(); QVERIFY(RetroArchInstallation::load(path).program.isEmpty());
        settings["program"] = probe(); settings["version"] = 2; write(); QVERIFY(RetroArchInstallation::load(path).program.isEmpty());
        QVERIFY(RetroArchInstallation::load(dir.filePath("missing")).program.isEmpty());
    }
};
QTEST_GUILESS_MAIN(RetroArchTests)
#include "RetroArchTests.moc"
