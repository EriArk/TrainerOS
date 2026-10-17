#include <QtTest>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QFile>
#include "features/social/LobbySession.h"
#include "features/social/LobbyCoordinator.h"
#include "features/social/LobbyRuntime.h"
#include "features/social/LobbyConnection.h"
#include "integrations/adventure/retroarch/RetroArchRemotePlay.h"
#ifdef Q_OS_LINUX
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "integrations/social/LobbyDirectory.h"
#include "integrations/social/LobbyWire.h"

namespace trainer {
class TestLobbyRuntime final : public LobbyRuntimeService {
public:
    Completion preparation,opening;
    int prepares=0,launches=0,cancels=0;
    bool live=false;int admissions=0,releases=0;
    bool supportsLive(const QJsonObject&) const override {return live;}
    bool admitLive(QString,QString,QString,int) override {++admissions;return live;}
    void releaseLive(QString,QString) override {++releases;}
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
    void explicitWatchAdmissionAndGameVotes() {
        LobbySession l;QVERIFY(l.create("1","Host","public"));
        QVERIFY(l.join("2","Player",1));QVERIFY(l.join("3","Viewer",1,true));
        QVERIFY(!l.spectate("1",true,l.generation()));
        auto choices=QJsonArray{QJsonObject{{"id","a"},{"title","Game A"},{"platform","nes"},{"mode","netplay"},{"seats",2}},QJsonObject{{"id","b"},{"title","Game B"},{"platform","snes"},{"mode","netplay"},{"seats",2}}};
        QVERIFY(l.proposeVote("1",choices));const auto id=l.state()["vote"].toObject()["id"].toString();
        QVERIFY(!l.vote("3",id,0));QVERIFY(!l.vote("unknown",id,0));QVERIFY(!l.vote("2","stale",0));QVERIFY(l.vote("2",id,1));
        QVERIFY(!l.confirmVote("2",id,1));QVERIFY(!l.confirmVote("1",id,0));QVERIFY(l.state()["game"].toObject()["id"].toString().isEmpty());
        QVERIFY(l.confirmVote("1",id,1));QCOMPARE(l.state()["game"].toObject()["id"].toString(),QString("b"));QVERIFY(!l.confirmVote("1",id,1));
        QVERIFY(l.proposeVote("1",choices));QVERIFY(l.vote("2",l.state()["vote"].toObject()["id"].toString(),0));QVERIFY(l.leave("2"));QVERIFY(l.state()["vote"].toObject()["votes"].toObject().isEmpty());
        QVERIFY(!l.proposeVote("1",QJsonArray{choices[0],choices[0]}));
    }

