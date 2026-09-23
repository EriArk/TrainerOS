#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "integrations/adventure/retroarch/RetroArchSave.h"
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
    void cartridgeRoutes_data() {
        QTest::addColumn<QString>("platform"); QTest::addColumn<QString>("core"); QTest::addColumn<QString>("suffix"); QTest::addColumn<QString>("library");
        QTest::newRow("snes-zip") << "snes" << "snes9x" << "ZIP" << "Snes9x";
        QTest::newRow("megadrive-zip") << "megadrive" << "genesis_plus_gx" << "zip" << "Genesis Plus GX";
        QTest::newRow("32x") << "sega32x" << "picodrive" << "32x" << "PicoDrive";
        QTest::newRow("ngpc") << "ngpc" << "mednafen_ngp" << "ngc" << "Beetle NeoPop";
        QTest::newRow("pce") << "pcengine" << "mednafen_pce_fast" << "pce" << "Beetle PCE Fast";
    }
    void cartridgeRoutes() {
        QFETCH(QString, platform); QFETCH(QString, core); QFETCH(QString, suffix); QFETCH(QString, library);
        QTemporaryDir dir; const auto content=dir.filePath("literal ; title."+suffix); touch(content);
        const auto config=dir.filePath("retroarch.cfg");
        QFile settings(config); QVERIFY(settings.open(QIODevice::WriteOnly));
        settings.write("auto_overrides_enable = \"true\"\nsavestate_auto_load = \"true\"\n"); settings.close();
        LocalStateStore store(dir.filePath("data")); store.open(); QTRY_VERIFY(store.ready());
        RetroArchInstallation installation{probe(), {}, config, {{core, dir.filePath(core+".so")}}};
        installation.saves=std::make_shared<RetroArchSaveSession>(); // No accidental mGBA ownership gate.
        RetroArchAdapter adapter(store, installation);
        AdventureRegistration r; r.adventure.id="cartridge"; r.adventure.domain="multiverse";
        r.adventure.platformId=platform; r.adventure.title="Fixture"; r.adventure.adapterId="unconfigured"; r.contentPath=content;
        adapter.prepareInstallation(r); QCOMPARE(r.adventure.adapterId,"retroarch"); QCOMPARE(r.integrationConfig["core"].toString(),core);
        bool done=false; store.saveAdventureAsync(r,this,[&](auto result){QVERIFY(result.success);done=true;}); QTRY_VERIFY(done);
        std::optional<ProcessCommand> invocation;
        adapter.requestLaunch=[&](const auto& cmd,const auto&){invocation=cmd;return true;};
        QVERIFY(adapter.launch(r.adventure).success); QVERIFY(invocation && invocation->prepare);
        const auto original=*invocation; std::atomic_bool cancelled{false};
        QVERIFY(invocation->prepare(*invocation,cancelled).isEmpty());
        QCOMPARE(invocation->arguments.last(),content);
        QFile overlay(invocation->arguments[invocation->arguments.size()-2]); QVERIFY(overlay.open(QIODevice::ReadOnly));
        const auto bytes=overlay.readAll(); QVERIFY(bytes.contains("savestate_auto_load = \"false\""));
        QVERIFY(!bytes.contains("savefile_directory"));
        // An active core-specific override is not silently discarded.
        const auto overrides=dir.filePath("config/"+library); QVERIFY(QDir().mkpath(overrides));
        const auto overrideFile=overrides+"/"+library+".cfg"; touch(overrideFile);
        auto retry=original; QVERIFY(!retry.prepare(retry,cancelled).isEmpty());
        QVERIFY(QFile::remove(overrideFile)); cancelled=true; QVERIFY(!retry.prepare(retry,cancelled).isEmpty());
        auto wrong=r; wrong.contentPath=dir.filePath("disc.chd"); adapter.prepareInstallation(wrong);
        QCOMPARE(wrong.adventure.adapterId,"unconfigured");
        wrong=r; wrong.adventure.adapterId="unconfigured"; wrong.adventure.platformId.clear();
        adapter.prepareInstallation(wrong); QCOMPARE(wrong.adventure.adapterId,"unconfigured"); // Never infer a ZIP's platform.
        r=*store.registration(r.adventure.id); r.adventure.platformId="psx"; done=false;
        store.saveAdventureAsync(r,this,[&](auto result){QVERIFY(result.success);done=true;}); QTRY_VERIFY(done);
        QVERIFY(!adapter.capabilities(r.adventure).launch); // Core metadata cannot bypass the platform route.
    }
    void ordinaryLaunchDoesNotNeedOrCreateStateFolders() {
        QTemporaryDir dir;
        RetroArchInstallation installation;
        installation.configFile = dir.filePath("retroarch.cfg");
        QFile config(installation.configFile); QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("auto_overrides_enable = \"false\"\nsavestate_auto_save = \"true\"\n"); config.close();
        AdventureRegistration record; record.adventure.id = "ordinary";
        record.contentPath = dir.filePath("original.gba");
        std::atomic_bool cancelled{false};
        ProcessCommand command{probe(), {"--config", installation.configFile, record.contentPath}, {}};
        QVERIFY(prepareRetroArchLaunch(command, record, installation, cancelled).isEmpty());
        QCOMPARE(command.arguments.last(), record.contentPath);
        const auto overridePath = command.arguments[command.arguments.size() - 2];
        QFile overrideFile(overridePath); QVERIFY(overrideFile.open(QIODevice::ReadOnly));
        const auto bytes = overrideFile.readAll(); overrideFile.close();
        QVERIFY(bytes.contains("savestate_auto_save = \"false\""));
        QVERIFY(bytes.contains("savestate_auto_load = \"false\""));
        QVERIFY(!bytes.contains("savestate_directory")); QVERIFY(!bytes.contains("savefile_directory"));
        QCOMPARE(QDir(dir.path()).entryList(QDir::Dirs | QDir::NoDotAndDotDot).size(), 0);
        auto next = ProcessCommand{probe(), {record.contentPath}, {}};
        QVERIFY(prepareRetroArchLaunch(next, record, installation, cancelled).isEmpty());
        // A different file is a conflict, never permission to overwrite it.
        QVERIFY(overrideFile.open(QIODevice::WriteOnly)); overrideFile.write("USER SETTINGS"); overrideFile.close();
        next.arguments = {record.contentPath};
        QVERIFY(!prepareRetroArchLaunch(next, record, installation, cancelled).isEmpty());
        QVERIFY(overrideFile.open(QIODevice::ReadOnly)); QCOMPARE(overrideFile.readAll(), QByteArray("USER SETTINGS"));
        cancelled = true; QVERIFY(!prepareRetroArchLaunch(next, record, installation, cancelled).isEmpty());
    }
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
        adapter.requestLaunch = [&](const ProcessCommand& command, const QString& id) { return lifecycle.launch(command, context, id); };
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
        touch(dir.filePath("fceumm_libretro.so"));
        QJsonObject settings{{"version", 1}, {"program", probe()}, {"prefixArguments", QJsonArray{"run", "a literal argument"}},
            {"configFile", config}, {"coresDirectory", dir.path()}};
        const auto write = [&] { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write(QJsonDocument(settings).toJson()); };
        write(); auto installation = RetroArchInstallation::load(path);
        QCOMPARE(installation.program, probe()); QCOMPARE(installation.cores.size(), 2);
        QVERIFY(installation.cores.contains("mgba"));
        settings["backupProtocol"] = "mgba-sram-v1"; settings["runtimeFile"] = probe(); write();
        const auto ordinary = RetroArchInstallation::load(path);
        QVERIFY(ordinary.saveBackups); QCOMPARE(ordinary.runtimeFile, probe()); QVERIFY(ordinary.resumeDirectory.isEmpty());
        // A user-provided NES hack gets a real launch route only when its core
        // was discovered. An extension from a different platform cannot use it.
        MockLibraryRepository repository; RetroArchAdapter adapter(repository, installation);
        AdventureRegistration record; record.adventure.adapterId = "unconfigured";
        record.contentPath = dir.filePath("custom.NES"); adapter.prepareInstallation(record);
        QCOMPARE(record.adventure.platformId, "nes"); QCOMPARE(record.adventure.adapterId, "retroarch");
        QCOMPARE(record.integrationConfig["core"].toString(), "fceumm");
        record.contentPath = dir.filePath("wrong.gba"); adapter.prepareInstallation(record);
        QCOMPARE(record.adventure.adapterId, "unconfigured"); QVERIFY(!record.integrationConfig.contains("core"));
        record.contentPath = dir.filePath("custom.nes");
        auto missing = installation; missing.cores.remove("fceumm");
        RetroArchAdapter unavailable(repository, missing); unavailable.prepareInstallation(record);
        QCOMPARE(record.adventure.adapterId, "unconfigured");
        settings["prefixArguments"] = QJsonArray{42}; write(); QVERIFY(RetroArchInstallation::load(path).program.isEmpty());
        settings["prefixArguments"] = QJsonArray{};
        settings["program"] = "relative-path"; write(); QVERIFY(RetroArchInstallation::load(path).program.isEmpty());
        settings["program"] = probe(); settings["version"] = 2; write(); QVERIFY(RetroArchInstallation::load(path).program.isEmpty());
        QVERIFY(RetroArchInstallation::load(dir.filePath("missing")).program.isEmpty());
    }
};
QTEST_GUILESS_MAIN(RetroArchTests)
#include "RetroArchTests.moc"
