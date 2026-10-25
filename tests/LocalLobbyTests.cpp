#include <QtTest>
#include <QTemporaryDir>
#include <QProcess>
#include <QFile>
#include "platform/network/LocalLobbyTransport.h"
#include "features/social/LobbyCoordinator.h"
#include "features/social/LobbyRuntime.h"
#include "features/adventure/GameParty.h"
#include "core/model/GamePlayers.h"

namespace trainer {
class LocalLobbyTests final : public QObject {
    Q_OBJECT
    static quint16 port() {QUdpSocket s;if(!s.bind(QHostAddress::LocalHost,0))return 0;return s.localPort();}
    static QJsonObject announcement(LocalLobbyTransport& host,QString room) {
        return {{"protocol","traineros.local-lobby.1"},{"owner",host.identity()},{"name",host.name()},
            {"lobby",room},{"title","Local test"},{"port",host.listeners_.begin().value()->serverPort()}};
    }
    static void advertise(const QJsonObject& a,quint16 port) {
        QUdpSocket s;s.writeDatagram(QJsonDocument(a).toJson(QJsonDocument::Compact),QHostAddress::LocalHost,port);
    }
    static void bind(LocalLobbyTransport& t,LobbyCoordinator& c) {
        c.configureLocal(t.identity(),t.name(),true);
        connect(&t,&LocalLobbyTransport::received,&c,[&c](QString p,QString n,QJsonObject o){c.receive(p,n,o);});
        connect(&t,&LocalLobbyTransport::disconnected,&c,&LobbyCoordinator::deliveryFailed);
        connect(&c,&LobbyCoordinator::outgoing,&t,[&t](QString p,QJsonObject o){t.send(p,o);});
    }
private slots:
    void persistentIdentityAndCorruptFilePreservation() {
        QTemporaryDir d;LocalLobbyTransport t(nullptr,port(),true);
        QVERIFY(t.configure(d.path(),"Host"));const auto id=t.identity();QVERIFY(LocalLobbyTransport::validIdentity(id));
        QVERIFY(t.configure(d.path(),"Renamed"));QCOMPARE(t.identity(),id);
        QFile f(d.filePath("identity.pem"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("broken");f.close();
        QVERIFY(!t.configure(d.path(),"Host"));QVERIFY(t.identity().isEmpty());
        QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("broken"));
    }
    void authenticatedAdmissionReadyStartAndReconnect() {
        QTemporaryDir a,b;LocalLobbyTransport host(nullptr,port(),true),guest(nullptr,port(),true);
        QVERIFY(host.configure(a.path(),"Host"));QVERIFY(guest.configure(b.path(),"Guest"));QVERIFY(host.open());QVERIFY(guest.open());
        LobbyCoordinator h,g;bind(host,h);bind(guest,g);
        auto* room=h.createRoom("public",true);QVERIFY(room);const auto id=room->state()["id"].toString();QVERIFY(h.activateRoom(id));
        QCOMPARE(room->state()["access"].toString(),QString("approval"));QVERIFY(!room->setAccess("public"));
        advertise(announcement(host,id),guest.discoveryPort_);QTRY_COMPARE(guest.rooms().size(),1);
        QVERIFY(g.join(host.identity(),id,true));QTRY_COMPARE(h.requests().size(),1);
        QCOMPARE(h.state()["members"].toArray().size(),1);QVERIFY(h.approve(guest.identity()));
        QTRY_COMPARE(g.state()["members"].toArray().size(),2);QCOMPARE(g.state()["type"].toString(),QString("local"));
        QVERIFY(!host.addressOf(guest.identity()).isEmpty());QVERIFY(!guest.addressOf(host.identity()).isEmpty());
        QSignalSpy disconnected(&guest,&LocalLobbyTransport::disconnected);
        auto* duplicate=new QSslSocket(&guest);guest.attach(duplicate,host.identity());
        duplicate->connectToHostEncrypted("127.0.0.1",host.listeners_.begin().value()->serverPort());
        QTRY_COMPARE(guest.links_.size(),1);QCOMPARE(disconnected.size(),0);
        // Fluxer login/logout is not the identity or lifetime of a Local room.
        h.configure("123","Account",true);h.configure({}, {},false);QCOMPARE(h.state()["id"].toString(),id);
        QVERIFY(h.selectGame({"test","Game","nes","netplay",2}));QTRY_COMPARE(g.state()["phase"].toString(),QString("gathering"));
        QVERIFY(h.seat(1));QTRY_COMPARE(g.state()["generation"],h.state()["generation"]);
        QVERIFY(g.seat(2));
        QTRY_COMPARE(g.state()["members"].toArray().at(1).toObject()["seat"].toInt(),2);
        const auto generation=quint64(h.state()["generation"].toInteger());
        QVERIFY(h.prepared(generation,true));QVERIFY(g.prepared(generation,true));
        QTRY_VERIFY(g.state()["members"].toArray().at(1).toObject()["prepared"].toBool());
        QVERIFY(h.ready(true));QVERIFY(g.ready(true));QTRY_VERIFY(h.activeRoom()->canStart());
        QSignalSpy launches(&g,&LobbyCoordinator::launchRequested);QTest::qWait(80);QCOMPARE(launches.size(),0);
        // Socket loss clears Ready. Reconnect restores membership, not launch.
        guest.drop(guest.connection(host.identity()));QTRY_VERIFY(!h.activeRoom()->canStart());
        g.reconnect();QTRY_VERIFY(g.state()["connection"].toString().isEmpty());
        QCOMPARE(g.state()["id"].toString(),id);QCOMPARE(launches.size(),0);
        QTRY_COMPARE(h.state()["phase"].toString(),QString("gathering"));
        const auto next=quint64(h.state()["generation"].toInteger());
        QVERIFY(h.prepared(next,true));QVERIFY(g.prepared(next,true));
        QTRY_VERIFY(g.state()["members"].toArray().at(1).toObject()["prepared"].toBool());
        QVERIFY(h.ready(true));QVERIFY(g.ready(true));QTRY_VERIFY(h.activeRoom()->canStart());
        QVERIFY(h.start());QTRY_COMPARE(launches.size(),1);
        const auto ticket=h.state()["ticket"].toString();QVERIFY(h.acknowledgeLaunch(ticket,true));QVERIFY(g.acknowledgeLaunch(ticket,true));
        QTRY_COMPARE(g.state()["phase"].toString(),QString("playing"));
        h.leave();QTRY_VERIFY(g.state().isEmpty());
    }
    void advertisedIdentityCannotReplaceTlsPeer() {
        QTemporaryDir a,b,c;LocalLobbyTransport host(nullptr,port(),true),guest(nullptr,port(),true),impostor(nullptr,port(),true);
        QVERIFY(host.configure(a.path(),"Host"));QVERIFY(guest.configure(b.path(),"Guest"));QVERIFY(impostor.configure(c.path(),"Same name"));
        QVERIFY(host.open());QVERIFY(guest.open());QVERIFY(impostor.open());
        auto ad=announcement(impostor,QUuid::createUuid().toString(QUuid::WithoutBraces));ad["owner"]=host.identity();
        advertise(ad,guest.discoveryPort_);QTRY_COMPARE(guest.rooms().size(),1);
        QSignalSpy received(&impostor,&LocalLobbyTransport::received),failed(&guest,&LocalLobbyTransport::disconnected);
        QVERIFY(guest.send(host.identity(),{{"secret","not delivered"}}));QTRY_VERIFY(!failed.isEmpty());
        QCOMPARE(received.size(),0);QVERIFY(guest.addressOf(host.identity()).isEmpty());
    }
    void localCheckpointRestoresWithoutAnAccountOrReady() {
        QTemporaryDir dir;const auto self="local-"+QString(64,'a'),peer="local-"+QString(64,'b');QString id;
        {
            LobbyCoordinator c;c.configureLocal(self,"Host",true);c.setLocalStorage(dir.filePath("rooms"));
            auto* r=c.createRoom("private",true);QVERIFY(r);id=r->state()["id"].toString();QVERIFY(c.activateRoom(id));
            QVERIFY(c.selectGame({"test","Game","nes","netplay",2}));QVERIFY(c.seat(1));QVERIFY(c.prepared(quint64(c.state()["generation"].toInteger()),true));
        }
        LobbyCoordinator restored;restored.configureLocal(self,"Host",false);restored.setLocalStorage(dir.filePath("rooms"));
        QCOMPARE(restored.state()["id"].toString(),id);QVERIFY(restored.activeRoom()->local());
        restored.configureLocal(self,"Host",true);QVERIFY(!restored.activeRoom()->canStart());
        for(const auto& value:restored.state()["members"].toArray()){QVERIFY(!value.toObject()["ready"].toBool());QVERIFY(!value.toObject()["prepared"].toBool());}
        restored.configure("123","Provider",true);restored.setStorage(dir.filePath("online"));
        QCOMPARE(restored.state()["id"].toString(),id);restored.configure({}, {},false);QCOMPARE(restored.state()["id"].toString(),id);
    }
    void roomTypesAndRuntimeCapabilitiesStaySeparate() {
        LobbyCoordinator c;c.configure("123","Online",true);c.configureLocal("local-"+QString(64,'a'),"Local",true);
        auto* local=c.createRoom("private",true);auto* online=c.createRoom("public");QVERIFY(local);QVERIFY(online);
        QVERIFY(local->local());QVERIFY(!online->local());QVERIFY(!local->invite("456"));QVERIFY(!online->invite("local-"+QString(64,'b')));
        QVERIFY(!localLobbyRuntime({{"id","remoteplay.v1"}}));QVERIFY(!localLobbyRuntime({{"id","unknown"}}));
        QVERIFY(!localLobbyRuntime({{"id","runtime.ppsspp.psp.v1"},{"connections",QJsonArray{"online"}}}));
        QVERIFY(localLobbyRuntime({{"id","runtime.armsx2.lan.v1"},{"connections",QJsonArray{"online"}}}));
        QVERIFY(localLobbyRuntime({{"id","runtime.retroarch.nes.fceumm.v1"},{"connections",QJsonArray{"nearby","online"}}}));
        QVERIFY(c.activateRoom(local->state()["id"].toString()));local->leave();
        LobbyCoordinator inviter;inviter.configure("456","Online friend",true);QVERIFY(inviter.create("private"));
        connect(&inviter,&LobbyCoordinator::outgoing,&c,[&](QString,QJsonObject p){c.receive("456","Online friend",p);});
        QVERIFY(inviter.invite("123"));QVERIFY(!c.invitation().isEmpty());QVERIFY(local->invitation().isEmpty());
    }
    void separateProcessTlsAndDiscoveryExpiry() {
        QTemporaryDir remote,local;QProcess process;
        process.start(QCoreApplication::applicationFilePath(),{"--local-peer",remote.path()});QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForReadyRead(10000));const auto ad=QJsonDocument::fromJson(process.readLine()).object();QVERIFY(!ad.isEmpty());
        LocalLobbyTransport guest(nullptr,port(),true);QVERIFY(guest.configure(local.path(),"Guest"));QVERIFY(guest.open());
        advertise(ad,guest.discoveryPort_);QTRY_COMPARE(guest.rooms().size(),1);
        QSignalSpy reply(&guest,&LocalLobbyTransport::received);QVERIFY(guest.send(ad["owner"].toString(),{{"probe",42}}));
        QTRY_COMPARE(reply.size(),1);QCOMPARE(reply[0][0].toString(),ad["owner"].toString());QCOMPARE(reply[0][2].toJsonObject()["probe"].toInt(),42);
        process.kill();QVERIFY(process.waitForFinished());QTRY_VERIFY_WITH_TIMEOUT(guest.rooms().isEmpty(),9000);
        guest.close();QVERIFY(!guest.running());QVERIFY(guest.links_.isEmpty());QVERIFY(guest.listeners_.isEmpty());
    }
public:
    static int peer(QString path) {
        LocalLobbyTransport host(nullptr,port(),true);if(!host.configure(path,"Process host")||!host.open())return 2;
        QFile output;if(!output.open(stdout,QIODevice::WriteOnly))return 3;
        output.write(QJsonDocument(announcement(host,QUuid::createUuid().toString(QUuid::WithoutBraces))).toJson(QJsonDocument::Compact)+'\n');output.flush();
        connect(&host,&LocalLobbyTransport::received,&host,[&](QString peer,QString,QJsonObject packet){host.send(peer,packet);});
        QTimer::singleShot(20000,QCoreApplication::instance(),&QCoreApplication::quit);return QCoreApplication::exec();
    }
};
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().size()==3&&app.arguments()[1]=="--local-peer")return trainer::LocalLobbyTests::peer(app.arguments()[2]);
    trainer::LocalLobbyTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "LocalLobbyTests.moc"