    void idleRuntimeQueriesNeverCreateAPhantomRoom() {
        LobbyCoordinator c;c.configure("1","Mika",true);
        QVERIFY(c.state().isEmpty());
        QVERIFY(!c.ready(false));
        QVERIFY(!c.requestPlay());
        QVERIFY(!c.requestLive("00000000-0000-4000-8000-000000000001"));
        QVERIFY(!c.finishGame("stale-ticket"));
        QVERIFY(c.state().isEmpty());
        auto* room=c.createRoom();QVERIFY(room);
        QCOMPARE(c.rooms().size(),1);
    }
    void multipleMembershipsKeepOneRuntimeAndRecoverIndependently() {
        QTemporaryDir dir;const auto file=dir.filePath("rooms.json");QString first,second;
        {
            LobbyCoordinator c;c.configure("1","Mika",true);c.setStorage(file);QVERIFY(c.create());
            first=c.state()["id"].toString();QVERIFY(c.activeRoom()->session_.approve("1","2","Ren"));QVERIFY(c.selectGame(game()));
            auto* r=c.createRoom();QVERIFY(r);second=r->state()["id"].toString();QVERIFY(first!=second);
            QCOMPARE(c.state()["id"].toString(),first);QVERIFY(!r->prepared(r->state()["generation"].toInteger(),true));
            QVERIFY(c.prepared(c.state()["generation"].toInteger(),true));QVERIFY(c.ready(true));
            QVERIFY(!c.activateRoom(second));QVERIFY(c.ready(false));QVERIFY(c.activateRoom(second));
            QCOMPARE(c.state()["id"].toString(),second);QVERIFY(c.room(first));
        }
        {
            LobbyCoordinator c;c.configure("1","Mika",true);c.setStorage(file);
            QVERIFY(c.room(first));QVERIFY(c.room(second));QCOMPARE(c.state()["id"].toString(),second);
            c.room(first)->forgetMembership();QVERIFY(!c.room(first));QVERIFY(c.room(second));
        }
        LobbyCoordinator c;c.configure("1","Mika",true);c.setStorage(file);
        QVERIFY(!c.room(first));QVERIFY(c.room(second)); // legacy checkpoint must not resurrect a departed room
    }
    void providerRemovalAndOwnershipTransferPreserveOtherRooms() {
        LobbyCoordinator host,guest;host.configure("1","Mika",true);guest.configure("2","Ren",true);
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString peer,QJsonObject p){if(peer=="2")guest.receive("1","Mika",p);});
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString peer,QJsonObject p){if(peer=="1")host.receive("2","Ren",p);});
        QVERIFY(host.create());QVERIFY(host.setConversation("300"));QVERIFY(host.invite("2"));guest.answer(true);
        const auto id=host.state()["id"].toString();QVERIFY(guest.room(id));
        const QJsonArray both{QJsonObject{{"id","1"}},QJsonObject{{"id","2"}}};
        host.activeRoom()->reconcileProvider("300","1",QJsonArray{QJsonObject{{"id","1"}}});
        QCOMPARE(host.state()["members"].toArray().size(),2); // a pending provider add must not evict accepted peers
        host.activeRoom()->reconcileProvider("300","1",both);guest.activeRoom()->reconcileProvider("300","1",both);
        auto* scheduled=guest.createRoom();QVERIFY(scheduled);const auto next=scheduled->state()["id"].toString();
        host.activeRoom()->forgetMembership();
        guest.activeRoom()->reconcileProvider("300","2",QJsonArray{QJsonObject{{"id","2"}}});
        QVERIFY(guest.hosting());QCOMPARE(guest.state()["owner"].toString(),QString("2"));
        QCOMPARE(guest.state()["members"].toArray().size(),1);QVERIFY(guest.room(next));
        guest.activeRoom()->reconcileProvider("300","2",QJsonArray{});QVERIFY(!guest.room(id));QVERIFY(guest.room(next));
    }

    void gatheringDetailsAreHostOwnedAndNeverLaunch() {
        LobbySession room;QSignalSpy launches(&room,&LobbySession::launchRequested);
        QVERIFY(room.create("1","Host","public"));QVERIFY(room.join("2","Guest",QDateTime::currentSecsSinceEpoch()));
        const auto at=QDateTime::currentSecsSinceEpoch()+7200;
        QVERIFY(!room.setDetails("2","Changed",at));QVERIFY(!room.setDetails("1","",at));
        QVERIFY(!room.setDetails("1","Friday",1));QVERIFY(room.setDetails("1","Friday racing",at));
        QCOMPARE(room.state()["startsAt"].toInteger(),at);QCOMPARE(room.state()["title"].toString(),QString("Friday racing"));
        QCOMPARE(launches.size(),0);LobbySession restored;QVERIFY(restored.restore(room.state()));
        QCOMPARE(restored.state()["startsAt"].toInteger(),at);QCOMPARE(restored.state()["title"],room.state()["title"]);
        QVERIFY(room.setDetails("1","Friday racing",0));QCOMPARE(room.state()["startsAt"].toInteger(),0);
    }
    void runtimeSignalingBindsPeerTicketGenerationAndKeepsSecretsOutOfState() {
        LobbyCoordinator host,guest;host.configure("1","Host",true);guest.configure("2","Guest",true);
        QJsonObject captured;
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString peer,QJsonObject p){if(peer=="2")guest.receive("1","Host",p);});
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){if(p["kind"]=="runtime-signal")captured=p;host.receive("2","Guest",p);});
        QVERIFY(host.create("public"));auto selected=game();selected.mode="remoteplay.v1";QVERIFY(host.selectGame(selected));
        QVERIFY(guest.join("1",host.state()["id"].toString()));
        const auto generation=host.state()["generation"].toInteger();
        QVERIFY(host.prepared(generation,true));QVERIFY(guest.prepared(generation,true));QVERIFY(host.ready(true));QVERIFY(guest.ready(true));
        const auto ticket=host.state()["ticket"].toString(),connection=QUuid::createUuid().toString();
        QSignalSpy received(&host,&LobbyCoordinator::runtimeSignal);
        QVERIFY(!guest.sendRuntimeSignal("1","old",connection,{{"description","private-offer"}}));
        QVERIFY(!guest.sendRuntimeSignal("3",ticket,connection,{{"description","private-offer"}}));
        QVERIFY(guest.sendRuntimeSignal("1",ticket,connection,{{"description","private-offer"}}));QCOMPARE(received.size(),1);
        QVERIFY(!QJsonDocument(host.state()).toJson().contains("private-offer"));
        host.receive("2","Guest",captured);QCOMPARE(received.size(),1);
        auto wrong=captured;wrong["serial"]=qint64(captured["serial"].toInteger()+1);wrong["generation"]=qint64(generation+1);
        host.receive("2","Guest",wrong);QCOMPARE(received.size(),1);
        ++guest.activeRoom()->serial_; // The injected packet consumed this sender's next serial.
        QVERIFY(host.acknowledgeLaunch(ticket,true));QVERIFY(guest.acknowledgeLaunch(ticket,true));
        QVERIFY(guest.finishGame(ticket));QCOMPARE(host.state()["phase"].toString(),"playing");QCOMPARE(host.state()["ticket"].toString(),ticket);
        QVERIFY(!guest.sendRuntimeSignal("1",ticket,connection,{{"description","late"}}));
        guest.activeRoom()->send("1","game-finished",{{"ticket",ticket}});QCOMPARE(host.state()["phase"].toString(),"playing");
        QVERIFY(host.finishGame(ticket));QCOMPARE(host.state()["phase"].toString(),"gathering");
    }
    void realLobbyConnectionOwnsHelperAndRevokesOnGuestRemoval_data() {
        QTest::addColumn<bool>("badCandidates");QTest::addColumn<bool>("rejoin");
        QTest::newRow("guest-removed")<<false<<false;QTest::newRow("invalid-address-batch")<<true<<false;
        QTest::newRow("guest-rejoined")<<false<<true;
    }
    void realLobbyConnectionOwnsHelperAndRevokesOnGuestRemoval() {
        QFETCH(bool,badCandidates);QFETCH(bool,rejoin);
        const auto helper=qEnvironmentVariable("TRAINEROS_STREAM_TRANSPORT");if(helper.isEmpty())QSKIP("Real Linux transport supplied by the ARM integration check");
        LobbyCoordinator host,guest;host.configure("1","Host",true);guest.configure("2","Guest",true);
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString peer,QJsonObject p){if(peer=="2")guest.receive("1","Host",p);});
        QString connectionIdentity;
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){
            const auto payload=p["data"].toObject()["payload"].toObject();if(payload["kind"]=="description")connectionIdentity=payload["identity"].toString();
            host.receive("2","Guest",p);
        });
        LobbyConnection hc(host),gc(guest);QSignalSpy hr(&hc,&LobbyConnection::connected),gr(&gc,&LobbyConnection::connected),ended(&hc,&LobbyConnection::ended),media(&gc,&LobbyConnection::mediaReady);
        connect(&host,&LobbyCoordinator::launchRequested,&hc,[&](QString ticket,QJsonObject){QVERIFY(hc.start("1",ticket,helper));});
        connect(&guest,&LobbyCoordinator::launchRequested,&gc,[&](QString ticket,QJsonObject){QVERIFY(gc.start("2",ticket,helper));});
        QVERIFY(host.create("public"));auto selected=game();selected.mode="remoteplay.v1";QVERIFY(host.selectGame(selected));
        QVERIFY(guest.join("1",host.state()["id"].toString()));const auto gen=host.state()["generation"].toInteger();
        QVERIFY(host.prepared(gen,true));QVERIFY(guest.prepared(gen,true));QVERIFY(host.ready(true));QVERIFY(guest.ready(true));
        QTRY_COMPARE_WITH_TIMEOUT(hr.size(),1,15000);QTRY_COMPARE_WITH_TIMEOUT(gr.size(),1,15000);QCOMPARE(hc.id(),gc.id());
        const auto hostFile=hr[0][0].toString(),guestFile=gr[0][0].toString();QFile file(hostFile);QVERIFY(file.open(QIODevice::ReadOnly));
        const auto config=file.readAll();QVERIFY(config.contains("mode=game\n"));QVERIFY(config.contains("seconds=0\n"));file.close();
        const auto ticket=host.state()["ticket"].toString();QVERIFY(host.acknowledgeLaunch(ticket,true));QVERIFY(guest.acknowledgeLaunch(ticket,true));
        QVERIFY(hc.announceMedia());QCOMPARE(media.size(),1);
        if(rejoin) {
            const auto oldConnection=hc.id();
            QVERIFY(!host.releaseRemotePlayer("1",ticket,{})); // P1 never belongs to the stream.
            QVERIFY(!host.releaseRemotePlayer("2","old",{}));
            QVERIFY(!guest.releaseRemotePlayer("2",ticket,{}));
            QVERIFY(!gc.start("2",ticket,helper));
            QVERIFY(host.releaseRemotePlayer("2",ticket,{}));
            QTRY_VERIFY(!hc.active()&&!gc.active());QVERIFY(!QFile::exists(hostFile));QVERIFY(!QFile::exists(guestFile));
            QVERIFY(host.setLiveJoining(true));const auto admission=QUuid::createUuid().toString();
            QVERIFY(guest.requestLive(admission));QVERIFY(host.admitLive("2",admission,2));
            QVERIFY(hc.start("1",ticket,helper));
            QVERIFY(guest.sendRuntimeSignal("1",ticket,oldConnection,{{"kind","connect"},{"admission",""}}));
            QVERIFY(hc.id().isEmpty());QVERIFY(gc.start("2",ticket,helper));
            QTRY_COMPARE_WITH_TIMEOUT(hr.size(),2,15000);QTRY_COMPARE_WITH_TIMEOUT(gr.size(),2,15000);
            QVERIFY(hc.id()!=oldConnection);QCOMPARE(hc.id(),gc.id());
            QVERIFY(!host.releaseRemotePlayer("2",ticket,{}));QVERIFY(!host.releaseRemotePlayer("2",ticket,QUuid::createUuid().toString()));
            QVERIFY(hc.active());QCOMPARE(host.state()["ticket"].toString(),ticket);
            QVERIFY(hc.announceMedia());QCOMPARE(media.size(),2);ended.clear();
        }
        if(badCandidates) {
            QJsonArray bad;for(int i=0;i<33;++i)bad.append(QJsonObject{});
            QVERIFY(guest.sendRuntimeSignal("1",ticket,hc.id(),{{"kind","candidates"},{"identity","wrong"},{"candidates",bad}}));
            QVERIFY(hc.active());QCOMPARE(ended.size(),0);
            QVERIFY(!connectionIdentity.isEmpty());
            QVERIFY(guest.sendRuntimeSignal("1",ticket,hc.id(),{{"kind","candidates"},{"identity",connectionIdentity},{"candidates",bad}}));
        } else QVERIFY(host.remove("2"));
        QCOMPARE(ended.size(),1);QVERIFY(!hc.active());QTRY_VERIFY(!gc.active());
        QVERIFY(!QFile::exists(hostFile));QVERIFY(!QFile::exists(guestFile));QCOMPARE(host.state()["phase"].toString(),"playing");
    }
    void remoteInputRequiresPlayingRemoteModeAndAssignedGuest() {
        LobbyCoordinator host;host.configure("1","Host",true);QVERIFY(host.create("public"));
        auto selected=game();selected.mode="remoteplay.v1";QVERIFY(host.selectGame(selected));
        QVERIFY(host.activeRoom()->session_.join("2","Guest",100));
        LobbyInput input(host);const auto connection=QUuid::createUuid().toString();
        QVERIFY(!input.authorize("2",connection,"missing",2));
        for(const auto& id:{QString("1"),QString("2")})QVERIFY(host.activeRoom()->session_.prepared(id,host.activeRoom()->session_.generation(),true));
        QVERIFY(host.activeRoom()->session_.ready("1",host.activeRoom()->session_.generation(),true));QVERIFY(host.activeRoom()->session_.ready("2",host.activeRoom()->session_.generation(),true));
        const auto ticket=host.state()["ticket"].toString();
        QVERIFY(!input.authorize("2",connection,ticket,2)); // No launch acknowledgement yet.
        QVERIFY(host.activeRoom()->session_.acknowledgeLaunch(ticket,true));
        QVERIFY(host.activeRoom()->session_.join("3","Viewer",100));
        QVERIFY(!input.authorize("3",connection,ticket,2));
        QVERIFY(!input.authorize("1",connection,ticket,1));
        QVERIFY(!input.authorize("2",connection,ticket,3));
        QVERIFY(!input.authorize("2",connection,"stale",2));
        QVERIFY(input.authorize("2",connection,ticket,2));
        QVERIFY(!input.authorize("2",QUuid::createUuid().toString(),ticket,2));
        input.release("stale-connection");QVERIFY(input.active());
        QVERIFY(host.activeRoom()->session_.leave("3"));QVERIFY(input.active()); // Viewers do not own the input grant.
        QVERIFY(host.activeRoom()->session_.connected("2",false));QVERIFY(!input.active());
        QVERIFY(host.activeRoom()->session_.connected("2",true));QVERIFY(!input.active()); // No implicit regrant.
        QVERIFY(input.authorize("2",QUuid::createUuid().toString(),ticket,2));
        QVERIFY(host.remove("2"));QVERIFY(!input.active());
        QCOMPARE(host.state()["phase"].toString(),"playing");
    }
    void remoteInputRevokesOnGameEndAndAccountSwitch() {
        LobbyCoordinator host;host.configure("1","Host",true);QVERIFY(host.create("public"));
        auto selected=game();selected.mode="remoteplay.v1";QVERIFY(host.selectGame(selected));
        QVERIFY(host.activeRoom()->session_.join("2","Guest",100));
        for(const auto& id:{QString("1"),QString("2")})QVERIFY(host.activeRoom()->session_.prepared(id,host.activeRoom()->session_.generation(),true));
        host.activeRoom()->session_.ready("1",host.activeRoom()->session_.generation(),true);host.activeRoom()->session_.ready("2",host.activeRoom()->session_.generation(),true);
        const auto ticket=host.state()["ticket"].toString();host.activeRoom()->session_.acknowledgeLaunch(ticket,true);
        LobbyInput input(host);const auto connection=QUuid::createUuid().toString();
        QVERIFY(input.authorize("2",connection,ticket,2));
#ifdef Q_OS_LINUX
        const auto fd=input.takeDescriptor(connection);QVERIFY(fd>=3);QCOMPARE(input.takeDescriptor(connection),-1);
        char packet[10];QCOMPARE(recv(fd,packet,sizeof(packet),0),ssize_t(9));QCOMPARE(packet[0],'1');
#endif
        QVERIFY(host.activeRoom()->session_.finishGame(ticket));QVERIFY(!input.active());
#ifdef Q_OS_LINUX
        QCOMPARE(recv(fd,packet,sizeof(packet),0),ssize_t(0));close(fd);
#endif
        QVERIFY(!input.authorize("2",connection,ticket,2));
        for(const auto& id:{QString("1"),QString("2")})QVERIFY(host.activeRoom()->session_.prepared(id,host.activeRoom()->session_.generation(),true));
        host.activeRoom()->session_.ready("1",host.activeRoom()->session_.generation(),true);host.activeRoom()->session_.ready("2",host.activeRoom()->session_.generation(),true);
        const auto next=host.state()["ticket"].toString();host.activeRoom()->session_.acknowledgeLaunch(next,true);
        QVERIFY(input.authorize("2",QUuid::createUuid().toString(),next,2));
        host.configure("other","Other",true);QVERIFY(!input.active());
    }
    void nativeNetplayNeverGrantsRemotePad() {
        LobbyCoordinator host;host.configure("1","Host",true);QVERIFY(host.create("public"));QVERIFY(host.selectGame(game()));
        QVERIFY(host.activeRoom()->session_.join("2","Guest",100));
        for(const auto& id:{QString("1"),QString("2")})QVERIFY(host.activeRoom()->session_.prepared(id,host.activeRoom()->session_.generation(),true));
        host.activeRoom()->session_.ready("1",host.activeRoom()->session_.generation(),true);host.activeRoom()->session_.ready("2",host.activeRoom()->session_.generation(),true);
        const auto ticket=host.state()["ticket"].toString();host.activeRoom()->session_.acknowledgeLaunch(ticket,true);
        LobbyInput input(host);QVERIFY(!input.authorize("2",QUuid::createUuid().toString(),ticket,2));
    }
    void remotePortRecipeRejectsAmbiguousIdentitiesAndPreservesSavePaths() {
        using namespace retroarch;
        for(const auto& name:{QString(),QString("TrainerOS Remote P2"),QString("045e:02ea Xbox"),QString("Pad\ninput=bad"),QString("Pad\""),QString("Pad\\other")})
            QVERIFY(remotePlayPortSettings({name}).isEmpty());
        const auto recipe=remotePlayPortSettings({"Xbox 360 Controller"});QVERIFY(!recipe.isEmpty());
        QVERIFY(recipe.contains("input_player2_device_reservation_type = \"2\""));
        QVERIFY(recipe.contains("input_player2_reserved_device = \"TrainerOS Remote P2\""));
        QVERIFY(recipe.contains("input_hotkey_follows_player1 = \"true\""));
        QVERIFY(recipe.contains("input_max_users = \"16\""));
        QVERIFY(recipe.contains("input_libretro_device_p3 = \"0\""));
        QVERIFY(!recipe.contains("savefile_directory"));QVERIFY(!recipe.contains("savestate_directory"));
        ProcessCommand cmd;cmd.arguments={"--config","unchanged","game"};RetroArchInstallation install;
        std::atomic_bool cancel=false;const auto before=cmd.arguments;
        QVERIFY(!prepareRemotePlay(cmd,install,{"Xbox 360 Controller"},cancel).isEmpty());QCOMPARE(cmd.arguments,before);
        QVERIFY(!cmd.runtimeControls.contains("remotePlay"));
    }
    void lostLiveAdmissionRetriesOnceAndOtherSpectatorsDoNotCancelOpening() {
        LobbyCoordinator host,guest;host.configure("1","Host",true);guest.configure("2","Guest",true);
        bool drop=false;
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString peer,QJsonObject p){
            if(peer!="2")return;
            if(drop&&p["kind"]=="state"&&p["data"].toObject()["members"].toArray().last().toObject()["admission"].toString().size()) {drop=false;return;}
            guest.receive("1","Host",p);
        });
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
        TestLobbyRuntime hs,gs;hs.live=gs.live=true;LobbyRuntime hr(host),gr(guest);
        hr.configure("1",true);gr.configure("2",true);hr.registerService("netplay",&hs);gr.registerService("netplay",&gs);
        QVERIFY(host.create("public"));QVERIFY(host.selectGame(game()));QVERIFY(hr.setLiveJoining(true));
        QVERIFY(hr.prepare());hs.preparation(true,{});host.ready(true);hs.opening(true,{});
        QVERIFY(guest.join("1",host.state()["id"].toString()));QVERIFY(gr.joinLive());drop=true;gs.preparation(true,{});
        QCOMPARE(gs.launches,0);QVERIFY(gr.busy());QVERIFY(!gr.joinLive());
        guest.activeRoom()->tick();QCOMPARE(hs.admissions,1);QCOMPARE(gs.launches,1);
        QVERIFY(host.activeRoom()->session_.join("3","Spectator",QDateTime::currentSecsSinceEpoch()));
        gs.opening(true,{});QVERIFY(!gr.busy());QCOMPARE(hs.launches,1);
        QVERIFY(gr.finished(host.state()["ticket"].toString()));QVERIFY(gr.liveAvailable());
    }
    void liveJoinKeepsHostLaunchAndReturnsGuestToSpectator() {
        LobbyCoordinator host,guest;host.configure("1","Host",true);guest.configure("2","Guest",true);
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){guest.receive("1","Host",p);});
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
        TestLobbyRuntime hs,gs;hs.live=gs.live=true;
        LobbyRuntime hr(host),gr(guest);hr.configure("1",true);gr.configure("2",true);
        hr.registerService("netplay",&hs);gr.registerService("netplay",&gs);
        QVERIFY(host.create("public"));QVERIFY(host.selectGame(game()));QVERIFY(hr.setLiveJoining(true));
        QVERIFY(hr.prepare());hs.preparation(true,{});QVERIFY(host.ready(true));hs.opening(true,{});
        const auto ticket=host.state()["ticket"].toString();QCOMPARE(host.state()["phase"].toString(),"playing");
        QVERIFY(guest.join("1",host.state()["id"].toString()));QVERIFY(gr.liveAvailable());
        QVERIFY(gr.joinLive());QCOMPARE(gs.launches,0);gs.preparation(true,{});
        QCOMPARE(hs.admissions,1);QCOMPARE(gs.launches,1);gs.opening(true,{});
        QCOMPARE(hs.launches,1);QCOMPARE(hs.cancels,0);QCOMPARE(host.state()["ticket"].toString(),ticket);
        guest.activeRoom()->send("1","game-finished",{{"ticket",ticket}}); // An obsolete report cannot end the host's game.
        QCOMPARE(host.state()["phase"].toString(),"playing");
        QVERIFY(!gr.liveAvailable());QVERIFY(gr.finished(ticket));
        QCOMPARE(host.state()["phase"].toString(),"playing");QCOMPARE(hs.releases,1);QVERIFY(gr.liveAvailable());
        QVERIFY(gr.joinLive());gs.preparation(true,{});gs.opening(true,{});
        QCOMPARE(gs.launches,2);QCOMPARE(hs.launches,1);QVERIFY(gr.finished(ticket));
        QVERIFY(hr.setLiveJoining(false));QVERIFY(!gr.liveAvailable());
    }
    void liveSeatAuthorizationAndStaleRelease() {
        LobbySession l;QVERIFY(l.create("h","Host","public"));QVERIFY(l.selectGame("h",game()));
        prepare(l,"h",1);QVERIFY(l.ready("h",l.generation(),true));const auto ticket=l.state()["ticket"].toString();
        QVERIFY(l.acknowledgeLaunch(ticket,true));QVERIFY(l.join("g","Guest",100));QVERIFY(l.join("v","Viewer",100));
        QCOMPARE(l.liveSeat("g"),0);QVERIFY(!l.setLiveJoining("g",true));QVERIFY(l.setLiveJoining("h",true));
        const auto token=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QCOMPARE(l.liveSeat("g"),2);QVERIFY(!l.admitLive("g",token,1));QVERIFY(l.admitLive("g",token,2));
        QCOMPARE(l.liveSeat("v"),0);QVERIFY(!l.releaseLive("g","old",token));QVERIFY(!l.releaseLive("g",ticket,"old"));
        QVERIFY(l.setLiveJoining("h",false));QVERIFY(l.releaseLive("g",ticket,token));QCOMPARE(l.liveSeat("v"),0);
        QCOMPARE(l.state()["phase"].toString(),"playing");QCOMPARE(l.state()["ticket"].toString(),ticket);
        LobbySession restored;QVERIFY(restored.restore(l.state()));QVERIFY(!restored.state()["liveJoining"].toBool());
    }
    void revokedLiveConsentDuringGuestPreparationDoesNotLaunch() {
        LobbyCoordinator host,guest;host.configure("1","Host",true);guest.configure("2","Guest",true);
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){guest.receive("1","Host",p);});
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
        TestLobbyRuntime hs,gs;hs.live=gs.live=true;LobbyRuntime hr(host),gr(guest);
        hr.configure("1",true);gr.configure("2",true);hr.registerService("netplay",&hs);gr.registerService("netplay",&gs);
        QVERIFY(host.create("public"));QVERIFY(host.selectGame(game()));QVERIFY(hr.setLiveJoining(true));
        QVERIFY(hr.prepare());hs.preparation(true,{});host.ready(true);hs.opening(true,{});
        QVERIFY(guest.join("1",host.state()["id"].toString()));QVERIFY(gr.joinLive());
        QVERIFY(hr.setLiveJoining(false));gs.preparation(true,{});
        QCOMPARE(gs.launches,0);QCOMPARE(hs.admissions,0);QCOMPARE(hs.launches,1);
        QVERIFY(!gr.busy());QCOMPARE(host.state()["phase"].toString(),"playing");
    }
    void spectatorArrivalLossAndDeparturePreserveRunningGame() {
        LobbySession l;QVERIFY(l.create("h","Host","public"));QVERIFY(l.selectGame("h",game("solo",1)));
        prepare(l,"h",1);QVERIFY(l.ready("h",l.generation(),true));
        const auto ticket=l.state()["ticket"].toString();QVERIFY(l.acknowledgeLaunch(ticket,true));
        const auto generation=l.generation();
        QVERIFY(l.join("viewer","Viewer",100));
        const auto members=l.state()["members"].toArray();
        const auto viewer=std::find_if(members.begin(),members.end(),[](const QJsonValue& v){return v.toObject()["id"]=="viewer";});
        QVERIFY(viewer!=members.end());QVERIFY(viewer->toObject()["spectator"].toBool());QCOMPARE(viewer->toObject()["seat"].toInt(),0);
        QVERIFY(!l.claimSeat("viewer",1,generation));QVERIFY(!l.ready("viewer",generation,true));
        QVERIFY(l.connected("viewer",false));QVERIFY(l.connected("viewer",true));QVERIFY(l.leave("viewer"));
        QCOMPARE(l.generation(),generation);QCOMPARE(l.state()["ticket"].toString(),ticket);QCOMPARE(l.state()["phase"].toString(),"playing");
        QVERIFY(l.state()["members"].toArray().first().toObject()["ready"].toBool());
    }
    void lostInviteAndReceiptRetryWithoutExtendingConsentOrReopeningDeclinedOffer() {
        LobbyCoordinator host,guest;host.configure("1","Host",true);guest.configure("2","Guest",true);
        int invitations=0;bool dropReceipt=true;
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){
            if(p["kind"]=="invite"&&++invitations==1)return;guest.receive("1","Host",p);
        });
        connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){if(!dropReceipt)host.receive("2","Guest",p);});
        QVERIFY(host.create());QVERIFY(host.invite("2"));QVERIFY(guest.invitation().isEmpty());
        host.activeRoom()->tick();QVERIFY(!guest.invitation().isEmpty());const auto expiry=guest.invitation()["expires"];
        host.activeRoom()->tick();QCOMPARE(guest.invitation()["expires"],expiry);guest.answer(false);
        dropReceipt=false;host.activeRoom()->tick();QVERIFY(guest.invitation().isEmpty());const auto sent=invitations;
        host.activeRoom()->tick();QCOMPARE(invitations,sent);QVERIFY(host.activeRoom()->pendingInvites_.isEmpty());
        QVERIFY(host.invite("2"));QVERIFY(!guest.invitation().isEmpty()); // Explicit new offer.
    }
    void directoryRevisionAndCategorySurviveRestartAndLeaveEndsIdentity() {
        QTemporaryDir dir;const auto file=dir.filePath("room.json");qint64 last=0;
        {LobbyCoordinator host;host.configure("1","Host",true);host.setStorage(file);QVERIFY(host.create("public"));
            host.setPublicationTopic("racing");for(int i=0;i<5;++i)last=host.nextPublicationRevision();}
        {LobbyCoordinator host;host.configure("1","Host",true);host.setStorage(file);QCOMPARE(host.publicationTopic(),"racing");
            QVERIFY(host.nextPublicationRevision()>last);host.leave();last=host.nextPublicationRevision();}
        LobbyCoordinator host;host.configure("1","Host",true);host.setStorage(file);QVERIFY(host.state().isEmpty());
        QVERIFY(host.create());QVERIFY(host.nextPublicationRevision()>0);
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
        QFile input(file+".index");QVERIFY(input.open(QIODevice::ReadOnly));const auto original=input.readAll();input.close();
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
