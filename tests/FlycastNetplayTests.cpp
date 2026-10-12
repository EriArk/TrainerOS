#include "integrations/adventure/standalone/FlycastNetplay.h"
#include "core/model/GamePlayers.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
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
};
QTEST_GUILESS_MAIN(FlycastNetplayTests)
#include "FlycastNetplayTests.moc"
