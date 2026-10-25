#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QProcess>
#include <vector>
#include <algorithm>
#include <QScopeGuard>
#include "platform/sdk/PlatformHost.h"
#include "platform/sdk/SdkNetwork.h"
#include "features/device/ControlsDashboard.h"
#include "features/trainer/AvatarAssets.h"
using namespace trainer;
class SdkNetworkTests final:public QObject {
    Q_OBJECT
private slots:
    void sixteenPeersCreditIsolationAndCleanup() {
        const auto python=qEnvironmentVariable("TRAINEROS_TEST_RECORD_PYTHON");
        if(python.isEmpty())QSKIP("Isolated helper interpreter required");
        QTemporaryDir dir;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
        struct Node {LobbyCoordinator lobby;LobbyRuntime runtime{lobby};std::unique_ptr<SdkNetwork> network;QJsonObject session;int received=0,acks=0,closed=0;};
        std::vector<std::unique_ptr<Node>> nodes;QStringList ids;QElapsedTimer elapsed;elapsed.start();
        const auto helper=QString::fromUtf8(SDK_SOURCE)+"/packaging/emulators/melonds/secure-channel.py";
        const QString app="org.example.group";
        for(int i=0;i<16;++i){ids<<QString::number(100+i);auto n=std::make_unique<Node>();
            n->lobby.configure(ids[i],"Player "+QString::number(i),true);n->runtime.configure(ids[i],true);
            n->network=std::make_unique<SdkNetwork>(n->lobby,n->runtime,python,helper,"127.0.0.1");
            n->network->setInstalledActivities({QVariantMap{{"app",app},{"title","Group test"},{"protocol","group.1"},{"local","crates:group"},{"seats",16}}});
            QJsonObject result;QVERIFY(n->network->request(app,"activity.register",{{"title","Group test"},{"protocol","group.1"}},result).isEmpty());QCOMPARE(result["seats"],16);
            nodes.push_back(std::move(n));}
        for(int i=0;i<16;++i){
            connect(&nodes[i]->lobby,&LobbyCoordinator::outgoing,this,[&,i](QString target,QJsonObject p){int j=ids.indexOf(target);if(j>=0)nodes[j]->lobby.receive(ids[i],"Player "+QString::number(i),p);});
            connect(nodes[i]->network.get(),&SdkNetwork::event,this,[&,i](QString,QJsonObject p){
                if(p["event"]=="network.ready")nodes[i]->session=p["session"].toObject();
                if(p["event"]=="network.closed"){++nodes[i]->closed;return;}
                if(p["event"]=="network.ack"){++nodes[i]->acks;return;}
                if(p["event"]=="network.data"){
                    ++nodes[i]->received;QJsonObject result;
                    // Intentionally hold one client's credit. Other edges must
                    // still make progress, and its window stays bounded.
                    if(i!=1)QVERIFY(nodes[i]->network->request(app,"network.consume",{{"channel",p["channel"]},{"sequence",p["sequence"]}},result).isEmpty());
                }
            });
        }
        const auto cleanup=qScopeGuard([&]{for(auto& n:nodes)n->lobby.disconnect();});
        QVERIFY(nodes[0]->lobby.create("private"));for(int i=1;i<16;++i){QVERIFY(nodes[0]->lobby.invite(ids[i]));nodes[i]->lobby.answer(true);}
        QCOMPARE(nodes[0]->lobby.state()["members"].toArray().size(),16);
        auto g=nodes[0]->network->activities().first().toMap();
        QVERIFY(nodes[0]->lobby.selectGame({g["id"].toString(),g["title"].toString(),g["platform"].toString(),g["mode"].toString(),16,QJsonObject::fromVariantMap(g["runtime"].toMap())}));
        for(auto& n:nodes)QVERIFY(n->runtime.prepare());for(auto& n:nodes)QVERIFY(n->lobby.ready(true));QVERIFY(nodes[0]->lobby.start());
        QTRY_VERIFY_WITH_TIMEOUT(std::all_of(nodes.begin(),nodes.end(),[](const auto& n){return !n->session.isEmpty();}),60000);
        qInfo()<<"16 peers connected (real TLS helpers), ms"<<elapsed.elapsed();
        QCOMPARE(nodes[0]->session["channels"].toArray().size(),15);
        for(int i=1;i<16;++i){QCOMPARE(nodes[i]->session["channels"].toArray().size(),1);QCOMPARE(nodes[i]->session["members"].toArray().size(),16);}
        QHash<QString,QString> grants;for(const auto& v:nodes[0]->session["channels"].toArray()){auto c=v.toObject();grants[c["peer"].toString()]=c["channel"].toString();}
        QJsonObject result;
        for(int i=0;i<64;++i)QVERIFY(nodes[0]->network->request(app,"network.send",{{"channel",grants[ids[1]]},{"data","eA=="}},result).isEmpty());
        QCOMPARE(nodes[0]->network->request(app,"network.send",{{"channel",grants[ids[1]]},{"data","eA=="}},result),QString("backpressure"));
        for(int i=2;i<16;++i)QVERIFY(nodes[0]->network->request(app,"network.send",{{"channel",grants[ids[i]]},{"data","eA=="}},result).isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(nodes[0]->acks,14,10000);QTRY_COMPARE(nodes[1]->received,64);
        QCOMPARE(nodes[1]->network->request(app,"network.send",{{"channel",grants[ids[2]]},{"data","eA=="}},result),QString("channel_unavailable"));
        QCOMPARE(nodes[0]->network->request(app,"network.consume",{{"channel",grants[ids[1]]},{"sequence","0"}},result),QString("invalid_sequence"));
        // A participant exit revokes the entire immutable match, not the room.
        nodes[7]->network->disconnected(app);
        QTRY_VERIFY_WITH_TIMEOUT(std::all_of(nodes.begin(),nodes.end(),[](const auto& n){return n->closed>0;}),15000);
        QCOMPARE(nodes[0]->lobby.state()["members"].toArray().size(),16);
        QCOMPARE(nodes[0]->network->request(app,"network.send",{{"channel",grants[ids[2]]},{"data","eA=="}},result),QString("channel_unavailable"));
        for(auto& n:nodes)n->lobby.leave();
    }
    void pairedProcessesAndFreshSession() { runPair(false); }
    void installedGamesLaunchOnlyAfterLobbyStart() { runPair(false,true); }
    void pairedGolfAvatars() {
        if(qEnvironmentVariable("TRAINER_AVATAR_GODOT").isEmpty()||qEnvironmentVariable("TRAINER_GOLF_PROJECT").isEmpty()||!AvatarAssets::available())
            QSKIP("Godot, Pocket Golf checkout and avatar pack required");
        runPair(true);
    }
private:
    void runPair(bool avatars,bool cold=false) {
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
        const QVariantList installed{QVariantMap{{"app",avatars?"art.abysstail.pocket-golf":"org.example.records"},{"title",avatars?"Pocket Golf":"Record test"},{"protocol",avatars?"pocket-golf.5":"test-1"},{"local","crates:counter"}}};
        an.setInstalledActivities(installed);bn.setInstalledActivities(installed);
        for(int pass=0;pass<2;++pass) {
            QProcess ac,bc;QByteArray ao,bo;
            const auto probe=QString::fromUtf8(SDK_SOURCE)+"/tests/SdkNetworkProbe.py";
            int launches=0;
            if(cold) {
                an.launchInstalled=[&](QString local){if(local!="crates:counter")return false;++launches;ac.start(QString::fromUtf8(SDK_PYTHON),{probe,ae,"host"});return true;};
                bn.launchInstalled=[&](QString local){if(local!="crates:counter")return false;++launches;bc.start(QString::fromUtf8(SDK_PYTHON),{probe,be,"guest"});return true;};
            }
            if(avatars) {
                publish(ah,"alice",appearance);
                auto guest=appearance;guest["hair"]="bob";guest["skinColor"]="#614132";publish(bh,"bob",guest);
                auto launch=[&](QProcess& process,const QString& endpoint,const QString& role){
                    auto env=QProcessEnvironment::systemEnvironment();env.insert("TRAINEROS_SDK_ENDPOINT",endpoint);env.insert("TRAINER_GOLF_ROLE",role);
                    process.setProcessEnvironment(env);
                    process.start(qEnvironmentVariable("TRAINER_AVATAR_GODOT"),{"--headless","--path",qEnvironmentVariable("TRAINER_GOLF_PROJECT"),"--script","tests/avatar_pair.gd"});
                };
                launch(ac,ae,"host");launch(bc,be,"guest");
            } else if(!cold) {
                ac.start(QString::fromUtf8(SDK_PYTHON),{probe,ae,"host"});bc.start(QString::fromUtf8(SDK_PYTHON),{probe,be,"guest"});
            }
            if(!cold){QVERIFY(ac.waitForStarted());QVERIFY(bc.waitForStarted());}
            const auto registered=[&](SdkNetwork& network){QJsonObject result;network.request(avatars?"art.abysstail.pocket-golf":"org.example.records","activity.get",{},result);return result["registered"].toBool();};
            if(!cold){QTRY_VERIFY_WITH_TIMEOUT(registered(an),10000);QTRY_VERIFY_WITH_TIMEOUT(registered(bn),10000);}
            QTRY_COMPARE_WITH_TIMEOUT(an.activities().size(),1,10000);QTRY_COMPARE_WITH_TIMEOUT(bn.activities().size(),1,10000);
            const auto g=an.activities().first().toMap();
            QVERIFY(a.selectGame({g["id"].toString(),g["title"].toString(),g["platform"].toString(),g["mode"].toString(),2,QJsonObject::fromVariantMap(g["runtime"].toMap())}));
            QVERIFY(ar.prepare());QVERIFY(br.prepare());QVERIFY(a.ready(true));QVERIFY(b.ready(true));
            if(cold){QCOMPARE(launches,0);QCOMPARE(ac.state(),QProcess::NotRunning);QCOMPARE(bc.state(),QProcess::NotRunning);}
            QVERIFY(a.start());
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
            if(cold)QCOMPARE(launches,2);
            QCOMPARE(an.activities().size(),1);QCOMPARE(bn.activities().size(),1);
            QTRY_COMPARE(a.state()["phase"].toString(),"gathering");
            QCOMPARE(a.state()["members"].toArray().size(),2);
        }
        a.leave();b.leave();
    }
};
QTEST_GUILESS_MAIN(SdkNetworkTests)
#include "SdkNetworkTests.moc"
