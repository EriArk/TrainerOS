#include "features/center/LinkController.h"
#include <QtTest>
#include <QTcpSocket>
#include <QJsonDocument>
#include "platform/network/NearbyService.h"
using namespace trainer;
class LinkPeerTests:public QObject {
    Q_OBJECT
private slots:
    void directAttemptHasIndependentDeadlinesAndCanRetry() {
        LinkController link;
        link.configure({},"11111111-1111-4111-8111-111111111111","Misty",
            [](const QString&,const QJsonObject&,QObject*,std::function<void(QJsonObject)> done){done({});},{},{});
        auto* radio=link.findChild<NearbyService*>();QVERIFY(radio);
        auto* response=link.findChild<QTimer*>("link-invitation-deadline");QVERIFY(response);
        auto* network=link.findChild<QTimer*>("link-connection-deadline");QVERIFY(network);
        QSignalSpy failed(&link,&LinkController::connectionFailed);
        const QJsonObject invite{{"event","invite"},{"peer","22222222-2222-4222-8222-222222222222"},{"name","Friend"}};
        emit radio->event(invite);QVERIFY(link.invitationOpen());QVERIFY(link.invitationIncoming());
        QCOMPARE(network->interval(),60000);QVERIFY(!response->isActive());
        link.answerInvitation(true);QCOMPARE(network->interval(),75000);
        QVERIFY(network->isActive());QVERIFY(!response->isActive());QVERIFY(link.invitationText().startsWith("Connecting"));
        emit radio->event(invite);QCOMPARE(network->interval(),75000);QVERIFY(!link.invitationIncoming());
        QVERIFY(QMetaObject::invokeMethod(network,"timeout",Qt::DirectConnection));
        QCOMPARE(failed.size(),1);QVERIFY(!link.invitationOpen());QVERIFY(!link.active());QCOMPARE(link.stage(),"browse");
        emit radio->event(invite);QVERIFY(link.invitationIncoming());link.answerInvitation(true);
        emit radio->event({{"event","closed"},{"error","Your friend did not respond. Invite them again."}});
        QCOMPARE(failed.size(),2);QVERIFY(!network->isActive());QVERIFY(!link.invitationOpen());
        emit radio->event(invite);QVERIFY(link.invitationIncoming());link.answerInvitation(false);
        QCOMPARE(failed.size(),2);QVERIFY(!link.invitationOpen());
    }
    void directPairSurvivesPageChangesAndActivityTimeout() {
        LinkController link;
        const QString other="22222222-2222-4222-8222-222222222222";
        link.configure({},"11111111-1111-4111-8111-111111111111","Misty",
            [](const QString&,const QJsonObject&,QObject*,std::function<void(QJsonObject)> done){done({});},{},{});
        auto* radio=link.findChild<NearbyService*>();QVERIFY(radio);
        auto* network=link.findChild<QTimer*>("link-connection-deadline");QVERIFY(network);
        auto* response=link.findChild<QTimer*>("link-invitation-deadline");QVERIFY(response);
        QSignalSpy failed(&link,&LinkController::connectionFailed);
        emit radio->event({{"event","invite"},{"peer",other},{"name","Friend"}});link.answerInvitation(true);
        emit radio->event({{"event","ready"},{"peer",other},{"interface","p2p-test"}});QCOMPARE(network->interval(),15000);
        QTcpSocket remote;remote.connectToHost(QHostAddress::LocalHost,47845);
        QTRY_COMPARE(remote.state(),QAbstractSocket::ConnectedState);QTRY_VERIFY(remote.bytesAvailable()>0);
        const auto hello=QJsonDocument::fromJson(remote.readLine()).object();
        QVERIFY(!hello.contains("save"));QVERIFY(!hello.contains("pokemon"));
        auto send=[&](QJsonObject value){value["version"]=2;remote.write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n');remote.flush();};
        send({{"type","hello"},{"id",other},{"nonce","33333333-3333-4333-8333-333333333333"},{"name","Friend"}});
        QTRY_COMPARE(link.code().size(),6);send({{"type","accept"},{"code",link.code()}});
        QTRY_VERIFY(link.connected());QVERIFY(!network->isActive());QVERIFY(!response->isActive());
        link.enter();link.leave();QVERIFY(link.connected());QVERIFY(!link.isOpen());
        const QString invitation="55555555-5555-4555-8555-555555555555";
        send({{"type","activity-invite"},{"id",invitation},{"mode","trade"}});QTRY_VERIFY(link.invitationIncoming());
        QVERIFY(QMetaObject::invokeMethod(response,"timeout",Qt::DirectConnection));
        QVERIFY(link.connected());QVERIFY(!link.invitationOpen());QCOMPARE(failed.size(),0);
        send({{"type","activity-invite"},{"id",invitation},{"mode","trade"}});QTRY_VERIFY(link.invitationIncoming());
        link.answerInvitation(false);QVERIFY(link.connected());QVERIFY(!link.invitationOpen());
        emit radio->event({{"event","unavailable"},{"error","Search unavailable"}});QVERIFY(link.connected());
    }
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
        QSignalSpy workspace(&link,&LinkController::workspaceRequested);
        QSignalSpy changedSave(&link,&LinkController::saveChanged);
        link.enter();QTcpSocket remote;remote.connectToHost(QHostAddress::LocalHost,47845);
        QTRY_COMPARE(remote.state(),QAbstractSocket::ConnectedState);
        QTRY_VERIFY(remote.bytesAvailable()>0);remote.readAll();
        auto send=[&](QJsonObject value){value["version"]=2;remote.write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n');remote.flush();};
        send({{"type","hello"},{"id",other},{"nonce","33333333-3333-4333-8333-333333333333"},{"name","Friend"},{"pending",transaction}});
        QTRY_COMPARE(link.code().size(),6);
        send({{"type","accept"},{"code",link.code()}});link.activate(0);
        QTRY_COMPARE(link.stage(),"saving");
        QCOMPARE(workspace.size(),1);
        send({{"type","receipt"},{"receipt",QJsonObject{{"id",transaction},{"peer",local},
            {"proposal",QString(64,'a')},{"stage","committed"},{"after",QString(64,'c')}}}});
        QTRY_COMPARE(link.stage(),"finished");QCOMPARE(changedSave.size(),1);
        link.setObservation({},GameProgress{},{});QTest::qWait(50);
        QCOMPARE(link.stage(),"finished");QCOMPARE(closed.size(),0);QVERIFY(!link.pending());
        link.dispatch(Action::Confirm);QCOMPARE(closed.size(),0);QCOMPARE(link.stage(),"lobby");QVERIFY(link.connected());
        link.leave();QVERIFY(!link.isOpen());QVERIFY(link.connected());
    }
    void explicitBothSidePairingBeforeAnyGameData() {
        LinkController link;
        link.configure({},"11111111-1111-4111-8111-111111111111","Test console",
            [](const QString&,const QJsonObject&,QObject*,std::function<void(QJsonObject)> done){done({});},
            [](const PracticeSource&,const GameProgress&,QObject*,std::function<void(bool)> done){done(false);},{});
        link.setTrainerName("Misty");QCOMPARE(link.stage(),"browse");
        QTcpSocket remote;remote.connectToHost(QHostAddress::LocalHost,47845);
        QTRY_COMPARE(remote.state(),QAbstractSocket::ConnectedState);QTRY_VERIFY(remote.bytesAvailable()>0);
        const auto hello=QJsonDocument::fromJson(remote.readLine()).object();QCOMPARE(hello["type"],"hello");
        QCOMPARE(hello["name"],"Misty");QVERIFY(!link.isOpen());
        QVERIFY(!hello.contains("save"));QVERIFY(!hello.contains("pokemon"));
        auto send=[&](QJsonObject value){value["version"]=2;remote.write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n');remote.flush();};
        send({{"type","hello"},{"id","22222222-2222-4222-8222-222222222222"},{"nonce","33333333-3333-4333-8333-333333333333"},{"name","Friend"}});
        QTRY_COMPARE(link.code().size(),6);
        send({{"type","mode"},{"mode","trade"}});QTest::qWait(30);QCOMPARE(link.stage(),"pair");
        send({{"type","accept"},{"code",link.code()}});QTest::qWait(30);QCOMPARE(link.stage(),"pair");
        link.activate(0);QTRY_COMPARE(link.stage(),"lobby");QCOMPARE(link.rows().size(),4);
        const QString invitation="55555555-5555-4555-8555-555555555555";
        link.setInvitationsAllowed(false);
        send({{"type","activity-invite"},{"id",invitation},{"mode","trade"}});
        QTest::qWait(30);QVERIFY(!link.invitationOpen());QVERIFY(link.connected());
        link.setInvitationsAllowed(true);
        link.activate(0);QVERIFY(link.invitationOpen());QVERIFY(!link.invitationIncoming());
        send({{"type","activity-invite"},{"id",invitation},{"mode","trade"}});
        QTest::qWait(30);QVERIFY(!link.invitationIncoming()); // Host resolves simultaneous invitations.
        link.answerInvitation(false);QVERIFY(!link.invitationOpen());
        send({{"type","activity-invite"},{"id",invitation},{"mode","trade"}});
        QTRY_VERIFY(link.invitationOpen());QVERIFY(link.invitationIncoming());QCOMPARE(link.stage(),"lobby");
        link.answerInvitation(false);QVERIFY(!link.invitationOpen());QVERIFY(link.connected());
        send({{"type","activity-invite"},{"id",invitation},{"mode","trade"}});
        QTRY_VERIFY(link.invitationOpen());link.answerInvitation(true);QTRY_COMPARE(link.stage(),"error");
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
