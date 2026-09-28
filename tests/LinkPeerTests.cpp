#include "features/center/LinkController.h"
#include <QtTest>
#include <QTcpSocket>
#include <QJsonDocument>
using namespace trainer;
class LinkPeerTests:public QObject {
    Q_OBJECT
private slots:
    void recoveredTradeKeepsResultUntilExplicitReturn() {
        const QString local="11111111-1111-4111-8111-111111111111";
        const QString other="22222222-2222-4222-8222-222222222222";
        const QString transaction="44444444-4444-4444-8444-444444444444";
        QJsonObject saved{{"id",transaction},{"peer",other},{"proposal",QString(64,'a')},
            {"stage","committed"},{"after",QString(64,'b')},{"peerAfter",QString(64,'c')},{"adventure","emerald"}};
        LinkController link;
        link.configure({},local,"Test console",
            [&](const QString& op,const QJsonObject&,QObject*,std::function<void(QJsonObject)> done){
                if(op=="finish")saved["stage"]="complete";
                done(saved);
            },{},saved);
        QSignalSpy closed(&link,&LinkController::closeRequested);
        QSignalSpy changedSave(&link,&LinkController::saveChanged);
        link.enter();QTcpSocket remote;remote.connectToHost(QHostAddress::LocalHost,47845);
        QTRY_COMPARE(remote.state(),QAbstractSocket::ConnectedState);
        QTRY_VERIFY(remote.bytesAvailable()>0);remote.readAll();
        auto send=[&](QJsonObject value){value["version"]=1;remote.write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n');remote.flush();};
        send({{"type","hello"},{"id",other},{"nonce","33333333-3333-4333-8333-333333333333"},{"name","Friend"},{"pending",transaction}});
        QTRY_COMPARE(link.code().size(),6);
        send({{"type","accept"},{"code",link.code()}});link.activate(0);
        QTRY_COMPARE(link.stage(),"saving");
        send({{"type","receipt"},{"receipt",QJsonObject{{"id",transaction},{"peer",local},
            {"proposal",QString(64,'a')},{"stage","committed"},{"after",QString(64,'c')}}}});
        QTRY_COMPARE(link.stage(),"finished");QCOMPARE(changedSave.size(),1);
        link.setObservation({},GameProgress{},{});QTest::qWait(50);
        QCOMPARE(link.stage(),"finished");QCOMPARE(closed.size(),0);QVERIFY(!link.pending());
        link.dispatch(Action::Confirm);QCOMPARE(closed.size(),1);QVERIFY(!link.isOpen());
    }
    void explicitBothSidePairingBeforeAnyGameData() {
        LinkController link;
        link.configure({},"11111111-1111-4111-8111-111111111111","Test console",
            [](const QString&,const QJsonObject&,QObject*,std::function<void(QJsonObject)> done){done({});},
            [](const PracticeSource&,const GameProgress&,QObject*,std::function<void(bool)> done){done(false);},{});
        link.enter();QCOMPARE(link.stage(),"browse");
        QTcpSocket remote;remote.connectToHost(QHostAddress::LocalHost,47845);
        QTRY_COMPARE(remote.state(),QAbstractSocket::ConnectedState);QTRY_VERIFY(remote.bytesAvailable()>0);
        const auto hello=QJsonDocument::fromJson(remote.readLine()).object();QCOMPARE(hello["type"],"hello");
        QVERIFY(!hello.contains("save"));QVERIFY(!hello.contains("pokemon"));
        auto send=[&](QJsonObject value){value["version"]=1;remote.write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n');remote.flush();};
        send({{"type","hello"},{"id","22222222-2222-4222-8222-222222222222"},{"nonce","33333333-3333-4333-8333-333333333333"},{"name","Friend"}});
        QTRY_COMPARE(link.code().size(),6);
        send({{"type","mode"},{"mode","trade"}});QTest::qWait(30);QCOMPARE(link.stage(),"pair");
        send({{"type","accept"},{"code",link.code()}});QTest::qWait(30);QCOMPARE(link.stage(),"pair");
        link.activate(0);QTRY_COMPARE(link.stage(),"lobby");QCOMPARE(link.rows().size(),2);
        send({{"type","mode"},{"mode","trade"}});QTRY_COMPARE(link.stage(),"error");
        QVERIFY(link.message().contains("saved Party"));
        // A disconnected peer releases ordinary navigation, no fake completion.
        remote.abort();QTRY_COMPARE(link.stage(),"browse");QVERIFY(!link.active());link.leave();
    }
    void oversizedPeerFrameDisconnects() {
        LocalLinkPeer peer;peer.configure("11111111-1111-4111-8111-111111111111","Test");QVERIFY(peer.open());
        QTcpSocket client;client.connectToHost(QHostAddress::LocalHost,47845);
        QTRY_COMPARE(client.state(),QAbstractSocket::ConnectedState);QTRY_VERIFY(peer.connected());
        client.write(QByteArray(32769,'x')+'\n');client.flush();QTRY_VERIFY(!peer.connected());peer.close();
    }
};
QTEST_GUILESS_MAIN(LinkPeerTests)
#include "LinkPeerTests.moc"
