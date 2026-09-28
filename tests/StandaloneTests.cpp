#include "integrations/adventure/AdapterRouter.h"
#include "integrations/adventure/standalone/StandaloneAdapter.h"
#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "core/storage/LocalStateStore.h"
#include "core/navigation/AdventureLaunchController.h"
#include "core/navigation/LaunchPreparation.h"
#include "core/repository/BatoceraLibrary.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>

using namespace trainer;
class StandaloneTests final : public QObject {
    Q_OBJECT
    static QString probe() {
        return QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            "trainer_process_probe.exe"
#else
            "trainer_process_probe"
#endif
        );
    }
    static void write(const QString& path, const QByteArray& data) {
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(data), data.size());
    }
private slots:
    void oneLaunchRequestPreparesAndLaunchesWithoutAnotherConfirmation() {
        QTemporaryDir dir;
        LocalStateStore store(dir.filePath("data"));store.open();QTRY_VERIFY(store.ready());
        AdventureRegistration record;record.adventure.id="setup-fixture";record.adventure.worldId="sinnoh";
        record.adventure.title="Setup fixture";record.adventure.platformId="nds";record.adventure.adapterId="unconfigured";
        record.contentPath=dir.filePath("fixture.nds");write(record.contentPath,"Original content-free fixture");
        bool saved=false;store.saveAdventureAsync(record,this,[&](auto r){QVERIFY(r.success);saved=true;});QTRY_VERIFY(saved);
        StandaloneAdapter ds("melonds",store,{probe(),probe(),{},{"nds"}});AdapterRouter router({&ds});
        LaunchPreparation launch(store,router);int starts=0;
        ds.requestLaunch=[&](const ProcessCommand&,const QString&){++starts;return true;};
        QSignalSpy errors(&launch,&LaunchPreparation::messageRequested);
        launch.launch(record.adventure.id);QVERIFY(launch.busy());launch.launch(record.adventure.id);
        QTRY_VERIFY(!launch.busy());QCOMPARE(errors.size(),0);QCOMPARE(starts,1);
        QCOMPARE(store.registration(record.adventure.id)->adventure.adapterId,"melonds");
        QCOMPARE(store.registration(record.adventure.id)->revision,2);
        launch.launch(record.adventure.id);QTRY_VERIFY(!launch.busy());QCOMPARE(starts,2);
        QCOMPARE(store.registration(record.adventure.id)->revision,2); // Recheck is idempotent.
        QVERIFY(QFile::remove(record.contentPath));
        launch.launch(record.adventure.id);QTRY_VERIFY(!launch.busy());
        QCOMPARE(starts,2);QCOMPARE(errors.size(),1);QVERIFY(errors.last()[0].toString().contains("missing"));
        QCOMPARE(store.registration(record.adventure.id)->revision,2);
        write(record.contentPath,"Original content-free fixture");
        record=*store.registration(record.adventure.id);record.adventure.adapterId="custom";record.integrationConfig={{"keep",true}};
        saved=false;store.saveAdventureAsync(record,this,[&](auto r){QVERIFY(r.success);saved=true;});QTRY_VERIFY(saved);
        launch.launch(record.adventure.id);QTRY_VERIFY(!launch.busy());
        QCOMPARE(store.registration(record.adventure.id)->adventure.adapterId,"custom");QVERIFY(store.registration(record.adventure.id)->integrationConfig["keep"].toBool());
        QCOMPARE(starts,2);QCOMPARE(errors.size(),2);
        launch.launch("absent-catalogue-edition");QCOMPARE(starts,2);QCOMPARE(errors.size(),3);
    }
    void gbaFolderDeterminesEmulatorWithoutTitleRegistration() {
        QTemporaryDir dir;const auto root=dir.filePath("roms");QVERIFY(QDir().mkpath(root+"/gba"));
        const auto file=root+"/gba/Any title.gba";write(file,"Content-free folder discovery fixture");
        const auto scan=scanBatoceraLibrary(root,{});QCOMPARE(scan.entries.size(),1);
        const auto record=scan.entries.first().record;QCOMPARE(record.adventure.platformId,"gba");
        LocalStateStore store(dir.filePath("data"));store.open();QTRY_VERIFY(store.ready());
        bool saved=false;store.saveAdventureAsync(record,this,[&](auto r){QVERIFY(r.success);saved=true;});QTRY_VERIFY(saved);
        const auto config=dir.filePath("retroarch.cfg"),core=dir.filePath("mgba_libretro.so");
        write(config,"config_save_on_exit = \"false\"\n");write(core,"Non-executable core fixture");
        RetroArchAdapter ra(store,{probe(),{},config,{{"mgba",core}}});AdapterRouter router({&ra});
        int starts=0;QStringList arguments;
        ra.requestLaunch=[&](const ProcessCommand& command,const QString&){++starts;arguments=command.arguments;return true;};
        LaunchPreparation launch(store,router);QSignalSpy errors(&launch,&LaunchPreparation::messageRequested);
        launch.launch(record.adventure.id);QTRY_COMPARE(starts,1);QCOMPARE(errors.size(),0);
        QVERIFY(arguments.contains(core));QVERIFY(arguments.contains(file));
        QCOMPARE(store.registration(record.adventure.id)->adventure.platformId,"gba");
    }
    void setupExplainsMissingCoreFormatAndRuntime() {
        MockLibraryRepository library;RetroArchAdapter retroarch(library,{probe(),{},"settings",{{"mgba","core"}}});
        AdventureRegistration r;r.adventure.adapterId="unconfigured";r.adventure.platformId="n64";r.contentPath="/game.z64";
        QVERIFY(retroarch.setupIssue(r).contains("missing"));
        r.adventure.platformId="gba";r.contentPath="/game.zip";QVERIFY(retroarch.setupIssue(r).contains("format"));
        StandaloneAdapter ds("melonds",library,{probe(),"/missing/runtime",{},{"nds"}});
        r.adventure.platformId="nds";r.contentPath="/game.nds";QVERIFY(ds.verifyInstallation(r).contains("unavailable"));
    }
    void routingAndAttachmentKeepOtherIntegrationsIntact() {
        MockLibraryRepository library;
        RetroArchAdapter retroarch(library, {probe(), {}, "settings", {{"mgba", "core"}}});
        StandaloneAdapter ds("melonds", library, {probe(), probe(), {}, {"nds"}});
        StandaloneAdapter dolphin("dolphin", library, {probe(), probe(), {}, {"gc"}});
        AdapterRouter router({&retroarch, &ds, &dolphin});
        AdventureRegistration record; record.adventure.adapterId = "unconfigured"; record.contentPath = "/games/Diamond.NDS";
        router.prepareInstallation(record);
        QCOMPARE(record.adventure.adapterId, "melonds"); QCOMPARE(record.adventure.platformId, "nds");
        record.adventure.adapterId = "unconfigured"; record.adventure.platformId = "gc"; record.contentPath = "/games/Colosseum.iso";
        router.prepareInstallation(record); QCOMPARE(record.adventure.adapterId, "dolphin");
        record.adventure.platformId = "wii"; router.prepareInstallation(record);
        QCOMPARE(record.adventure.adapterId, "unconfigured"); // Not enabled for this installation.
        record.adventure.platformId = "gba"; record.contentPath = "/games/Ruby.gba";
        router.prepareInstallation(record); QCOMPARE(record.adventure.adapterId, "retroarch");
        record.adventure.adapterId = "custom"; record.integrationConfig = {{"preserve", true}};
        router.prepareInstallation(record); QCOMPARE(record.adventure.adapterId, "custom"); QVERIFY(record.integrationConfig["preserve"].toBool());
        QVERIFY(!router.capabilities(record.adventure).launch); QVERIFY(!router.launch(record.adventure).success);
        QVERIFY(!router.resume(record.adventure, {}).success);
    }
    void configurationRequiresMatchingExplicitPlatformValidation() {
        QTemporaryDir dir; const auto path = dir.filePath("install.json");
        QJsonObject value{{"version", 1}, {"adapter", "melonds"}, {"program", probe()}, {"runtimeFile", probe()},
            {"prefixArguments", QJsonArray{}}, {"validatedPlatforms", QJsonArray{"nds"}}};
        const auto load = [&] { write(path, QJsonDocument(value).toJson()); return StandaloneInstallation::load(path, "melonds"); };
        QCOMPARE(load().platforms, QStringList{"nds"});
        QVERIFY(!load().melonDsSaveBackups);
        value["backupProtocol"] = "melonds-sav-v1"; value["configFile"] = dir.filePath("melonDS.toml");
        QVERIFY(!load().melonDsSaveBackups); QCOMPARE(load().platforms, QStringList{"nds"});
        write(value["configFile"].toString(), "verified configuration fixture"); QVERIFY(load().melonDsSaveBackups);
        value["validatedPlatforms"] = QJsonArray{"gc"}; QVERIFY(load().program.isEmpty());
        value["validatedPlatforms"] = QJsonArray{"nds"}; value["runtimeFile"] = dir.filePath("missing"); QVERIFY(load().program.isEmpty());
        value["runtimeFile"] = probe(); value["prefixArguments"] = QJsonArray{42}; QVERIFY(load().program.isEmpty());
        value["prefixArguments"] = QJsonArray{}; value["adapter"] = "dolphin"; QVERIFY(load().program.isEmpty());
    }
    void literalLaunchRoundTripAndMissingFileRecovery_data() {
        QTest::addColumn<QString>("adapterId"); QTest::addColumn<QString>("platform"); QTest::addColumn<QString>("extension");
        QTest::newRow("DS") << QString("melonds") << QString("nds") << QString("nds");
        QTest::newRow("PSP") << QString("ppsspp") << QString("psp") << QString("cso");
        QTest::newRow("PS2") << QString("armsx2") << QString("ps2") << QString("chd");
        QTest::newRow("GameCube") << QString("dolphin") << QString("gc") << QString("iso");
        QTest::newRow("Wii") << QString("dolphin") << QString("wii") << QString("wad");
    }
    void literalLaunchRoundTripAndMissingFileRecovery() {
        QFETCH(QString, adapterId); QFETCH(QString, platform); QFETCH(QString, extension);
        QTemporaryDir dir;
        LocalStateStore store(dir.filePath("data")); store.open(); QTRY_VERIFY(store.ready());
        AdventureRegistration record; record.adventure.id = "ds-fixture"; record.adventure.worldId = "sinnoh";
        record.adventure.title = "Content-free launch fixture"; record.adventure.platformId = platform; record.adventure.adapterId = adapterId;
        record.contentPath = dir.filePath("Adventure ; $(literal) ' quote." + extension); write(record.contentPath, "Original content-free fixture");
        bool saved = false; store.saveAdventureAsync(record, this, [&](LibraryWriteResult result) { saved = result.success; }); QTRY_VERIFY(saved);
        const auto receipt = dir.filePath("arguments.json");
        StandaloneAdapter ds(adapterId, store, {probe(), probe(), {"arguments", receipt}, {platform}});
        AdapterRouter router({&ds});
        QVERIFY(router.capabilities(record.adventure).launch); QVERIFY(!router.capabilities(record.adventure).directResume);
        QVERIFY(!router.resume(record.adventure, {}).success);
        ProcessService process; AdventureLaunchController lifecycle(process);
        connect(&lifecycle, &AdventureLaunchController::checkpointRequested, &store, [&](quint64 token, const QJsonObject& state) {
            store.saveNavigation(state, &lifecycle, [&, token](const QString& error) { lifecycle.checkpointCompleted(token, error); });
        });
        const QJsonObject context{{"page", "worlds"}, {"selected", record.adventure.id}};
        ds.requestLaunch = [&](const ProcessCommand& command, const QString& id) { return lifecycle.launch(command, context, id); };
        QSignalSpy restored(&lifecycle, &AdventureLaunchController::restoreRequested);
        QVERIFY(router.launch(record.adventure).inProgress); QVERIFY(!router.launch(record.adventure).success);
        QTRY_COMPARE(restored.size(), 1); QCOMPARE(lifecycle.state(), "returned"); QCOMPARE(store.navigation(), context);
        QFile output(receipt); QVERIFY(output.open(QIODevice::ReadOnly));
        const auto arguments = adapterId == "melonds" ? QJsonArray{"-f", record.contentPath}
            : adapterId == "ppsspp" ? QJsonArray{"--fullscreen", "--pause-menu-exit", record.contentPath}
            : adapterId == "armsx2" ? QJsonArray{"-batch", "-fullscreen", "--", record.contentPath}
            : QJsonArray{"-b", "-C", "Dolphin.Display.Fullscreen=True", "-e", record.contentPath};
        QCOMPARE(QJsonDocument::fromJson(output.readAll()).array(), arguments); output.close();
        QVERIFY(QFile::remove(receipt)); QVERIFY(QFile::rename(record.contentPath, record.contentPath + ".moved"));
        QVERIFY(router.launch(record.adventure).inProgress); QTRY_COMPARE(restored.size(), 2);
        QCOMPARE(lifecycle.state(), "failed"); QVERIFY(lifecycle.error().contains("missing")); QVERIFY(!QFileInfo::exists(receipt));
        QVERIFY(QFile::exists(record.contentPath + ".moved")); // Recovery never deletes the source.
    }
};
QTEST_GUILESS_MAIN(StandaloneTests)
#include "StandaloneTests.moc"
