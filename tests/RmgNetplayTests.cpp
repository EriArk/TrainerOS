#include "integrations/adventure/standalone/RmgNetplay.h"
#include <QtTest>
#include "core/model/GamePlayers.h"
#include <QWebSocketServer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QScopeGuard>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QTcpSocket>

using namespace trainer;
class RmgNetplayTests final : public QObject {
    Q_OBJECT
    static void write(const QString& name,const QByteArray& bytes) {
        QDir().mkpath(QFileInfo(name).absolutePath());
        QFile f(name);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(bytes),bytes.size());
        f.setPermissions(f.permissions()|QFileDevice::ExeOwner|QFileDevice::ExeGroup|QFileDevice::ExeOther);
    }
    static QJsonObject identity(){return {{"id","runtime.rmg.n64.v1"},{"content",QString(64,'a')},
        {"runtime",QString(64,'b')},{"players",4},{"lateJoin",false}};}
    static QString roomId(const rmg::Lobby::Request& r){return QString::fromLatin1(QCryptographicHash::hash(r.token.toLatin1(),QCryptographicHash::Sha256).toHex().left(24));}
    static QString player(const rmg::Lobby::Request& r,int slot){return roomId(r)+"-"+QString::number(slot);}
    static rmg::Lobby::Request request(int slot,bool host=false) {
        rmg::Lobby::Request r;r.identity=identity();r.token=QString(32,'c');r.host=host;r.slot=slot;
        r.seats={1,2,3,4};r.port=45001;return r;
    }
    static void send(QWebSocket* s,const QJsonObject& p){s->sendTextMessage(QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact)));}
