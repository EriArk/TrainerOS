#include "integrations/adventure/retroarch/RetroArchAdapter.h"
#include "integrations/adventure/retroarch/RetroArchNetplay.h"
#include "integrations/adventure/retroarch/RetroArchHandheldLink.h"
#include "integrations/adventure/retroarch/HandheldSavePair.h"
#include "../packaging/emulators/handheld/TrainerMbc3Rtc.h"
#include <QtEndian>
#include "integrations/adventure/retroarch/RetroArchSave.h"
#include "integrations/adventure/retroarch/RetroArchDisc.h"
#include "integrations/adventure/retroarch/RetroArchConfiguration.h"
#include "core/repository/RomPlatforms.h"
#include "core/model/GamePlayers.h"
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
    void linkedPairUsesExistingRollbackWithOwnScreenAndNoPersonalSaveAccess() {
        QTemporaryDir dir;std::atomic_bool cancel=false;
        const auto write=[](const QString& path,const QByteArray& bytes) {
            QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(bytes),bytes.size());
        };
        const auto read=[](const QString& path) {QFile f(path);if(!f.open(QIODevice::ReadOnly))return QByteArray();return f.readAll();};
        const auto rom=[](quint8 type) {
            QByteArray b(32768,0);b.replace(0x134,9,"LINK DEMO");b[0x147]=char(type);
            quint8 checksum=0;for(int n=0x134;n<=0x14c;++n)checksum=quint8(checksum-quint8(b[n])-1);
            b[0x14d]=char(checksum);return b;
        };
        AdventureRegistration r;r.adventure.platformId="gb";r.adventure.title="Independent linked game";
        r.contentPath=dir.filePath("game.gb");r.integrationConfig["core"]="gambatte";
        RetroArchInstallation i;i.configFile=dir.filePath("retroarch.cfg");i.runtimeFile=dir.filePath("runtime");
        i.cores["DoubleCherryGB"]=dir.filePath("link-core");
        write(i.runtimeFile,"runtime");write(i.cores["DoubleCherryGB"],"link-core");
        const auto personal=dir.filePath("game.srm"),rtc=dir.filePath("game.rtc");
        write(personal,"personal save must stay untouched");write(rtc,"personal clock");
        write(i.configFile,("savefile_directory = \""+dir.path()+"\"\n").toUtf8());
        write(r.contentPath,rom(0));const auto identity=retroarch::netplayIdentity(r,i,cancel);
        QVERIFY(!identity.isEmpty());QCOMPARE(identity["transport"].toString(),QString("rollback"));
        QCOMPARE(identity["players"].toInt(),2);
        QVERIFY(!permitsMultiplayer({{"players","1"}},identity));
        QVERIFY(permitsMultiplayer({{"players","1-2"}},identity));
        for(int slot:{1,2}) {
            retroarch::NetplayRequest n;n.expected=identity;n.host=slot==1;n.slot=slot;
            n.password="private";n.nickname="player";n.address="127.0.0.1";
            ProcessCommand cmd;cmd.arguments={"--libretro","ordinary-core",r.contentPath};
            QVERIFY(retroarch::prepareNetplay(cmd,r,i,n,cancel).isEmpty());
            QCOMPARE(cmd.arguments[cmd.arguments.indexOf("--libretro")+1],i.cores["DoubleCherryGB"]);
            QCOMPARE(cmd.arguments[cmd.arguments.indexOf("--sram-mode")+1],QString("noload-nosave"));
            QVERIFY(!cmd.finalize);QVERIFY(cmd.runtimeControls["temporaryProgress"].toBool());
            const auto config=retroarch::readSettings(cmd.arguments[cmd.arguments.indexOf("--appendconfig")+1]);
            const auto path=config.value("savefile_directory");
            QVERIFY(!QFileInfo(path+"/game.srm").exists());
            QCOMPARE(config.value("input_max_users"),QString("2"));
            QCOMPARE(config.value("netplay_request_device_p"+QString::number(slot)),QString("true"));
            QCOMPARE(config.value("netplay_request_device_p"+QString::number(3-slot)),QString("false"));
            const auto options=retroarch::readSettings(config.value("core_options_path"));
            QCOMPARE(options.value("dcgb_emulated_gameboys"),QString("2"));
            QCOMPARE(options.value("dcgb_gblink_enable"),QString("enabled"));
            QCOMPARE(options.value("dcgb_single_screen_mp"),QString("player %1 only").arg(slot));
            QCOMPARE(options.value("dcgb_audio_output"),QString("Game Boy #%1").arg(slot));
            write(path+"/game.srm","session copy");cmd.settled({true,0,false,false});
            QVERIFY(!QFileInfo(path).exists());
            QCOMPARE(read(personal),QByteArray("personal save must stay untouched"));QCOMPARE(read(rtc),QByteArray("personal clock"));
        }
        auto changed=rom(0);changed[0x200]=1;write(r.contentPath,changed);
        QVERIFY(!sameMultiplayerGame(identity,retroarch::netplayIdentity(r,i,cancel)));
        // All battery/RTC/special peripheral cartridges remain outside this
        // route; neither a filename nor multiplayer metadata bypasses ownership.
        for(int type=0;type<256;++type) {
            write(r.contentPath,rom(quint8(type)));
            const bool supported=QList<int>{0x00,0x01,0x02,0x05,0x08,0x11,0x12,0x19,0x1a,0x1c,0x1d}.contains(type);
            QCOMPARE(!retroarch::handheldLinkProfile(r).isEmpty(),supported);
        }
        auto corrupt=rom(0);corrupt[0x14d]^=1;write(r.contentPath,corrupt);
        QVERIFY(retroarch::handheldLinkProfile(r).isEmpty());
        write(r.contentPath,rom(0).left(512));QVERIFY(retroarch::handheldLinkProfile(r).isEmpty());
    }
    void mbc3ClockUsesCartridgeDaysAndSurvivesPauseAndEpochWrap() {
        TrainerMbc3Rtc clock;const quint64 now=1800000000;
        clock.base=now-(300*86400+13*3600+27*60+19);
        QCOMPARE(clock.get(8,now),19);QCOMPARE(clock.get(9,now),27);
        QCOMPARE(clock.get(10,now),13);QCOMPARE(clock.get(11,now),44);QCOMPARE(clock.get(12,now),1);
        clock.set(11,45,now);QCOMPARE(clock.get(11,now),45);
        clock.set(12,0x41,now);QCOMPARE(clock.get(8,now+500),19);
        clock.set(12,1,now+500);QCOMPARE(clock.get(8,now+502),21);
        clock.base=now-511*86400-61;clock.flags=0;
        QCOMPARE(clock.get(9,now),1);QCOMPARE(clock.get(8,now),1);QCOMPARE(clock.get(12,now),0x80);
        QCOMPARE(clock.base,now-61);
        clock.base=now+20;QCOMPARE(clock.get(8,now),0);
    }
    void handheldClockPairRecoversInterruptedReturnAndRejectsConflicts() {
        using namespace trainer::retroarch::handheld;
        QTemporaryDir dir;const auto save=dir.filePath("Gold.srm"),rtc=rtcPath(save);
        QByteArray oldSave(32768,'a'),newSave(32768,'b'),oldRtc(8,0),newRtc(8,0);
        qToLittleEndian<quint64>(1700000000,oldRtc.data());qToLittleEndian<quint64>(1700000060,newRtc.data());
        QVERIFY(writeFile(save,oldSave));QVERIFY(writeFile(rtc,oldRtc));
        QVERIFY(returnPair(save,oldSave,newSave,oldRtc,newRtc).isEmpty());
        QCOMPARE(readFile(save),newSave);QCOMPARE(readFile(rtc),newRtc);
        QCOMPARE(readFile(rtc+".before-link"),oldRtc);
        // Interruption after the RTC replacement: the next ordinary launch
        // finishes the intended SRAM write. No emulator may see a mixed pair.
        QJsonObject j{{"version",1},{"saveExisted",true},{"rtcExisted",true}};
        for(const auto& v:QList<QPair<QString,QByteArray>>{{"oldSave",oldSave},{"newSave",newSave},{"oldRtc",oldRtc},{"newRtc",newRtc}})
            j[v.first]=QString::fromLatin1(v.second.toBase64());
        const auto intent=save+".link-return.json";
        QVERIFY(writeFile(save,oldSave));QVERIFY(writeFile(intent,QJsonDocument(j).toJson()));
        QVERIFY(recoverPair(save).isEmpty());QCOMPARE(readFile(save),newSave);QVERIFY(!QFileInfo::exists(intent));
        QVERIFY(writeFile(save,QByteArray(32768,'c')));QVERIFY(writeFile(intent,QJsonDocument(j).toJson()));
        QVERIFY(!recoverPair(save).isEmpty());QCOMPARE(readFile(save),QByteArray(32768,'c'));QVERIFY(QFileInfo::exists(intent));
    }
    void handheldGenTwoSeedsAndReturnsOwnClockIncludingGuest() {
        using namespace trainer::retroarch::handheld;
        QTemporaryDir dir;QDir().mkpath(dir.filePath("session"));std::atomic_bool cancel=false;
        AdventureRegistration r;r.adventure.platformId="gbc";r.contentPath=dir.filePath("Gold.gbc");r.integrationConfig["core"]="gambatte";
        QByteArray rom(512,0);rom.replace(0x134,14,"POKEMON_GLDAAUE");rom[0x147]=0x10;QVERIFY(writeFile(r.contentPath,rom));
        const auto save=dir.filePath("Gold.srm"),rtc=rtcPath(save);QByteArray clock(8,0);qToLittleEndian<quint64>(1700000000,clock.data());
        QVERIFY(writeFile(save,QByteArray(32768,'a')));QVERIFY(writeFile(rtc,clock));
        RetroArchInstallation i;i.configFile=dir.filePath("retroarch.cfg");i.cores["DoubleCherryGB"]=dir.filePath("core");
        QVERIFY(writeFile(i.configFile,("savefile_directory = \""+dir.path()+"\"\nauto_overrides_enable = \"false\"\n").toUtf8()));
        ProcessCommand cmd;QVERIFY(!retroarch::prepareHandheldSave(cmd,r,i,dir.filePath("session"),cancel).isEmpty());
        QVERIFY(writeFile(i.cores["DoubleCherryGB"],"traineros-mbc3-rtc-v1"));
        QVERIFY(retroarch::prepareHandheldSave(cmd,r,i,dir.filePath("session"),cancel).isEmpty());
        QCOMPARE(readFile(dir.filePath("session/.netplay/Gold.rtc")),clock);
        qToLittleEndian<quint64>(1700000010,clock.data());
        QVERIFY(writeFile(dir.filePath("session/.netplay/Gold.rtc"),clock));
        QVERIFY(writeFile(dir.filePath("session/.netplay/Gold.srm"),QByteArray(32768,'b')));
        QVERIFY(cmd.finalize({true,0,false,false}).isEmpty());QCOMPARE(readFile(rtc),clock);QCOMPARE(readFile(save),QByteArray(32768,'b'));
        ProcessCommand conflict;QVERIFY(retroarch::prepareHandheldSave(conflict,r,i,dir.filePath("session"),cancel).isEmpty());
        QVERIFY(writeFile(dir.filePath("session/Gold.srm"),QByteArray(32768,'c')));QVERIFY(writeFile(rtc,QByteArray(8,'x')));
        QVERIFY(!conflict.finalize({true,0,false,false}).isEmpty());QCOMPARE(readFile(save),QByteArray(32768,'b'));QCOMPARE(readFile(rtc),QByteArray(8,'x'));
    }
    void handheldProfilesKeepOwnCartridgeAndPlayerOne() {
        QTemporaryDir dir;std::atomic_bool cancel=false;
        AdventureRegistration r;r.adventure.platformId="gba";r.integrationConfig["core"]="mgba";
        r.contentPath=dir.filePath("Emerald.gba");
        QByteArray rom(512,'\0');rom.replace(0xac,4,"BPEE");
        QFile f(r.contentPath);QVERIFY(f.open(QIODevice::WriteOnly));f.write(rom);f.close();
        const auto p=retroarch::handheldLinkProfile(r);
        QCOMPARE(p["transport"].toString(),QString("netpacket"));
        QCOMPARE(retroarch::handheldLinkCore(p),QString("gpsp"));
        QVERIFY(retroarch::handheldLinkOptions(p).contains("mul_poke"));
        QVERIFY(permitsMultiplayer({{"players","1"}},p));
        QVERIFY(!permitsMultiplayer({{"players","1"}},{}));
        auto left=p;left["content"]=QString(64,'a');left["core"]="same";
        auto right=left;right["content"]=QString(64,'b');
        QVERIFY(!sameMultiplayerGame(left,right)); // Same header is not cross-hack compatibility.
        right=left;QVERIFY(sameMultiplayerGame(left,right));
        left["content"]="a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af";
        right["content"]="3d0c79f1627022e18765766f6cb5ea067f6b5bf7dca115552189ad65a5c3a8ac";
        QVERIFY(sameMultiplayerGame(left,right));
        right["core"]="updated";QVERIFY(!sameMultiplayerGame(left,right));
        const auto controls=retroarch::netplayControllers(p,false,2);
        QVERIFY(controls.contains("input_max_users = \"1\""));
        QVERIFY(!controls.contains("netplay_request_device_p2"));
        QCOMPARE(retroarch::netplayControllerArguments(p),QStringList({"--device","1:1"}));
        rom.replace(0xac,4,"NOPE");QVERIFY(f.open(QIODevice::WriteOnly));f.write(rom);f.close();
        QVERIFY(retroarch::handheldLinkProfile(r).isEmpty());
        QVERIFY(retroarch::netplayProfile("gba","mgba",QString(64,'a')).isEmpty());
    }
    void handheldSaveReturnsOnlyOwnSramAndPreservesRtcAndConflicts() {
        QTemporaryDir dir;QDir().mkpath(dir.filePath("session"));
        const auto write=[](QString p,QByteArray b){QFile f(p);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(b),b.size());};
        const auto read=[](QString p){QFile f(p);if(!f.open(QIODevice::ReadOnly))return QByteArray();return f.readAll();};
        const auto target=dir.filePath("Red.srm"),rtc=dir.filePath("Red.rtc");
        write(target,QByteArray(32768,'a'));write(rtc,"ordinary clock");
        write(dir.filePath("retroarch.cfg"),("savefile_directory = \""+dir.path()+"\"\nauto_overrides_enable = \"false\"\n").toUtf8());
        RetroArchInstallation i;i.configFile=dir.filePath("retroarch.cfg");
        AdventureRegistration r;r.adventure.platformId="gb";r.integrationConfig["core"]="gambatte";r.contentPath=dir.filePath("Red.gb");
        ProcessCommand cmd;std::atomic_bool cancel=false;
        QVERIFY(retroarch::prepareHandheldSave(cmd,r,i,dir.filePath("session"),cancel).isEmpty());
        QCOMPARE(read(dir.filePath("session/Red.srm")),read(target));
        write(dir.filePath("session/Red.srm"),QByteArray(32768,'b'));
        write(dir.filePath("session/Red.rtc"),"different clock format");
        QVERIFY(cmd.finalize({true,0,false,false}).isEmpty());
        QCOMPARE(read(target),QByteArray(32768,'b'));QCOMPARE(read(rtc),QByteArray("ordinary clock"));
        QCOMPARE(read(target+".before-link"),QByteArray(32768,'a'));
        ProcessCommand conflict;QVERIFY(retroarch::prepareHandheldSave(conflict,r,i,dir.filePath("session"),cancel).isEmpty());
        write(target,QByteArray(32768,'c'));write(dir.filePath("session/Red.srm"),QByteArray(32768,'d'));
        QVERIFY(!conflict.finalize({true,0,false,false}).isEmpty());
        QCOMPARE(read(target),QByteArray(32768,'c'));
        QVERIFY(!QDir(dir.path()).entryList({"Red.srm.link-*.srm"}).isEmpty());
        ProcessCommand missing;
        QVERIFY(retroarch::prepareHandheldSave(missing,r,i,dir.filePath("session"),cancel).isEmpty());
        QVERIFY(QFile::remove(dir.filePath("session/Red.srm")));
        QVERIFY(QFile::remove(dir.filePath("session/.netplay/Red.srm")));
        QVERIFY(!missing.finalize({true,0,false,false}).isEmpty());
        QCOMPARE(read(target),QByteArray(32768,'c'));
        ProcessCommand crash;
        QVERIFY(retroarch::prepareHandheldSave(crash,r,i,dir.filePath("session"),cancel).isEmpty());
        write(dir.filePath("session/Red.srm"),QByteArray(32768,'e'));
        QVERIFY(!crash.finalize({true,9,true,true}).isEmpty());
        QCOMPARE(read(target),QByteArray(32768,'c'));
        ProcessCommand guest;
        QVERIFY(retroarch::prepareHandheldSave(guest,r,i,dir.filePath("session"),cancel).isEmpty());
        QCOMPARE(read(dir.filePath("session/.netplay/Red.srm")),read(target));
        write(dir.filePath("session/.netplay/Red.srm"),QByteArray(32768,'g'));
        QVERIFY(guest.finalize({true,0,false,false}).isEmpty());
        QCOMPARE(read(target),QByteArray(32768,'g'));
        QVERIFY(QFile::remove(target));
        QVERIFY(QFile::remove(dir.filePath("session/.netplay/Red.srm")));
        ProcessCommand fresh;
        QVERIFY(retroarch::prepareHandheldSave(fresh,r,i,dir.filePath("session"),cancel).isEmpty());
        write(dir.filePath("session/Red.srm"),QByteArray(131073,'f'));
        QVERIFY(!fresh.finalize({true,0,false,false}).isEmpty());
        QVERIFY(!QFileInfo::exists(target));
    }
    void arcadeDependenciesIgnoreUnrelatedBiosButDetectParentsAndShadowSets() {
        QTemporaryDir dir;std::atomic_bool cancel=false;
        const auto write=[&](QString name,QByteArray bytes) {QFile f(dir.filePath(name));QVERIFY(f.open(QIODevice::WriteOnly));f.write(bytes);};
        QDir().mkpath(dir.filePath("bios/fbneo"));QDir().mkpath(dir.filePath("roms"));
        write("retroarch.cfg",("system_directory = \""+dir.filePath("bios")+"\"\n").toUtf8());
        write("core","core");write("runtime","runtime");write("roms/batcir.zip","game");
        RetroArchInstallation i;i.configFile=dir.filePath("retroarch.cfg");i.runtimeFile=dir.filePath("runtime");i.cores["fbneo"]=dir.filePath("core");
        AdventureRegistration r;r.adventure.platformId="fbneo";r.integrationConfig["core"]="fbneo";r.contentPath=dir.filePath("roms/batcir.zip");
        const auto before=retroarch::netplayIdentity(r,i,cancel);QVERIFY(!before.isEmpty());
        write("bios/neogeo.zip","unrelated");write("bios/fbneo/qsound.zip","also unrelated");
        QVERIFY(sameMultiplayerGame(before,retroarch::netplayIdentity(r,i,cancel)));
        write("bios/fbneo/batcir.zip","alternate set");
        QVERIFY(!sameMultiplayerGame(before,retroarch::netplayIdentity(r,i,cancel)));
        QFile::remove(dir.filePath("bios/fbneo/batcir.zip"));
        write("roms/batcirj.zip","clone");r.contentPath=dir.filePath("roms/batcirj.zip");
        const auto clone=retroarch::netplayIdentity(r,i,cancel);QVERIFY(!clone.isEmpty());
        write("roms/batcir.zip","changed parent");QVERIFY(!sameMultiplayerGame(clone,retroarch::netplayIdentity(r,i,cancel)));
        write("roms/mslug.zip","neogeo game");r.contentPath=dir.filePath("roms/mslug.zip");
        const auto neo=retroarch::netplayIdentity(r,i,cancel);QVERIFY(!neo.isEmpty());
        QVERIFY(QFile::rename(dir.filePath("bios/neogeo.zip"),dir.filePath("bios/fbneo/neogeo.zip")));
        QVERIFY(sameMultiplayerGame(neo,retroarch::netplayIdentity(r,i,cancel)));
        write("bios/fbneo/neogeo.zip","different firmware");QVERIFY(!sameMultiplayerGame(neo,retroarch::netplayIdentity(r,i,cancel)));
        write("roms/not-a-reviewed-driver.zip","unknown");r.contentPath=dir.filePath("roms/not-a-reviewed-driver.zip");
        QVERIFY(retroarch::netplayIdentity(r,i,cancel).isEmpty());
    }
    void battleCircuitCabinetIsPrivateAndCloneIsNotPromoted() {
        QCOMPARE(retroarch::netplayCapacity("fbneo","fbneo",{{"players","1-4"}},"/games/batcir.zip"),4);
        QCOMPARE(retroarch::netplayCapacity("fbneo","fbneo",{{"players","1-4"}},"/games/batcirj.zip"),2);
        QTemporaryDir dir;std::atomic_bool cancel=false;
        for(const auto& name:{"batcir.zip","runtime","core","retroarch.cfg"})touch(dir.filePath(name));
        RetroArchInstallation i;i.configFile=dir.filePath("retroarch.cfg");i.runtimeFile=dir.filePath("runtime");i.cores["fbneo"]=dir.filePath("core");
        AdventureRegistration r;r.adventure.platformId="fbneo";r.contentPath=dir.filePath("batcir.zip");r.integrationConfig["core"]="fbneo";
        retroarch::NetplayRequest n;n.expected=retroarch::netplayIdentity(r,i,cancel,nullptr,4);n.host=true;n.slot=1;n.password="private";n.nickname="host";
        QVERIFY(!n.expected.isEmpty());QCOMPARE(n.expected["cabinet"].toString(),QString("batcir-eu-four-chutes-v1"));
        ProcessCommand cmd;cmd.arguments={r.contentPath};QVERIFY(retroarch::prepareNetplay(cmd,r,i,n,cancel).isEmpty());
        const auto settings=retroarch::readSettings(cmd.arguments[cmd.arguments.indexOf("--appendconfig")+1]);
        QCOMPARE(settings.value("sort_savefiles_enable"),QString("false"));
        const auto path=settings.value("savefile_directory");
        QFile nv(path+"/fbneo/batcir.nv");QVERIFY(nv.open(QIODevice::ReadOnly));const auto bytes=nv.readAll();nv.close();
        QCOMPARE(bytes.size(),128);QCOMPARE(bytes[0x26],char(6));QCOMPARE(bytes.mid(24,24),bytes.mid(72,24));
        QFile options(settings.value("core_options_path"));QVERIFY(options.open(QIODevice::ReadOnly));QVERIFY(options.readAll().contains("fbneo-diagnostic-input = \"None\""));options.close();
        cmd.settled({});QVERIFY(!QFileInfo(path).exists());
        r.contentPath=dir.filePath("batcirj.zip");touch(r.contentPath);QVERIFY(retroarch::netplayIdentity(r,i,cancel,nullptr,4).isEmpty());
        QFile original(i.configFile);QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),QByteArray("original test fixture"));
    }
    void metadataSelectsOnlyReviewedMultiPadLayouts() {
        for(const auto& pair:QList<QPair<QString,QString>>{{"snes","snes9x"},{"pcengine","mednafen_pce_fast"},
                {"pcenginecd","mednafen_pce_fast"},{"supergrafx","mednafen_supergrafx"}}) {
            for(int count:{3,4}) {
                const auto capacity=retroarch::netplayCapacity(pair.first,pair.second,{{"players","1-"+QString::number(count)}});
                QCOMPARE(capacity,count);
                const auto profile=retroarch::netplayProfile(pair.first,pair.second,QString(64,'a'),capacity);
                QCOMPARE(profile["players"].toInt(),count);
                for(int slot=1;slot<=count;++slot) {
                    const auto config=retroarch::netplayControllers(profile,slot==1,slot);
                    QVERIFY(config.contains("netplay_max_connections = \""+QByteArray::number(count-1)+"\""));
                    for(int pad=1;pad<=16;++pad)QVERIFY(config.contains("netplay_request_device_p"+QByteArray::number(pad)+
                        " = \""+(pad==slot?QByteArray("true"):QByteArray("false"))+"\""));
                }
                QVERIFY(retroarch::netplayControllers(profile,false,count+1).isEmpty());
                QVERIFY(retroarch::netplayControllerArguments(profile).contains(pair.second=="snes9x"?"2:257":"2:1"));
            }
            QCOMPARE(retroarch::netplayCapacity(pair.first,pair.second,{}),2);
            QCOMPARE(retroarch::netplayCapacity(pair.first,pair.second,{{"players","1-8"}}),4);
            QCOMPARE(retroarch::netplayCapacity(pair.first,pair.second,{{"players","4 alternating"}}),2);
        }
        for(const auto& pair:QList<QPair<QString,QString>>{{"megadrive","genesis_plus_gx"},{"nes","fceumm"},
                {"neogeo","fbneo"},{"atari2600","stella"},{"mame","mame"}}) {
            QCOMPARE(retroarch::netplayCapacity(pair.first,pair.second,{{"players","4"}}),2);
            QVERIFY(retroarch::netplayProfile(pair.first,pair.second,QString(64,'a'),4).isEmpty());
        }
        QVERIFY(retroarch::netplayProfile("snes","snes9x",QString(64,'a'),5).isEmpty());
        // Driver ports support four, but a title's cabinet may default to two.
        QCOMPARE(retroarch::netplayCapacity("fbneo","fbneo",{{"players","1-4"}}),2);
        const auto arcade=retroarch::netplayProfile("fbneo","fbneo",QString(64,'a'),4);
        QVERIFY(retroarch::netplayControllerArguments(arcade).contains("4:1"));
        QVERIFY(retroarch::netplayControllers(arcade,false,4).contains("netplay_request_device_p4 = \"true\""));
        // Known exact two-player titles retain their real profile even with bad metadata.
        const auto contra=retroarch::netplayProfile("snes","snes9x","a93ea87fc835c530b5135c5294433d15eef6dbf656144b387e89ac19cf864996",4);
        QCOMPARE(contra["players"].toInt(2),2);
    }
    void multiPadLaunchKeepsCoreOptionsPrivateAndRevalidatesLayout() {
        QTemporaryDir dir;std::atomic_bool cancel=false;
        for(const auto& file:{"game.pce","runtime","core","retroarch.cfg"})touch(dir.filePath(file));
        RetroArchInstallation i;i.configFile=dir.filePath("retroarch.cfg");i.runtimeFile=dir.filePath("runtime");
        i.cores["mednafen_supergrafx"]=dir.filePath("core");
        AdventureRegistration r;r.adventure.platformId="supergrafx";r.contentPath=dir.filePath("game.pce");r.integrationConfig["core"]="mednafen_supergrafx";
        retroarch::NetplayRequest request;request.password="private";request.nickname="p3";request.address="127.0.0.1";request.slot=3;
        request.expected=retroarch::netplayIdentity(r,i,cancel,nullptr,3);
        QVERIFY(!request.expected.isEmpty());
        ProcessCommand cmd;cmd.arguments={r.contentPath};
        QVERIFY(retroarch::prepareNetplay(cmd,r,i,request,cancel).isEmpty());
        QVERIFY(cmd.arguments.contains("3:1"));QVERIFY(cmd.arguments.contains("noload-nosave"));
        const auto config=cmd.arguments[cmd.arguments.indexOf("--appendconfig")+1];
        const auto settings=retroarch::readSettings(config);
        QFile options(settings.value("core_options_path"));QVERIFY(options.open(QIODevice::ReadOnly));
        QCOMPARE(options.readAll(),QByteArray("sgx_multitap = \"enabled\"\n"));options.close();
        QVERIFY(cmd.runtimeControls["temporaryProgress"].toBool());cmd.settled({});QVERIFY(!QFileInfo(config).exists());
        request.expected["settings"]="snes9x-snes-multitap-no-sram-v1";
        ProcessCommand rejected;rejected.arguments={r.contentPath};
        QVERIFY(!retroarch::prepareNetplay(rejected,r,i,request,cancel).isEmpty());QCOMPARE(rejected.arguments,QStringList{r.contentPath});
        QFile original(i.configFile);QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),QByteArray("original test fixture"));
    }
    void netplaySessionsKeepOptionsAndCleanupSeparate() {
        QTemporaryDir dir;
        const auto rom=dir.filePath("new-game.nes");touch(rom);
        const auto write=[&](const QString& name,const QByteArray& bytes){
            QFile f(dir.filePath(name));return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();
        };
        const QByteArray original="global_core_options = \"false\"\ngame_specific_options = \"true\"\n";
        QVERIFY(write("retroarch.cfg",original));QVERIFY(write("core", "core fixture"));QVERIFY(write("runtime", "runtime fixture"));
        RetroArchInstallation i;i.configFile=dir.filePath("retroarch.cfg");
        i.cores["fceumm"]=dir.filePath("core");i.runtimeFile=dir.filePath("runtime");
        AdventureRegistration r;r.adventure.platformId="nes";r.contentPath=rom;r.integrationConfig["core"]="fceumm";
        std::atomic_bool cancel{false};
        retroarch::NetplayRequest request;request.host=true;request.slot=1;request.password="private";request.nickname="test";
        request.expected=retroarch::netplayIdentity(r,i,cancel);QVERIFY(!request.expected.isEmpty());
        ProcessCommand a,b;a.arguments={rom};b.arguments={"--appendconfig",i.configFile,rom};
        QVERIFY(retroarch::prepareNetplay(a,r,i,request,cancel).isEmpty());
        QVERIFY(retroarch::prepareNetplay(b,r,i,request,cancel).isEmpty());
        const auto config=[](const ProcessCommand& cmd){return cmd.arguments[cmd.arguments.indexOf("--appendconfig")+1].section('|',-1);};
        const auto ca=config(a),cb=config(b);QVERIFY(ca!=cb);
        for(const auto& path:{ca,cb}) {
            const auto settings=retroarch::readSettings(path);
            QCOMPARE(settings.value("global_core_options"),QString("true"));
            QCOMPARE(settings.value("game_specific_options"),QString("false"));
            const auto options=settings.value("core_options_path");
            QCOMPARE(QFileInfo(options).absolutePath(),QFileInfo(path).absolutePath());
            QVERIFY(QFileInfo(options).exists());QCOMPARE(QFileInfo(options).size(),0);
        }
        a.settled({});QVERIFY(!QFileInfo(ca).exists());QVERIFY(QFileInfo(cb).exists());
        b.settled({});QVERIFY(!QFileInfo(cb).exists());
        QFile f(i.configFile);QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),original);
        auto old=request;old.expected["settings"]="fceumm-four-score-no-sram-v1";
        ProcessCommand rejected;rejected.arguments={rom};
        QVERIFY(!retroarch::prepareNetplay(rejected,r,i,old,cancel).isEmpty());
        QCOMPARE(rejected.arguments,QStringList{rom});
    }
    void fourPlayerControllersKeepPartySeatsIndependentOfArrival() {
        const auto profile=retroarch::netplayProfile("nes","fceumm",
            "063eec9f883b44a0a11aa63238316d4e676034aab72eae0b9a092ed50f2bceed");
        QCOMPARE(profile["players"].toInt(),4);
        QCOMPARE(profile["settings"].toString(),QString("fceumm-four-score-no-sram-v2"));
        QCOMPARE(retroarch::netplayControllerArguments(profile),QStringList({
            "--device","1:513","--device","2:513","--device","3:513","--device","4:513","--nodevice","5"}));
        QTemporaryDir dir;
        for(int slot:{1,4,2,3}) {
            const auto bytes=retroarch::netplayControllers(profile,slot==1,slot);
            QVERIFY(!bytes.isEmpty());
            const auto path=dir.filePath(QString::number(slot)+".cfg");
            {QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(bytes),bytes.size());}
            const auto settings=retroarch::readSettings(path);
            QCOMPARE(settings.value("netplay_max_connections"),QString("3"));
            int requests=0;
            for(int port=1;port<=16;++port) {
                const bool requested=settings.value("netplay_request_device_p"+QString::number(port))=="true";
                QCOMPARE(requested,port==slot);requests+=requested;
            }
            QCOMPARE(requests,1);
        }
        for(int bad:{0,1,5,16,-1})QVERIFY(retroarch::netplayControllers(profile,false,bad).isEmpty());
        QVERIFY(retroarch::netplayControllers(profile,true,2).isEmpty());
        const auto pair=retroarch::netplayProfile("nes","fceumm",
            "b9116433d8f5d3293adfe871b47af68198e1596d40eccc2c3a99b14e2ca2afe0");
        QCOMPARE(pair["settings"].toString(),QString("fceumm-default-no-sram-v3"));
        QCOMPARE(retroarch::netplayControllers(pair,true,0),QByteArray("netplay_max_connections = \"1\"\n"));
        QVERIFY(retroarch::netplayControllers(pair,false,3).isEmpty());
        QVERIFY(retroarch::netplayControllerArguments(pair).isEmpty());
    }
    void multiplayerProfilesRequireExactContentAndPlatform() {
        const auto gunstar=retroarch::netplayProfile("megadrive","genesis_plus_gx",
            "f177810ce614be21c1a9214c0ca4d8f8d357b04a497c02fae185e4b2f97b6b87");
        QCOMPARE(gunstar["id"].toString(),QString("runtime.genesis.gunstar-us.v1"));
        QCOMPARE(gunstar,retroarch::netplayProfile("megadrive","genesis_plus_gx",
            "626061be86f7eb488da0eb0ef011c5a13e8a951cf6bb039340b4774c25a039bd"));
        QVERIFY(retroarch::netplayProfile("snes","genesis_plus_gx",gunstar["content"].toString()).isEmpty());
        QVERIFY(!retroarch::netplayProfile("megadrive","picodrive",gunstar["content"].toString()).isEmpty());
        QVERIFY(!retroarch::netplayProfile("megadrive","genesis_plus_gx",QString(64,'0')).isEmpty());
        const auto streets=retroarch::netplayProfile("megadrive","genesis_plus_gx",
            "4a314edbfee92282850fe95c4c764921916efd9d3c2277fdec2581279b1369b1");
        QVERIFY(!streets.isEmpty());QVERIFY(streets["id"]!=gunstar["id"]);
        QVERIFY(!retroarch::netplayProfile("snes","snes9x",
            "a93ea87fc835c530b5135c5294433d15eef6dbf656144b387e89ac19cf864996").isEmpty());
    }
    void nesMultiplayerRequiresSupportedContentAndCore() {
        const QString digest("b9116433d8f5d3293adfe871b47af68198e1596d40eccc2c3a99b14e2ca2afe0");
        const auto game=retroarch::netplayProfile("nes","fceumm",digest);
        QCOMPARE(game["id"].toString(),QString("runtime.fceumm.pong-homebrew.v1"));
        QCOMPARE(game["content"].toString(),digest);
        QVERIFY(retroarch::netplayProfile("nes","fceumm",{}).isEmpty());
        QVERIFY(!retroarch::netplayProfile("nes","fceumm",QString(64,'0')).isEmpty());
        QVERIFY(!retroarch::netplayProfile("nes","nestopia",digest).isEmpty());
        QVERIFY(retroarch::netplayProfile("snes","fceumm",digest).isEmpty());
    }
    void sharedProfilesCoverCatalogueWithoutPretendingHandheldLink() {
        for(const auto& p:romPlatforms()) {
            if(!QStringList{"atari2600","fbneo","mame","mastersystem","megadrive","neogeo",
                "nes","pcengine","pcenginecd","sega32x","segacd","sg1000","snes","supergrafx"}.contains(p.id))continue;
            QVERIFY2(retroarch::netplaySupported(p.id,p.core),qPrintable(p.id));
            const auto a=retroarch::netplayProfile(p.id,p.core,QString(64,'a'));
            const auto b=retroarch::netplayProfile(p.id,p.core,QString(64,'b'));
            QVERIFY(!a.isEmpty());QVERIFY(!sameMultiplayerGame(a,b));
            const auto controls=retroarch::netplayControllers(a,false,2);
            QVERIFY(controls.contains("netplay_request_device_p2 = \"true\""));
            QVERIFY(controls.contains("netplay_request_device_p1 = \"false\""));
            QVERIFY(retroarch::netplayControllers(a,false,3).isEmpty());
        }
        QVERIFY(retroarch::netplaySupported("gba","mgba")); // Candidate only; header/core still required.
        QVERIFY(!retroarch::netplaySupported("gamegear","genesis_plus_gx"));
        QVERIFY(retroarch::netplaySupported("gb","gambatte")); // Candidate only; header/core still required.
        QVERIFY(!retroarch::netplaySupported("n64","mupen64plus_next"));
        QVERIFY(!retroarch::netplaySupported("atari7800","prosystem")); // Serialized, not declared deterministic.
        QVERIFY(retroarch::netplayProfile("nes","fceumm","not-a-digest").isEmpty());
    }
    void genericIdentityTracksActualDiscFirmwareAndNotLocalName() {
        QTemporaryDir dir;std::atomic_bool cancel=false;
        const auto write=[&](QString name,QByteArray bytes){QFile f(dir.filePath(name));QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(bytes),bytes.size());};
        write("runtime","runtime");write("core","core");write("track.bin","disc content");
        write("game.cue","FILE \"track.bin\" BINARY\n TRACK 01 MODE1/2352\n");
        write("syscard3.pce","BIOS A");
        write("retroarch.cfg","system_directory = \""+dir.path().toUtf8()+"\"\n");
        RetroArchInstallation i;i.runtimeFile=dir.filePath("runtime");i.configFile=dir.filePath("retroarch.cfg");i.cores["mednafen_pce_fast"]=dir.filePath("core");
        AdventureRegistration r;r.adventure.platformId="pcenginecd";r.adventure.title="My game";
        r.contentPath=dir.filePath("game.cue");r.integrationConfig["core"]="mednafen_pce_fast";
        retroarch::NetplayDigestCache cache;
        const auto before=retroarch::netplayIdentity(r,i,cancel,&cache);QVERIFY(!before.isEmpty());
        const auto count=cache.size();QCOMPARE(retroarch::netplayIdentity(r,i,cancel,&cache),before);QCOMPARE(cache.size(),count);
        r.adventure.title="Renamed locally";
        QVERIFY(sameMultiplayerGame(before,retroarch::netplayIdentity(r,i,cancel)));
        write("track.bin","different game bytes");QVERIFY(!sameMultiplayerGame(before,retroarch::netplayIdentity(r,i,cancel)));
        write("track.bin","disc content");write("syscard3.pce","BIOS B");
        QVERIFY(!sameMultiplayerGame(before,retroarch::netplayIdentity(r,i,cancel)));
        write("game.cue","FILE \"../escape.bin\" BINARY\n TRACK 01 MODE1/2352\n");
        QVERIFY(retroarch::netplayIdentity(r,i,cancel).isEmpty());
        r.contentPath=dir.filePath("game.m3u");write("game.m3u","game.cue\n");
        QVERIFY(retroarch::netplayIdentity(r,i,cancel).isEmpty());
    }
    void relayDirectoryRequiresCompleteUnambiguousEndpoint() {
        const QByteArray reply="status=OK\r\ntunnel_addr=europe-west1.relay.retroarch.com\r\ntunnel_port=55435\r\n";
        QCOMPARE(retroarch::netplayRelayEndpoint(reply),QString("europe-west1.relay.retroarch.com|55435"));
        for(const auto& bad:QList<QByteArray>{
            "<html>Temporarily unavailable</html>","status=ERROR\ntunnel_addr=relay.example\ntunnel_port=55435",
            "status=OK\ntunnel_addr=relay.example",reply+"tunnel_port=1\n",
            "status=OK\ntunnel_addr=relay.example\ntunnel_port=65536",
            "status=OK\ntunnel_addr=relay.example\ntunnel_port=0",
            "status=OK\ntunnel_addr=relay.example\"\nnetplay_password=bad\ntunnel_port=55435",
            "status=OK\ntunnel_addr=relay.example|1234\ntunnel_port=55435",QByteArray(4097,'x')})
            QVERIFY2(retroarch::netplayRelayEndpoint(bad).isEmpty(),bad.constData());
    }
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
    void automaticArtworkMatchesEditionAndMasksOpaqueSystemFallback() {
        QTemporaryDir dir;const auto root=dir.filePath("overlays"),cache=dir.filePath("cache");
        QVERIFY(QDir().mkpath(root+"/GameBezels/GBA"));QVERIFY(QDir().mkpath(root+"/GameBezels/GBC"));
        auto write=[&](const QString& path,Qt::GlobalColor colour){QImage image(192,108,QImage::Format_ARGB32);image.fill(colour);return image.save(path);};
        const auto emerald=root+"/GameBezels/GBA/Pokemon - Emerald Version (USA, Europe).png";
        const auto ruby=root+"/GameBezels/GBA/Pokemon - Ruby Version (USA).png";
        const auto fallback=root+"/Nintendo-Game-Boy-Advance.png";
        QVERIFY(write(emerald,Qt::red));QVERIFY(write(ruby,Qt::blue));QVERIFY(write(fallback,Qt::green));
        retroarch::BezelGame game{"gba","renamed--a9dec84dfe.gba",QString::fromUtf8("Pok\xc3\xa9mon Emerald")};
        auto result=retroarch::automaticBezel(game,{root},cache,{1920,1080},0);
        QCOMPARE(result.source,emerald);QCOMPARE(result.match,QString("game"));
        auto imageFor=[&](const auto& value){return QImage(QFileInfo(value.config).dir().filePath(QFileInfo(value.config).completeBaseName()+".png"));};
        const auto image=imageFor(result);QCOMPARE(image.size(),QSize(1920,1080));
        QCOMPARE(image.pixelColor(0,540),QColor(Qt::red));QCOMPARE(image.pixelColor(1919,540),QColor(Qt::red));
        for(int y=0;y<1080;++y)for(int x=150;x<1770;++x)QVERIFY(qAlpha(image.pixel(x,y))==0);
        game.contentPath="Pokemon - Ruby Version (USA).gba";
        QCOMPARE(retroarch::automaticBezel(game,{root},cache,{1920,1080},0).source,ruby); // File wins over catalogue.
        game={"gba","Pokemon - Emerald (Randomizer).gba",{}};
        QCOMPARE(retroarch::automaticBezel(game,{root},cache,{1920,1080},0).source,fallback);
        game={"gba","Pokemon - Emerald Version (Europe) (SGB Enhanced).gba",{}};
        QCOMPARE(retroarch::automaticBezel(game,{root},cache,{1920,1080},0).source,emerald);
        game={"gba","Pokemon - Emerald Version (Europe).gba",{}};
        QCOMPARE(retroarch::automaticBezel(game,{root},cache,{1920,1080},0).source,emerald);
        {QFile file(emerald);QVERIFY(file.open(QIODevice::WriteOnly));file.write("bad image");}
        result=retroarch::automaticBezel(game,{root},cache,{1920,1080},0);
        QCOMPARE(result.source,fallback);QCOMPARE(result.match,QString("system"));
        QCOMPARE(imageFor(result).pixelColor(0,540),QColor(Qt::green));QCOMPARE(imageFor(result).pixelColor(960,540).alpha(),0);
        QVERIFY(retroarch::automaticBezel(game,{root},cache,{1920,1080},4).config.isEmpty());
        game.platform="gbc";QVERIFY(retroarch::automaticBezel(game,{root},cache,{1920,1080},0).config.isEmpty());
    }
    void automaticArtworkHonoursOffDefaultsAndShaderCoexistence() {
        QTemporaryDir dir;const auto base=dir.filePath("base.cfg");touch(base);
        QVERIFY(QDir().mkpath(dir.filePath("overlays")));
        QImage image(192,108,QImage::Format_ARGB32);image.fill(Qt::blue);
        QVERIFY(image.save(dir.filePath("overlays/Nintendo-Game-Boy-Advance.png")));
        const auto id=QUuid::createUuid().toString();const retroarch::BezelGame game{"gba","unknown.gba",{}};
        auto launch=[&](){ProcessCommand cmd;cmd.arguments={game.contentPath};retroarch::prepareAppearance(cmd,id,base,{},QSize(1920,1080),game);return cmd;};
        auto cmd=launch();QCOMPARE(cmd.runtimeControls["automaticBezelMatch"].toString(),QString("system"));
        {QFile cfg(cmd.arguments.value(cmd.arguments.indexOf("--appendconfig")+1));QVERIFY(cfg.open(QIODevice::ReadOnly));const auto bytes=cfg.readAll();
         QVERIFY(bytes.contains("custom_viewport_width = \"1620\""));QVERIFY(bytes.contains("custom_viewport_height = \"1080\""));}
        const auto rows=retroarch::appearanceChoices(id,"bezel",cmd.runtimeControls);QCOMPARE(rows.first().toMap()["id"].toString(),QString("bezel:auto"));
        QVERIFY(retroarch::chooseAppearance(id,"bezel:off",cmd.runtimeControls));QVERIFY(launch().runtimeControls["automaticBezelSource"].toString().isEmpty());
        QVERIFY(retroarch::chooseAppearance(id,"bezel:auto",cmd.runtimeControls));QVERIFY(!launch().runtimeControls["automaticBezelSource"].toString().isEmpty());
        QVERIFY(retroarch::changeAppearance(id,"reset-appearance"));QVERIFY(launch().runtimeControls["automaticBezelSource"].toString().isEmpty());
        QVERIFY(retroarch::chooseAppearance(id,"bezel:auto",cmd.runtimeControls));
        const auto mega=dir.filePath("shaders/Mega_Bezel/Presets/Base_CRT_Presets");QVERIFY(QDir().mkpath(mega));
        touch(mega+"/MBZ__4__STD-NO-REFLECT__GDV-MINI.slangp");
        {QFile file(base);QVERIFY(file.open(QIODevice::WriteOnly));file.write("video_driver = \"glcore\"\n");}
        cmd=launch();QVERIFY(retroarch::chooseAppearance(id,cmd.runtimeControls["shaderChoices"].toList().first().toMap()["id"].toString(),cmd.runtimeControls));
        cmd=launch();QVERIFY(cmd.arguments.contains("--set-shader"));QCOMPARE(cmd.runtimeControls["automaticBezelMatch"].toString(),QString("system"));
        QFile cfg(cmd.arguments.value(cmd.arguments.indexOf("--appendconfig")+1));QVERIFY(cfg.open(QIODevice::ReadOnly));
        const auto bytes=cfg.readAll();QVERIFY(bytes.contains("aspect_ratio_index = \"24\""));QCOMPARE(bytes.count("input_overlay_enable"),1);QVERIFY(bytes.contains("input_overlay_enable = \"true\""));
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
