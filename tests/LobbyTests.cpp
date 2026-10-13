#include <QtTest>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QFile>
#include "features/social/LobbySession.h"
#include "features/social/LobbyCoordinator.h"
#include "features/social/LobbyRuntime.h"
#include "integrations/social/LobbyDirectory.h"
#include "integrations/social/LobbyWire.h"

namespace trainer {
class TestLobbyRuntime final : public LobbyRuntimeService {
public:
    Completion preparation,opening;
    int prepares=0,launches=0,cancels=0;
    void prepare(QString,QJsonObject,Completion done) override {++prepares;preparation=done;}
    void launch(QString,QString,QJsonObject,Completion done) override {++launches;opening=done;}
    void cancel(QString) override {++cancels;}
};
class LobbyTests final : public QObject {
    Q_OBJECT
    static LobbySession::Game game(QString id="test",int seats=2) {return {id,"Game","nes","netplay",seats};}
    static void prepare(LobbySession& l,QString peer,int seat) {
        QVERIFY(l.claimSeat(peer,seat,l.generation()));QVERIFY(l.prepared(peer,l.generation(),true));
    }
private slots:
    void lostInviteAndReceiptRetryWithoutExtendingConsentOrReopeningDeclinedOffer() {
        LobbyCoordinator host,guest;host.configure("1","Host",true);guest.configure("2","Guest",true);
        int invitations=0;bool dropReceipt=true;
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){
            if(p["kind"]=="invite"&&++invitations==1)return;guest.receive("1","Host",p);
        });
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){if(!dropReceipt)host.receive("2","Guest",p);});
        QVERIFY(host.create());QVERIFY(host.invite("2"));QVERIFY(guest.invitation().isEmpty());
        host.tick();QVERIFY(!guest.invitation().isEmpty());const auto expiry=guest.invitation()["expires"];
        host.tick();QCOMPARE(guest.invitation()["expires"],expiry);guest.answer(false);
        dropReceipt=false;host.tick();QVERIFY(guest.invitation().isEmpty());const auto sent=invitations;
        host.tick();QCOMPARE(invitations,sent);QVERIFY(host.pendingInvites_.isEmpty());
        QVERIFY(host.invite("2"));QVERIFY(!guest.invitation().isEmpty()); // Explicit new offer.
    }
    void directoryRevisionAndCategorySurviveRestartAndExplicitLeave() {
        QTemporaryDir dir;const auto file=dir.filePath("room.json");qint64 last=0;
        {LobbyCoordinator host;host.configure("1","Host",true);host.setStorage(file);QVERIFY(host.create("public"));
            host.setPublicationTopic("racing");for(int i=0;i<5;++i)last=host.nextPublicationRevision();}
        {LobbyCoordinator host;host.configure("1","Host",true);host.setStorage(file);QCOMPARE(host.publicationTopic(),"racing");
            QVERIFY(host.nextPublicationRevision()>last);host.leave();last=host.nextPublicationRevision();}
        LobbyCoordinator host;host.configure("1","Host",true);host.setStorage(file);QVERIFY(host.state().isEmpty());
        QVERIFY(host.nextPublicationRevision()>last);
    }
    void restartRestoresPrivateMembershipButNeverOldReadyOrLaunch() {
        QTemporaryDir dir;QString room,ticket;QJsonObject stale;
        auto host=std::make_unique<LobbyCoordinator>();auto guest=std::make_unique<LobbyCoordinator>();
        const auto wire=[&]{
            connect(host.get(),&LobbyCoordinator::outgoing,guest.get(),[&](QString,QJsonObject p){if(p["kind"]=="state")stale=p;guest->receive("1","Host",p);});
            connect(guest.get(),&LobbyCoordinator::outgoing,host.get(),[&](QString,QJsonObject p){host->receive("2","Guest",p);});
        };
        host->configure("1","Host",true);guest->configure("2","Guest",true);
        host->setStorage(dir.filePath("host.json"));guest->setStorage(dir.filePath("guest.json"));wire();
        QVERIFY(host->create());QVERIFY(host->setConversation("123"));QVERIFY(host->selectGame(game()));
        QVERIFY(host->invite("2"));guest->answer(true);room=host->state()["id"].toString();
        const auto generation=quint64(host->state()["generation"].toInteger());
        QVERIFY(host->prepared(generation,true));QVERIFY(guest->prepared(generation,true));
        QVERIFY(host->ready(true));QVERIFY(guest->ready(true));ticket=host->state()["ticket"].toString();QVERIFY(!ticket.isEmpty());
        host.reset();guest.reset();host=std::make_unique<LobbyCoordinator>();guest=std::make_unique<LobbyCoordinator>();
        host->configure("1","Host",true);guest->configure("2","Guest",true);
        QSignalSpy hostLaunch(host.get(),&LobbyCoordinator::launchRequested),guestLaunch(guest.get(),&LobbyCoordinator::launchRequested);
        host->setStorage(dir.filePath("host.json"));guest->setStorage(dir.filePath("guest.json"));
        QVERIFY(guest->state()["recovering"].toBool());QVERIFY(!guest->ready(true));
        const auto replay=stale;wire();guest->reconnect();
        QCOMPARE(host->state()["id"].toString(),room);QCOMPARE(guest->state()["id"].toString(),room);
        QCOMPARE(guest->state()["conversation"].toString(),"123");QVERIFY(!guest->state()["recovering"].toBool());
        QVERIFY(host->state()["ticket"].toString().isEmpty());QCOMPARE(hostLaunch.size(),0);QCOMPARE(guestLaunch.size(),0);
        QVERIFY(host->state()["generation"].toInteger()>qint64(generation));
        guest->receive("1","Host",replay);QVERIFY(guest->state()["ticket"].toString().isEmpty());
        QVERIFY(!host->acknowledgeLaunch(ticket,true));QVERIFY(!guest->acknowledgeLaunch(ticket,true));
        for(const auto& v:guest->state()["members"].toArray()) {QVERIFY(!v.toObject()["ready"].toBool());QVERIFY(!v.toObject()["prepared"].toBool());}
        guest->leave();guest.reset();guest=std::make_unique<LobbyCoordinator>();guest->configure("2","Guest",true);guest->setStorage(dir.filePath("guest.json"));QCOMPARE(guest->state(),QJsonObject{});
    }
    void runningGuestAcceptsRestartedHostOnlyAfterItsOwnProbe() {
        QTemporaryDir dir;auto host=std::make_unique<LobbyCoordinator>();LobbyCoordinator guest;
        host->configure("1","Host",true);host->setStorage(dir.filePath("host.json"));guest.configure("2","Guest",true);
        auto wireHost=[&]{connect(host.get(),&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){guest.receive("1","Host",p);});};
        connect(&guest,&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){host->receive("2","Guest",p);});wireHost();
        QVERIFY(host->create("public"));QVERIFY(guest.join("1",host->state()["id"].toString()));
        auto before=guest.state();host.reset();host=std::make_unique<LobbyCoordinator>();host->configure("1","Host",true);
        wireHost();host->setStorage(dir.filePath("host.json"));QCOMPARE(guest.state(),before);
        guest.reconnect();QVERIFY(guest.state()["generation"].toInteger()>before["generation"].toInteger());
        QCOMPARE(guest.state()["members"].toArray().size(),2);
    }
    void removedAndClosedRoomsCannotReadmitARecoveredSnapshot() {
        for(bool close:{false,true}) {
            QTemporaryDir dir;LobbyCoordinator host;auto guest=std::make_unique<LobbyCoordinator>();
            host.configure("1","Host",true);guest->configure("2","Guest",true);guest->setStorage(dir.filePath("guest.json"));
            connect(&host,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){if(guest)guest->receive("1","Host",p);});
            connect(guest.get(),&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
            QVERIFY(host.create("public"));QVERIFY(guest->join("1",host.state()["id"].toString()));guest.reset();
            if(close)host.leave();else QVERIFY(host.remove("2"));
            guest=std::make_unique<LobbyCoordinator>();guest->configure("2","Guest",true);
            connect(guest.get(),&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
            guest->setStorage(dir.filePath("guest.json"));QCOMPARE(guest->state(),QJsonObject{});
        }
    }
    void corruptOrOtherAccountCheckpointsDoNotCreateMembership() {
        QTemporaryDir dir;const auto file=dir.filePath("room.json");
        {LobbyCoordinator host;host.configure("1","Host",true);host.setStorage(file);QVERIFY(host.create());}
        LobbyCoordinator other;other.configure("2","Guest",true);other.setStorage(file);QVERIFY(other.state().isEmpty());
        QFile input(file);QVERIFY(input.open(QIODevice::ReadOnly));const auto original=input.readAll();input.close();
        other.leave();QVERIFY(input.open(QIODevice::ReadOnly));QCOMPARE(input.readAll(),original);input.close();
        QVERIFY(input.open(QIODevice::WriteOnly));input.write("{broken");input.close();
        LobbyCoordinator corrupt;corrupt.configure("1","Host",true);corrupt.setStorage(file);QVERIFY(corrupt.state().isEmpty());
    }
    void acknowledgedGameSurvivesRoomClosureUntilSafeRuntimeReturn() {
        LobbyCoordinator host;TestLobbyRuntime service;LobbyRuntime runtime(host);
        host.configure("1","Host",true);runtime.configure("1",true);runtime.registerService("netplay",&service);
        QVERIFY(host.create());QVERIFY(host.selectGame(game()));QVERIFY(runtime.prepare());service.preparation(true,{});
        QVERIFY(host.ready(true));const auto ticket=host.state()["ticket"].toString();service.opening(true,{});
        QCOMPARE(host.state()["phase"].toString(),"playing");host.leave();QCOMPARE(service.cancels,0);
        runtime.configure("1",false);QCOMPARE(service.cancels,0);QVERIFY(!runtime.available());
        QVERIFY(runtime.finished(ticket));QCOMPARE(service.cancels,1);
    }
    void closingLobbyWithdrawsTheDeferredInvitationAndIgnoresItsReplay() {
        LobbyCoordinator host,guest;host.configure("1","Host",true);guest.configure("2","Guest",true);
        QJsonObject original;
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){if(p["kind"]=="invite")original=p;guest.receive("1","Host",p);});
        QVERIFY(host.create());QVERIFY(host.invite("2"));QVERIFY(!guest.invitation().isEmpty());
        host.leave();QVERIFY(guest.invitation().isEmpty());
        guest.receive("1","Host",original);QVERIFY(guest.invitation().isEmpty());
    }
    void privateActivityRequestStillNeedsOwnerApproval() {
        LobbyCoordinator host,guest;host.configure("1","Host",true);guest.configure("2","Guest",true);
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){guest.receive("1","Host",p);});
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
        QVERIFY(host.create());QVERIFY(host.selectGame(game()));
        QVERIFY(guest.join("1",host.state()["id"].toString(),true));
        QCOMPARE(host.state()["members"].toArray().size(),1);QCOMPARE(host.requests().size(),1);
        QCOMPARE(guest.state()["phase"].toString(),QString("joining"));QVERIFY(host.approve("2"));
        QCOMPARE(guest.state()["members"].toArray().size(),2);
        QCOMPARE(guest.state()["members"].toArray()[1].toObject()["seat"].toInt(),2);
        QVERIFY(!guest.ready(true));
    }
    void runtimeServicesRequireLocalPreparationBothLaunchResultsAndKeepConversation() {
        LobbyCoordinator host,guest;TestLobbyRuntime hs,gs;LobbyRuntime hr(host),gr(guest);
        host.configure("1","Host",true);guest.configure("2","Guest",true);hr.configure("1",true);gr.configure("2",true);
        hr.registerService("netplay",&hs);gr.registerService("netplay",&gs);
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){guest.receive("1","Host",p);});
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
        QVERIFY(host.create("public"));QVERIFY(host.setConversation("123"));QVERIFY(guest.join("1",host.state()["id"].toString()));
        QVERIFY(host.selectGame(game()));QVERIFY(host.seat(1));QVERIFY(guest.seat(2));
        QVERIFY(hr.prepare());QVERIFY(gr.prepare());QVERIFY(!host.ready(true));
        hs.preparation(true,{});gs.preparation(true,{});QCOMPARE(hs.launches,0);
        QVERIFY(host.ready(true));QVERIFY(guest.ready(true));QCOMPARE(hs.launches,1);QCOMPARE(gs.launches,1);
        const auto ticket=host.state()["ticket"].toString();hs.opening(true,{});
        QCOMPARE(host.state()["phase"].toString(),"launching");gs.opening(true,{});
        QCOMPARE(host.state()["phase"].toString(),"playing");hs.opening(true,{});QCOMPARE(hs.launches,1);
        host.deliveryFailed("2");QCOMPARE(hs.cancels,0);QCOMPARE(gs.cancels,0);
        gr.configure("2",false);guest.configure("2","Guest",false);
        QCOMPARE(gs.cancels,0);QVERIFY(gr.finished(ticket));QCOMPARE(gs.cancels,1);
        QCOMPARE(host.state()["phase"].toString(),"playing");
        gr.configure("2",true);guest.configure("2","Guest",true);
        QCOMPARE(host.state()["phase"].toString(),"gathering");
        QCOMPARE(guest.state()["conversation"].toString(),"123");QVERIFY(!host.ready(true));QCOMPARE(host.state()["members"].toArray().size(),2);
    }
    void stalePreparationAndFailedLaunchNeverBecomeReadyOrPlaying() {
        LobbyCoordinator host;TestLobbyRuntime service;LobbyRuntime runtime(host);
        host.configure("1","Host",true);runtime.configure("1",true);runtime.registerService("netplay",&service);
        QVERIFY(host.create());QVERIFY(host.selectGame(game()));QVERIFY(host.seat(1));QVERIFY(runtime.prepare());
        auto late=service.preparation;QVERIFY(host.selectGame(game("other")));late(true,{});QVERIFY(!host.ready(true));QCOMPARE(service.cancels,1);
        QVERIFY(host.seat(1));QVERIFY(runtime.prepare());service.preparation(true,{});QVERIFY(host.ready(true));QCOMPARE(service.launches,1);
        service.opening(false,"Save protection refused launch");QCOMPARE(host.state()["phase"].toString(),"gathering");
        QVERIFY(!host.ready(true));QCOMPARE(runtime.status(),"Save protection refused launch");
        QVERIFY(runtime.prepare());service.preparation(true,{});runtime.registerService("netplay",nullptr);
        QVERIFY(!runtime.available());QVERIFY(!host.ready(true));
    }
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
        QCOMPARE(host.state()["phase"].toString(),"launching");QVERIFY(guest.acknowledgeLaunch(ticket,true));
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
    void conversationSurvivesGameAndReturnButNotNewRoom() {
        LobbySession l;QVERIFY(l.create("host","Host"));
        QVERIFY(!l.setConversation("guest","123"));QVERIFY(!l.setConversation("host","bad/id"));
        QVERIFY(l.setConversation("host","123"));QVERIFY(!l.setConversation("host","456"));
        QVERIFY(l.selectGame("host",game()));prepare(l,"host",1);
        QSignalSpy launch(&l,&LobbySession::launchRequested);QVERIFY(l.ready("host",l.generation(),true));
        const auto ticket=launch.first()[0].toString();QVERIFY(l.acknowledgeLaunch(ticket,true));QVERIFY(l.finishGame(ticket));
        QCOMPARE(l.state()["conversation"].toString(),"123");
        QVERIFY(l.close("host"));QVERIFY(l.create("host","Host"));QVERIFY(l.state()["conversation"].toString().isEmpty());
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
            const auto m=value.toObject();QCOMPARE(m["seat"].toInt(),m["id"]=="h"?1:m["id"]=="a"?2:0);QVERIFY(!m["ready"].toBool());QVERIFY(!m["prepared"].toBool());
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
