#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QProcess>
#include "platform/sdk/PlatformHost.h"
#include "platform/sdk/SdkNetwork.h"
#include "features/device/ControlsDashboard.h"
#include "features/trainer/AvatarAssets.h"
using namespace trainer;
class SdkNetworkTests final:public QObject {
    Q_OBJECT
private slots:
    void pairedProcessesAndFreshSession() { runPair(false); }
    void pairedGolfAvatars() {
        if(qEnvironmentVariable("TRAINER_AVATAR_GODOT").isEmpty()||qEnvironmentVariable("TRAINER_GOLF_PROJECT").isEmpty()||!AvatarAssets::available())
            QSKIP("Godot, Pocket Golf checkout and avatar pack required");
        runPair(true);
    }
private:
    void runPair(bool avatars) {
        const auto python=qEnvironmentVariable("TRAINEROS_TEST_RECORD_PYTHON");
        if(python.isEmpty())QSKIP("Set TRAINEROS_TEST_RECORD_PYTHON to the isolated helper venv interpreter");
        QTemporaryDir dir;
        QCoreApplication::setOrganizationName("TrainerSdkNetworkTests");QCoreApplication::setApplicationName("Isolated");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
        LobbyCoordinator a,b;LobbyRuntime ar(a),br(b);
        a.configure("1","Host",true);b.configure("2","Guest",true);ar.configure("1",true);br.configure("2",true);
        // Provider-authenticated signaling is an explicit in-process test fixture.
        // Application packets never use these signals: the helper uses real TLS.
        connect(&a,&LobbyCoordinator::outgoing,&b,[&](QString peer,QJsonObject p){if(peer!="2")return;
            if(p["kind"]=="state"&&p["data"].toObject()["phase"]=="playing")
                QTimer::singleShot(200,&b,[&,p]{b.receive("1","Host",p);});
            else b.receive("1","Host",p);
        });
        connect(&b,&LobbyCoordinator::outgoing,&a,[&](QString peer,QJsonObject p){if(peer=="1")a.receive("2","Guest",p);});
        const auto helper=QString::fromUtf8(SDK_SOURCE)+"/packaging/emulators/melonds/secure-channel.py";
        SdkNetwork an(a,ar,python,helper,"127.0.0.1"),bn(b,br,python,helper,"127.0.0.1");
        ControlsDashboard ab,bb;ab.setOwner("alice");bb.setOwner("bob");
        PlatformHost ah(ab),bh(bb);ah.setNetwork(&an);bh.setNetwork(&bn);
        ah.setContext({{"profile","alice"}});bh.setContext({{"profile","bob"}});
        auto appearance=AvatarAssets::defaults();
        auto publish=[&](PlatformHost& host,const QString& profile,const QJsonObject& value){
            auto descriptor=AvatarAssets::resolve(value,{},dir.filePath("avatars/"+profile));
            descriptor["profile"]=profile;host.setAvatar(descriptor,true);
        };
        const auto ae=dir.filePath("a/endpoint.json"),be=dir.filePath("b/endpoint.json");QVERIFY(ah.start(ae));QVERIFY(bh.start(be));
        QVERIFY(a.create("private"));QVERIFY(a.invite("2"));b.answer(true);
        QCOMPARE(a.state()["members"].toArray().size(),2);
        for(int pass=0;pass<2;++pass) {
            QProcess ac,bc;QByteArray ao,bo;
            const auto probe=QString::fromUtf8(SDK_SOURCE)+"/tests/SdkNetworkProbe.py";
            if(avatars) {
                publish(ah,"alice",appearance);
                auto guest=appearance;guest["hair"]="bob";guest["skinColor"]="#614132";publish(bh,"bob",guest);
                auto launch=[&](QProcess& process,const QString& endpoint,const QString& role){
                    auto env=QProcessEnvironment::systemEnvironment();env.insert("TRAINEROS_SDK_ENDPOINT",endpoint);env.insert("TRAINER_GOLF_ROLE",role);
                    process.setProcessEnvironment(env);
                    process.start(qEnvironmentVariable("TRAINER_AVATAR_GODOT"),{"--headless","--path",qEnvironmentVariable("TRAINER_GOLF_PROJECT"),"--script","tests/avatar_pair.gd"});
                };
                launch(ac,ae,"host");launch(bc,be,"guest");
            } else {
                ac.start(QString::fromUtf8(SDK_PYTHON),{probe,ae,"host"});bc.start(QString::fromUtf8(SDK_PYTHON),{probe,be,"guest"});
            }
            QVERIFY(ac.waitForStarted());QVERIFY(bc.waitForStarted());
            QTRY_COMPARE_WITH_TIMEOUT(an.activities().size(),1,10000);QTRY_COMPARE_WITH_TIMEOUT(bn.activities().size(),1,10000);
            const auto g=an.activities().first().toMap();
            QVERIFY(a.selectGame({g["id"].toString(),g["title"].toString(),g["platform"].toString(),g["mode"].toString(),2,QJsonObject::fromVariantMap(g["runtime"].toMap())}));
            QVERIFY(ar.prepare());QVERIFY(br.prepare());QVERIFY(a.ready(true));QVERIFY(b.ready(true));QVERIFY(a.start());
            QTRY_VERIFY_WITH_TIMEOUT((ao+=ac.readAllStandardOutput()).contains("received"),30000);
            QTRY_VERIFY_WITH_TIMEOUT((bo+=bc.readAllStandardOutput()).contains("consumed"),10000);
            QCOMPARE(a.state()["phase"].toString(),"playing");
            if(avatars) {
                auto changed=appearance;changed["hair"]="ponytail";changed["eyes"]="almond";publish(ah,"alice",changed);
                QTRY_VERIFY_WITH_TIMEOUT((bo+=bc.readAllStandardOutput()).contains("peer-updated"),30000);
                QVERIFY2(!ac.readAllStandardError().contains("ERROR"),ao.constData());
            }
            // Crash removes only the app route; the normal lobby survives.
            ac.kill();ac.waitForFinished(3000);
            QTRY_COMPARE_WITH_TIMEOUT(bc.state(),QProcess::NotRunning,10000);
            QVERIFY2(bc.exitCode()==0,bc.readAllStandardError().constData());
            bo+=bc.readAllStandardOutput();QVERIFY(bo.contains("closed"));
            QTRY_VERIFY(an.activities().isEmpty());QTRY_VERIFY(bn.activities().isEmpty());
            QTRY_COMPARE(a.state()["phase"].toString(),"gathering");
            QCOMPARE(a.state()["members"].toArray().size(),2);
        }
        a.leave();b.leave();
    }
};
QTEST_GUILESS_MAIN(SdkNetworkTests)
#include "SdkNetworkTests.moc"
