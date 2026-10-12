#include "integrations/adventure/AdapterRouter.h"
#include "integrations/adventure/standalone/StandaloneAdapter.h"
#include "integrations/adventure/standalone/PpssppNetplay.h"
#include "integrations/adventure/standalone/DolphinNetplay.h"
#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "core/storage/LocalStateStore.h"
#include "core/navigation/AdventureLaunchController.h"
#include "core/navigation/LaunchPreparation.h"
#include "core/repository/BatoceraLibrary.h"
#include "platform/emulation/EmulatorDiscovery.h"
#include "core/model/GamePlayers.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>
#include <QStandardPaths>
#include <QScopeGuard>
#include <QCryptographicHash>
#include <QtEndian>

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
    void pspClockPolicyStaysScopedToTheReviewedLuminesBuild() {
        ppsspp::NetplayRequest r{{{"id","runtime.ppsspp.adhoc.v1"}},true,true,"socom.cc","Alice\n[Network]"};
        auto bytes=ppsspp::networkSettings(r);
        QVERIFY(bytes.contains("[General]\nForceLagSync2 = False\n"));
        QVERIFY(bytes.contains("EnableAdhocServer = False\n"));QVERIFY(bytes.contains("AdhocServerRelayMode = 1\n"));
        QVERIFY(!bytes.contains("MacAddress"));QVERIFY(!bytes.contains("Alice\n"));
        r.expected["id"]="runtime.ppsspp.lumines-us.v1";
        QVERIFY(ppsspp::networkSettings(r).contains("[General]\nForceLagSync2 = True\n"));
        r.online=false;r.address="192.168.1.2";
        bytes=ppsspp::networkSettings(r);QVERIFY(bytes.contains("EnableAdhocServer = True\n"));
        QVERIFY(bytes.contains("AdhocServerRelayMode = 2\n"));
        r.host=false;QVERIFY(ppsspp::networkSettings(r).contains("EnableAdhocServer = False\n"));
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
            "[General]\nForceLagSync2 = False\n";
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
