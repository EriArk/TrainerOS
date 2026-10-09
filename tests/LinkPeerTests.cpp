#include "adapters/pokemon/center/LinkController.h"
#include <QtTest>
#include <QTcpSocket>
#include <QJsonDocument>
#include "platform/network/NearbyService.h"
using namespace trainer;
class LinkPeerTests:public QObject {
    Q_OBJECT
private slots:
    void runtimePartyKeepsThreeGuestSocketsIndependent() {
        QTcpServer reserve;QVERIFY(reserve.listen(QHostAddress::LocalHost,0));const auto port=reserve.serverPort();reserve.close();
        LocalLinkPeer host(nullptr,port,0,"party-test",true);host.configure("11111111-1111-4111-8111-111111111111","Host");QVERIFY(host.open());
        QSignalSpy received(&host,&LocalLinkPeer::partyReceived),left(&host,&LocalLinkPeer::partyDisconnected);
        QTcpSocket b,c,d;const QList<QTcpSocket*> sockets{&b,&c,&d};
        const QStringList ids{"22222222-2222-4222-8222-222222222222","33333333-3333-4333-8333-333333333333","44444444-4444-4444-8444-444444444444"};
        for(int i=0;i<3;++i){auto* s=sockets[i];s->connectToHost(QHostAddress::LocalHost,port);QTRY_VERIFY(s->bytesAvailable()>0);s->readAll();
            s->write(QJsonDocument(QJsonObject{{"partyHello",ids[i]},{"name",QString::number(i)}}).toJson(QJsonDocument::Compact)+'\n');
            s->write("{\"kind\":\"query\"}\n");QTRY_COMPARE(received.count(),i+1);QCOMPARE(received.last()[0].toString(),ids[i]);}
        for(int i=0;i<3;++i){host.sendTo(ids[i],{{"slot",i+2}});QTRY_VERIFY(sockets[i]->bytesAvailable()>0);QCOMPARE(QJsonDocument::fromJson(sockets[i]->readLine()).object()["slot"].toInt(),i+2);}
        c.disconnectFromHost();QTRY_COMPARE(left.count(),1);QCOMPARE(left.first()[0].toString(),ids[1]);
        QVERIFY(!host.addressOf(ids[0]).isEmpty());QVERIFY(!host.addressOf(ids[2]).isEmpty());
        b.write("{\"kind\":\"still-here\"}\n");QTRY_COMPARE(received.count(),4);
    }
    void onlineReusesConsentWorkspaceAndPreservesRecovery() {
        const QString local="11111111-1111-4111-8111-111111111111",remote="22222222-2222-4222-8222-222222222222";
        LinkController link;QStringList operations;
        link.configure({},local,"Misty",[&](const QString& op,const QJsonObject&,QObject*,auto done){operations<<op;done(QJsonObject{});},
            [](const PracticeSource&,const GameProgress&,QObject*,auto done){done(true);},{});
        GameProgress progress;progress.availability=ProgressAvailability::Available;
        progress.contentRevision="a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af";
        progress.saveRevision="save";progress.contextRevision="owner";progress.party=PartySnapshot{};
        PokemonRecord mon;mon.kind=PokemonSlotKind::Known;mon.speciesName="Test";mon.speciesId="1";mon.formId="standard";progress.party->party.append(mon);
        link.setObservation({"trainer","emerald","owner",progress.contentRevision,"save"},progress,{});
        QCOMPARE(link.onlineCapabilities().size(),3);
        QSignalSpy sent(&link,&LinkController::onlineSend),opened(&link,&LinkController::workspaceRequested);
        QVERIFY(!link.beginOnline(local,remote,"Friend","org.traineros.emerald.battle",true));
        QVERIFY(link.beginOnline(local,remote,"Friend","org.traineros.emerald.trade",true));
        QCOMPARE(opened.size(),1);QCOMPARE(link.stage(),"choose");QVERIFY(operations.isEmpty());
        QVERIFY(!link.canBrowseForRecovery());
        QVERIFY(!link.activityModes().contains("battle"));
        link.dispatch(Action::Back);QCOMPARE(link.stage(),"lobby");QVERIFY(link.connected());
        link.receiveOnline({{"type","activity-invite"},{"version",2},{"id","44444444-4444-4444-8444-444444444444"},{"mode","battle"}});
        QVERIFY(!link.invitationOpen());
        link.leave();QVERIFY(link.connected());link.showOnline();QVERIFY(link.isOpen());QCOMPARE(opened.size(),2);
        link.endOnline();QVERIFY(!link.connected());
        const QJsonObject pending{{"id","55555555-5555-4555-8555-555555555555"},{"peer",remote},{"stage","prepared"},{"kind","trade"},{"owner","trainer"},{"adventure","emerald"}};
        link.configure({},local,"Misty",[&](const QString& op,const QJsonObject&,QObject*,auto done){operations<<op;done(pending);},
            [](const PracticeSource&,const GameProgress&,QObject*,auto done){done(true);},pending);
        QVERIFY(link.canBrowseForRecovery());
        QVERIFY(!link.beginOnline(local,"33333333-3333-4333-8333-333333333333","Other","org.traineros.emerald.trade",true));
        QVERIFY(link.beginOnline(local,remote,"Friend","org.traineros.emerald.trade",true));
        QCOMPARE(operations,QStringList{"status"});QVERIFY(link.pending());
        QVERIFY(!link.canBrowseForRecovery());
        const auto receipt=sent.last()[0].toJsonObject()["receipt"].toObject();
        QVERIFY(!receipt.contains("owner"));QVERIFY(!receipt.contains("adventure"));
        link.endOnline();QVERIFY(link.pending());QCOMPARE(operations,QStringList{"status"});
        QVERIFY(link.canBrowseForRecovery());QVERIFY(link.navigationBlocked());
    }
    void bluetoothBridgeCarriesExistingFramesOverLoopback() {
        LocalLinkPeer link;link.configure("11111111-1111-4111-8111-111111111111","Misty");
        QTcpServer bridge;QVERIFY(bridge.listen(QHostAddress::LocalHost,0));
        QSignalSpy received(&link,&LocalLinkPeer::received);
        QSignalSpy disconnected(&link,&LocalLinkPeer::disconnectedFromPeer);
        link.connectBridge(bridge.serverPort());
        QTRY_VERIFY(bridge.hasPendingConnections());auto* remote=bridge.nextPendingConnection();
        QTRY_VERIFY(link.connected());QVERIFY(link.outgoing());
        link.send({{"type","hello"},{"version",2}});QTRY_VERIFY(remote->bytesAvailable()>0);
        QCOMPARE(QJsonDocument::fromJson(remote->readLine()).object()["type"],"hello");
        remote->write("{\"type\":\"ping\",");remote->flush();QTest::qWait(10);QCOMPARE(received.size(),0);
        remote->write("\"version\":2}\n");remote->flush();QTRY_COMPARE(received.size(),1);
        QCOMPARE(received[0][0].value<QJsonObject>()["type"],"ping");
        remote->disconnectFromHost();QTRY_COMPARE(disconnected.size(),1);
        QVERIFY(!link.connected());remote->deleteLater();
    }
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
        emit radio->event({{"event","ready"},{"peer",other},{"transport","bluetooth"},{"incoming",true}});QCOMPARE(network->interval(),15000);
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
        QVERIFY(link.invitationText().contains("an exchange"));
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
