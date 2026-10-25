#include "integrations/adventure/AdapterRouter.h"
#include "integrations/adventure/standalone/StandaloneAdapter.h"
#include "integrations/adventure/standalone/PpssppNetplay.h"
#include "integrations/adventure/standalone/DolphinNetplay.h"
#include "integrations/adventure/standalone/DolphinLan.h"
#include "integrations/adventure/standalone/AzaharNetplay.h"
#include "integrations/adventure/standalone/VitaNetplay.h"
#include "integrations/adventure/standalone/MelonDsNetplay.h"
#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "core/storage/LocalStateStore.h"
#include "core/navigation/AdventureLaunchController.h"
#include "core/navigation/LaunchPreparation.h"
#include "core/repository/BatoceraLibrary.h"
#include "platform/emulation/EmulatorDiscovery.h"
#include "core/model/GamePlayers.h"
#include <QtTest>
#include "CartridgeArchiveFixture.h"
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QStandardPaths>
#include <QScopeGuard>
#include <QCryptographicHash>
#include <QtEndian>
#include <QSettings>

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
    void gameCubeLanUsesNativeBbaAndIsolatedSaves() {
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const auto connection=dir.filePath("session.conf");
        write(connection,("mode=bba\nhost=true\nmac=00:09:bf:ab:cd:01\nsocket="+dir.filePath("bba.sock")+"\n").toUtf8());
        QVERIFY(QFile::setPermissions(connection,QFile::ReadOwner|QFile::WriteOwner));
        write(dir.filePath("Dolphin.ini"),"ordinary settings");
        write(dir.filePath("GCPadNew.ini"),"[GCPad1]\nDevice = SDL/0/Pad\n");
        StandaloneInstallation installation{probe(),probe(),{},{"gc"},dir.filePath("Dolphin.ini")};
        QJsonObject profile{{"id","runtime.dolphin.bba.v1"},{"runtime",QString(64,'a')},{"players",2},{"temporaryProgress",true}};
        ProcessCommand command;
        const auto result=dolphinlan::configure(command,installation,"/roms/game.rvz",connection,true,profile);
        QVERIFY2(result.isEmpty(),qPrintable(result));
        QVERIFY(command.arguments.contains("-e"));QVERIFY(command.arguments.contains("/roms/game.rvz"));
        QCOMPARE(command.arguments.mid(0,2),QStringList({"-u","TRAINEROS_DOLPHIN_SESSION"}));
        const auto root=command.arguments.at(command.arguments.lastIndexOf("-u")+1);
        QSettings config(root+"/Config/Dolphin.ini",QSettings::IniFormat);
        QCOMPARE(config.value("Core/SerialPort1").toInt(),11);
        QCOMPARE(config.value("Core/SIDevice0").toInt(),6);
        QCOMPARE(config.value("Core/SIDevice1").toInt(),0);
        QCOMPARE(config.value("Core/BBA_TAPSERVER_DESTINATION").toString(),dir.filePath("bba.sock"));
        QVERIFY(!config.contains("Core/MemcardAPath"));QVERIFY(!config.contains("Core/GCIPathA"));
        QFile original(dir.filePath("Dolphin.ini"));QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),QByteArray("ordinary settings"));
        QVERIFY(!dolphinlan::configure(command,installation,"/roms/game.rvz",connection,false,profile).isEmpty());
        write(connection,"mode=bba\nhost=true\nmac=00:09:bf:ab:cd:01\nsocket=/tmp/foreign.sock\n");
        QVERIFY(!dolphinlan::configure(command,installation,"/roms/game.rvz",connection,true,profile).isEmpty());
        command.settled({});QVERIFY(!QFileInfo::exists(root));
        QVERIFY(dolphinlan::supportedDisc("GM4E01"));QVERIFY(dolphinlan::supportedDisc("GKYJ01"));
        QVERIFY(dolphinlan::supportedDisc("GTEJ01"));QVERIFY(dolphinlan::supportedDisc("GM4K01"));
        QVERIFY(!dolphinlan::supportedDisc("GALE01"));QVERIFY(!dolphinlan::supportedDisc("Mario Kart"));
        QVERIFY(!dolphinlan::supportedDisc("GM4E01extra"));QVERIFY(!dolphinlan::supportedDisc("GM4E99"));
        QCOMPARE(multiplayerPresentation(profile)["label"].toString(),QString("LAN (in-game)"));QVERIFY(localLobbyRuntime(profile));
    }

    void modernDiscoverySeedsControlsWithoutReplacingUserSettings() {
        QTemporaryDir dir; EmulatorEnvironment env;
        env.home=dir.path(); env.configHome=dir.filePath("config"); env.stateDirectory=dir.filePath("state");
        env.executables={{"rpcs3",probe()},{"eden",probe()}};
        env.controllerName="Handheld Pad";
        env.switchBindings={{"button_a","engine:sdl,port:0,guid:00112233,button:1"},
            {"lstick","engine:sdl,port:0,guid:00112233,axis_x:0,axis_y:1"}};
        const auto found=prepareEmulators(env);
        for (const auto& id:QStringList{"rpcs3","eden"}) {
            const auto value=StandaloneInstallation::fromJson(found.profiles[id],id);
            QCOMPARE(value.program,probe()); QCOMPARE(value.platforms,QStringList{id=="eden"?"switch":"ps3"});
        }
        QVERIFY(found.profiles["rpcs3"]["prefixArguments"].toArray().contains("--input-config"));
        QFile pad(dir.filePath("config/rpcs3/input_configs/global/TrainerOS.yml"));
        QVERIFY(pad.open(QIODevice::ReadOnly)); QVERIFY(pad.readAll().contains("Handheld Pad 1"));
        const auto rpcsConfig=dir.filePath("config/rpcs3/config.yml");
        const auto rpcsGui=dir.filePath("config/rpcs3/GuiConfigs/CurrentSettings.ini");
        QFile graphics(rpcsConfig); QVERIFY(graphics.open(QIODevice::ReadOnly));
        QVERIFY(graphics.readAll().contains("Async Recompiler (multi-threaded)")); graphics.close();
        write(rpcsConfig,"existing renderer preferences");
        write(rpcsGui,"existing desktop preferences");
        const auto config=dir.filePath("config/eden/qt-config.ini");
        QFile input(config); QVERIFY(input.open(QIODevice::ReadOnly));
        QVERIFY(input.readAll().contains("player_0_button_a=\"engine:sdl")); input.close();
        write(config,"user preferences and controller mapping");
        prepareEmulators(env); QVERIFY(input.open(QIODevice::ReadOnly));
        QCOMPARE(input.readAll(),QByteArray("user preferences and controller mapping"));
        QVERIFY(graphics.open(QIODevice::ReadOnly));
        QCOMPARE(graphics.readAll(),QByteArray("existing renderer preferences"));
        QFile gui(rpcsGui); QVERIFY(gui.open(QIODevice::ReadOnly));
        QCOMPARE(gui.readAll(),QByteArray("existing desktop preferences"));
    }
    void standaloneAlternativesNeverRebindExistingCoreSaves() {
        MockLibraryRepository library;
        StandaloneAdapter cemu("cemu",library,{probe(),probe(),{},{"wiiu"}});
        StandaloneAdapter ps2("armsx2",library,{probe(),probe(),{},{"ps2"}});
        StandaloneAdapter ps1("duckstation",library,{probe(),probe(),{},{"psx"}});
        AdapterRouter router({&cemu,&ps2,&ps1});
        for(const auto& platform:QStringList{"wiiu","ps2","psx"}) {
            const auto core=platform=="wiiu"?QString("cemu"):platform=="ps2"?QString("armsx2"):QString("pcsx_rearmed");
            AdventureRegistration r;r.adventure.platformId=platform;r.adventure.adapterId="retroarch";
            r.contentPath=platform=="wiiu"?"/roms/game.wua":"/roms/game.chd";r.integrationConfig={{"core",core}};r.revision=12;
            router.prepareInstallation(r);QCOMPARE(r.adventure.adapterId,"retroarch");QCOMPARE(r.integrationConfig["core"].toString(),core);QCOMPARE(r.revision,12);
            r.adventure.adapterId="unconfigured";r.integrationConfig={};router.prepareInstallation(r);
            QCOMPARE(r.adventure.adapterId,platform=="wiiu"?QString("cemu"):platform=="ps2"?QString("armsx2"):QString("duckstation"));
        }
    }
    void playStationFormatsRegisterWithoutChangingExistingBindings() {
        MockLibraryRepository library;
        StandaloneAdapter adapter("duckstation",library,{probe(),probe(),{},{"psx"}});
        for(const auto& extension:{"cue","chd","pbp","m3u","iso"}) {
            AdventureRegistration r;r.adventure.platformId="psx";r.adventure.adapterId="unconfigured";
            r.contentPath=QString("/roms/game.%1").arg(extension);
            adapter.prepareInstallation(r);QCOMPARE(r.adventure.adapterId,QString("duckstation"));
            QVERIFY(adapter.setupIssue(r).isEmpty());
            r.adventure.adapterId="retroarch";r.integrationConfig={{"core","pcsx_rearmed"}};
            adapter.prepareInstallation(r);QCOMPARE(r.adventure.adapterId,QString("retroarch"));
            QCOMPARE(r.integrationConfig["core"].toString(),QString("pcsx_rearmed"));
        }
    }
    void cemuAndDuckstationDiscoveryPreservesExistingPreferences() {
        QTemporaryDir dir;EmulatorEnvironment env;env.home=dir.path();env.configHome=dir.filePath("config");
        env.stateDirectory=dir.filePath("state");env.executables={{"Cemu",probe()},{"DuckStation",probe()}};
        const auto found=prepareEmulators(env);
        for(const auto& id:QStringList{"cemu","duckstation"}) {
            const auto installed=StandaloneInstallation::fromJson(found.profiles[id],id);
            QCOMPARE(installed.program,probe());QVERIFY(!installed.platforms.isEmpty());
        }
        env.executables.clear();const auto again=prepareEmulators(env);
        QCOMPARE(again.profiles["cemu"],found.profiles["cemu"]);
        QCOMPARE(again.profiles["duckstation"],found.profiles["duckstation"]);
    }
    void wirelessFileAliasesPreserveExactIdentity_data() {
        QTest::addColumn<bool>("ds");
        QTest::newRow("DS-nds-srl")<<true;QTest::newRow("3DS-cci-3ds")<<false;
    }
    void wirelessFileAliasesPreserveExactIdentity() {
#ifdef Q_OS_WIN
        QSKIP("Native Linux executable permissions; run these admission cases on Linux/ARM64.");
#endif
        QFETCH(bool,ds);
        const bool previous=QStandardPaths::isTestModeEnabled();QStandardPaths::setTestModeEnabled(true);
        const auto restore=qScopeGuard([previous]{QStandardPaths::setTestModeEnabled(previous);});
        const auto root=ds?melonds::bundleRoot():azahar::bundleRoot();
        QVERIFY(!QFileInfo::exists(root));QVERIFY(QDir().mkpath(root+"/transport/venv/bin"));QVERIFY(QDir().mkpath(root+"/bin"));
        const auto cleanup=qScopeGuard([root]{QDir(root).removeRecursively();});
        const QString native=ds?"bin/traineros-ds":"bin/azahar";
        QVERIFY(QFile::copy(probe(),root+'/'+native));QVERIFY(QFile::copy(probe(),root+"/transport/venv/bin/python"));
        QStringList names{native,"transport/secure-channel.py","transport/requirements.lock"};
        if(!ds)names<<"transport/room-session.py";
        QJsonObject files;
        const auto hash=[](const QString& path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return QString();return QString::fromLatin1(QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256).toHex());};
        for(const auto& name:names){if(name!=native)write(root+'/'+name,"fixture helper");files[name]=hash(root+'/'+name);}
        write(root+"/manifest.json",QJsonDocument(QJsonObject{{"protocol",1},{"source",ds?melonds::sourceRevision:azahar::sourceRevision},
            {"settings",ds?"detmp-two-owned-v1":"native-room-two-private-v1"},{"files",files},
            {"interpreterSha256",hash(root+"/transport/venv/bin/python")}}).toJson());
        QTemporaryDir dir;std::atomic_bool cancel=false;
        AdventureRegistration r;r.adventure.platformId=ds?"nds":"n3ds";r.adventure.adapterId=ds?"melonds":"azahar";
        QByteArray data(1024,'x');if(!ds)data.replace(256,4,"NCSD");
        r.contentPath=dir.filePath(ds?"Unknown.nds":"Unknown.cci");write(r.contentPath,data);
        const auto identify=[&]{return ds?melonds::netplayIdentity(r,cancel):azahar::netplayIdentity(r,cancel);};
        const auto identity=identify();QVERIFY(!identity.isEmpty());
        r.contentPath=dir.filePath(ds?"Any name.SRL":"Any name.3DS");write(r.contentPath,data);
        QVERIFY(sameMultiplayerGame(identity,identify()));
        if(ds) {
            for(const auto& ext:QStringList{"zip","7z"}) {
                r.contentPath=dir.filePath("container."+ext);
                write(r.contentPath,cartridgeArchive(ext,{{"nested/Actual.srl",data}}));
                QVERIFY(sameMultiplayerGame(identity,identify()));
                write(r.contentPath,cartridgeArchive(ext,{{"one.nds",data},{"two.nds",data}}));QVERIFY(identify().isEmpty());
            }
            r.contentPath=dir.filePath("Any name.SRL");
        } else {
            for(const auto& ext:QStringList{"zcci","zcxi","z3dsx"}) {
                QByteArray z(64,'\0');z.replace(0,4,"Z3DS");z.replace(4,4,ext=="zcci"?"NCSD":ext=="zcxi"?"NCCH":"3DSX");
                z[8]=1;qToLittleEndian<quint16>(32,z.data()+10);qToLittleEndian<quint64>(32,z.data()+16);qToLittleEndian<quint64>(1024,z.data()+24);
                z.replace(39,4,QByteArray::fromHex("5e2a4d18"));qToLittleEndian<quint32>(17,z.data()+43);
                qToLittleEndian<quint32>(7,z.data()+47);qToLittleEndian<quint32>(1024,z.data()+51);
                qToLittleEndian<quint32>(1,z.data()+55);z.replace(60,4,QByteArray::fromHex("b1ea928f"));
                r.contentPath=dir.filePath("native."+ext);write(r.contentPath,z);QVERIFY(!identify().isEmpty());
                z[24]=1;write(r.contentPath,z);QVERIFY(identify().isEmpty());
            }
            QByteArray elf(512,'\0');elf.replace(0,6,QByteArray("\x7f" "ELF\x01\x01",6));qToLittleEndian<quint16>(40,elf.data()+18);
            for(const auto& ext:QStringList{"elf","axf"}){r.contentPath=dir.filePath("homebrew."+ext);write(r.contentPath,elf);QVERIFY(!identify().isEmpty());}
            r.contentPath=dir.filePath("Any name.3DS");
        }
        data[700]='y';write(r.contentPath,data);QVERIFY(!sameMultiplayerGame(identity,identify()));
        r.adventure.collectionOnly=true;QVERIFY(identify().isEmpty());r.adventure.collectionOnly=false;
        if(!ds){data.replace(256,4,"nope");write(r.contentPath,data);QVERIFY(identify().isEmpty());}
        r.contentPath=dir.filePath("unsupported.zip");write(r.contentPath,data);QVERIFY(identify().isEmpty());
        cancel=true;QVERIFY(identify().isEmpty());
    }
    void azaharPreparationKeepsEachPlayersOwnStorageAndSettings() {
#ifdef Q_OS_WIN
        QSKIP("Native Linux bundle paths; covered on the ARM64 build.");
#endif
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const auto oldData=qgetenv("XDG_DATA_HOME"),oldConfig=qgetenv("XDG_CONFIG_HOME");
        const bool oldMode=QStandardPaths::isTestModeEnabled();QStandardPaths::setTestModeEnabled(false);
        qputenv("XDG_DATA_HOME",dir.filePath("data").toUtf8());qputenv("XDG_CONFIG_HOME",dir.filePath("config").toUtf8());
        const auto restore=qScopeGuard([&]{
            if(oldData.isNull())qunsetenv("XDG_DATA_HOME");else qputenv("XDG_DATA_HOME",oldData);
            if(oldConfig.isNull())qunsetenv("XDG_CONFIG_HOME");else qputenv("XDG_CONFIG_HOME",oldConfig);
            QStandardPaths::setTestModeEnabled(oldMode);
        });
        const auto root=azahar::bundleRoot();QVERIFY(root.startsWith(dir.path()+'/'));
        QVERIFY(QDir().mkpath(root+"/bin"));QVERIFY(QDir().mkpath(root+"/transport/venv/bin"));
        QVERIFY(QFile::copy(probe(),root+"/bin/azahar"));QVERIFY(QFile::copy(probe(),root+"/transport/venv/bin/python"));
        write(root+"/transport/room-session.py","fixture coordinator");write(root+"/transport/secure-channel.py","fixture channel");
        write(root+"/transport/requirements.lock","fixture dependencies");
        const auto hash=[](const QString& path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return QString();return QString::fromLatin1(QCryptographicHash::hash(file.readAll(),QCryptographicHash::Sha256).toHex());};
        QJsonObject files;for(const auto& name:QStringList{"bin/azahar","transport/room-session.py","transport/secure-channel.py","transport/requirements.lock"})files[name]=hash(root+'/'+name);
        write(root+"/manifest.json",QJsonDocument(QJsonObject{{"protocol",1},{"source",azahar::sourceRevision},
            {"settings","native-room-two-private-v1"},{"files",files},{"interpreterSha256",hash(root+"/transport/venv/bin/python")}}).toJson());
        AdventureRegistration r;r.adventure.id="homebrew";r.adventure.platformId="n3ds";r.adventure.adapterId="azahar";
        r.contentPath=dir.filePath("game.3dsx");write(r.contentPath,QByteArray("3DSX")+QByteArray(1024,'x'));
        std::atomic_bool cancel=false;const auto identity=azahar::netplayIdentity(r,cancel);QVERIFY(!identity.isEmpty());
        QVERIFY(!identity["temporaryProgress"].toBool());
        const auto own=dir.filePath("custom-progress");QVERIFY(QDir().mkpath(own+"/nand"));QVERIFY(QDir().mkpath(own+"/sdmc"));
        write(own+"/sdmc/own-save","the owner's unchanged progress");
        QVERIFY(QDir().mkpath(dir.filePath("config/azahar-emu")));
        const auto configFile=dir.filePath("config/azahar-emu/qt-config.ini");
        { QSettings original(configFile,QSettings::IniFormat);original.setValue("Data Storage/use_custom_storage",true);
          original.setValue("Data Storage/nand_directory",own+"/nand");original.setValue("Data Storage/sdmc_directory",own+"/sdmc");
          original.setValue("Controls/profile",3);original.setValue("Renderer/graphics_api",2);original.sync(); }
        const auto configHash=hash(configFile),saveHash=hash(own+"/sdmc/own-save");
        auto command=azahar::command(r,{identity,{},true,false,QString(64,'a'),"127.0.0.1"});
        QCOMPARE(command.prepare(command,cancel),QString());
        QFile network(command.workingDirectory+"/network.json");QVERIFY(network.open(QIODevice::ReadOnly));
        const auto prepared=QJsonDocument::fromJson(network.readAll()).object();
        QSettings privateConfig(prepared["user"].toString()+"/config/qt-config.ini",QSettings::IniFormat);
        QCOMPARE(QDir::cleanPath(privateConfig.value("Data Storage/nand_directory").toString()),QFileInfo(own+"/nand").canonicalFilePath());
        QCOMPARE(QDir::cleanPath(privateConfig.value("Data Storage/sdmc_directory").toString()),QFileInfo(own+"/sdmc").canonicalFilePath());
        QCOMPARE(privateConfig.value("Controls/profile").toInt(),3);
        QCOMPARE(hash(configFile),configHash);QCOMPARE(hash(own+"/sdmc/own-save"),saveHash);
        write(root+"/transport/room-session.py","changed helper");QVERIFY(azahar::netplayIdentity(r,cancel).isEmpty());
        auto stale=azahar::command(r,{identity,{},true,false,QString(64,'a'),"127.0.0.1"});
        QVERIFY(!stale.prepare(stale,cancel).isEmpty());
    }
    void vitaEventsRejectUnboundedOrCrossRuntimeAdmission() {
        const auto line=[](const QJsonObject& value){return QByteArray("TRAINEROS_VITA ")+QJsonDocument(value).toJson(QJsonDocument::Compact).toBase64();};
        QCOMPARE(vita::event(line({{"event","running"}}))["event"].toString(),QString("running"));
        QVERIFY(vita::event("[TrainerOSDs] running").isEmpty());
        QVERIFY(vita::event(line({{"event","running"},{"unexpected",true}})).isEmpty());
        const QJsonObject endpoint{{"mode","nearby"},{"port",12345},{"fingerprint",QString(64,'a')},{"token",QString(32,'b')}};
        QCOMPARE(vita::event(line({{"event","ready"},{"endpoint",endpoint}}))["endpoint"].toObject(),endpoint);
        auto bad=endpoint;bad["port"]=65536;
        QVERIFY(vita::event(line({{"event","ready"},{"endpoint",bad}})).isEmpty());
        bad=endpoint;bad["fingerprint"]="";
        QVERIFY(vita::event(line({{"event","ready"},{"endpoint",bad}})).isEmpty());
        QVERIFY(vita::event(QByteArray("TRAINEROS_VITA ")+QByteArray(4096,'A')).isEmpty());
        QVERIFY(vita::event("TRAINEROS_VITA !!!").isEmpty());
    }

    void azaharEventsRejectUnboundedOrCrossRuntimeAdmission() {
        const auto line=[](const QJsonObject& value){return QByteArray("TRAINEROS_3DS ")+QJsonDocument(value).toJson(QJsonDocument::Compact).toBase64();};
        QCOMPARE(azahar::event(line({{"event","running"}}))["event"].toString(),QString("running"));
        QVERIFY(azahar::event("[TrainerOSDs] running").isEmpty());
        QVERIFY(azahar::event(line({{"event","running"},{"unexpected",true}})).isEmpty());
        const QJsonObject endpoint{{"mode","nearby"},{"port",12345},{"fingerprint",QString(64,'a')},{"token",QString(32,'b')}};
        QCOMPARE(azahar::event(line({{"event","ready"},{"endpoint",endpoint}}))["endpoint"].toObject(),endpoint);
        auto bad=endpoint;bad["port"]=65536;
        QVERIFY(azahar::event(line({{"event","ready"},{"endpoint",bad}})).isEmpty());
        bad=endpoint;bad["fingerprint"]="";
        QVERIFY(azahar::event(line({{"event","ready"},{"endpoint",bad}})).isEmpty());
        QVERIFY(azahar::event(QByteArray("TRAINEROS_3DS ")+QByteArray(4096,'A')).isEmpty());
        QVERIFY(azahar::event("TRAINEROS_3DS !!!").isEmpty());
    }

    void dolphinGenericGameCubeIdentityKeepsNativeBridgeAndDiscChecks() {
        const bool previousTestMode=QStandardPaths::isTestModeEnabled();
        QStandardPaths::setTestModeEnabled(true);
        const auto restoreMode=qScopeGuard([previousTestMode]{QStandardPaths::setTestModeEnabled(previousTestMode);});
        const auto root=dolphin::bridgeRoot();
        // Never replace even an existing test installation to make a fixture.
        QVERIFY(!QFileInfo::exists(root));
        QVERIFY(QDir().mkpath(root+"/bin"));
        const auto cleanup=qScopeGuard([root]{
            QFile::remove(root+"/manifest.json");QFile::remove(root+"/bin/dolphin-emu");
            QDir().rmdir(root+"/bin");QDir().rmdir(root);
        });
        QTemporaryDir dir;std::atomic_bool cancel=false;
        write(dolphin::bridgeFile(),"fixture runtime");
        const auto hash=QString::fromLatin1(QCryptographicHash::hash("fixture runtime",QCryptographicHash::Sha256).toHex());
        const auto manifest=[&](const QString& runtime,QString hotseat={}){write(root+"/manifest.json",QJsonDocument(QJsonObject{
            {"protocol",1},{"source","c77bbaa0f372c3f72281602a8b087206706542cb"},{"sha256",runtime},{"hotseat",hotseat}}).toJson());};
        manifest(hash);
        QByteArray disc(1024,'\0');disc.replace(0,6,"GTEST0");disc.replace(0x1c,4,QByteArray::fromHex("c2339f3d"));
        write(dir.filePath("game.iso"),disc);
        AdventureRegistration r;r.adventure.platformId="gc";r.adventure.adapterId="dolphin";
        r.adventure.title="Local title";r.contentPath=dir.filePath("game.iso");
        StandaloneInstallation i;i.platforms={"gc","wii"};
        const auto identity=dolphin::netplayIdentity(r,i,cancel,4);
        QCOMPARE(identity["id"].toString(),QString("runtime.dolphin.gc.v1"));
        QCOMPARE(identity["players"].toInt(),4);QVERIFY(!identity["lateJoin"].toBool(true));
        r.adventure.title="Other local name";
        QVERIFY(sameMultiplayerGame(identity,dolphin::netplayIdentity(r,i,cancel,4)));
        QCOMPARE(dolphin::netplayIdentity(r,i,cancel)["players"].toInt(),2);
        QVERIFY(sameMultiplayerGame(identity,dolphin::netplayIdentity(r,i,cancel)));
        QVERIFY(multiplayerModeProfile(identity,"pass-p1").isEmpty());
        manifest(hash,"dolphin-golf-p1-v1");
        const auto capable=dolphin::netplayIdentity(r,i,cancel,4);
        const auto turns=multiplayerModeProfile(capable,"pass-p1");
        QVERIFY(usesSharedController(turns));QCOMPARE(turns["players"].toInt(),2);
        QVERIFY(supportsMultiplayerSession(capable,turns));QVERIFY(!supportsMultiplayerSession(identity,turns));
        QCOMPARE(multiplayerModeProfile(capable,"separate")["players"].toInt(),4);
        manifest(hash);
        QVERIFY(dolphin::netplayIdentity(r,i,cancel,5).isEmpty());
        disc[512]=1;write(r.contentPath,disc);
        QVERIFY(!sameMultiplayerGame(identity,dolphin::netplayIdentity(r,i,cancel,4)));
        manifest(QString(64,'0'));QVERIFY(dolphin::netplayIdentity(r,i,cancel).isEmpty());manifest(hash);
        disc.replace(0x1c,4,QByteArray(4,'\0'));disc.replace(0x18,4,QByteArray::fromHex("5d1c9ea3"));
        write(r.contentPath,disc);QVERIFY(dolphin::netplayIdentity(r,i,cancel).isEmpty()); // Wii needs its own input route.
        r.adventure.platformId="wii";QVERIFY(dolphin::netplayIdentity(r,i,cancel).isEmpty());
        r.adventure.platformId="gc";r.contentPath=dir.filePath("game.rvz");write(r.contentPath,disc);
        QVERIFY(dolphin::netplayIdentity(r,i,cancel).isEmpty());
    }
    void dolphinCompressedGameCubeUsesNativeReader_data() {
        QTest::addColumn<QString>("extension");
        for(const auto& extension:{"rvz","wia","gcz","ciso","tgc"})QTest::newRow(extension)<<QString(extension);
    }
    void dolphinSplitImagesMatchEveryNativePart() {
        QTemporaryDir dir;std::atomic_bool cancel=false;
        for(const auto& name:QStringList{"game.part0.iso","game.wbfs"}) {
            const auto path=dir.filePath(name),part=dir.filePath(name.endsWith("iso")?"game.part1.iso":"game.wbf1");
            write(path,"first part");QStringList observed;
            const auto single=dolphin::contentIdentity(path,1024,cancel,[&](const QString& p){observed.append(p);});
            QVERIFY(!single.isEmpty());QVERIFY(observed.contains(part));
            write(part,"second part");const auto both=dolphin::contentIdentity(path,1024,cancel);
            QVERIFY(!both.isEmpty());QVERIFY(both!=single);
            write(part,"changed part");QVERIFY(dolphin::contentIdentity(path,1024,cancel)!=both);
            QVERIFY(dolphin::contentIdentity(path,12,cancel).isEmpty());
            QVERIFY(QFile::remove(part));QCOMPARE(dolphin::contentIdentity(path,1024,cancel),single);
        }
    }
    void dolphinCompressedGameCubeUsesNativeReader() {
        QFETCH(QString,extension);
        const bool previous=QStandardPaths::isTestModeEnabled();QStandardPaths::setTestModeEnabled(true);
        const auto restore=qScopeGuard([previous]{QStandardPaths::setTestModeEnabled(previous);});
        const auto root=dolphin::bridgeRoot();QVERIFY(!QFileInfo::exists(root));QVERIFY(QDir().mkpath(root+"/bin"));
        const auto cleanup=qScopeGuard([root]{QDir(root).removeRecursively();}); // Fixture directory created above only.
        write(dolphin::bridgeFile(),"fixture runtime");QVERIFY(QFile::copy(probe(),root+"/bin/dolphin-tool"));
        QFile reader(probe());QVERIFY(reader.open(QIODevice::ReadOnly));
        QJsonObject manifest{{"protocol",1},{"source","c77bbaa0f372c3f72281602a8b087206706542cb"},
            {"sha256",QString::fromLatin1(QCryptographicHash::hash("fixture runtime",QCryptographicHash::Sha256).toHex())},
            {"toolSha256",QString::fromLatin1(QCryptographicHash::hash(reader.readAll(),QCryptographicHash::Sha256).toHex())}};
        const auto saveManifest=[&]{write(root+"/manifest.json",QJsonDocument(manifest).toJson());};saveManifest();
        QTemporaryDir dir;std::atomic_bool cancel=false;
        AdventureRegistration r;r.adventure.platformId="gc";r.adventure.adapterId="dolphin";
        r.adventure.title="Any multiplayer title";r.contentPath=dir.filePath("unlisted."+extension);
        write(r.contentPath,QJsonDocument(QJsonObject{{"platform","gc"},{"game_id","GXYZ01"},{"revision",0}}).toJson());
        StandaloneInstallation installation;installation.platforms={"gc","wii"};
        const auto identity=dolphin::netplayIdentity(r,installation,cancel,4);
        QCOMPARE(identity["id"].toString(),QString("runtime.dolphin.gc.v1"));
        QCOMPARE(identity["players"].toInt(),4);QVERIFY(identity["temporaryProgress"].toBool());
        write(r.contentPath,QJsonDocument(QJsonObject{{"platform","gc"},{"revision",1}}).toJson());
        QVERIFY(!sameMultiplayerGame(identity,dolphin::netplayIdentity(r,installation,cancel,4)));
        manifest["toolSha256"]=QString(64,'0');saveManifest();QVERIFY(dolphin::netplayIdentity(r,installation,cancel).isEmpty());
        manifest["toolSha256"]=QString::fromLatin1(QCryptographicHash::hash([&]{reader.seek(0);return reader.readAll();}(),QCryptographicHash::Sha256).toHex());saveManifest();
        write(r.contentPath,QJsonDocument(QJsonObject{{"platform","wii"},{"revision",0}}).toJson());
        QVERIFY(dolphin::netplayIdentity(r,installation,cancel).isEmpty());
        write(r.contentPath,"Invalid compressed volume, even with the right extension.");
        QVERIFY(dolphin::netplayIdentity(r,installation,cancel).isEmpty());
        cancel=true;QVERIFY(dolphin::netplayIdentity(r,installation,cancel).isEmpty());
    }
    void dolphinWiiUsesVerifiedNativeVolumeReaderAndSeparateRemotes() {
        const bool previous=QStandardPaths::isTestModeEnabled();QStandardPaths::setTestModeEnabled(true);
        const auto restore=qScopeGuard([previous]{QStandardPaths::setTestModeEnabled(previous);});
        const auto root=dolphin::bridgeRoot();QVERIFY(!QFileInfo::exists(root));QVERIFY(QDir().mkpath(root+"/bin"));
        const auto cleanup=qScopeGuard([root]{
            QFile::remove(root+"/manifest.json");QFile::remove(root+"/bin/dolphin-emu");QFile::remove(root+"/bin/dolphin-tool");
            QDir().rmdir(root+"/bin");QDir().rmdir(root);
        });
        write(dolphin::bridgeFile(),"fixture runtime");QVERIFY(QFile::copy(probe(),root+"/bin/dolphin-tool"));
        QFile reader(probe());QVERIFY(reader.open(QIODevice::ReadOnly));
        QJsonObject manifest{{"protocol",1},{"source","c77bbaa0f372c3f72281602a8b087206706542cb"},
            {"sha256",QString::fromLatin1(QCryptographicHash::hash("fixture runtime",QCryptographicHash::Sha256).toHex())},
            {"toolSha256",QString::fromLatin1(QCryptographicHash::hash(reader.readAll(),QCryptographicHash::Sha256).toHex())},
            {"wiimote","dolphin-wiimote-v1"},{"hotseat","dolphin-golf-p1-v1"}};
        const auto saveManifest=[&]{write(root+"/manifest.json",QJsonDocument(manifest).toJson());};saveManifest();
        QTemporaryDir dir;std::atomic_bool cancel=false;
        AdventureRegistration r;r.adventure.platformId="wii";r.adventure.adapterId="dolphin";
        r.adventure.title="Unrelated local name";r.contentPath=dir.filePath("game.rvz");
        write(r.contentPath,QJsonDocument(QJsonObject{{"platform","wii"},{"revision",0}}).toJson());
        StandaloneInstallation i;i.platforms={"gc","wii"};
        const auto identity=dolphin::netplayIdentity(r,i,cancel,4);
        QCOMPARE(identity["id"].toString(),QString("runtime.dolphin.wii.v1"));QCOMPARE(identity["players"].toInt(),4);
        QVERIFY(identity["temporaryProgress"].toBool());
        QVERIFY(multiplayerModeProfile(identity,"pass-p1").isEmpty());
        const auto originalPath=r.contentPath;
        r.contentPath=dir.filePath("channel.wad");write(r.contentPath,QJsonDocument(QJsonObject{{"platform","wii"},{"revision",0}}).toJson());
        QVERIFY(dolphin::netplayIdentity(r,i,cancel,4).isEmpty());
        manifest["wad"]="dolphin-wad-channel-v1";saveManifest();
        QVERIFY(!dolphin::netplayIdentity(r,i,cancel,4).isEmpty());r.contentPath=originalPath;
        write(r.contentPath,QJsonDocument(QJsonObject{{"platform","gc"},{"revision",0}}).toJson());
        QVERIFY(dolphin::netplayIdentity(r,i,cancel,4).isEmpty());
        write(r.contentPath,QJsonDocument(QJsonObject{{"platform","wii"},{"revision",1}}).toJson());
        QVERIFY(!sameMultiplayerGame(identity,dolphin::netplayIdentity(r,i,cancel,4)));
        manifest["toolSha256"]=QString(64,'0');saveManifest();QVERIFY(dolphin::netplayIdentity(r,i,cancel,4).isEmpty());
        write(dir.filePath("Dolphin.ini"),"[General]\nNANDRootPath = /personal/wii\n");
        const QByteArray mapping="[Wiimote1]\nDevice = SDL/0/Gamepad\nExtension = Nunchuk\n";
        write(dir.filePath("WiimoteNew.ini"),mapping);i.configFile=dir.filePath("Dolphin.ini");
        dolphin::NetplayRequest request{identity,false,false,"192.168.1.5",QString(32,'a'),2626,2,{1,2}};
        ProcessCommand cmd;QVERIFY(dolphin::configureNetplay(cmd,i,r.contentPath,request,cancel).isEmpty());
        const auto sessionRoot=cmd.arguments.last();
        QFile input(sessionRoot+"/Config/WiimoteNew.ini");QVERIFY(input.open(QIODevice::ReadOnly));QCOMPARE(input.readAll(),mapping);input.close();
        QFile config(sessionRoot+"/Config/Dolphin.ini");QVERIFY(config.open(QIODevice::ReadOnly));QVERIFY(!config.readAll().contains("/personal"));config.close();
        QFile session(sessionRoot+"/session.json");QVERIFY(session.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(session.readAll()).object()["controllers"].toString(),QString("wiimote"));session.close();
        QVERIFY(!QFileInfo::exists(sessionRoot+"/Wii"));QVERIFY(cmd.runtimeControls["hotseat"].toString().isEmpty());
        cmd.settled({});QVERIFY(!QFileInfo::exists(sessionRoot));
        request.expected["playMode"]="pass-p1";request.expected["players"]=2;
        request.expected["controllerModes"]=QJsonArray{"pass-p1"};
        QVERIFY(!dolphin::configureNetplay(cmd,i,r.contentPath,request,cancel).isEmpty());
    }
    void dolphinSessionKeepsControllerBridgeAndNeverLinksPersonalSaves() {
        QTemporaryDir dir;QVERIFY(dir.isValid());
        write(dir.filePath("Dolphin.ini"),"[Core]\nMemcardAPath = /personal/card.raw\n");
        write(dir.filePath("GCPadNew.ini"),"[GCPad1]\nDevice = SDL/0/Gamepad\n");
        StandaloneInstallation i{probe(),probe(),{"/controller.py","--","flatpak","run","org.DolphinEmu.dolphin-emu"},{"gc"},dir.filePath("Dolphin.ini")};
        dolphin::NetplayRequest r{{},false,false,"192.168.1.5",QString(32,'a'),2626,3,{1,3}};
        ProcessCommand cmd;std::atomic_bool cancel=false;
        QVERIFY(dolphin::configureNetplay(cmd,i,"/roms/Game.iso",r,cancel).isEmpty());
        QCOMPARE(cmd.program,QString("/usr/bin/env"));
        QVERIFY(cmd.arguments.contains("/controller.py"));QVERIFY(!cmd.arguments.contains("flatpak"));
        const int overrideIndex=cmd.arguments.indexOf("-C");
        QVERIFY(cmd.arguments.contains("-b"));
        QVERIFY(overrideIndex>=0);
        QCOMPARE(cmd.arguments.value(overrideIndex+1),QString("Dolphin.Interface.ConfirmStop=False"));
        const auto root=cmd.arguments.last();QVERIFY(QFileInfo(root).isDir());
        QFile config(root+"/Config/Dolphin.ini");QVERIFY(config.open(QIODevice::ReadOnly));
        const auto bytes=config.readAll();config.close();
        QVERIFY(!bytes.contains("/personal"));QVERIFY(bytes.contains("WriteSaveData = False"));
        QVERIFY(!QFileInfo::exists(root+"/GC"));
        QFile session(root+"/session.json");QVERIFY(session.open(QIODevice::ReadOnly));
        const auto json=QJsonDocument::fromJson(session.readAll()).object();session.close();
        QCOMPARE(json["slot"].toInt(),3);QCOMPARE(json["slots"].toArray(),QJsonArray({1,3}));
        cmd.settled({});QVERIFY(!QFileInfo::exists(root));
        QFile original(dir.filePath("Dolphin.ini"));QVERIFY(original.open(QIODevice::ReadOnly));
        QVERIFY(original.readAll().contains("/personal/card.raw"));
        i.program="/usr/bin/flatpak";i.prefixArguments={"run","org.DolphinEmu.dolphin-emu"};
        ProcessCommand native;
        QVERIFY(dolphin::configureNetplay(native,i,"/roms/Game.iso",r,cancel).isEmpty());
        QVERIFY(!native.arguments.contains("/usr/bin/flatpak"));
        QCOMPARE(native.arguments.value(1),dolphin::bridgeFile());
        native.settled({});
        r.expected=multiplayerModeProfile(QJsonObject{{"players",4},{"controllerModes",QJsonArray{"pass-p1"}}},"pass-p1");
        QVERIFY(!dolphin::configureNetplay(native,i,"/roms/Game.iso",r,cancel).isEmpty()); // no sparse slot 3 in turns
        r.slot=2;r.seats={1,2};
        QVERIFY(dolphin::configureNetplay(native,i,"/roms/Game.iso",r,cancel).isEmpty());
        QCOMPARE(native.runtimeControls["hotseat"].toString(),QString("dolphin-golf-p1-v1"));
        QFile turnsSession(native.arguments.last()+"/session.json");QVERIFY(turnsSession.open(QIODevice::ReadOnly));
        QVERIFY(QJsonDocument::fromJson(turnsSession.readAll()).object()["hotseat"].toBool());turnsSession.close();
        native.settled({});r.expected={};r.slot=3;
        r.seats={1,1};ProcessCommand rejected;
        QVERIFY(!dolphin::configureNetplay(rejected,i,"/roms/Game.iso",r,cancel).isEmpty());
        QVERIFY(rejected.program.isEmpty());QVERIFY(!rejected.settled);
        r.seats={1,3};r.online=true;r.address="bad\n[Core]";
        QVERIFY(dolphin::configureNetplay(rejected,i,"/roms/Game.iso",r,cancel).contains("address"));
    }

    void dolphinReinvitationRetainsShadersButDiscardsSessionProgress() {
#ifndef Q_OS_UNIX
        QSKIP("The installed NetPlay runtimes use Unix cache links.");
#endif
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const auto oldCache=qgetenv("XDG_CACHE_HOME");
        const auto oldMode=QStandardPaths::isTestModeEnabled();
        QStandardPaths::setTestModeEnabled(false);
        qputenv("XDG_CACHE_HOME",dir.filePath("cache").toUtf8());
        const auto restore=qScopeGuard([&]{
            if(oldCache.isNull())qunsetenv("XDG_CACHE_HOME");else qputenv("XDG_CACHE_HOME",oldCache);
            QStandardPaths::setTestModeEnabled(oldMode);
        });
        write(dir.filePath("Dolphin.ini"),"ordinary settings");
        write(dir.filePath("GCPadNew.ini"),"[GCPad1]\nDevice = SDL/0/Gamepad\n");
        StandaloneInstallation installation{probe(),probe(),{},{"gc"},dir.filePath("Dolphin.ini")};
        dolphin::NetplayRequest request{{{"runtime",QString(64,'a')}},true,false,{},QString(32,'b'),2626,1,{1,2}};
        ProcessCommand first;std::atomic_bool cancel=false;
        QVERIFY(dolphin::configureNetplay(first,installation,"/roms/game.iso",request,cancel).isEmpty());
        const auto session=first.arguments.last();
        const auto cache=QFileInfo(session+"/Cache/Shaders").canonicalFilePath();
        QVERIFY(cache.startsWith(dir.filePath("cache/TrainerOS/netplay/")));
        write(session+"/Cache/Shaders/pipeline.cache","compiled renderer cache");
        QVERIFY(QDir().mkpath(session+"/GC"));write(session+"/GC/card.raw","temporary progress");
        first.settled({});QVERIFY(!QFileInfo::exists(session));
        QVERIFY(QFileInfo::exists(cache+"/pipeline.cache"));
        ProcessCommand next;
        QVERIFY(dolphin::configureNetplay(next,installation,"/roms/game.iso",request,cancel).isEmpty());
        QFile shader(next.arguments.last()+"/Cache/Shaders/pipeline.cache");QVERIFY(shader.open(QIODevice::ReadOnly));
        QCOMPARE(shader.readAll(),QByteArray("compiled renderer cache"));shader.close();
        QVERIFY(!QFileInfo::exists(next.arguments.last()+"/GC/card.raw"));next.settled({});
        request.expected["runtime"]=QString(64,'c');
        ProcessCommand updated;
        QVERIFY(dolphin::configureNetplay(updated,installation,"/roms/game.iso",request,cancel).isEmpty());
        QVERIFY(!QFileInfo::exists(updated.arguments.last()+"/Cache/Shaders/pipeline.cache"));updated.settled({});
        QFile original(dir.filePath("Dolphin.ini"));QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(),QByteArray("ordinary settings"));
    }

    void pspRawDiscProbeRejectsAmbiguityAndOutOfBounds() {
        QTemporaryDir dir;std::atomic_bool cancel=false;
        const auto entry=[](quint32 sector,quint32 bytes,const QByteArray& name,bool directory) {
            QByteArray r((33+name.size()+1)&~1,'\0');r[0]=char(r.size());r[25]=directory?2:0;r[32]=char(name.size());
            qToLittleEndian(sector,r.data()+2);qToBigEndian(sector,r.data()+6);
            qToLittleEndian(bytes,r.data()+10);qToBigEndian(bytes,r.data()+14);
            qToLittleEndian<quint16>(1,r.data()+28);qToBigEndian<quint16>(1,r.data()+30);
            r.replace(33,name.size(),name);return r;
        };
        QByteArray sfo(88,'\0');sfo.replace(0,4,QByteArray::fromHex("00505346"));
        qToLittleEndian<quint32>(0x101,sfo.data()+4);
        qToLittleEndian<quint32>(52,sfo.data()+8);qToLittleEndian<quint32>(72,sfo.data()+12);qToLittleEndian<quint32>(2,sfo.data()+16);
        for(int pos:{20,36})qToLittleEndian<quint16>(0x204,sfo.data()+pos+2);
        qToLittleEndian<quint32>(3,sfo.data()+24);qToLittleEndian<quint32>(4,sfo.data()+28);
        qToLittleEndian<quint16>(9,sfo.data()+36);qToLittleEndian<quint32>(10,sfo.data()+40);
        qToLittleEndian<quint32>(12,sfo.data()+44);qToLittleEndian<quint32>(4,sfo.data()+48);
        sfo.replace(52,17,QByteArray("CATEGORY\0DISC_ID\0",17));sfo.replace(72,2,"UG");sfo.replace(76,9,"TEST00001");
        QByteArray disc(24*2048,'\0');const int pvd=16*2048;
        disc.replace(pvd,7,QByteArray::fromHex("01434430303101"));
        qToLittleEndian<quint32>(24,disc.data()+pvd+80);qToBigEndian<quint32>(24,disc.data()+pvd+84);
        qToLittleEndian<quint16>(2048,disc.data()+pvd+128);qToBigEndian<quint16>(2048,disc.data()+pvd+130);
        const auto root=entry(20,2048,QByteArray(1,'\0'),true);
        const auto game=entry(21,2048,"PSP_GAME",true);
        const auto param=entry(22,sfo.size(),"PARAM.SFO;1",false);
        disc.replace(pvd+156,root.size(),root);disc.replace(20*2048,game.size(),game);
        disc.replace(21*2048,param.size(),param);disc.replace(22*2048,sfo.size(),sfo);
        const auto path=dir.filePath("Unrelated filename.iso");write(path,disc);
        QCOMPARE(ppsspp::discId(path,cancel),QString("TEST00001"));
        const auto bad=[&](QByteArray changed){write(path,changed);QVERIFY(ppsspp::discId(path,cancel).isEmpty());};
        auto changed=disc;changed[22*2048+73]='V';bad(changed); // UMD video, not a game
        changed=disc;changed.replace(20*2048+game.size(),game.size(),game);bad(changed); // duplicate directory
        changed=disc;qToLittleEndian<quint32>(0xffffffff,changed.data()+21*2048+2);
        qToBigEndian<quint32>(0xffffffff,changed.data()+21*2048+6);bad(changed);
        changed=disc;qToLittleEndian<quint32>(21,changed.data()+pvd+80);
        qToBigEndian<quint32>(21,changed.data()+pvd+84);bad(changed); // entry beyond declared volume
        changed=disc;qToLittleEndian<quint32>(0xffffffff,changed.data()+22*2048+48);bad(changed);
        changed=disc;qToLittleEndian<quint16>(0,changed.data()+22*2048+36);bad(changed); // duplicate SFO key
        changed=disc;changed[22*2048+85]='X';bad(changed); // missing terminator
        changed=disc;changed[20*2048]=33;bad(changed); // short directory entry
        bad(disc.left(22*2048));
        write(dir.filePath("game.cso"),disc);QVERIFY(ppsspp::discId(dir.filePath("game.cso"),cancel).isEmpty());
        write(path,disc);cancel=true;QVERIFY(ppsspp::discId(path,cancel).isEmpty());cancel=false;
        AdventureRegistration r;r.adventure.platformId="psp";r.adventure.adapterId="ppsspp";r.contentPath=path;
        StandaloneInstallation i{probe(),probe(),{},{"psp"},{}};
        QVERIFY(ppsspp::netplayIdentity(r,i,cancel).isEmpty()); // valid disc does not authorize an unknown runtime
    }
    void pspPackedProbeRequiresVerifiedReaderAndStrictReply() {
        const bool previous=QStandardPaths::isTestModeEnabled();QStandardPaths::setTestModeEnabled(true);
        const auto restore=qScopeGuard([previous]{QStandardPaths::setTestModeEnabled(previous);});
        const auto root=ppsspp::contentProbeRoot();
        QVERIFY(!QFileInfo::exists(root));QVERIFY(QDir().mkpath(root));
        const auto cleanup=qScopeGuard([root]{QDir(root).removeRecursively();});
        QTemporaryDir dir;std::atomic_bool cancel=false;
        const auto path=dir.filePath("Arbitrary PSP name.cso");
        const QByteArray reply=R"({"protocol":1,"disc":"TEST00001","format":"cso"})";
        write(path,reply);QVERIFY(ppsspp::discId(path,cancel).isEmpty());
        QVERIFY(QFile::copy(probe(),ppsspp::contentProbeFile()));
        QFile::setPermissions(ppsspp::contentProbeFile(),QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        QFile executable(ppsspp::contentProbeFile());QVERIFY(executable.open(QIODevice::ReadOnly));
        const auto hash=QString::fromLatin1(QCryptographicHash::hash(executable.readAll(),QCryptographicHash::Sha256).toHex());executable.close();
        QJsonObject manifest{{"recipe","psp-content-v1"},{"source","fa50bb1976065c4f8b1b47af227d367fe9771555"},
            {"files",QJsonObject{{QFileInfo(ppsspp::contentProbeFile()).fileName(),hash}}}};
        const auto publish=[&]{write(root+"/manifest.json",QJsonDocument(manifest).toJson());};publish();
        QCOMPARE(ppsspp::discId(path,cancel),QString("TEST00001"));
        const auto pbp=dir.filePath("EBOOT.PBP");auto pbpReply=reply;pbpReply.replace("cso","pbp");write(pbp,pbpReply);
        QCOMPARE(ppsspp::discId(pbp,cancel),QString("TEST00001"));
        for(const auto& bad:{QByteArray("not json"),QByteArray(5000,'x'),QByteArray(R"({"protocol":2,"disc":"TEST00001","format":"cso"})"),pbpReply,
            QByteArray(R"({"protocol":1,"disc":"TEST00001\n","format":"cso"})")}) {
            write(path,bad);QVERIFY(ppsspp::discId(path,cancel).isEmpty());
        }
        write(path,reply);cancel=true;QVERIFY(ppsspp::discId(path,cancel).isEmpty());cancel=false;
        manifest["recipe"]="unreviewed";publish();QVERIFY(ppsspp::discId(path,cancel).isEmpty());
        manifest["recipe"]="psp-content-v1";manifest["source"]="unreviewed";publish();QVERIFY(ppsspp::discId(path,cancel).isEmpty());
        manifest["source"]="fa50bb1976065c4f8b1b47af227d367fe9771555";
        manifest["files"]=QJsonObject{{QFileInfo(ppsspp::contentProbeFile()).fileName(),QString(64,'0')}};publish();
        QVERIFY(ppsspp::discId(path,cancel).isEmpty());
    }
    void pspClockPolicyStaysScopedToTheReviewedLuminesBuild() {
        ppsspp::NetplayRequest r{{{"id","runtime.ppsspp.adhoc.v1"}},true,true,"socom.cc","Alice\n[Network]"};
        auto bytes=ppsspp::networkSettings(r);
        QVERIFY(bytes.contains("[General]\nForceLagSync2 = False\n"));
        QVERIFY(bytes.contains("EnableAdhocServer = False\n"));QVERIFY(bytes.contains("AdhocServerRelayMode = 1\n"));
        QVERIFY(!bytes.contains("MacAddress"));QVERIFY(!bytes.contains("Alice\n"));
        QVERIFY(bytes.contains("PauseOnLostFocus = False\nPauseWhenMinimized = False\n"));
        r.expected["id"]="runtime.ppsspp.lumines-us.v1";
        QVERIFY(ppsspp::networkSettings(r).contains("[General]\nForceLagSync2 = True\n"));
        r.online=false;r.address="192.168.1.2";
        bytes=ppsspp::networkSettings(r);QVERIFY(bytes.contains("EnableAdhocServer = True\n"));
        QVERIFY(bytes.contains("AdhocServerRelayMode = 2\n"));
        r.host=false;QVERIFY(ppsspp::networkSettings(r).contains("EnableAdhocServer = False\n"));
    }
    void pspHostRelayOnlyUsesItsOwnLoopbackEndpoints() {
        ppsspp::NetplayRequest r{{{"id","runtime.ppsspp.adhoc.v1"}},true,true,"127.0.0.1","Host",true};
        auto bytes=ppsspp::networkSettings(r);
        QVERIFY(bytes.contains("EnableAdhocServer = True\n"));
        QVERIFY(bytes.contains("AdhocServerRelayMode = 1\n"));
        r.host=false;r.address="127.0.0.2";
        QVERIFY(ppsspp::networkSettings(r).contains("EnableAdhocServer = False\n"));
        ProcessCommand command;StandaloneInstallation installation;std::atomic_bool cancel=false;
        for(const auto& address:{"socom.cc","192.168.1.1","127.0.0.1"}) {
            r.address=address;
            QCOMPARE(ppsspp::configureNetplay(command,installation,r,cancel),"This multiplayer address is invalid.");
        }
        r.address="127.0.0.2";r.online=false;
        QCOMPARE(ppsspp::configureNetplay(command,installation,r,cancel),"This multiplayer address is invalid.");
        r.online=true;r.hostRelay=false;
        QCOMPARE(ppsspp::configureNetplay(command,installation,r,cancel),"This multiplayer address is invalid.");
    }
    void pspMultiplayerKeepsSettingsPrivateAndOrdinarySavesShared() {
#ifdef Q_OS_WIN
        QSKIP("Linux memory-stick directory symlinks.");
#endif
        QTemporaryDir dir;
        const auto system=dir.filePath("PSP/SYSTEM");
        QVERIFY(QDir().mkpath(system));QVERIFY(QDir().mkpath(dir.filePath("PSP/SAVEDATA")));
        const QByteArray original="[Network]\nEnableWlan = False\n[Graphics]\nRenderingMode = 1\n"
            "[SystemParam]\nMacAddress = 04:11:22:33:44:55\n";
        const QByteArray originalGame="[SystemParam]\nMacAddress = 04:66:77:88:99:aa\n"
            "[General]\nForceLagSync2 = False\nPauseOnLostFocus = True\nPauseWhenMinimized = True\n";
        write(system+"/ppsspp.ini",original);write(system+"/controls.ini","My controller bindings");
        write(system+"/ULUS10002_ppsspp.ini",originalGame);
        write(dir.filePath("PSP/SAVEDATA/existing.bin"),"Existing progress");
        StandaloneInstallation installation{probe(),probe(),{"run","org.ppsspp.PPSSPP"},{"psp"},system+"/ppsspp.ini"};
        ProcessCommand command{"/usr/bin/flatpak",{"run","org.ppsspp.PPSSPP","--fullscreen","/roms/PSP title.iso"},{}};
        std::atomic_bool cancel=false;
        ppsspp::NetplayRequest request{{{"id","runtime.ppsspp.lumines-us.v1"}},true,true,"socom.cc","Alice\n[Network]"};
        QVERIFY(ppsspp::configureNetplay(command,installation,request,cancel).isEmpty());
        QCOMPARE(command.arguments.last(),"/roms/PSP title.iso");
        const auto argument=command.arguments.filter("XDG_CONFIG_HOME=").first();
        const auto root=argument.mid(QString("XDG_CONFIG_HOME=").size());
        QCOMPARE(command.arguments.indexOf(argument)-1,command.arguments.indexOf("org.ppsspp.PPSSPP"));
        QVERIFY(command.arguments.contains("--command=env"));
        QFile copied(root+"/ppsspp/PSP/SYSTEM/controls.ini");QVERIFY(copied.open(QIODevice::ReadOnly));
        QCOMPARE(copied.readAll(),"My controller bindings");copied.close();
        QFile copiedGlobal(root+"/ppsspp/PSP/SYSTEM/ppsspp.ini");QVERIFY(copiedGlobal.open(QIODevice::ReadOnly));
        QCOMPARE(copiedGlobal.readAll(),original);copiedGlobal.close();
        QFile copiedGame(root+"/ppsspp/PSP/SYSTEM/ULUS10002_ppsspp.ini");QVERIFY(copiedGame.open(QIODevice::ReadOnly));
        QCOMPARE(copiedGame.readAll(),originalGame);copiedGame.close();
        QCOMPARE(QFileInfo(root+"/ppsspp/PSP/SAVEDATA").canonicalFilePath(),QFileInfo(dir.filePath("PSP/SAVEDATA")).canonicalFilePath());
        QFile network(command.arguments.filter("--appendconfig=").first().mid(15));QVERIFY(network.open(QIODevice::ReadOnly));
        const auto settings=network.readAll();network.close();
        QVERIFY(settings.contains("AdhocServerRelayMode = 1"));QVERIFY(settings.contains("EnableAdhocServer = False"));
        QVERIFY(settings.contains("AchievementsEnable = False"));QVERIFY(!settings.contains("Alice\n"));
        QVERIFY(!settings.contains("MacAddress")); // appended options must not replace either saved identity
        // Upstream reads clock synchronization from General, not Network/CPU.
        QVERIFY(settings.contains("[General]\nForceLagSync2 = True\n"));
        QVERIFY(settings.contains("PauseOnLostFocus = False\nPauseWhenMinimized = False\n"));
        // Simulate PPSSPP saving merged options, including its per-game INI.
        write(root+"/ppsspp/PSP/SYSTEM/ppsspp.ini","Session changes");
        write(root+"/ppsspp/PSP/SYSTEM/ULUS10002_ppsspp.ini","Session changes");
        command.settled({});QVERIFY(!QFileInfo::exists(root));
        QFile originalFile(system+"/ppsspp.ini");QVERIFY(originalFile.open(QIODevice::ReadOnly));QCOMPARE(originalFile.readAll(),original);
        QFile perGame(system+"/ULUS10002_ppsspp.ini");QVERIFY(perGame.open(QIODevice::ReadOnly));QCOMPARE(perGame.readAll(),originalGame);
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
        QVERIFY(QDir().mkpath(env.home+"/.local/bin"));
        const auto executableSuffix=QFileInfo(probe()).suffix();
        QVERIFY(QFile::copy(probe(),env.home+"/.local/bin/azahar"+
            (executableSuffix.isEmpty()?QString():'.'+executableSuffix)));
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
        const auto dsInstallation=StandaloneInstallation::fromJson(found.profiles["melonds"],"melonds");
        QVERIFY(dsInstallation.melonDsSaveBackups);QCOMPARE(dsInstallation.configFile,ds);
        const auto az=dir.filePath("config/azahar-emu/qt-config.ini");QFile azFile(az);QVERIFY(azFile.open(QIODevice::ReadOnly));
        const auto azInitial=azFile.readAll();azFile.close();QVERIFY(azInitial.contains("engine:sdl,maptype:all"));
        QVERIFY(azInitial.contains("graphics_api=2"));QVERIFY(found.profiles.contains("azahar"));
        write(az,"User 3DS controls and preferences\n");
        write(ra.configFile,"User settings must stay exactly as written\n");
        write(ds,"User controller preferences\n");
        env.executables["PPSSPPSDL"]=dir.filePath("different-runtime");
        found=prepareEmulators(env);
        QCOMPARE(found.profiles["ppsspp"]["program"].toString(),probe()); // no silent save-namespace switch
        QVERIFY(config.open(QIODevice::ReadOnly));QCOMPARE(config.readAll(),"User settings must stay exactly as written\n");
        QVERIFY(dsFile.open(QIODevice::ReadOnly));QCOMPARE(dsFile.readAll(),"User controller preferences\n");
        QVERIFY(azFile.open(QIODevice::ReadOnly));QCOMPARE(azFile.readAll(),"User 3DS controls and preferences\n");
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
    void flatpakCurrentBranchSurvivesRollbackAndUserOverride() {
#ifdef Q_OS_UNIX
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto user=dir.filePath("user"), system=dir.filePath("system");
        const QString app="org.ppsspp.PPSSPP";
        const auto base=system+"/app/"+app;
        QVERIFY(QDir().mkpath(base+"/aarch64/stable/active"));
        QVERIFY(QDir().mkpath(base+"/aarch64/traineros-relay/active"));
        write(base+"/aarch64/stable/active/metadata","old");
        write(base+"/aarch64/traineros-relay/active/metadata","new");
        QVERIFY(QFile::link(base+"/aarch64/traineros-relay",base+"/current"));
        QCOMPARE(currentFlatpakDeployment(app,{user,system}),base+"/current/active");
        QVERIFY(QFile::remove(base+"/current"));
        QVERIFY(QFile::link(base+"/aarch64/stable",base+"/current"));
        QCOMPARE(QFileInfo(currentFlatpakDeployment(app,{user,system})).canonicalFilePath(),base+"/aarch64/stable/active");
        QVERIFY(QDir().mkpath(user+"/app/"+app+"/current/active"));
        write(user+"/app/"+app+"/current/active/metadata","user");
        QCOMPARE(currentFlatpakDeployment(app,{user,system}),user+"/app/"+app+"/current/active");
        QVERIFY(currentFlatpakDeployment("missing.app",{user,system}).isEmpty());
#endif
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
        launch.launch(record.adventure.id);QVERIFY(launch.busy());launch.cancel();
        QVERIFY(launch.cancelling());QTRY_VERIFY(!launch.busy());QCOMPARE(starts,0);QCOMPARE(errors.size(),0);
        QCOMPARE(store.registration(record.adventure.id)->revision,1);
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
        r.adventure.platformId="gba";r.contentPath="/game.zip";QVERIFY(retroarch.setupIssue(r).isEmpty());
        r.contentPath="/game.rar";QVERIFY(retroarch.setupIssue(r).contains("format"));
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
        QTest::newRow("3DS") << QString("azahar") << QString("n3ds") << QString("3dsx");
        QTest::newRow("DS") << QString("melonds") << QString("nds") << QString("nds");
        QTest::newRow("PSP") << QString("ppsspp") << QString("psp") << QString("cso");
        QTest::newRow("PSP ISO") << QString("ppsspp") << QString("psp") << QString("iso");
        QTest::newRow("PSP PBP") << QString("ppsspp") << QString("psp") << QString("pbp");
        QTest::newRow("PS2") << QString("armsx2") << QString("ps2") << QString("chd");
        QTest::newRow("WiiU") << QString("cemu") << QString("wiiu") << QString("wua");
        QTest::newRow("PS1") << QString("duckstation") << QString("psx") << QString("chd");
        for(const auto& extension:{"iso","pbp","m3u"})
            QTest::newRow(qPrintable(QString("PS1-")+extension))<<QString("duckstation")<<QString("psx")<<QString(extension);
        QTest::newRow("PS3-homebrew") << QString("rpcs3") << QString("ps3") << QString("self");
        QTest::newRow("Switch-homebrew") << QString("eden") << QString("switch") << QString("nro");
        QTest::newRow("Xbox-disc") << QString("xemu") << QString("xbox") << QString("iso");
        QTest::newRow("GameCube") << QString("dolphin") << QString("gc") << QString("iso");
        QTest::newRow("Wii") << QString("dolphin") << QString("wii") << QString("wad");
        QTest::newRow("DS-SRL") << QString("melonds") << QString("nds") << QString("srl");
        QTest::newRow("3DS-cartridge") << QString("azahar") << QString("n3ds") << QString("3ds");
        for(const auto& ext:QStringList{"app","elf","axf","zcci","zcxi","z3dsx"})
            QTest::newRow(qPrintable("3DS-"+ext))<<QString("azahar")<<QString("n3ds")<<ext;
        for(const auto& ext:QStringList{"cso","zso","gz","bin","img","mdf","nrg"})
            QTest::newRow(qPrintable("PS2-"+ext))<<QString("armsx2")<<QString("ps2")<<ext;
        for(const auto& ext:QStringList{"bin","img","mdf","mds","ccd"})
            QTest::newRow(qPrintable("PS1-"+ext))<<QString("duckstation")<<QString("psx")<<ext;
        for(const auto& ext:QStringList{"zip","7z"})QTest::newRow(qPrintable("DS-"+ext))<<QString("melonds")<<QString("nds")<<ext;
        QTest::newRow("GC-TGC")<<QString("dolphin")<<QString("gc")<<QString("tgc");
        QTest::newRow("PSP-CHD")<<QString("ppsspp")<<QString("psp")<<QString("chd");
        for(const auto& platform:{"gc","wii"})for(const auto& extension:{"wia","gcz","ciso","rvz"})
            QTest::newRow(qPrintable(QString(platform)+'-'+extension))<<QString("dolphin")<<QString(platform)<<QString(extension);
    }
    void literalLaunchRoundTripAndMissingFileRecovery() {
        QFETCH(QString, adapterId); QFETCH(QString, platform); QFETCH(QString, extension);
        QTemporaryDir dir;
        LocalStateStore store(dir.filePath("data")); store.open(); QTRY_VERIFY(store.ready());
        AdventureRegistration record; record.adventure.id = "ds-fixture"; record.adventure.worldId = "sinnoh";
        record.adventure.title = "Content-free launch fixture"; record.adventure.platformId = platform; record.adventure.adapterId = adapterId;
        record.contentPath = dir.filePath("Adventure ; $(literal) ' quote." + extension); write(record.contentPath, "Original content-free fixture");
        const bool dsArchive=adapterId=="melonds"&&(extension=="zip"||extension=="7z");
        if(dsArchive)write(record.contentPath,cartridgeArchive(extension,{{"nested/Actual.srl",QByteArray(1024,'D')}}));
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
        auto arguments = adapterId == "melonds" ? QJsonArray{"-f", record.contentPath}
            : adapterId == "azahar" ? QJsonArray{"-f", record.contentPath}
            : adapterId == "ppsspp" ? QJsonArray{"--fullscreen", "--pause-menu-exit", record.contentPath}
            : adapterId == "armsx2" ? QJsonArray{"-batch", "-fullscreen", "--", record.contentPath}
            : adapterId == "cemu" ? QJsonArray{"-f", "-g", record.contentPath}
            : adapterId == "duckstation" ? QJsonArray{"-batch", "-fullscreen", "--", record.contentPath}
            : adapterId == "rpcs3" ? QJsonArray{"--no-gui", "--fullscreen", record.contentPath}
            : adapterId == "eden" ? QJsonArray{"-f", "-g", record.contentPath}
            : adapterId == "xemu" ? QJsonArray{"-full-screen", "-dvd_path", record.contentPath}
            : QJsonArray{"-b", "-C", "Dolphin.Display.Fullscreen=True", "-C", "Dolphin.Interface.ConfirmStop=False", "-e", record.contentPath};
        if(dsArchive){arguments.append("--archive-file");arguments.append("nested/Actual.srl");}
        QCOMPARE(QJsonDocument::fromJson(output.readAll()).array(), arguments); output.close();
        QVERIFY(QFile::remove(receipt)); QVERIFY(QFile::rename(record.contentPath, record.contentPath + ".moved"));
        QVERIFY(router.launch(record.adventure).inProgress); QTRY_COMPARE(restored.size(), 2);
        QCOMPARE(lifecycle.state(), "failed"); QVERIFY(lifecycle.error().contains("missing")); QVERIFY(!QFileInfo::exists(receipt));
        QVERIFY(QFile::exists(record.contentPath + ".moved")); // Recovery never deletes the source.
    }
};
QTEST_GUILESS_MAIN(StandaloneTests)
#include "StandaloneTests.moc"
