#include "integrations/adventure/standalone/FlycastNetplay.h"
#include "core/model/GamePlayers.h"
#include "features/adventure/RuntimeIdentity.h"
#include <QProcess>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <QtEndian>
using namespace trainer;
class FlycastNetplayTests final:public QObject {
    Q_OBJECT
    static void write(const QString& p,const QByteArray& b){QDir().mkpath(QFileInfo(p).absolutePath());QFile f(p);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(b),b.size());}
    static QByteArray read(const QString& p){QFile f(p);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
private slots:
    void isolationIdentityAndLaunch() {
        QStandardPaths::setTestModeEnabled(true);const auto root=flycast::bundleRoot();QVERIFY(!QFileInfo::exists(root));
        const auto cleanup=qScopeGuard([&]{QDir(root).removeRecursively();QStandardPaths::setTestModeEnabled(false);});
        const QMap<QString,QByteArray> files={{"bin/flycast","fixture executable"},
            {"firmware/dc_boot.bin",QByteArray(2*1024*1024,'B')},{"firmware/dc_flash.bin",QByteArray(128*1024,'F')}};
        QJsonObject hashes;for(auto it=files.begin();it!=files.end();++it){write(root+"/"+it.key(),it.value());hashes[it.key()]=QString::fromLatin1(QCryptographicHash::hash(it.value(),QCryptographicHash::Sha256).toHex());}
        QJsonObject manifest{{"protocol",1},{"source",flycast::sourceRevision},{"files",hashes},
            {"onlineRelay","udp://relay.example.invalid:8001"},{"renderer","vulkan"}};
        const auto save=[&]{write(root+"/manifest.json",QJsonDocument(manifest).toJson());};save();
        QTemporaryDir temp;const auto rom=temp.filePath("disc.chd");
        QByteArray content(124,'\0');content.replace(0,16,QByteArray::fromHex("4d436f6d707248440000007c00000005"));write(rom,content);
        AdventureRegistration r;r.adventure.id="disc";r.adventure.platformId="dreamcast";
        r.adventure.adapterId="retroarch";r.adventure.title="Disc";r.contentPath=rom;
        std::atomic_bool cancel=false;
#ifdef Q_OS_UNIX
        // A Unix bundle must contain an executable. Exercise that gate before
        // granting the fixture the permissions a real installed binary has.
        QVERIFY(flycast::netplayIdentity(r,cancel).isEmpty());
        QFile executable(root+"/bin/flycast");
        QVERIFY(executable.setPermissions(executable.permissions()|QFileDevice::ExeOwner));
#endif
        const auto identity=flycast::netplayIdentity(r,cancel);
        QVERIFY(!identity.isEmpty());QCOMPARE(identity["players"].toInt(),2);QVERIFY(!identity["lateJoin"].toBool());
        const auto chdPath=r.contentPath;
        write(temp.filePath("first.bin"),QByteArray(2048,'a'));write(temp.filePath("audio.raw"),QByteArray(2352,'b'));
        write(temp.filePath("last.bin"),QByteArray(2048,'c'));
        const QByteArray gdi="3\n1 0 4 2048 first.bin 0\n2 450 0 2352 audio.raw 0\n3 45000 4 2048 last.bin 0\n";
        r.contentPath=temp.filePath("disc.gdi");write(r.contentPath,gdi);
        QStringList observed;
        const auto gdiIdentity=flycast::netplayIdentity(r,cancel,[&](const QString& p){observed.append(p);});QVERIFY(!gdiIdentity.isEmpty());
        QVERIFY(observed.contains(temp.filePath("last.bin")));
        auto gdiRequest=flycast::Request{gdiIdentity,true,false,"127.0.0.1",QString(32,'a'),{}};
        auto gdiCommand=flycast::command(r,gdiRequest);QVERIFY(gdiCommand.prepare(gdiCommand,cancel).isEmpty());
        QCOMPARE(gdiCommand.arguments.last(),r.contentPath);gdiCommand.settled={};
        write(temp.filePath("last.bin"),QByteArray(2048,'d'));
        QVERIFY(!sameMultiplayerGame(gdiIdentity,flycast::netplayIdentity(r,cancel)));
        auto changed=flycast::command(r,gdiRequest);QVERIFY(!changed.prepare(changed,cancel).isEmpty());
        QVERIFY(QFile::remove(temp.filePath("last.bin")));observed.clear();
        QVERIFY(flycast::netplayIdentity(r,cancel,[&](const QString& p){observed.append(p);}).isEmpty());
        QVERIFY(observed.contains(temp.filePath("last.bin")));write(temp.filePath("last.bin"),QByteArray(2048,'c'));
        write(r.contentPath,QByteArray(gdi).replace("first.bin","../escape.bin"));QVERIFY(flycast::netplayIdentity(r,cancel).isEmpty());write(r.contentPath,gdi);
        r.contentPath=temp.filePath("disc.cue");write(r.contentPath,"FILE \"first.bin\" BINARY\n TRACK 01 MODE1/2048\n INDEX 01 00:00:00\n");
        QVERIFY(!flycast::netplayIdentity(r,cancel).isEmpty());
        r.contentPath=temp.filePath("set.m3u");write(r.contentPath,"disc.gdi\ndisc.chd\n");
        gdiRequest.expected=flycast::netplayIdentity(r,cancel);QVERIFY(!gdiRequest.expected.isEmpty());
        auto playlist=flycast::command(r,gdiRequest);QVERIFY(playlist.prepare(playlist,cancel).isEmpty());
        QCOMPARE(playlist.arguments.last(),temp.filePath("disc.gdi"));playlist.settled={};
        write(r.contentPath,"set.m3u\n");QVERIFY(flycast::netplayIdentity(r,cancel).isEmpty());
        QByteArray cdi(4096,'\0');qToLittleEndian<quint16>(2,cdi.data()+3000);
        qToLittleEndian<quint32>(0x80000005,cdi.data()+4088);qToLittleEndian<quint32>(3000,cdi.data()+4092);
        r.contentPath=temp.filePath("disc.cdi");write(r.contentPath,cdi);QVERIFY(!flycast::netplayIdentity(r,cancel).isEmpty());
        qToLittleEndian<quint32>(9999,cdi.data()+4092);write(r.contentPath,cdi);QVERIFY(flycast::netplayIdentity(r,cancel).isEmpty());
        r.contentPath=chdPath;
        flycast::Request request{identity,true,true,{},QString(32,'a'),{}};
        request.expected["label"]="Different local title";
        auto cmd=flycast::command(r,request,{"/input-bridge","--"});QVERIFY(cmd.prepare(cmd,cancel).isEmpty());
        const auto session=cmd.workingDirectory;QVERIFY(cmd.runtimeControls["temporaryProgress"].toBool());
        QVERIFY(cmd.arguments.contains("FLYCAST_ROOT="+session));QVERIFY(cmd.arguments.contains("/input-bridge"));
        QVERIFY(cmd.arguments.indexOf("--")<cmd.arguments.indexOf(root+"/bin/flycast"));
        auto config=read(session+"/emu.cfg");QVERIFY(config.contains("Relay = yes"));QVERIFY(config.contains("pvr.rend = 4"));QVERIFY(config.contains("GGPODelay = 0"));
        QCOMPARE(read(session+"/data/dc_flash.bin"),files["firmware/dc_flash.bin"]);
        write(session+"/data/dc_flash.bin","session change");write(session+"/data/vmu_save_A1.bin","session save");
        QCOMPARE(read(root+"/firmware/dc_flash.bin"),files["firmware/dc_flash.bin"]);
        request.host=false;request.key="Ab12cD";
        auto guest=flycast::command(r,request);QVERIFY(guest.prepare(guest,cancel).isEmpty());
        QVERIFY(guest.workingDirectory!=session);QVERIFY(!QFileInfo::exists(guest.workingDirectory+"/data/vmu_save_A1.bin"));
        config=read(guest.workingDirectory+"/emu.cfg");QVERIFY(config.contains("ActAsServer = no"));QVERIFY(config.contains("RelayKey = Ab12cD"));
        guest.settled={};cmd.settled={};QVERIFY(!QFileInfo::exists(session));
        request.online=false;request.address="192.0.2.3";
        auto nearby=flycast::command(r,request);QVERIFY(nearby.prepare(nearby,cancel).isEmpty());
        QVERIFY(read(nearby.workingDirectory+"/emu.cfg").contains("Relay = no"));nearby.settled={};
        // Nearby needs no operator relay; malformed supplied values still fail closed.
        manifest.remove("onlineRelay");save();
        auto localIdentity=flycast::netplayIdentity(r,cancel);QVERIFY(!localIdentity.isEmpty());
        QVERIFY(localIdentity["temporaryProgress"].toBool());
        QVERIFY(multiplayerConnectionAllowed(localIdentity,"nearby"));QVERIFY(!multiplayerConnectionAllowed(localIdentity,"online"));
        request.expected=localIdentity;auto local=flycast::command(r,request);QVERIFY(local.prepare(local,cancel).isEmpty());local.settled={};
        request.online=true;auto unavailable=flycast::command(r,request);QVERIFY(!unavailable.prepare(unavailable,cancel).isEmpty());
        request.online=false;
        manifest["onlineRelay"]="";save();QCOMPARE(flycast::netplayIdentity(r,cancel),localIdentity);
        for(const auto invalid:{QJsonValue(42),QJsonValue("https://invalid.example")}) {
            manifest["onlineRelay"]=invalid;save();QVERIFY(flycast::bundle(cancel).isEmpty());
        }
        manifest["onlineRelay"]="udp://relay.example.invalid:8001";save();request.expected=identity;
        request.address="invalid\naddress";auto bad=flycast::command(r,request);QVERIFY(!bad.prepare(bad,cancel).isEmpty());
        request.online=true;request.key="abc\n12";bad=flycast::command(r,request);QVERIFY(!bad.prepare(bad,cancel).isEmpty());
        request.key="Ab12cD";content[40]=1;write(rom,content);bad=flycast::command(r,request);QVERIFY(!bad.prepare(bad,cancel).isEmpty());
        cancel=true;QVERIFY(flycast::netplayIdentity(r,cancel).isEmpty());cancel=false;
        write(root+"/bin/flycast","changed");QVERIFY(flycast::netplayIdentity(r,cancel).isEmpty());
        manifest["onlineRelay"]="https://invalid.example";save();QVERIFY(flycast::bundle(cancel).isEmpty());
    }
    void boundedMarkers() {
        QCOMPARE(flycast::event("[TrainerOSFlycast] relay Ab12cD\r")["key"].toString(),"Ab12cD");
        QCOMPARE(flycast::event("[TrainerOSFlycast] running")["event"].toString(),"running");
        QVERIFY(flycast::event("RECEIVED RELAY KEY Ab12cD").isEmpty());
        QVERIFY(flycast::event("[TrainerOSFlycast] relay bad key").isEmpty());
        QVERIFY(flycast::event(QByteArray(200,'x')).isEmpty());
    }
    void cabinetCapabilityAndPrivateLaunch() {
        if(!QFileInfo("/usr/bin/bwrap").isExecutable())QSKIP("Run this namespace integration check on the Linux host.");
        QProcess namespaceProbe;namespaceProbe.setStandardErrorFile(QProcess::nullDevice());
        namespaceProbe.start("/usr/bin/bwrap",{"--unshare-net","--bind","/","/","--","/bin/true"});
        if(!namespaceProbe.waitForFinished(5000)||namespaceProbe.exitCode()!=0)
            QSKIP("Builder disallows nested namespaces; run this check on the Linux host.");
        QStandardPaths::setTestModeEnabled(true);
        const auto root=flycast::bundleRoot(),cabinet=flycast::cabinetRoot();
        QVERIFY(!QFileInfo::exists(root));QVERIFY(!QFileInfo::exists(cabinet));
        const auto cleanup=qScopeGuard([&]{QDir(root).removeRecursively();QDir(cabinet).removeRecursively();QStandardPaths::setTestModeEnabled(false);});
        const auto recipe=qEnvironmentVariable("TRAINEROS_TEST_FLYCAST_RECIPE",
            QFileInfo(QString::fromUtf8(__FILE__)).dir().absoluteFilePath("../packaging/emulators/flycast"));
        QCOMPARE(QProcess::execute("/usr/bin/python3",{recipe+"/check-arcade.py","--fixture",root}),0);
        QVERIFY(QFile::copy(recipe+"/arcade.py",root+"/arcade.py"));
        const auto sha=[](const QByteArray& bytes){return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());};
        const QMap<QString,QByteArray> files={{"bin/flycast","fixture"},
            {"firmware/dc_boot.bin",QByteArray(2*1024*1024,'B')},{"firmware/dc_flash.bin",QByteArray(128*1024,'F')}};
        QJsonObject hashes;
        for(auto it=files.begin();it!=files.end();++it){write(root+'/'+it.key(),it.value());hashes[it.key()]=sha(it.value());}
        QVERIFY(QFile::setPermissions(root+"/bin/flycast",QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QJsonObject manifest{{"protocol",1},{"source",flycast::sourceRevision},{"files",hashes},{"renderer","vulkan"},
            {"arcade",QJsonObject{{"protocol","flycast-arcade-sets-v1"},{"arcade.py",sha(read(root+"/arcade.py"))},
                {"arcade-catalog.json",sha(read(root+"/arcade-catalog.json"))}}}};
        write(root+"/manifest.json",QJsonDocument(manifest).toJson());
        const auto capability=[&](const QByteArray& link) {
            QJsonObject files;
            for(const auto& name:{"bin/flycast","cabinet_endpoint","stream_transport"}) {
                const QByteArray bytes=QByteArray(name)=="bin/flycast"
                    ? "#!/bin/sh\n[ \"$TRAINEROS_FLYCAST_INSPECT\" = 1 ] || exit 2\n"
                      "[ \"$FLYCAST_ROOT\" = \"$XDG_DATA_HOME\" ] || exit 3\n"
                      "grep -q 'aica.Volume = 0' \"$FLYCAST_ROOT/emu.cfg\" || exit 4\n"
                      "printf '%s\\n' '[TrainerOSArcade] {\"protocol\":1,\"link\":\""+link+"\"}'\n"
                    : QByteArray("#!/bin/sh\nexit 1\n");
                write(cabinet+'/'+name,bytes);files[name]=sha(bytes);
                QVERIFY(QFile::setPermissions(cabinet+'/'+name,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
            }
            write(cabinet+"/manifest.json",QJsonDocument(QJsonObject{{"protocol",1},{"source",flycast::sourceRevision},
                {"settings","flycast-cabinet-private-v1"},{"files",files}}).toJson());
        };
        std::atomic_bool cancel=false;
        AdventureRegistration r;r.adventure.id="cabinet";r.adventure.platformId="naomi";
        r.adventure.adapterId="retroarch";r.contentPath=root+"/naomi.zip";
        capability("none");QVERIFY(flycast::cabinetIdentity(r,cancel).isEmpty());
        QVERIFY(!flycast::netplayIdentity(r,cancel).isEmpty());
        for(const auto& link:{QByteArray("naomi"),QByteArray("serial")}) {
            capability(link);QStringList observed;
            const auto identity=flycast::cabinetIdentity(r,cancel,[&](const QString& path){observed.append(path);});
            QCOMPARE(identity["id"].toString(),"runtime.flycast.cabinet.v1");
            QCOMPARE(identity["link"].toString(),QString::fromUtf8(link));
            QVERIFY(observed.contains(cabinet+"/manifest.json"));
            QVERIFY(identity["temporaryProgress"].toBool());QVERIFY(!identity["lateJoin"].toBool());
            QTemporaryDir connection;
            const auto config=connection.filePath("connection.cfg");
            for(const bool host:{true,false}) {
                write(config,"mode=cabinet\nhost="+QByteArray(host?"true":"false")+"\nsocket="+connection.path().toUtf8()+"/cabinet.sock\n");
                QVERIFY(QFile::setPermissions(config,QFile::ReadOwner|QFile::WriteOwner));
                auto command=flycast::cabinetCommand(r,identity,config,host,{"/input-bridge","--"});
                const auto error=command.prepare(command,cancel);QVERIFY2(error.isEmpty(),qPrintable(error));
                QCOMPARE(command.program,"/usr/bin/bwrap");QVERIFY(command.arguments.contains("--unshare-net"));
                QVERIFY(command.arguments.contains(cabinet+"/cabinet_endpoint"));QVERIFY(command.arguments.contains("/input-bridge"));
                const auto stage=command.workingDirectory;const auto ini=read(stage+"/emu.cfg");
                QVERIFY(ini.contains("TrainerCabinet = yes"));QVERIFY(ini.contains("GGPO = no"));
                QVERIFY(ini.contains(host?"LocalPort = 37391":"LocalPort = 37392"));
                QVERIFY(ini.contains(host?"server = 127.0.0.1:37392":"server = 127.0.0.1:37391"));
                write(stage+"/data/private.eeprom","session progress");command.settled={};
                QVERIFY(!QFileInfo::exists(stage));QVERIFY(!QFileInfo::exists(root+"/data/private.eeprom"));
            }
            write(config,"mode=ps2\nhost=true\n");
            auto invalid=flycast::cabinetCommand(r,identity,config,true);QVERIFY(!invalid.prepare(invalid,cancel).isEmpty());
            write(cabinet+"/cabinet_endpoint","changed");QVERIFY(flycast::cabinetIdentity(r,cancel).isEmpty());
        }
    }
    void arcadeNativeSetsUseSharedIdentityLaunchAndCleanup() {
#ifndef Q_OS_UNIX
        QSKIP("Native arcade helper uses the supported Linux libarchive installation.");
#else
        QStandardPaths::setTestModeEnabled(true);const auto root=flycast::bundleRoot();QVERIFY(!QFileInfo::exists(root));
        const auto cleanup=qScopeGuard([&]{QDir(root).removeRecursively();QStandardPaths::setTestModeEnabled(false);});
        const auto recipe=QFileInfo(QString::fromUtf8(__FILE__)).dir().absoluteFilePath("../packaging/emulators/flycast");
        QCOMPARE(QProcess::execute("/usr/bin/python3",{recipe+"/check-arcade.py","--fixture",root}),0);
        QVERIFY(QFile::copy(recipe+"/arcade.py",root+"/arcade.py"));
        const QMap<QString,QByteArray> files={{"bin/flycast","fixture executable"},
            {"firmware/dc_boot.bin",QByteArray(2*1024*1024,'B')},{"firmware/dc_flash.bin",QByteArray(128*1024,'F')}};
        const auto sha=[](const QByteArray& b){return QString::fromLatin1(QCryptographicHash::hash(b,QCryptographicHash::Sha256).toHex());};
        QJsonObject hashes;for(auto it=files.begin();it!=files.end();++it){write(root+'/'+it.key(),it.value());hashes[it.key()]=sha(it.value());}
        QFile runtime(root+"/bin/flycast");QVERIFY(runtime.setPermissions(runtime.permissions()|QFileDevice::ExeOwner));
        QJsonObject arcade{{"protocol","flycast-arcade-sets-v1"},{"arcade.py",sha(read(root+"/arcade.py"))},
            {"arcade-catalog.json",sha(read(root+"/arcade-catalog.json"))}};
        QJsonObject manifest{{"protocol",1},{"source",flycast::sourceRevision},{"files",hashes},{"arcade",arcade},
            {"onlineRelay","udp://relay.example.invalid:8001"},{"renderer","opengl"}};
        write(root+"/manifest.json",QJsonDocument(manifest).toJson());
        std::atomic_bool cancel=false;
        for(const auto platform:{"naomi","naomi2","atomiswave"}) {
            AdventureRegistration r;r.adventure.id=platform;r.adventure.platformId=platform;r.adventure.adapterId="retroarch";
            r.adventure.title="Arcade fixture";r.contentPath=root+'/'+platform+".zip";
            QStringList observed;
            const auto identity=flycast::netplayIdentity(r,cancel,[&](const QString& path){observed.append(path);});
            QCOMPARE(identity["id"].toString(),"runtime.flycast-dojo.arcade.v1");
            QCOMPARE(runtimeIdentity(r,{},{},{},cancel,2),identity);
            QVERIFY(localLobbyRuntime(identity));QCOMPARE(multiplayerPresentation(identity)["label"].toString(),"Netplay");
            QVERIFY(observed.contains(root+"/firmware/testbios.zip"));
            auto command=flycast::command(r,{identity,true,true,{},QString(32,'b'),{}},{"/input-bridge","--"});
            const auto error=command.prepare(command,cancel);QVERIFY2(error.isEmpty(),qPrintable(error));
            const auto stage=command.workingDirectory;
            QVERIFY(command.arguments.last().startsWith(stage+"/roms/"));
            QVERIFY(QFileInfo(command.arguments.last()).isSymLink());
            QVERIFY(!QFileInfo(stage+"/data/dc_flash.bin").exists());
            QVERIFY(read(stage+"/emu.cfg").contains("IgnoreNetSave = yes"));
            QVERIFY(read(stage+"/emu.cfg").contains("Relay = yes"));
            write(stage+"/data/test.eeprom","private arcade progress");
            command.settled={};QVERIFY(!QFileInfo::exists(stage));
            QVERIFY(!QFileInfo(root+"/data/test.eeprom").exists());
            auto stale=identity;stale["content"]=QString(64,'0');
            auto changed=flycast::command(r,{stale,true,true,{},QString(32,'b'),{}});
            QVERIFY(!changed.prepare(changed,cancel).isEmpty());
        }
        manifest.remove("arcade");write(root+"/manifest.json",QJsonDocument(manifest).toJson());
        AdventureRegistration r;r.adventure.platformId="naomi";r.adventure.adapterId="retroarch";r.contentPath=root+"/naomi.zip";
        QVERIFY(flycast::netplayIdentity(r,cancel).isEmpty());
#endif
    }
};
QTEST_GUILESS_MAIN(FlycastNetplayTests)
#include "FlycastNetplayTests.moc"
