#include "integrations/adventure/AdapterRouter.h"
#include "integrations/adventure/standalone/StandaloneAdapter.h"
#include "integrations/adventure/standalone/PpssppNetplay.h"
#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "core/storage/LocalStateStore.h"
#include "core/navigation/AdventureLaunchController.h"
#include "core/navigation/LaunchPreparation.h"
#include "core/repository/BatoceraLibrary.h"
#include "platform/emulation/EmulatorDiscovery.h"
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
    void pspMultiplayerKeepsSettingsPrivateAndOrdinarySavesShared() {
#ifdef Q_OS_WIN
        QSKIP("Linux memory-stick directory symlinks.");
#endif
        QTemporaryDir dir;
        const auto system=dir.filePath("PSP/SYSTEM");
        QVERIFY(QDir().mkpath(system));QVERIFY(QDir().mkpath(dir.filePath("PSP/SAVEDATA")));
        const QByteArray original="[Network]\nEnableWlan = False\n[Graphics]\nRenderingMode = 1\n";
        write(system+"/ppsspp.ini",original);write(system+"/controls.ini","My controller bindings");
        write(system+"/ULUS10002_ppsspp.ini","My per-game settings");
        write(dir.filePath("PSP/SAVEDATA/existing.bin"),"Existing progress");
        StandaloneInstallation installation{probe(),probe(),{"run","org.ppsspp.PPSSPP"},{"psp"},system+"/ppsspp.ini"};
        ProcessCommand command{"/usr/bin/flatpak",{"run","org.ppsspp.PPSSPP","--fullscreen","/roms/PSP title.iso"},{}};
        std::atomic_bool cancel=false;
        ppsspp::NetplayRequest request{{{"id","fixture"}},true,true,"socom.cc","Alice\n[Network]"};
        QVERIFY(ppsspp::configureNetplay(command,installation,request,cancel).isEmpty());
        QCOMPARE(command.arguments.last(),"/roms/PSP title.iso");
        const auto argument=command.arguments.filter("XDG_CONFIG_HOME=").first();
        const auto root=argument.mid(QString("XDG_CONFIG_HOME=").size());
        QCOMPARE(command.arguments.indexOf(argument)-1,command.arguments.indexOf("org.ppsspp.PPSSPP"));
        QVERIFY(command.arguments.contains("--command=env"));
        QFile copied(root+"/ppsspp/PSP/SYSTEM/controls.ini");QVERIFY(copied.open(QIODevice::ReadOnly));
        QCOMPARE(copied.readAll(),"My controller bindings");copied.close();
        QVERIFY(QFileInfo::exists(root+"/ppsspp/PSP/SYSTEM/ULUS10002_ppsspp.ini"));
        QCOMPARE(QFileInfo(root+"/ppsspp/PSP/SAVEDATA").canonicalFilePath(),QFileInfo(dir.filePath("PSP/SAVEDATA")).canonicalFilePath());
        QFile network(command.arguments.filter("--appendconfig=").first().mid(15));QVERIFY(network.open(QIODevice::ReadOnly));
        const auto settings=network.readAll();network.close();
        QVERIFY(settings.contains("AdhocServerRelayMode = 1"));QVERIFY(settings.contains("EnableAdhocServer = False"));
        QVERIFY(settings.contains("AchievementsEnable = False"));QVERIFY(!settings.contains("Alice\n"));
        // Simulate PPSSPP saving merged options, including its per-game INI.
        write(root+"/ppsspp/PSP/SYSTEM/ppsspp.ini","Session changes");
        write(root+"/ppsspp/PSP/SYSTEM/ULUS10002_ppsspp.ini","Session changes");
        command.settled({});QVERIFY(!QFileInfo::exists(root));
        QFile originalFile(system+"/ppsspp.ini");QVERIFY(originalFile.open(QIODevice::ReadOnly));QCOMPARE(originalFile.readAll(),original);
        QFile perGame(system+"/ULUS10002_ppsspp.ini");QVERIFY(perGame.open(QIODevice::ReadOnly));QCOMPARE(perGame.readAll(),"My per-game settings");
        QFile save(dir.filePath("PSP/SAVEDATA/existing.bin"));QVERIFY(save.open(QIODevice::ReadOnly));QCOMPARE(save.readAll(),"Existing progress");
    }
    void pspMultiplayerRejectsBadEndpointAndCancellationBeforeMutation() {
        ProcessCommand command{probe(),{"/roms/game.iso"},{}};
        StandaloneInstallation installation{probe(),probe(),{},{"psp"},"/missing/ppsspp.ini"};
        std::atomic_bool cancel=false;
        ppsspp::NetplayRequest request{{},false,true,"untrusted.example","Trainer"};
        QVERIFY(ppsspp::configureNetplay(command,installation,request,cancel).contains("address"));
        cancel=true;QVERIFY(ppsspp::configureNetplay(command,installation,request,cancel).contains("cancelled"));
        QCOMPARE(command.program,probe());QCOMPARE(command.arguments,QStringList{"/roms/game.iso"});
        QVERIFY(!command.settled);
    }
    void discoveryIsAutomaticAndPreservesUserSettingsAndRoute() {
        QTemporaryDir dir;
        EmulatorEnvironment env; env.home=dir.path();env.configHome=dir.filePath("config");
        env.stateDirectory=dir.filePath("state");env.libraryRoot=dir.filePath("roms");
        env.executables={{"PPSSPPSDL",probe()},{"retroarch",probe()},{"melonDS",probe()}};
        env.controllerButtons={{"A",1},{"B",0},{"X",3},{"Y",2},{"L",4},{"R",5},
            {"Start",7},{"Select",6},{"Up",257},{"Down",260},{"Left",264},{"Right",258}};
        const auto cores=dir.filePath("config/retroarch/cores");QVERIFY(QDir().mkpath(cores));
        write(cores+"/mgba_libretro.so","Non-executable core fixture");
        auto found=prepareEmulators(env);QVERIFY(found.profiles.contains("ppsspp"));QVERIFY(found.profiles.contains("retroarch"));
        auto ra=RetroArchInstallation::fromJson(found.profiles["retroarch"]);
        QVERIFY(ra.cores.contains("mgba"));QVERIFY(ra.saveBackups);QVERIFY(!ra.saves);
        QFile config(ra.configFile);QVERIFY(config.open(QIODevice::ReadOnly));const auto original=config.readAll();config.close();
        QVERIFY(original.contains("input_autodetect_enable"));
        const auto ds=dir.filePath("config/melonDS/melonDS.toml");QFile dsFile(ds);QVERIFY(dsFile.open(QIODevice::ReadOnly));
        QVERIFY(dsFile.readAll().contains("A = 1"));dsFile.close();
        write(ra.configFile,"User settings must stay exactly as written\n");
        write(ds,"User controller preferences\n");
        env.executables["PPSSPPSDL"]=dir.filePath("different-runtime");
        found=prepareEmulators(env);
        QCOMPARE(found.profiles["ppsspp"]["program"].toString(),probe()); // no silent save-namespace switch
        QVERIFY(config.open(QIODevice::ReadOnly));QCOMPARE(config.readAll(),"User settings must stay exactly as written\n");
        QVERIFY(dsFile.open(QIODevice::ReadOnly));QCOMPARE(dsFile.readAll(),"User controller preferences\n");
        auto psp=StandaloneInstallation::fromJson(found.profiles["ppsspp"],"ppsspp");
        MockLibraryRepository library;StandaloneAdapter standalone("ppsspp",library,psp);AdapterRouter router({&standalone});
        AdventureRegistration game;game.adventure.adapterId="unconfigured";game.adventure.platformId="psp";game.contentPath="/roms/psp/Any game.iso";
        router.prepareInstallation(game);QCOMPARE(game.adventure.adapterId,"ppsspp");
    }
    void explicitBrokenIntegrationDoesNotFallBackOrGetOverwritten() {
        QTemporaryDir dir;EmulatorEnvironment env;env.home=dir.path();env.configHome=dir.filePath("config");env.stateDirectory=dir.filePath("state");
        env.executables={{"ARMSX2",probe()},{"PPSSPPSDL",probe()}};
        QVERIFY(QDir().mkpath(dir.filePath("state/integrations")));
        const auto explicitFile=dir.filePath("state/integrations/armsx2.json");
        const QJsonObject custom{{"version",1},{"adapter","armsx2"},{"program",probe()},{"runtimeFile",probe()},
            {"prefixArguments",QJsonArray{"existing-wrapper","--private-settings"}},{"validatedPlatforms",QJsonArray{"ps2"}}};
        write(explicitFile,QJsonDocument(custom).toJson());
        const auto broken=dir.filePath("state/integrations/ppsspp.json");write(broken,"broken custom config");
        auto found=prepareEmulators(env);QCOMPARE(found.profiles["armsx2"],custom);QVERIFY(!found.profiles.contains("ppsspp"));
        QFile f(broken);QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),"broken custom config");
        QVERIFY(!QFileInfo::exists(dir.filePath("state/integrations/discovered/armsx2.json")));
    }
    void flatpakUpdateRefreshesOnlyRuntimeAndKeepsWrapperAndDataPaths() {
#ifdef Q_OS_WIN
        QSKIP("Linux Flatpak executable filenames; run this case on the ARM64 Linux build.");
#endif
        QTemporaryDir dir;EmulatorEnvironment env;env.home=dir.path();env.configHome=dir.filePath("config");env.stateDirectory=dir.filePath("state");
        const auto deployment=dir.filePath("flatpak/app/org.ppsspp.PPSSPP/current/active");
        QVERIFY(QDir().mkpath(deployment+"/files/bin"));const auto runtime=deployment+"/files/bin/PPSSPPSDL";
        QVERIFY(QFile::copy(probe(),runtime));
        env.executables["flatpak"]=probe();env.flatpaks["org.ppsspp.PPSSPP"]=deployment;
        auto found=prepareEmulators(env);auto profile=found.profiles["ppsspp"];
        QCOMPARE(profile["runtimeFile"].toString(),runtime);QVERIFY(!StandaloneInstallation::fromJson(profile,"ppsspp").program.isEmpty());
        profile["program"]=probe();profile["prefixArguments"]=QJsonArray{"controller-bridge","--",probe(),"run","org.ppsspp.PPSSPP"};
        profile["runtimeFile"]=dir.filePath("flatpak/app/org.ppsspp.PPSSPP/aarch64/stable/removed-version/files/bin/PPSSPPSDL");
        profile["configFile"]="/keep/all/personal/data";
        const auto path=dir.filePath("state/integrations/ppsspp.json");const auto bytes=QJsonDocument(profile).toJson();write(path,bytes);
        found=prepareEmulators(env);auto repaired=found.profiles["ppsspp"];
        QCOMPARE(repaired["runtimeFile"].toString(),runtime);repaired["runtimeFile"]=profile["runtimeFile"];QCOMPARE(repaired,profile);
        QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),bytes); // runtime fix is snapshot-only
        env.flatpaks.clear();found=prepareEmulators(env);QCOMPARE(found.profiles["ppsspp"],profile);
        QVERIFY(StandaloneInstallation::fromJson(found.profiles["ppsspp"],"ppsspp").program.isEmpty());
    }
    void oneLaunchRequestPreparesAndLaunchesWithoutAnotherConfirmation() {
        QTemporaryDir dir;
        LocalStateStore store(dir.filePath("data"));store.open();QTRY_VERIFY(store.ready());
        AdventureRegistration record;record.adventure.id="setup-fixture";record.adventure.worldId="sinnoh";
        record.adventure.title="Setup fixture";record.adventure.platformId="nds";record.adventure.adapterId="unconfigured";
        record.contentPath=dir.filePath("fixture.nds");write(record.contentPath,"Original content-free fixture");
        bool saved=false;store.saveAdventureAsync(record,this,[&](auto r){QVERIFY(r.success);saved=true;});QTRY_VERIFY(saved);
        StandaloneAdapter ds("melonds",store,{probe(),probe(),{},{"nds"}});AdapterRouter router({&ds});
        LaunchPreparation launch(store,router);int starts=0;
        ds.requestLaunch=[&](const ProcessCommand&,const QString&){++starts;return AdventureResult{true, {}, true};};
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
        ra.requestLaunch=[&](const ProcessCommand& command,const QString&){++starts;arguments=command.arguments;return AdventureResult{true, {}, true};};
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
        const StandaloneInstallation installation{probe(), probe(), {"arguments", receipt}, {platform}};
        StandaloneAdapter ds(adapterId, store, installation);
        AdapterRouter router({&ds});
        const auto revision = store.registration(record.adventure.id)->revision;
        QVERIFY(ds.updateInstallation({}));
        QVERIFY(!router.capabilities(record.adventure).launch);
        QVERIFY(ds.updateInstallation(installation));
        QVERIFY(!ds.updateInstallation(installation));
        QCOMPARE(store.registration(record.adventure.id)->revision, revision);
        QVERIFY(router.capabilities(record.adventure).launch); QVERIFY(!router.capabilities(record.adventure).directResume);
        QVERIFY(!router.resume(record.adventure, {}).success);
        ProcessService process; AdventureLaunchController lifecycle(process);
        connect(&lifecycle, &AdventureLaunchController::checkpointRequested, &store, [&](quint64 token, const QJsonObject& state) {
            store.saveNavigation(state, &lifecycle, [&, token](const QString& error) { lifecycle.checkpointCompleted(token, error); });
        });
        const QJsonObject context{{"page", "worlds"}, {"selected", record.adventure.id}};
        ds.requestLaunch = [&](const ProcessCommand& command, const QString& id) { const bool started=lifecycle.launch(command, context, id); return AdventureResult{started, {}, started}; };
        const auto acceptLaunch = ds.requestLaunch;
        ds.requestLaunch = [](const auto&, const auto&) {
            return AdventureResult{false, "Reconnect with your friend to finish the exchange before playing."};
        };
        const auto refused = router.launch(record.adventure);
        QVERIFY(!refused.success); QVERIFY(!refused.inProgress);
        QCOMPARE(refused.message, "Reconnect with your friend to finish the exchange before playing.");
        QCOMPARE(lifecycle.state(), "idle");
        ds.requestLaunch = acceptLaunch;
        QSignalSpy restored(&lifecycle, &AdventureLaunchController::restoreRequested);
        QVERIFY(router.launch(record.adventure).inProgress); QVERIFY(!router.launch(record.adventure).success);
        QTRY_COMPARE(restored.size(), 1); QCOMPARE(lifecycle.state(), "returned"); QCOMPARE(store.navigation(), context);
        QFile output(receipt); QVERIFY(output.open(QIODevice::ReadOnly));
        const auto arguments = adapterId == "melonds" ? QJsonArray{"-f", record.contentPath}
            : adapterId == "ppsspp" ? QJsonArray{"--fullscreen", "--pause-menu-exit", record.contentPath}
            : adapterId == "armsx2" ? QJsonArray{"-batch", "-fullscreen", "--", record.contentPath}
            : QJsonArray{"-b", "-C", "Dolphin.Display.Fullscreen=True", "-C", "Dolphin.Interface.ConfirmStop=False", "-e", record.contentPath};
        QCOMPARE(QJsonDocument::fromJson(output.readAll()).array(), arguments); output.close();
        QVERIFY(QFile::remove(receipt)); QVERIFY(QFile::rename(record.contentPath, record.contentPath + ".moved"));
        QVERIFY(router.launch(record.adventure).inProgress); QTRY_COMPARE(restored.size(), 2);
        QCOMPARE(lifecycle.state(), "failed"); QVERIFY(lifecycle.error().contains("missing")); QVERIFY(!QFileInfo::exists(receipt));
        QVERIFY(QFile::exists(record.contentPath + ".moved")); // Recovery never deletes the source.
    }
};
QTEST_GUILESS_MAIN(StandaloneTests)
#include "StandaloneTests.moc"