private slots:
    void isolatedCommandAndExactBundle() {
        QStandardPaths::setTestModeEnabled(true);
        const auto root=rmg::bundleRoot();QVERIFY(!QFileInfo::exists(root));
        const auto cleanup=qScopeGuard([root]{QDir(root).removeRecursively();QStandardPaths::setTestModeEnabled(false);});
        const QStringList files={"bin/RMG","bin/rmg-netplay-server","lib/libRMG-Core.so",
            "lib/RMG/Core/libmupen64plus.so","lib/RMG/Plugin/Input/RMG-Input.so","lib/RMG/Plugin/Audio/RMG-Audio.so",
            "lib/RMG/Plugin/RSP/mupen64plus-rsp-hle.so","lib/RMG/Plugin/GFX/mupen64plus-video-GLideN64.so",
            "lib/RMG/Plugin/RSP/mupen64plus-rsp-parallel.so","lib/RMG/Plugin/GFX/mupen64plus-video-parallel.so",
            "share/RMG/mupen64plus.ini","share/RMG/gamecontrollerdb.txt","share/RMG/InputProfileDB.json"};
        QJsonObject hashes;
        for(const auto& f:files){write(root+"/"+f,f.toUtf8());hashes[f]=QString::fromLatin1(QCryptographicHash::hash(f.toUtf8(),QCryptographicHash::Sha256).toHex());}
        QJsonObject m{{"protocol",1},{"source",rmg::sourceRevision},{"serverSource",rmg::serverRevision},
            {"onlineServer","wss://rmg.example.invalid:45000"},{"renderer","vulkan"},{"files",hashes}};
        const auto saveManifest=[&]{write(root+"/manifest.json",QJsonDocument(m).toJson());};saveManifest();
        QTemporaryDir temp;const auto rom=temp.filePath("cartridge.z64");
        QByteArray bytes(4096,'\0');bytes.replace(0,4,QByteArray::fromHex("80371240"));write(rom,bytes);
        AdventureRegistration record;record.adventure.id="game";record.adventure.platformId="n64";
        record.adventure.adapterId="retroarch";record.adventure.title="Cartridge";record.contentPath=rom;
        std::atomic_bool cancel=false;
        const auto id=rmg::netplayIdentity(record,cancel,4);QVERIFY(!id.isEmpty());
        QVERIFY(multiplayerConnectionAllowed(id,"online"));
        m.remove("onlineServer");saveManifest();
        const auto nearby=rmg::netplayIdentity(record,cancel,4);QVERIFY(!nearby.isEmpty());
        QVERIFY(multiplayerConnectionAllowed(nearby,"nearby"));
        QVERIFY(!multiplayerConnectionAllowed(nearby,"online"));
        QVERIFY(sameMultiplayerGame(id,nearby));
        m["onlineServer"]="";saveManifest();QCOMPARE(rmg::netplayIdentity(record,cancel,4),nearby);
        m["onlineServer"]="ws://public.example.invalid";saveManifest();QVERIFY(rmg::bundle(cancel).isEmpty());
        m["onlineServer"]=12;saveManifest();QVERIFY(rmg::bundle(cancel).isEmpty());
        m["onlineServer"]="wss://rmg.example.invalid:45000";saveManifest();
        const auto solo=temp.filePath("ordinary.sra");write(solo,"KEEP ORDINARY PROGRESS");
        auto renamed=id;renamed["label"]="Friend's custom title";
        auto command=rmg::command(record,renamed,"127.0.0.1",45001,3,{"/device/input-bridge","--"});
        QVERIFY(command.prepare(command,cancel).isEmpty());QCOMPARE(command.runtimeControls["kind"].toString(),"rmg");
        QVERIFY(command.runtimeControls["temporaryProgress"].toBool());
        QVERIFY(command.arguments.contains("/device/input-bridge"));QVERIFY(!command.arguments.contains("-L"));
        const auto sessionArg=*std::find_if(command.arguments.cbegin(),command.arguments.cend(),[](const auto& s){return s.startsWith("TRAINEROS_RMG_SESSION=");});
        const auto sessionPath=sessionArg.mid(QString("TRAINEROS_RMG_SESSION=").size());
        QFile session(sessionPath);QVERIFY(session.open(QIODevice::ReadOnly));
        const auto data=QJsonDocument::fromJson(session.readAll()).object();session.close();
        QCOMPARE(data["controller"].toInt(),3);QCOMPARE(data["renderer"].toString(),"vulkan");
        command.settled={};QVERIFY(!QFileInfo::exists(sessionPath));
        QFile ordinary(solo);QVERIFY(ordinary.open(QIODevice::ReadOnly));QCOMPARE(ordinary.readAll(),QByteArray("KEEP ORDINARY PROGRESS"));
        auto bad=rmg::command(record,id,"invalid address",45001,3);QVERIFY(!bad.prepare(bad,cancel).isEmpty());
        bytes[20]=1;write(rom,bytes);bad=rmg::command(record,id,"127.0.0.1",45001,3);QVERIFY(!bad.prepare(bad,cancel).isEmpty());
        write(root+"/lib/RMG/Plugin/Input/RMG-Input.so","changed");QVERIFY(rmg::netplayIdentity(record,cancel,4).isEmpty());
        cancel=true;QVERIFY(rmg::bundle(cancel).isEmpty());cancel=false;
        m["onlineServer"]="ws://public.example.invalid";saveManifest();QVERIFY(rmg::bundle(cancel).isEmpty());
    }
    void upstreamAssignedControllerAndCancellation() {
        QWebSocketServer server("fixture",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost,0));
        rmg::Lobby client;auto r=request(3);r.server=QUrl("ws://127.0.0.1:"+QString::number(server.serverPort()));
        QSignalSpy ready(&client,&rmg::Lobby::launchReady),failed(&client,&rmg::Lobby::failed);
        QWebSocket* socket=nullptr;std::unique_ptr<QSignalSpy> messages;
        connect(&server,&QWebSocketServer::newConnection,&server,[&]{socket=server.nextPendingConnection();messages=std::make_unique<QSignalSpy>(socket,&QWebSocket::textMessageReceived);});
        client.start(r);QTRY_VERIFY(messages&&!messages->isEmpty());
        const auto join=QJsonDocument::fromJson(messages->takeFirst().at(0).toString().toUtf8()).object();
        QCOMPARE(join["type"].toString(),"request_join_room");QCOMPARE(join["netplay_version"].toInt(),17);
        QVERIFY(!join["player_name"].toString().contains(r.token));
        QVERIFY(!join["room"].toObject()["room_name"].toString().contains(r.token));
        QCOMPARE(join["room"].toObject()["password"].toString(),r.token);
        send(socket,{{"type","reply_join_room"},{"accept",0},{"player_name",player(r,3)},
            {"room",QJsonObject{{"MD5",r.identity["content"]},{"port",r.port},{"protected",true},
                {"room_name","TrainerOS-"+roomId(r)},{"features",QJsonObject{}}}}});
        send(socket,{{"type","reply_players"},{"player_names",QJsonArray{player(r,1),player(r,3),player(r,4),player(r,2)}}});
        send(socket,{{"type","reply_begin_game"},{"accept",0}});
        QTRY_COMPARE(ready.size(),1);QCOMPARE(ready.first().at(2).toInt(),2);QCOMPARE(failed.size(),0);
        client.stop();send(socket,{{"type","reply_begin_game"},{"accept",0}});QTest::qWait(20);QCOMPARE(ready.size(),1);QCOMPARE(failed.size(),0);
        socket->deleteLater();
    }
    void rejectsIncompleteAndForeignRoster_data() {
        QTest::addColumn<bool>("foreign");QTest::newRow("incomplete")<<false;QTest::newRow("foreign")<<true;
    }
    void rejectsIncompleteAndForeignRoster() {
        QFETCH(bool,foreign);
        QWebSocketServer server("fixture",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost,0));
        rmg::Lobby client;auto r=request(2);r.server=QUrl("ws://127.0.0.1:"+QString::number(server.serverPort()));
        QSignalSpy ready(&client,&rmg::Lobby::launchReady),failed(&client,&rmg::Lobby::failed),connected(&server,&QWebSocketServer::newConnection);
        client.start(r);QTRY_COMPARE(connected.size(),1);auto* socket=server.nextPendingConnection();
        send(socket,{{"type","reply_join_room"},{"accept",0},{"player_name",player(r,2)},
            {"room",QJsonObject{{"MD5",r.identity["content"]},{"port",r.port},{"protected",true},
                {"room_name","TrainerOS-"+roomId(r)},{"features",QJsonObject{}}}}});
        send(socket,{{"type","reply_players"},{"player_names",QJsonArray{player(r,1),player(r,2),foreign?"uninvited":"",""}}});
        if(!foreign)send(socket,{{"type","reply_begin_game"},{"accept",0}});
        QTRY_COMPARE(failed.size(),1);QCOMPARE(ready.size(),0);socket->deleteLater();
    }
    void realUpstreamLobbyFourClients_data() {
        QTest::addColumn<bool>("tls");QTest::newRow("nearby")<<false;QTest::newRow("tls-lobby")<<true;
    }
    void realUpstreamLobbyFourClients() {
        QFETCH(bool,tls);
        const auto binary=qEnvironmentVariable("TRAINEROS_RMG_TEST_SERVER");
        if(binary.isEmpty())QSKIP("Set TRAINEROS_RMG_TEST_SERVER to the pinned Go build for the bounded upstream integration check.");
        QVERIFY(QFileInfo(binary).isExecutable());
        rmg::Lobby host,second,third,fourth;auto h=request(1,true);h.server=QUrl("ws://127.0.0.1:45000");h.localServer=binary;
        QProcess tlsServer;
        const auto ssl=QSslConfiguration::defaultConfiguration();
        const auto restore=qScopeGuard([&]{host.stop();tlsServer.kill();tlsServer.waitForFinished(1000);QSslConfiguration::setDefaultConfiguration(ssl);});
        if(tls) {
            const auto cert=qEnvironmentVariable("TRAINEROS_RMG_TEST_CERT"),key=qEnvironmentVariable("TRAINEROS_RMG_TEST_KEY");
            if(cert.isEmpty()||key.isEmpty())QSKIP("TLS integration needs a temporary trusted loopback certificate/key.");
            auto trusted=ssl;auto certificates=trusted.caCertificates();certificates+=QSslCertificate::fromPath(cert);
            trusted.setCaCertificates(certificates);QSslConfiguration::setDefaultConfiguration(trusted);
            tlsServer.setStandardOutputFile(QProcess::nullDevice());tlsServer.setStandardErrorFile(QProcess::nullDevice());
            tlsServer.start(binary,{"--name","TrainerOS-test","--disable-broadcast","--verify-ip","--max-games","1","--tls-cert",cert,"--tls-key",key});
            QVERIFY(tlsServer.waitForStarted(3000));
            QTRY_VERIFY_WITH_TIMEOUT(([&]{QTcpSocket socket;socket.connectToHost(QHostAddress::LocalHost,45000);return socket.waitForConnected(50);}()),5000);
            h.localServer.clear();h.server=QUrl("wss://127.0.0.1:45000");
        }
        QSignalSpy room(&host,&rmg::Lobby::roomReady),hReady(&host,&rmg::Lobby::launchReady),hFailed(&host,&rmg::Lobby::failed);
        QSignalSpy sReady(&second,&rmg::Lobby::launchReady),tReady(&third,&rmg::Lobby::launchReady),fReady(&fourth,&rmg::Lobby::launchReady);
        host.start(h);QTRY_COMPARE_WITH_TIMEOUT(room.size(),1,10000);QCOMPARE(hFailed.size(),0);
        // Reverse arrival explicitly exercises the native controller assignment.
        auto a=request(4);a.server=h.server;a.port=room.first().first().toInt();fourth.start(a);
        QTest::qWait(150);QCOMPARE(hReady.size(),0);
        a.slot=3;third.start(a);QTest::qWait(150);QCOMPARE(hReady.size(),0);
        a.slot=2;second.start(a);
        QTRY_COMPARE_WITH_TIMEOUT(hReady.size(),1,10000);QTRY_COMPARE(sReady.size(),1);QTRY_COMPARE(tReady.size(),1);QTRY_COMPARE(fReady.size(),1);
        QSet<int> seats={hReady.first()[2].toInt(),sReady.first()[2].toInt(),tReady.first()[2].toInt(),fReady.first()[2].toInt()};
        QCOMPARE(seats,QSet<int>({1,2,3,4}));QCOMPARE(hFailed.size(),0);
        second.stop();third.stop();fourth.stop();host.stop();
    }
};
QTEST_GUILESS_MAIN(RmgNetplayTests)
#include "RmgNetplayTests.moc"
