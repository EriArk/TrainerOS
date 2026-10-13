#include <QtTest>
#include <QJsonArray>
#include "features/social/LobbySession.h"
#include "features/social/LobbyCoordinator.h"
#include "integrations/social/LobbyDirectory.h"
#include "integrations/social/LobbyWire.h"

namespace trainer {
class LobbyTests final : public QObject {
    Q_OBJECT
    static LobbySession::Game game(QString id="test",int seats=2) {return {id,"Game","nes","netplay",seats};}
    static void prepare(LobbySession& l,QString peer,int seat) {
        QVERIFY(l.claimSeat(peer,seat,l.generation()));QVERIFY(l.prepared(peer,l.generation(),true));
    }
private slots:
    void twoMembersKeepOneLobbyAcrossReadyLaunchAndReturn() {
        LobbyCoordinator host,guest;
        host.configure("1","Host",true);guest.configure("2","Guest",true);
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString peer,QJsonObject packet){if(peer=="2")guest.receive("1","Host",packet);});
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString peer,QJsonObject packet){if(peer=="1")host.receive("2","Guest",packet);});
        QVERIFY(host.create());QVERIFY(host.invite("2"));QVERIFY(!guest.invitation().isEmpty());
        QVERIFY(guest.state().isEmpty());guest.answer(true);
        const auto id=host.state()["id"].toString();QCOMPARE(guest.state()["id"].toString(),id);
        QVERIFY(host.selectGame(game()));QVERIFY(host.seat(1));QVERIFY(guest.seat(2));
        const auto generation=quint64(host.state()["generation"].toInteger());
        QVERIFY(host.prepared(generation,true));QVERIFY(guest.prepared(generation,true));
        QSignalSpy hostLaunch(&host,&LobbyCoordinator::launchRequested),guestLaunch(&guest,&LobbyCoordinator::launchRequested);
        QVERIFY(guest.ready(true));QCOMPARE(guestLaunch.size(),0);QVERIFY(host.ready(true));
        QCOMPARE(hostLaunch.size(),1);QCOMPARE(guestLaunch.size(),1);
        const auto ticket=hostLaunch[0][0].toString();QVERIFY(host.acknowledgeLaunch(ticket,true));
        QVERIFY(host.finishGame(ticket));QCOMPARE(guest.state()["id"].toString(),id);
        QVERIFY(!guest.ready(true));QCOMPARE(guest.state()["members"].toArray().size(),2);
        host.leave();QVERIFY(guest.state().isEmpty());
    }
    void publicJoinApprovalAndUnknownHostCannotForceLaunch() {
        LobbyCoordinator host,guest;
        host.configure("1","Host",true);guest.configure("2","Guest",true);
        QJsonObject latest;
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){latest=p;guest.receive("1","Host",p);});
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
        QVERIFY(host.create("approval"));QVERIFY(guest.join("1",host.state()["id"].toString()));
        QCOMPARE(guest.state()["phase"].toString(),"joining");QCOMPARE(host.requests().size(),1);QVERIFY(host.approve("2"));
        QVERIFY(!guest.state().isEmpty());QVERIFY(host.selectGame(game()));QVERIFY(host.seat(1));QVERIFY(guest.seat(2));
        QSignalSpy launches(&guest,&LobbyCoordinator::launchRequested);
        auto forged=host.state();forged["phase"]="launching";forged["ticket"]="forged";
        forged["revision"]=forged["revision"].toInteger()+1;
        auto members=forged["members"].toArray();
        for(int i=0;i<members.size();++i){auto m=members[i].toObject();m["prepared"]=true;m["ready"]=true;members[i]=m;}
        forged["members"]=members;
        auto packet=latest;packet["serial"]=latest["serial"].toInteger()+1;packet["data"]=forged;
        guest.receive("3","Stranger",packet);QCOMPARE(launches.size(),0);
        guest.receive("1","Host",packet);QCOMPARE(launches.size(),0);
        QCOMPARE(guest.state()["phase"].toString(),"launching"); // Valid host snapshot still cannot supply local preparation/consent.
    }
    void delayedReadyCannotOverrideNewerNotReady() {
        LobbyCoordinator host,guest;
        host.configure("1","Host",true);guest.configure("2","Guest",true);
        QList<QJsonObject> held;bool hold=false;
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){guest.receive("1","Host",p);});
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){if(hold)held.append(p);else host.receive("2","Guest",p);});
        QVERIFY(host.create("public"));QVERIFY(guest.join("1",host.state()["id"].toString()));
        QVERIFY(host.selectGame(game()));QVERIFY(host.seat(1));QVERIFY(guest.seat(2));
        const auto generation=host.state()["generation"].toInteger();
        QVERIFY(host.prepared(generation,true));QVERIFY(guest.prepared(generation,true));
        hold=true;QVERIFY(guest.ready(true));QVERIFY(guest.ready(false));QCOMPARE(held.size(),2);
        host.receive("2","Guest",held.last());host.receive("2","Guest",held.first());
        QSignalSpy launches(&host,&LobbyCoordinator::launchRequested);
        QVERIFY(host.ready(true));QCOMPARE(launches.size(),0);
        hold=false;QVERIFY(guest.ready(true));QCOMPARE(launches.size(),1);
    }
    void wireKeepsInterleavedPeersSeparateAndRejectsExpiredFrames() {
        const QJsonObject packet{{"kind","state"},{"text",QString(4000,'x')}};
        const auto messages=LobbyWire::encode(packet,100);QVERIFY(messages.size()>1);
        LobbyWire receiver;
        for(int i=0;i<messages.size()-1;++i) {
            QVERIFY(receiver.receive("a",LobbyWire::frame(messages[i]),101).isEmpty());
            QVERIFY(receiver.receive("b",LobbyWire::frame(messages[i]),101).isEmpty());
            QVERIFY(receiver.receive("a",LobbyWire::frame(messages[i]),101).isEmpty());
        }
        QCOMPARE(receiver.receive("a",LobbyWire::frame(messages.last()),101),packet);
        QVERIFY(receiver.receive("b",LobbyWire::frame(messages.last()),191).isEmpty());
        QVERIFY(LobbyWire::encode(QJsonObject{{"text",QString(33000,'x')}},100).isEmpty());
        auto invalid=LobbyWire::frame(messages.first());invalid["count"]=999;
        QVERIFY(receiver.receive("c",invalid,101).isEmpty());
    }
    void directoryRequiresBotIdentityAndFreshPublicFields() {
        const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QJsonObject entry{{"v",1},{"lobby",id},{"owner","123"},{"name","Host"},{"revision",1},
            {"expires",200},{"status","choosing"},{"seats",0},{"available",0},{"access","public"},{"endpoint","private"}};
        QJsonObject message{{"author",QJsonObject{{"id",lobbyDirectory::bot}}},{"content",lobbyDirectory::encode(entry)}};
        const auto parsed=lobbyDirectory::listing(message,100);QCOMPARE(parsed["lobby"].toString(),id);QVERIFY(!parsed.contains("endpoint"));
        QVERIFY(lobbyDirectory::listing(message,200).isEmpty());QVERIFY(lobbyDirectory::listing(message,-200).isEmpty());
        message["author"]=QJsonObject{{"id","123"}};QVERIFY(lobbyDirectory::listing(message,100).isEmpty());
        message["author"]=QJsonObject{{"id",lobbyDirectory::bot}};message["webhook_id"]="456";
        QVERIFY(lobbyDirectory::listing(message,100).isEmpty());
        QVERIFY(lobbyDirectory::decode(lobbyDirectory::encode(QJsonObject{{"v",true}})).isEmpty());
    }
    void gameFreeMembershipAndAuthority() {
        LobbySession l;QSignalSpy launches(&l,&LobbySession::launchRequested);
        QVERIFY(l.create("host","Host"));const auto id=l.id();
        QVERIFY(!l.join("guest","Guest",100));QVERIFY(!l.invite("stranger","guest",200));
        QVERIFY(l.invite("host","guest",200));QVERIFY(!l.join("guest","Guest",200));
        QVERIFY(l.invite("host","guest",300));QVERIFY(l.join("guest","Guest",200));
        QVERIFY(!l.ready("host",l.generation(),true));QCOMPARE(launches.size(),0);
        QVERIFY(!l.selectGame("guest",game()));QVERIFY(!l.close("guest"));
        QVERIFY(l.selectGame("host",game()));QCOMPARE(l.id(),id);
        QCOMPARE(l.state()["members"].toArray().size(),2);
        QVERIFY(l.close("host"));QVERIFY(!l.join("guest","Guest",201));
        QVERIFY(!l.prepared("guest",l.generation(),true));
    }
    void noSecondStartAndNoMinimumPlayers() {
        LobbySession l;QVERIFY(l.create("host","Host"));QVERIFY(l.selectGame("host",game()));
        QSignalSpy launches(&l,&LobbySession::launchRequested);
        prepare(l,"host",1);QCOMPARE(launches.size(),0);
        QVERIFY(l.ready("host",l.generation(),true));QCOMPARE(launches.size(),1);
        const auto ticket=launches[0][0].toString();
        QVERIFY(!l.ready("host",l.generation(),true));QVERIFY(!l.selectGame("host",game("other")));
        QCOMPARE(l.state()["phase"].toString(),"launching");
        QVERIFY(!l.acknowledgeLaunch("old",true));QVERIFY(l.acknowledgeLaunch(ticket,true));
        QCOMPARE(l.state()["phase"].toString(),"playing");
        const auto id=l.id();const auto generation=l.generation();QVERIFY(l.finishGame(ticket));
        QCOMPARE(l.id(),id);QVERIFY(l.generation()>generation);QCOMPARE(launches.size(),1);
        QVERIFY(!l.finishGame(ticket));QVERIFY(!l.ready("host",generation,true));
    }
    void unseatedPlayerBlocksButExplicitSpectatorDoesNot() {
        LobbySession l;QVERIFY(l.create("host","Host","public"));QVERIFY(l.join("guest","Guest",100));
        QVERIFY(l.selectGame("host",game()));QSignalSpy launches(&l,&LobbySession::launchRequested);
        prepare(l,"host",1);QVERIFY(l.ready("host",l.generation(),true));QCOMPARE(launches.size(),0);
        QVERIFY(l.spectate("guest",true,l.generation()));QCOMPARE(launches.size(),0);
        QVERIFY(!l.ready("guest",l.generation(),true));
        QVERIFY(l.ready("host",l.generation(),true));QCOMPARE(launches.size(),1);
    }
    void seatsCannotBeDoubleAssignedAndGameChangesInvalidatePreparation() {
        LobbySession l;QVERIFY(l.create("h","Host","public"));
        QVERIFY(l.join("a","A",100));QVERIFY(l.join("b","B",100));QVERIFY(l.selectGame("h",game()));
        prepare(l,"h",1);prepare(l,"a",2);QVERIFY(!l.claimSeat("b",2,l.generation()));
        QVERIFY(!l.claimSeat("b",3,l.generation()));
        const auto old=l.generation();QVERIFY(l.selectGame("h",game("next")));
        QVERIFY(!l.prepared("a",old,true));QVERIFY(!l.ready("a",old,true));
        for(const auto& value:l.state()["members"].toArray()) {
            const auto m=value.toObject();QCOMPARE(m["seat"].toInt(),0);QVERIFY(!m["ready"].toBool());QVERIFY(!m["prepared"].toBool());
        }
    }
    void lossCancelsPendingLaunchAndRejoinNeedsNewPreparation() {
        LobbySession l;QVERIFY(l.create("h","Host","public"));QVERIFY(l.join("g","Guest",100));
        QVERIFY(l.selectGame("h",game()));prepare(l,"h",1);prepare(l,"g",2);
        QSignalSpy launches(&l,&LobbySession::launchRequested);
        QVERIFY(l.ready("h",l.generation(),true));QVERIFY(l.ready("g",l.generation(),true));
        const auto ticket=launches[0][0].toString(), id=l.id();
        QVERIFY(l.connected("g",false));QVERIFY(!l.acknowledgeLaunch(ticket,true));
        QVERIFY(l.connected("g",true));QVERIFY(!l.ready("g",l.generation(),true));QCOMPARE(l.id(),id);
        QVERIFY(l.connected("h",false));QVERIFY(!l.join("new","New",101));
        QVERIFY(!l.prepared("g",l.generation(),true));QVERIFY(l.connected("h",true));
        prepare(l,"h",1);prepare(l,"g",2);QVERIFY(l.ready("h",l.generation(),true));
        QVERIFY(l.ready("g",l.generation(),true));QCOMPARE(launches.size(),2);
    }
    void failedRuntimeDoesNotClaimPlayingOrDestroyLobby() {
        LobbySession l;QVERIFY(l.create("h","Host"));QVERIFY(l.selectGame("h",game()));
        prepare(l,"h",1);QSignalSpy launches(&l,&LobbySession::launchRequested);
        QVERIFY(l.ready("h",l.generation(),true));QVERIFY(l.acknowledgeLaunch(launches[0][0].toString(),false));
        QCOMPARE(l.state()["phase"].toString(),"gathering");QVERIFY(!l.ready("h",l.generation(),true));
        QCOMPARE(l.state()["members"].toArray().size(),1);
    }
};
}
QTEST_GUILESS_MAIN(trainer::LobbyTests)
#include "LobbyTests.moc"
