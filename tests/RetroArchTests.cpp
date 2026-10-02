#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "integrations/adventure/retroarch/RetroArchSave.h"
#include "integrations/adventure/retroarch/RetroArchDisc.h"
#include "integrations/adventure/retroarch/RetroArchConfiguration.h"
#include "core/repository/RomPlatforms.h"
#include <QCryptographicHash>
#include "core/navigation/AdventureLaunchController.h"
#include "core/storage/LocalStateStore.h"
#include <QtTest>
#include <QSettings>
#include <QStandardPaths>
#include "integrations/adventure/retroarch/RetroArchAppearance.h"
#include "integrations/achievements/RetroArchAchievementSession.h"
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QImage>

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
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);QCoreApplication::setOrganizationName("TrainerOSTests");QCoreApplication::setApplicationName("RetroArchTests");}
    void displayOverlayPreservesBaseAndIsPerGame() {
        QTemporaryDir dir;const auto base=dir.filePath("base.cfg");
        QFile file(base);QVERIFY(file.open(QIODevice::WriteOnly));const QByteArray original="video_smooth = \"true\"\nvideo_shader_enable = \"false\"\nsavefile_directory = \"original\"\n";file.write(original);file.close();
        const auto id=QUuid::createUuid().toString();
        ProcessCommand cmd;cmd.arguments={"--appendconfig",base,"game.gba"};
        QVERIFY(retroarch::changeAppearance(id,"ratio"));QVERIFY(retroarch::changeAppearance(id,"filter"));
        QVERIFY(retroarch::prepareAppearance(cmd,id,base).isEmpty());
        QCOMPARE(cmd.arguments.last(),QString("game.gba"));
        const auto paths=cmd.arguments[1].split('|');QCOMPARE(paths.size(),2);QCOMPARE(paths.first(),base);
        QFile overlay(paths.last());QVERIFY(overlay.open(QIODevice::ReadOnly));const auto bytes=overlay.readAll();overlay.close();
        QVERIFY(bytes.contains("stdin_cmd_enable"));QVERIFY(bytes.contains("aspect_ratio_index"));QVERIFY(bytes.contains("video_smooth = \"false\""));QVERIFY(!bytes.contains("savefile_directory"));
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
        QCOMPARE(cmd.runtimeControls["game"].toString(),id);
        auto withAccount=cmd;withAccount.arguments.prepend(base);withAccount.arguments.prepend("--config");
        useRetroArchAchievementAccount(withAccount,{});const std::atomic_bool running{false};
        QVERIFY(withAccount.prepare(withAccount,running).isEmpty());
        QCOMPARE(withAccount.arguments[withAccount.arguments.indexOf("--appendconfig")+1].split('|').size(),3);
        withAccount.settled({true,0,false,false});QVERIFY(QFileInfo(paths.last()).isFile());
        QVERIFY(retroarch::changeAppearance(id,"reset-appearance"));cmd.arguments={"game.gba"};
        QVERIFY(retroarch::prepareAppearance(cmd,id,base).isEmpty());QVERIFY(overlay.open(QIODevice::ReadOnly));
        const auto defaults=overlay.readAll();QVERIFY(!defaults.contains("video_smooth"));QVERIFY(!defaults.contains("aspect_ratio_index"));
    }
    void namedAppearanceKeepsFamiliesIndependentAndRecoversMissingPresets() {
        QTemporaryDir dir;QVERIFY(QDir().mkpath(dir.filePath("shaders")));QVERIFY(QDir().mkpath(dir.filePath("overlays/borders")));
        const auto base=dir.filePath("base.cfg"),preset=dir.filePath("shaders/Soft_LCD.slangp"),border=dir.filePath("overlays/borders/Plain.cfg");
        const QByteArray original="video_driver = \"glcore\"\nvideo_shader_enable = \"true\"\n";
        {QFile f(base);QVERIFY(f.open(QIODevice::WriteOnly));f.write(original);}
        touch(preset);touch(dir.filePath("shaders/Incompatible.glslp"));
        {QFile f(border);QVERIFY(f.open(QIODevice::WriteOnly));f.write("overlays = 1\noverlay0_descs = 0\noverlay0_overlay = frame.png\noverlay0_full_screen = true\n");}
        QImage image(200,100,QImage::Format_ARGB32);image.fill(Qt::black);
        for(int y=2;y<98;++y)for(int x=40;x<160;++x)image.setPixel(x,y,qRgba(0,0,0,0));
        QVERIFY(image.save(dir.filePath("overlays/borders/frame.png")));
        {QFile f(dir.filePath("overlays/borders/Controller.cfg"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("overlays = 1\noverlay0_descs = 8\n");}
        const auto id=QUuid::createUuid().toString();ProcessCommand cmd;cmd.arguments={"game.gba"};
        QVERIFY(retroarch::prepareAppearance(cmd,id,base).isEmpty());
        QCOMPARE(cmd.runtimeControls["shaderChoices"].toList().size(),1);QCOMPARE(cmd.runtimeControls["bezelChoices"].toList().size(),1);
        const auto shaderChoice=cmd.runtimeControls["shaderChoices"].toList().first().toMap()["id"].toString();
        const auto bezelChoice=cmd.runtimeControls["bezelChoices"].toList().first().toMap()["id"].toString();
        QVERIFY(!retroarch::chooseAppearance(id,"shader:untrusted-path",cmd.runtimeControls));
        QVERIFY(retroarch::chooseAppearance(id,shaderChoice,cmd.runtimeControls));
        QVERIFY(retroarch::chooseAppearance(id,bezelChoice,cmd.runtimeControls));
        ProcessCommand selected;selected.arguments={"game.gba"};QVERIFY(retroarch::prepareAppearance(selected,id,base,{},QSize(1000,500)).isEmpty());
        QCOMPARE(selected.arguments.last(),QString("game.gba"));QCOMPARE(selected.arguments.value(selected.arguments.indexOf("--set-shader")+1),preset);
        auto overlayBytes=[&](const ProcessCommand& command){QFile f(command.arguments.value(command.arguments.indexOf("--appendconfig")+1));if(!f.open(QIODevice::ReadOnly))return QByteArray();return f.readAll();};
        QVERIFY(overlayBytes(selected).contains(border.toUtf8()));QVERIFY(overlayBytes(selected).contains("auto_shaders_enable = \"false\""));
        QVERIFY(overlayBytes(selected).contains("custom_viewport_x = \"0\""));
        QVERIFY(overlayBytes(selected).contains("custom_viewport_width = \"580\""));
        image.setPixel(80,40,qRgb(0,0,0));QVERIFY(image.save(dir.filePath("overlays/borders/frame.png")));
        ProcessCommand obstructed;obstructed.arguments={"game.gba"};QVERIFY(retroarch::prepareAppearance(obstructed,id,base,{},QSize(1000,500)).isEmpty());
        QVERIFY(!overlayBytes(obstructed).contains("input_overlay_enable"));QVERIFY(!obstructed.runtimeControls["appearanceNotice"].toString().isEmpty());
        QVERIFY(retroarch::chooseAppearance(id,"bezel:off",cmd.runtimeControls));
        ProcessCommand off;off.arguments={"game.gba"};QVERIFY(retroarch::prepareAppearance(off,id,base).isEmpty());
        QVERIFY(off.arguments.contains(preset));QVERIFY(overlayBytes(off).contains("input_overlay_enable = \"false\""));
        ProcessCommand other;other.arguments={"game.gba"};QVERIFY(retroarch::prepareAppearance(other,QUuid::createUuid().toString(),base).isEmpty());
        QVERIFY(!other.arguments.contains("--set-shader"));QVERIFY(!overlayBytes(other).contains("input_overlay"));
        QVERIFY(QFile::remove(preset));ProcessCommand missing;missing.arguments={"game.gba"};
        QVERIFY(retroarch::prepareAppearance(missing,id,base).isEmpty());QVERIFY(!missing.arguments.contains("--set-shader"));
        QVERIFY(!missing.runtimeControls["appearanceNotice"].toString().isEmpty());
        QVERIFY(retroarch::changeAppearance(id,"reset-appearance"));ProcessCommand reset;reset.arguments={"game.gba"};
        QVERIFY(retroarch::prepareAppearance(reset,id,base).isEmpty());QVERIFY(!overlayBytes(reset).contains("input_overlay"));
        QFile unchanged(base);QVERIFY(unchanged.open(QIODevice::ReadOnly));QCOMPARE(unchanged.readAll(),original);
    }
    void megaBezelKeepsGameLargeAndAvoidsDoubleFrames() {
        QTemporaryDir dir;
        const auto shared=dir.filePath("stock/shaders_slang/bezel/Mega_Bezel/Presets/Base_CRT_Presets");
        QVERIFY(QDir().mkpath(shared));
        const auto preset=shared+"/MBZ__4__STD-NO-REFLECT__GDV-MINI.slangp";touch(preset);
        const auto base=dir.filePath("base.cfg");
        {QFile f(base);QVERIFY(f.open(QIODevice::WriteOnly));f.write(("video_driver = \"glcore\"\nvideo_shader_dir = \""+dir.filePath("stock")+"\"\ninput_overlay_enable = \"true\"\n").toUtf8());}
        const auto id=QUuid::createUuid().toString();ProcessCommand cmd;cmd.arguments={"game.gba"};
        QVERIFY(retroarch::prepareAppearance(cmd,id,base).isEmpty());
        const auto choices=cmd.runtimeControls["shaderChoices"].toList();QCOMPARE(choices.size(),1);
        QCOMPARE(choices.first().toMap()["label"].toString(),QString("Mega Bezel - Light"));
        QVERIFY(QDir().mkpath(dir.filePath("shaders/Mega_Bezel/Presets/Base_CRT_Presets")));
        QVERIFY(QFile::copy(preset,dir.filePath("shaders/Mega_Bezel/Presets/Base_CRT_Presets/MBZ__4__STD-NO-REFLECT__GDV-MINI.slangp")));
        ProcessCommand duplicate;duplicate.arguments={"game.gba"};
        QVERIFY(retroarch::prepareAppearance(duplicate,id,base).isEmpty());
        QCOMPARE(duplicate.runtimeControls["shaderChoices"].toList().size(),1);
        QVERIFY(retroarch::chooseAppearance(id,choices.first().toMap()["id"].toString(),cmd.runtimeControls));
        ProcessCommand selected;selected.arguments={"game.gba"};
        QVERIFY(retroarch::prepareAppearance(selected,id,base).isEmpty());
        QFile wrapper(selected.arguments.value(selected.arguments.indexOf("--set-shader")+1));QVERIFY(wrapper.open(QIODevice::ReadOnly));
        const auto source=wrapper.readAll();QVERIFY(source.contains(preset.toUtf8()));QVERIFY(source.contains("HSM_NON_INTEGER_SCALE = \"96\""));
        QFile overlay(selected.arguments.value(selected.arguments.indexOf("--appendconfig")+1));QVERIFY(overlay.open(QIODevice::ReadOnly));
        const auto config=overlay.readAll();QVERIFY(config.contains("video_force_aspect = \"false\""));QVERIFY(config.contains("input_overlay_enable = \"false\""));
        QVERIFY(retroarch::changeAppearance(id,"reset-appearance"));
    }
    void flatpakAppearanceUsesRuntimePathsAndSurvivesDeploymentPathChange() {
        QTemporaryDir dir;const auto deployment=dir.filePath("org.libretro.RetroArch/revision/files");
        QVERIFY(QDir().mkpath(deployment+"/share/shaders"));const auto preset=deployment+"/share/shaders/scanlines.slangp";touch(preset);
        const auto base=dir.filePath("base.cfg");{QFile f(base);QVERIFY(f.open(QIODevice::WriteOnly));f.write("video_driver = \"vulkan\"\nvideo_shader_dir = \"/app/share/shaders\"\n");}
        const auto id=QUuid::createUuid().toString();ProcessCommand cmd;cmd.arguments={"game.gba"};
        QVERIFY(retroarch::prepareAppearance(cmd,id,base,deployment+"/bin/retroarch").isEmpty());
        const auto rows=cmd.runtimeControls["shaderChoices"].toList();QCOMPARE(rows.size(),1);
        QCOMPARE(rows.first().toMap()["path"].toString(),QString("/app/share/shaders/scanlines.slangp"));
        QVERIFY(retroarch::chooseAppearance(id,rows.first().toMap()["id"].toString(),cmd.runtimeControls));
        const auto updated=dir.filePath("org.libretro.RetroArch/new-revision/files");QVERIFY(QDir().mkpath(updated+"/share/shaders"));
        QVERIFY(QFile::copy(preset,updated+"/share/shaders/scanlines.slangp"));
        ProcessCommand next;next.arguments={"game.gba"};QVERIFY(retroarch::prepareAppearance(next,id,base,updated+"/bin/retroarch").isEmpty());
        QVERIFY(next.arguments.contains("/app/share/shaders/scanlines.slangp"));QVERIFY(!next.arguments.contains(preset));
        QVERIFY(retroarch::changeAppearance(id,"reset-appearance"));
    }
    void genericOrdinaryLaunchKeepsSaveProvidersSeparate() {
        QTemporaryDir dir;const auto content=dir.filePath("literal ; title.chd"),core=dir.filePath("pcsx_rearmed_libretro.so"),cfg=dir.filePath("retroarch.cfg");
        touch(content);touch(core);
        {QFile f(cfg);QVERIFY(f.open(QIODevice::WriteOnly));f.write("cheevos_enable = \"true\"\ncheevos_hardcore_mode_enable = \"true\"\n");}
        LocalStateStore store(dir.filePath("state"));store.open();QTRY_VERIFY(store.ready());
        RetroArchInstallation installation{probe(),{},cfg};installation.cores.insert("pcsx_rearmed",core);
        installation.saves=std::make_shared<RetroArchSaveSession>(); // No exact save owner/provider required for PS1.
        RetroArchAdapter adapter(store,installation);AdventureRegistration r;
        r.adventure.id="generic";r.adventure.title="Fixture";r.adventure.domain="multiverse";r.adventure.platformId="psx";
        r.adventure.adapterId="unconfigured";r.contentPath=content;
        adapter.prepareInstallation(r);QCOMPARE(r.adventure.adapterId,QString("retroarch"));
        bool saved=false;store.saveAdventureAsync(r,this,[&](auto result){QVERIFY(result.success);saved=true;});QTRY_VERIFY(saved);
        std::optional<ProcessCommand> invocation;adapter.requestLaunch=[&](const auto& cmd,const auto&){invocation=cmd;return AdventureResult{true, {}, true};};
        QVERIFY(adapter.launch(r.adventure).success);QVERIFY(invocation && invocation->prepare);
        auto cmd=*invocation;std::atomic_bool cancel{false};QVERIFY(cmd.prepare(cmd,cancel).isEmpty());
        QCOMPARE(cmd.arguments.last(),content);QVERIFY(cmd.arguments.contains("--appendconfig"));
        QVERIFY(!resolveRetroArchSave(r,installation).supported);
        QVERIFY(!cmd.inspectOutput("[ERROR] Failed to load content.").isEmpty());
        QVERIFY(cmd.inspectOutput("Frame: 1").isEmpty());
        QVERIFY(QFile::remove(content));cmd=*invocation;QVERIFY(!cmd.prepare(cmd,cancel).isEmpty());
        touch(content);QVERIFY(QFile::remove(core));cmd=*invocation;QVERIFY(!cmd.prepare(cmd,cancel).isEmpty());
        auto other=r;other.adventure.adapterId="custom";adapter.prepareInstallation(other);QCOMPARE(other.adventure.adapterId,QString("custom"));
        other=r;other.contentPath=dir.filePath("wrong.gba");adapter.prepareInstallation(other);QCOMPARE(other.adventure.adapterId,QString("unconfigured"));
    }
    void platformRegistryIsConsistent() {
        QSet<QString> ids,folders;
        for(const auto& p:romPlatforms()) {
            QVERIFY(!ids.contains(p.id));QVERIFY(!folders.contains(p.folder));ids.insert(p.id);folders.insert(p.folder);
            QVERIFY(!p.name.isEmpty());QVERIFY(!p.extensions.isEmpty());QVERIFY(!p.folder.contains('/'));
            QCOMPARE(romPlatformId(p.folder),p.id);
            if(!p.core.isEmpty())QVERIFY(romContentSupported(p.id,p.core,p.extensions.first()));
        }
        QVERIFY(ids.size()>=100);QVERIFY(!ids.contains("xbox"));
        QCOMPARE(romCore("psp"),QString("ppsspp"));QCOMPARE(romCore("dreamcast"),QString("flycast"));
        QVERIFY(!romContentSupported("megadrive","genesis_plus_gx","chd"));
    }
    void movingRomRetainsOrdinarySaveDirectory() {
        QTemporaryDir dir;QVERIFY(QDir().mkpath(dir.filePath("roms/gba/Favorites")));
        const auto source=dir.filePath("roms/gba/game.gba");touch(source);
        const auto cfg=dir.filePath("retroarch.cfg");
        {QFile f(cfg);QVERIFY(f.open(QIODevice::WriteOnly));f.write(("auto_overrides_enable = \"true\"\n"
            "savefiles_in_content_dir = \"false\"\nsort_savefiles_enable = \"true\"\n"
            "sort_savefiles_by_content_enable = \"true\"\nsavefile_directory = \""+dir.filePath("saves")+"\"\n").toUtf8());}
        RetroArchInstallation installation{probe(),{},cfg};
        AdventureRegistration r;r.adventure.id="moved";r.adventure.adapterId="retroarch";r.contentPath=source;r.integrationConfig["core"]="mgba";
        LibraryEdit edit{LibraryEditKind::MoveFile,r.adventure.id,1};edit.text=dir.filePath("roms/gba/Favorites");
        QVERIFY(retroarch::prepareFileMove(r,edit,installation).isEmpty());
        QCOMPARE(edit.retainedSaveBase,dir.filePath("saves/gba"));QVERIFY(edit.retainedSaveSortCore);
        r.integrationConfig["librarySaveBase"]=edit.retainedSaveBase;r.integrationConfig["librarySaveSortCore"]=true;
        r.contentPath=dir.filePath("roms/gba/Favorites/game.gba");QVERIFY(QFile::rename(source,r.contentPath));
        LibraryEdit again=edit;again.text=dir.filePath("roms/gba");
        QVERIFY(retroarch::prepareFileMove(r,again,installation).isEmpty());QCOMPARE(again.retainedSaveBase,edit.retainedSaveBase);
        ProcessCommand cmd;cmd.arguments={r.contentPath};std::atomic_bool cancel{false};
        QVERIFY(prepareRetroArchLaunch(cmd,r,installation,cancel).isEmpty());
        QCOMPARE(cmd.arguments.last(),r.contentPath);QVERIFY(cmd.arguments.contains("--appendconfig"));
        QFile extra(cmd.arguments[cmd.arguments.size()-2]);QVERIFY(extra.open(QIODevice::ReadOnly));const auto bytes=extra.readAll();
        QVERIFY(bytes.contains(dir.filePath("saves/gba").toUtf8()));QVERIFY(bytes.contains("sort_savefiles_by_content_enable = \"false\""));
        QVERIFY(bytes.contains("sort_savefiles_enable = \"true\""));
        QVERIFY(QDir().mkpath(dir.filePath("config/mGBA")));touch(dir.filePath("config/mGBA/Favorites.cfg"));
        QVERIFY(!retroarch::prepareFileMove(r,again,installation).isEmpty());
    }
    void discTracksStayTogether() {
        QTemporaryDir dir; std::atomic_bool cancel{false};
        const auto cue=dir.filePath("game.cue"),track=dir.filePath("track one.img"); touch(track);
        const auto write=[&](const QByteArray& bytes){QFile f(cue);QVERIFY(f.open(QIODevice::WriteOnly));f.write(bytes);};
        write("REM fixture\nFILE \"track one.img\" BINARY\n  TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n");
        QVERIFY(retroarch::validateDiscContent(cue,cancel).isEmpty());
        QVERIFY(QFile::remove(track)); QVERIFY(!retroarch::validateDiscContent(cue,cancel).isEmpty()); touch(track);
        write("FILE \"../outside.img\" BINARY\nTRACK 01 AUDIO\n"); QVERIFY(!retroarch::validateDiscContent(cue,cancel).isEmpty());
        write("FILE \"track one.img\" BINARY\n"); QVERIFY(!retroarch::validateDiscContent(cue,cancel).isEmpty());
        write("FILE \"track one.img\" BINARY\nTRACK 01 AUDIO\nFILE \"missing.wav\" WAVE\nTRACK 02 AUDIO\n");
        QVERIFY(!retroarch::validateDiscContent(cue,cancel).isEmpty());
        write(QByteArray(65537,' ')); QVERIFY(!retroarch::validateDiscContent(cue,cancel).isEmpty());
        const auto chd=dir.filePath("game.chd"); QFile image(chd);QVERIFY(image.open(QIODevice::WriteOnly));image.write("MComprHDfixture");image.close();
        QVERIFY(retroarch::validateDiscContent(chd,cancel).isEmpty());
        touch(chd); QVERIFY(!retroarch::validateDiscContent(chd,cancel).isEmpty());
        cancel=true;QVERIFY(!retroarch::validateDiscContent(chd,cancel).isEmpty());
    }
    void discFirmwareAndLaunchAreRechecked() {
        QTemporaryDir dir; const auto root=dir.path(),config=dir.filePath("retroarch.cfg"),setup=dir.filePath("installation.json");
        QVERIFY(QDir().mkpath(dir.filePath("neocd")));touch(dir.filePath("neocd/neocd.bin"));
        touch(dir.filePath("neocd_libretro.so"));touch(dir.filePath("genesis_plus_gx_libretro.so"));
        const auto digest=QString::fromLatin1(QCryptographicHash::hash("original test fixture",QCryptographicHash::Sha256).toHex());
        QJsonObject sega;for(const auto& name:{"bios_CD_U.bin","bios_CD_E.bin","bios_CD_J.bin"}){touch(dir.filePath(name));sega.insert(name,digest);}
        QFile cfg(config);QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.write(("system_directory = \""+root+"\"\nauto_overrides_enable = \"false\"\n").toUtf8());cfg.close();
        QJsonObject settings{{"version",1},{"program",probe()},{"prefixArguments",QJsonArray{}},{"configFile",config},{"coresDirectory",root},
            {"discFirmware",QJsonObject{{"segacd",sega},{"neogeocd",QJsonObject{{"neocd/neocd.bin",digest}}}}}};
        QFile file(setup);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(settings).toJson());file.close();
        auto installation=RetroArchInstallation::load(setup); QCOMPARE(installation.readyDiscPlatforms.size(),2);
        LocalStateStore store(dir.filePath("data"));store.open();QTRY_VERIFY(store.ready());
        RetroArchAdapter adapter(store,installation); AdventureRegistration r;
        r.adventure.id="disc";r.adventure.title="Fixture";r.adventure.domain="multiverse";r.adventure.platformId="segacd";r.adventure.adapterId="unconfigured";
        r.contentPath=dir.filePath("game.chd");QFile chd(r.contentPath);QVERIFY(chd.open(QIODevice::WriteOnly));chd.write("MComprHDfixture");chd.close();
        adapter.prepareInstallation(r);QCOMPARE(r.adventure.adapterId,"retroarch");
        auto wrong=r;wrong.adventure.platformId="megadrive";adapter.prepareInstallation(wrong);QCOMPARE(wrong.adventure.adapterId,"unconfigured");
        wrong=r;wrong.contentPath=dir.filePath("cartridge.bin");adapter.prepareInstallation(wrong);QCOMPARE(wrong.adventure.adapterId,"unconfigured");
        bool done=false;store.saveAdventureAsync(r,this,[&](auto result){QVERIFY(result.success);done=true;});QTRY_VERIFY(done);
        std::optional<ProcessCommand> invocation;adapter.requestLaunch=[&](const auto& cmd,const auto&){invocation=cmd;return AdventureResult{true, {}, true};};
        QVERIFY(adapter.launch(r.adventure).success);QVERIFY(invocation && invocation->prepare);
        auto cmd=*invocation;std::atomic_bool cancel{false};QVERIFY(cmd.prepare(cmd,cancel).isEmpty());
        QCOMPARE(cmd.arguments.last(),r.contentPath);QVERIFY(cmd.arguments.contains("--appendconfig"));
        QFile bios(dir.filePath("bios_CD_U.bin"));QVERIFY(bios.open(QIODevice::WriteOnly));bios.write("changed");bios.close();
        cmd=*invocation;QVERIFY(!cmd.prepare(cmd,cancel).isEmpty());
        QVERIFY(adapter.capabilities(r.adventure).launch); // Cached browsing does not touch storage.
        QVERIFY(!RetroArchInstallation::load(setup).readyDiscPlatforms.contains("segacd"));
        installation.discFirmware["neogeocd"]={{"neocd/../bios_CD_E.bin",digest}};
        QVERIFY(!retroarch::verifiedDiscFirmware(installation,"neogeocd",cancel));
        installation.discFirmware["neogeocd"]={{"neocd/neocd.bin",QString(64,'0')}};
        QVERIFY(!retroarch::verifiedDiscFirmware(installation,"neogeocd",cancel));
        installation.discFirmware["neogeocd"]={{"neocd/neocd.bin",digest}};
        cancel=true;QVERIFY(!retroarch::verifiedDiscFirmware(installation,"neogeocd",cancel));
    }
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
        adapter.requestLaunch=[&](const auto& cmd,const auto&){invocation=cmd;return AdventureResult{true, {}, true};};
        QVERIFY(adapter.launch(r.adventure).success); QVERIFY(invocation && invocation->prepare);
        const auto original=*invocation; std::atomic_bool cancelled{false};
        QVERIFY(invocation->prepare(*invocation,cancelled).isEmpty());
        QCOMPARE(invocation->arguments.last(),content);
        QFile overlay(invocation->arguments[invocation->arguments.size()-2].section('|',0,0)); QVERIFY(overlay.open(QIODevice::ReadOnly));
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
        const auto config = dir.filePath("frontend settings.cfg");
        {QFile f(config);QVERIFY(f.open(QIODevice::WriteOnly));f.write("savestate_auto_load = \"false\"\n");}
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
        adapter.requestLaunch = [&](const ProcessCommand& command, const QString& id) { const bool started=lifecycle.launch(command, context, id); return AdventureResult{started, {}, started}; };
        const auto acceptLaunch = adapter.requestLaunch;
        adapter.requestLaunch = [](const auto&, const auto&) {
            return AdventureResult{false, "Reconnect with your friend to finish the exchange before playing."};
        };
        const auto refused = adapter.launch(record.adventure);
        QVERIFY(!refused.success); QVERIFY(!refused.inProgress);
        QCOMPARE(refused.message, "Reconnect with your friend to finish the exchange before playing.");
        QCOMPARE(lifecycle.state(), "idle");
        adapter.requestLaunch = acceptLaunch;
        QSignalSpy restored(&lifecycle, &AdventureLaunchController::restoreRequested);
        const auto accepted = adapter.launch(record.adventure); QVERIFY(accepted.success && accepted.inProgress);
        QVERIFY(!adapter.launch(record.adventure).success); // No second child while checkpointing.
        QTRY_COMPARE(restored.size(), 1); QCOMPARE(lifecycle.state(), "returned"); QCOMPARE(store.navigation(), context);
        QFile output(resultFile); QVERIFY(output.open(QIODevice::ReadOnly));
        auto arguments=QJsonDocument::fromJson(output.readAll()).array();
        const auto configPaths=arguments[6].toString().split('|');QCOMPARE(configPaths.size(),2);QVERIFY(QFileInfo(configPaths.last()).isFile());
        arguments[6]=configPaths.first();
        QCOMPARE(arguments, QJsonArray::fromStringList(
            {"--fullscreen", "--config", config, "--libretro", core, "--appendconfig", dir.filePath("traineros-generic-ordinary-v1.cfg"), content}));
        // Removing the media doesn't block metadata-only browsing. The external
        // worker preflight checks loading; a failure restores the shell.
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
