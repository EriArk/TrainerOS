#include "features/adventure/RuntimeMultiplayer.h"
#include "features/adventure/LobbyImportController.h"
#include "features/adventure/LaunchPresentation.h"
#include "core/navigation/LaunchPreparation.h"
#include "features/adventure/RemotePlayRuntime.h"
#include "features/adventure/AdventureExitPresentation.h"
#include "features/social/SocialController.h"
#include "integrations/social/FluxerSession.h"
#include "platform/input/AdventureOverlayService.h"
#include "core/navigation/AdventureLaunchController.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QDir>
#include <QPromise>

namespace trainer {
class RuntimeMultiplayerTests final : public QObject {
    Q_OBJECT
    QTemporaryDir settings_;
    struct Library final : LibraryRepository {
        std::function<void()> inspect;
        QList<AdventureRegistration> records;
        int writes=0;
        bool editable() const override {return true;}
        void saveAdventureAsync(const AdventureRegistration& r,QObject*,std::function<void(LibraryWriteResult)> done) override {++writes;records.append(r);done({true,{}});}

        QList<AdventureRegistration> registrations() const override { return records; }
        QList<World> worlds() const override { return {}; }
        QList<Adventure> adventures() const override { return {}; }
        QList<ResumePoint> resumePoints() const override { return {}; }
        HomeSnapshot home() const override { return {}; }
        std::optional<AdventureRegistration> registration(const QString& id) const override {
            if(inspect)inspect();for(const auto& r:records)if(r.adventure.id==id)return r;return {};
        }
    };
    struct Fixture {
        QTemporaryDir dir;
        Library library;
        RetroArchAdapter retro{library,{}};
        StandaloneAdapter psp{"ppsspp",library,{}},dolphin{"dolphin",library,{}};
        SocialController social;
        ProcessService process;
        AdventureLaunchController lifecycle{process};
        AdventureExitPresentation view{lifecycle.exitController()};
        RuntimeMultiplayer runtime{library,retro,psp,dolphin,social,process,lifecycle,view};
        Fixture() {
            QObject::connect(&lifecycle,&AdventureLaunchController::checkpointRequested,&lifecycle,
                [this](quint64 token,const QJsonObject&){lifecycle.checkpointCompleted(token,{});});
            QObject::connect(&view,&AdventureExitPresentation::menuCaptureRequested,&view,
                [this](quint64 token){view.menuCaptureCompleted(token,{});});
            QObject::connect(&lifecycle.exitController(),&AdventureExitController::captureRequested,&view,
                [this](quint64 token){lifecycle.exitController().captureCompleted(token,{},"No test frame");});
            QObject::connect(&lifecycle.exitController(),&AdventureExitController::gracefulExitRequested,&view,
                [this](quint64){QFile f(dir.filePath("control"));if(f.open(QIODevice::WriteOnly))f.write("exit");});
        }
        bool start(bool finalizerFails=false,QString game={},bool hotseat=false,QString kind="retroarch",QJsonObject netplay={}) {
#ifdef Q_OS_WIN
            const auto name="trainer_process_probe.exe";
#else
            const auto name="trainer_process_probe";
#endif
            ProcessCommand command{QDir(QCoreApplication::applicationDirPath()).filePath(name),
                {"controlled",dir.filePath("control"),dir.filePath("pid")},{}};
            if(!game.isEmpty())command.runtimeControls={{"game",game}};
            if(!netplay.isEmpty()) {command.runtimeControls["netplay"]=netplay.toVariantMap();command.runtimeControls["temporaryProgress"]=true;}
            if(hotseat) {
                command.arguments[0]="stdin-control";
                command.runtimeControls={{"game",game},{"kind",kind},{"netplay",true},{"netplayHost",runtime.host_},
                    {"hotseat",kind=="dolphin"?"dolphin-golf-p1-v1":"retroarch-p1-v1"}};
            }
            if(finalizerFails)command.finalize=[](const ProcessOutcome&){return QString("Save recovery required");};
            return lifecycle.launch(command,{},"ordinary",AdventureSavePolicy::ManualConfirm);
        }
    };
    void requestRestart(Fixture& f,QJsonObject profile={{"transport","netpacket"}}) {
        f.lifecycle.exitController().setAvailable(true);
        QVERIFY(f.view.requestMenu());
        f.runtime.active_=true;f.runtime.host_=true;
        f.runtime.game_="missing-after-close";
        f.runtime.descriptor_=profile;
        f.runtime.deadline_=QDateTime::currentSecsSinceEpoch()+1;
        f.runtime.timer_.start();
        f.runtime.prepareHost();
        QCOMPARE(f.lifecycle.exitController().phase(),AdventureExitController::Phase::Confirming);
    }
    QJsonObject inviteWhilePlaying(Fixture& f,GameParty& host) {
        const QJsonObject game{{"id","runtime.test"},{"label","Another game"},{"content","exact"},{"players",2}};
        f.runtime.allowed_=true;f.runtime.games_={{"ordinary",game}};f.runtime.update();
        host.configure("host",{game},game,true);
        connect(&host,&GameParty::outgoing,&f.runtime,[&f](QString,QJsonObject packet){f.runtime.party_.receive("host","host",packet);});
        connect(&f.runtime.party_,&GameParty::outgoing,&host,[&host](QString,QJsonObject packet){host.receive("guest","guest",packet);});
        host.invite("guest");return game;
    }
    void readyMenu(Fixture& f) {
        f.lifecycle.exitController().setAvailable(true);QVERIFY(f.view.requestMenu());
        f.view.setInputIsolated(true);f.view.setWindowFocused(true);
        f.view.updateInput(f.view.inputGeneration(),{true,true});QVERIFY(f.view.ready());
    }
    void finishOrdinary(Fixture& f) {
        QFile stop(f.dir.filePath("control"));QVERIFY(stop.open(QIODevice::WriteOnly));stop.write("exit");stop.close();
        QTRY_VERIFY(!f.lifecycle.active());
    }
private slots:
    void liveJoinCapabilityBelongsToTheLocalInstallation() {
        Fixture f;f.runtime.allowed_=true;
        const QJsonObject local{{"id","runtime.retroarch.nes.fceumm.v1"},{"players",2},
            {"runtime","8d57b47c88d416c05145187c8c906b7ab0013edb7e45af69d9b6bb841aea4a24"},
            {"core","1b13b00d4680394dad8000d5175f97be727107e0945bc9b412da91d70c07b267"}};
        auto other=local;other["runtime"]="other-architecture";other["core"]="other-core-binary";
        f.runtime.games_={{"nes",local}};QVERIFY(f.runtime.supportsLive(other));
        QVERIFY(f.runtime.supportsRejoin(other));auto excluded=other;excluded["lateJoin"]=false;QVERIFY(!f.runtime.supportsRejoin(excluded));
        excluded=other;excluded["transport"]="netpacket";QVERIFY(!f.runtime.supportsRejoin(excluded));
        excluded=other;excluded["playMode"]="hotseat";QVERIFY(!f.runtime.supportsRejoin(excluded));
        f.runtime.games_={{"nes",other}};QVERIFY(!f.runtime.supportsLive(local));
    }
    void finishedGameClearsRelayForNextInvitation() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        f.runtime.relayStarted_=true;f.runtime.relayWaiting_=true;f.runtime.relayTicket_="previous";
        f.runtime.selectedRelay_="madrid";f.runtime.selectedRelayEndpoint_="old.example|1234";
        finishOrdinary(f);
        QVERIFY(!f.runtime.relayStarted_);QVERIFY(!f.runtime.relayWaiting_);
        QVERIFY(f.runtime.relayTicket_.isEmpty());QVERIFY(f.runtime.selectedRelay_.isEmpty());
        QVERIFY(f.runtime.selectedRelayEndpoint_.isEmpty());
    }
    void destructionCannotReenterFromExternalLobbySignals() {
        Fixture f;
        auto* runtime=new RuntimeMultiplayer(f.library,f.retro,f.psp,f.dolphin,f.social,f.process,f.lifecycle,f.view);
        QSignalSpy changed(runtime,&RuntimeMultiplayer::changed);
        // QObject emits destroyed after the derived class's value members have
        // died. A registered service's removal can reconcile this external room.
        connect(runtime,&QObject::destroyed,f.social.lobbyCoordinator(),[&]{emit f.social.lobbyCoordinator()->changed();});
        delete runtime;
        QCOMPARE(changed.size(),0);
    }
    void lobbyImportChecksBeforeWritingAndInvalidatesLateReplies() {
        Fixture f;f.runtime.allowed_=true;
        const auto file=f.dir.filePath("fixture.nes"),core=f.dir.filePath("core"),exe=f.dir.filePath("runtime");
        auto write=[](QString path,QByteArray bytes){QFile out(path);if(out.open(QIODevice::WriteOnly))out.write(bytes);};
        write(file,"Content-free import fixture");write(core,"Core fixture");write(exe,"Runtime fixture");
        RetroArchInstallation installation;installation.program=exe;installation.runtimeFile=exe;installation.cores={{"fceumm",core}};
        f.runtime.installation_=installation;
        RetroArchAdapter adapter(f.library,installation);
        AdventureRegistration candidate;candidate.adventure.platformId="nes";candidate.adventure.adapterId="unconfigured";candidate.contentPath=file;adapter.prepareInstallation(candidate);
        const std::atomic_bool cancelled=false;const auto profile=retroarch::netplayIdentity(candidate,installation,cancelled);QVERIFY(!profile.isEmpty());
        const auto id=QString::fromLatin1(QCryptographicHash::hash((profile["id"].toString()+"/"+profile["content"].toString()).toUtf8(),QCryptographicHash::Sha256).toHex());
        auto* room=f.social.lobbyCoordinator();room->configure("1","Host",true);QVERIFY(room->create());
        QVERIFY(room->selectGame({id,"Fixture","nes",profile["id"].toString(),2,profile}));
        LobbyImportController importer(f.library,adapter,nullptr,f.social,f.runtime);
        const auto begin=[&]{importer.open_=true;importer.context_=importer.context();importer.error_.clear();};
        begin();importer.select(file+".missing");QTRY_VERIFY(!importer.busy());QVERIFY(!importer.error().isEmpty());QCOMPARE(f.library.writes,0);
        importer.cancel();write(f.dir.filePath("wrong.nes"),"A different content-free edition");
        begin();importer.select(f.dir.filePath("wrong.nes"));QTRY_VERIFY(!importer.busy());QVERIFY(!importer.error().isEmpty());QCOMPARE(f.library.writes,0);
        importer.cancel();begin();importer.select(file);importer.cancel();QTest::qWait(100);QCOMPARE(f.library.writes,0);
        begin();importer.select(file);QVERIFY(room->setDetails("Changed title",0)); // A title alone does not change game identity.
        auto other=profile;other["content"]="different";QVERIFY(room->selectGame({"different","Different","nes",profile["id"].toString(),2,other}));
        QTest::qWait(100);QCOMPARE(f.library.writes,0);
        importer.cancel();QVERIFY(room->selectGame({id,"Fixture","nes",profile["id"].toString(),2,profile}));
        begin();importer.select(file);QTRY_VERIFY(!importer.busy());QCOMPARE(f.library.writes,1);QVERIFY(!importer.open());
        QCOMPARE(f.library.records.first().contentPath,file);QCOMPARE(f.library.records.first().adventure.domain,QString("multiverse"));
    }
    void commonSplashCancelsBeforeCheckpointAndLeavesRunningGamesAlone() {
        Fixture f;LaunchPreparation preparation(f.library,f.retro);
        LaunchPresentation presentation(f.library,preparation,f.lifecycle,f.runtime,f.social,f.view);
        QObject::disconnect(&f.lifecycle,&AdventureLaunchController::checkpointRequested,&f.lifecycle,nullptr);
        QSignalSpy checkpoints(&f.lifecycle,&AdventureLaunchController::checkpointRequested);
        QVERIFY(f.lifecycle.launch({"unused",{},{}},{},"missing"));QVERIFY(presentation.visible());
        presentation.cancel();QVERIFY(!presentation.visible());
        f.lifecycle.checkpointCompleted(checkpoints.first()[0].toULongLong(),{});QVERIFY(!f.process.active());
        QObject::connect(&f.lifecycle,&AdventureLaunchController::checkpointRequested,&f.lifecycle,[&](quint64 token,const QJsonObject&){f.lifecycle.checkpointCompleted(token,{});});
        QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),QString("running"));QVERIFY(!presentation.visible());
        presentation.cancel();QVERIFY(f.process.active());finishOrdinary(f);
    }
    void remoteGuestUsesSharedSaveBarrierWithoutRequiringLocalContent_data() {
        QTest::addColumn<bool>("saveFails");QTest::addColumn<bool>("live");
        QTest::newRow("settled")<<false<<false;QTest::newRow("save-error")<<true<<false;
        QTest::newRow("rejoin-settled")<<false<<true;QTest::newRow("rejoin-save-error")<<true<<true;
    }
    void remoteGuestUsesSharedSaveBarrierWithoutRequiringLocalContent() {
        QFETCH(bool,saveFails);QFETCH(bool,live);Fixture f;f.social.snapshot_={{"state","connected"},{"userId","2"}};
        RemotePlayRuntime remote(f.library,f.retro,f.social,f.process,f.lifecycle,f.runtime.gamePreparation());
        remote.root_=f.dir.path();remote.allowed=[] {return true;};
        const QJsonObject profile{{"id","host-runtime"},{"content","host-only"}};
        const auto id=QString::fromLatin1(QCryptographicHash::hash("remoteplay.v1/"+QJsonDocument(profile).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
        const auto operation=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QJsonObject context{{"self","2"},{"owner","1"},{"game",QJsonObject{{"id",id},{"mode","remoteplay.v1"},{"seats",2},{"runtime",profile}}},
            {"members",QJsonArray{QJsonObject{{"id","1"},{"seat",1},{"connected",true}},QJsonObject{{"id","2"},{"seat",live?0:2},{"spectator",live},{"connected",true}}}}};
        if(live)context["liveAdmission"]=operation;
        QVERIFY(f.start(saveFails));QTRY_COMPARE(f.lifecycle.state(),"running");readyMenu(f);
        int callbacks=0;bool prepared=false;
        remote.prepare(operation,context,[&](bool ok,QString){++callbacks;prepared=ok;});
        QCOMPARE(callbacks,0);QVERIFY(f.process.active());QCOMPARE(f.view.panel(),"lobby-prepare");
        f.view.updateInput(f.view.inputGeneration(),{true,true});f.runtime.action("multiplayer-lobby-saved:"+operation);
        QTRY_COMPARE(callbacks,1);QCOMPARE(prepared,!saveFails);QVERIFY(f.library.records.isEmpty());
        remote.cancel(operation);
    }
    void liveLobbyAdmissionPreservesTheOwnedHostProcessAndExactIdentity_data() {
        QTest::addColumn<QString>("profileId");QTest::addColumn<QString>("core");QTest::addColumn<bool>("together");QTest::addColumn<QString>("recovery");
        const QString nes="1b13b00d4680394dad8000d5175f97be727107e0945bc9b412da91d70c07b267";
        QTest::newRow("first-late-guest")<<QString("runtime.fceumm.pong-homebrew.v1")<<nes<<false<<QString("success");
        QTest::newRow("nes-returning-original-player")<<QString("runtime.fceumm.pong-homebrew.v1")<<nes<<true<<QString("success");
        QTest::newRow("snes-returning-original-player")<<QString("runtime.snes9x.test.v1")<<QString("e8d8f5cf93898c7be3ca2590d4d75dc8e9a89b891589c2da1a7917fa27411d21")<<true<<QString("success");
        QTest::newRow("megadrive-returning-original-player")<<QString("runtime.genesis.test.v1")<<QString("de8ad971eee89fa5e4e00dd1c048f0a8cc53cd9558b8c143aa022f5d7623d273")<<true<<QString("success");
        for(const auto mode:{"offline","cancel","expired","host-ended","three-attempts","lost-admission","pending-expired","pending-cancel","emulator-ended","explicit-exit"})QTest::newRow(mode)<<QString("runtime.fceumm.pong-homebrew.v1")<<nes<<true<<QString(mode);
    }
    void liveLobbyAdmissionPreservesTheOwnedHostProcessAndExactIdentity() {
        QFETCH(QString,profileId);QFETCH(QString,core);QFETCH(bool,together);QFETCH(QString,recovery);
        Fixture a,b;
        // Historical two-pad profiles omit players; the established default is 2.
        const QJsonObject profile{{"id",profileId},{"label","Test game"},{"content","exact"},
            {"runtime","8d57b47c88d416c05145187c8c906b7ab0013edb7e45af69d9b6bb841aea4a24"},
            {"core",core}};
        const auto digest=QString::fromLatin1(QCryptographicHash::hash((profile["id"].toString()+"/exact").toUtf8(),QCryptographicHash::Sha256).toHex());
        for(auto* f:{&a,&b}) {
            const auto id=f==&a?QString("1"):QString("2");
            disconnect(&f->social,&SocialController::commandRequested,f->social.session_,nullptr);disconnect(f->social.session_,nullptr,&f->social,nullptr);
            f->social.snapshot_={{"state","connected"},{"userId",id},{"name",id}};
            f->social.lobbyCoordinator()->configure(id,id,true);f->social.lobbyRuntime()->configure(id,true);
            f->social.lobbyRuntime()->registerService(profile["id"].toString(),&f->runtime);
            f->runtime.allowed_=true;f->runtime.games_={{"local",profile}};
            AdventureRegistration record;record.adventure.id="local";f->library.records={record};
            disconnect(&f->runtime.party_,&GameParty::startRequested,&f->runtime,&RuntimeMultiplayer::startParty);
            connect(&f->runtime.party_,&GameParty::startRequested,&f->runtime,[f](bool host,QJsonObject){
                const auto selected=f->runtime.party_.game();f->runtime.active_=true;f->runtime.host_=host;f->runtime.descriptor_=selected;f->runtime.game_="local";
                QFile::remove(f->dir.filePath("control"));
                QVERIFY(f->start(false,"local",false,"retroarch",selected));
                if(host)f->runtime.party_.ready({{"kind","ready"},{"identity",selected}});
            });
        }
        auto* h=a.social.lobbyCoordinator();auto* g=b.social.lobbyCoordinator();
        connect(h,&LobbyCoordinator::outgoing,g,[&](QString,QJsonObject p){g->receive("1","Host",p);});
        connect(g,&LobbyCoordinator::outgoing,h,[&](QString,QJsonObject p){
            if(p["kind"]=="live-request"&&(recovery=="lost-admission"||recovery.startsWith("pending-")))return;
            h->receive("2","Guest",p);
        });
        connect(&a.runtime.party_,&GameParty::outgoing,&b.runtime,[&](QString,QJsonObject p){b.runtime.party_.receive("online:1","Host",p);});
        connect(&b.runtime.party_,&GameParty::outgoing,&a.runtime,[&](QString,QJsonObject p){a.runtime.party_.receive("online:2","Guest",p);});
        QVERIFY(h->create("public"));QVERIFY(h->selectGame({digest,"Game","nes",profile["id"].toString(),2,profile}));
        if(together)QVERIFY(g->join("1",h->state()["id"].toString()));else QVERIFY(a.social.lobbyRuntime()->setLiveJoining(true));
        QVERIFY(a.social.lobbyRuntime()->prepare());
        if(together) {QVERIFY(b.social.lobbyRuntime()->prepare());QVERIFY(g->ready(true));}
        QVERIFY(h->ready(true));QVERIFY(h->start());
        QTRY_COMPARE(h->state()["phase"].toString(),"playing");const auto pid=a.process.processId();const auto ticket=h->state()["ticket"].toString();
        if(!together) {QVERIFY(g->join("1",h->state()["id"].toString()));QVERIFY(b.social.lobbyRuntime()->joinLive());}
        QTRY_COMPARE(b.lifecycle.state(),"running");QCOMPARE(a.process.processId(),pid);QCOMPARE(a.runtime.descriptor_,profile);
        if(together) {
            const auto guestPid=b.process.processId();
            b.lifecycle.exitController().setAvailable(true);
            b.runtime.output("[Netplay] disconnected\n");
            QTRY_VERIFY(b.social.lobbyRuntime()->liveAvailable());QVERIFY(b.process.active());
            QCOMPARE(a.process.processId(),pid);QCOMPARE(h->state()["ticket"].toString(),ticket);
            QVERIFY(!h->state()["liveJoining"].toBool());
            bool action=false;for(const auto& v:b.social.lobbyActions())if(v.toMap()["id"]=="join-live"&&v.toMap()["label"]=="Rejoin game")action=true;
            QVERIFY(action);
            const auto reserved=b.social.lobbyPlayers().value(1).toMap();QCOMPARE(reserved["id"].toString(),QString("2"));
            QVERIFY(!reserved["empty"].toBool());QCOMPARE(reserved["status"].toString(),QString("Can return to P2"));
            QVERIFY(!b.runtime.rejoinTicket_.isEmpty());
            if(recovery=="explicit-exit") {
                QVERIFY(b.lifecycle.exitController().requestExit());
                QVERIFY(b.runtime.rejoinTicket_.isEmpty());
                QVERIFY(b.lifecycle.exitController().confirm());QTRY_VERIFY(!b.lifecycle.active());
                b.runtime.recoverConnection();QVERIFY(!b.process.active());finishOrdinary(a);return;
            }
            if(recovery=="emulator-ended") {
                finishOrdinary(b);QVERIFY(!b.runtime.rejoinTicket_.isEmpty());
            }
            if(recovery=="cancel"||recovery=="expired"||recovery=="host-ended") {
                if(recovery=="cancel")b.runtime.leaveParty();
                else if(recovery=="expired")b.runtime.rejoinUntil_=QDateTime::currentSecsSinceEpoch()-1;
                else finishOrdinary(a);
                b.runtime.recoverConnection();QVERIFY(b.runtime.rejoinTicket_.isEmpty());
                QCOMPARE(b.process.processId(),guestPid);finishOrdinary(b);
                if(a.lifecycle.active())finishOrdinary(a);return;
            }
            if(recovery=="offline") {
                b.social.lobbyRuntime()->configure("2",false);b.runtime.rejoinNext_=0;b.runtime.recoverConnection();
                QTRY_VERIFY(!b.lifecycle.active());b.runtime.recoverConnection();
                QCOMPARE(b.runtime.rejoinAttempts_,0);QVERIFY(!b.process.active());
                b.social.lobbyRuntime()->configure("2",true);
            }
            if(recovery=="lost-admission"||recovery.startsWith("pending-")) {
                QTRY_VERIFY_WITH_TIMEOUT(b.social.lobbyRuntime()->busy()&&!b.process.active(),8000);
                QVERIFY(b.runtime.opening());
                if(recovery=="pending-cancel")b.runtime.cancelOpening();
                else if(recovery=="pending-expired") {
                    b.runtime.rejoinUntil_=QDateTime::currentSecsSinceEpoch()-1;b.runtime.recoverConnection();
                } else for(int attempt=1;attempt<=3;++attempt) {
                    QCOMPARE(b.runtime.rejoinAttempts_,attempt);
                    b.runtime.rejoinAttemptUntil_=QDateTime::currentSecsSinceEpoch()-1;b.runtime.recoverConnection();
                    QVERIFY(!b.social.lobbyRuntime()->busy());b.runtime.rejoinNext_=0;b.runtime.recoverConnection();
                }
                QVERIFY(!b.social.lobbyRuntime()->busy());QVERIFY(b.runtime.rejoinTicket_.isEmpty());QVERIFY(!b.process.active());
                QCOMPARE(a.process.processId(),pid);finishOrdinary(a);return;
            }
            QTRY_VERIFY_WITH_TIMEOUT(b.process.processId()>0&&b.process.processId()!=guestPid&&b.lifecycle.state()=="running",8000);
            QCOMPARE(a.process.processId(),pid);QCOMPARE(h->state()["ticket"].toString(),ticket);
            QCOMPARE(b.runtime.rejoinAttempts_,1); // A spawned child is not yet a confirmed native rejoin.
            if(recovery=="three-attempts") {
                for(int attempt=1;attempt<=3;++attempt) {
                    const auto old=b.process.processId();b.lifecycle.exitController().setAvailable(true);
                    b.runtime.output("[Netplay] disconnected\n");
                    QTRY_VERIFY(b.social.lobbyRuntime()->liveAvailable());b.runtime.rejoinNext_=0;b.runtime.recoverConnection();
                    if(attempt<3) {
                        QTRY_VERIFY_WITH_TIMEOUT(b.process.processId()>0&&b.process.processId()!=old&&b.lifecycle.state()=="running",8000);
                        QCOMPARE(b.runtime.rejoinAttempts_,attempt+1);
                    } else {
                        QTRY_VERIFY(b.runtime.rejoinTicket_.isEmpty());QVERIFY(!b.process.active());
                    }
                    QCOMPARE(a.process.processId(),pid);
                }
                finishOrdinary(a);return;
            }
            b.runtime.output("[Netplay] You have joined as player 2\n");
            QVERIFY(b.runtime.rejoinTicket_.isEmpty());
        }
        finishOrdinary(b);QTRY_VERIFY(b.social.lobbyRuntime()->liveAvailable());QCOMPARE(h->state()["ticket"].toString(),ticket);
        QVERIFY(b.runtime.rejoinTicket_.isEmpty()); // Explicit game exit never schedules another launch.
        QVERIFY(QFile::remove(b.dir.filePath("control")));QVERIFY(b.social.lobbyRuntime()->joinLive());
        QTRY_COMPARE(b.lifecycle.state(),"running");QCOMPARE(a.process.processId(),pid);
        finishOrdinary(b);finishOrdinary(a);
    }
    void automaticPreparationRequiresTemporaryProgressAndSuccessfulFinalization_data() {
        QTest::addColumn<bool>("temporary");QTest::addColumn<bool>("saveFails");
        QTest::newRow("ordinary-manual-save")<<false<<false;
        QTest::newRow("isolated-progress")<<true<<false;
        QTest::newRow("finalization-failure")<<true<<true;
    }
    void automaticPreparationRequiresTemporaryProgressAndSuccessfulFinalization() {
        QFETCH(bool,temporary);QFETCH(bool,saveFails);Fixture f;
        QVERIFY(f.start(saveFails,"local",false,"retroarch",temporary?QJsonObject{{"id","test"}}:QJsonObject{}));
        QTRY_COMPARE(f.lifecycle.state(),"running");f.lifecycle.exitController().setAvailable(true);
        int calls=0;bool ready=false;
        f.runtime.gamePreparation().recover("retry",f.process.processId(),[&](bool ok,QString){++calls;ready=ok;});
        QTRY_COMPARE(calls,1);QCOMPARE(ready,temporary&&!saveFails);
        if(!temporary){QVERIFY(f.process.active());finishOrdinary(f);}
        else QVERIFY(!f.process.active());
    }
    void contentOverlaySharesTheLeaseAndNeverReplaysHeldGameButtons() {
        Fixture f;AdventureOverlayService overlay(f.process,f.lifecycle,f.view,{});
        QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");const auto pid=f.process.processId();
        overlay.active_=true;overlay.protocol_=4;f.lifecycle.exitController().setAvailable(true);
        QVERIFY(f.view.requestMenu());f.view.setInputIsolated(true);f.view.setWindowFocused(true);
        f.view.setPanel("test-content",{},{});f.view.setContentInput(true);
        QSignalSpy content(&overlay,&AdventureOverlayService::contentAction),shell(&overlay,&AdventureOverlayService::shellAction);
        const auto sample=[&](QJsonObject packet,quint64 epoch=0){packet["event"]="input";packet["epoch"]=QString::number(epoch?epoch:f.view.inputGeneration());
            overlay.buffer_=QJsonDocument(packet).toJson(QJsonDocument::Compact)+'\n';overlay.receive();};
        sample({{"connected",true},{"confirm",true}});QVERIFY(content.isEmpty());
        sample({{"connected",true},{"neutral",true}});sample({{"connected",true},{"confirm",true}});QCOMPARE(content.size(),1);
        QCOMPARE(qvariant_cast<Action>(content.last()[0]),Action::Confirm);sample({{"connected",true},{"confirm",true}});QCOMPARE(content.size(),1);
        const auto old=f.view.inputGeneration();f.view.setWindowFocused(false);sample({{"connected",true},{"back",true}},old);QCOMPARE(content.size(),1);
        f.view.setWindowFocused(true);sample({{"connected",true},{"back",true}});QCOMPARE(content.size(),1);
        sample({{"connected",true},{"neutral",true}});sample({{"connected",true},{"left",true}});QCOMPARE(content.size(),2);
        QCOMPARE(qvariant_cast<Action>(content.last()[0]),Action::Left);QVERIFY(shell.isEmpty());
        overlay.lost();QVERIFY(!f.view.visible());QVERIFY(!f.view.contentInput());QCOMPARE(f.process.processId(),pid);
        finishOrdinary(f);
    }
    void shellEntryWaitsForFreshInputAndCancellationKeepsTheOwnedGame() {
        Fixture f;AdventureOverlayService overlay(f.process,f.lifecycle,f.view,{});
        QVERIFY(!overlay.requestShell());QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        // Model an admitted helper; native helper/input acceptance is separate.
        overlay.active_=true;overlay.protocol_=4;f.lifecycle.exitController().setAvailable(true);
        const auto pid=f.process.processId();QVERIFY(overlay.requestShell());
        QVERIFY(overlay.openingShell_);QVERIFY(!overlay.minimizing_);
        QVERIFY(f.view.requestMenu());f.view.dismissMenu();QVERIFY(!overlay.openingShell_);
        f.view.updateInput(f.view.inputGeneration(),{true,true});QVERIFY(!overlay.minimizing_);
        QVERIFY(overlay.requestShell());QVERIFY(f.view.requestMenu());
        f.view.setInputIsolated(true);f.view.setWindowFocused(true);QVERIFY(!overlay.minimizing_);
        f.view.updateInput(f.view.inputGeneration(),{true,true});QVERIFY(overlay.minimizing_);QVERIFY(!overlay.openingShell_);
        QVERIFY(overlay.requestShell());QCOMPARE(f.process.processId(),pid);QVERIFY(!f.lifecycle.minimized());
        overlay.lost();QVERIFY(!overlay.minimizing_);QCOMPARE(f.process.processId(),pid);
        finishOrdinary(f);
    }
    void deferredLobbyInvitationSurvivesAnIndependentGameExit() {
        Fixture f;disconnect(&f.social,&SocialController::commandRequested,f.social.session_,nullptr);
        disconnect(f.social.session_,nullptr,&f.social,nullptr);
        f.runtime.allowed_=true;f.social.snapshot_={{"state","connected"},{"userId","2"},{"name","Guest"}};
        f.social.lobbyCoordinator()->configure("2","Guest",true);
        LobbyCoordinator host;host.configure("1","Host",true);QVERIFY(host.create());
        connect(&host,&LobbyCoordinator::outgoing,&f.social,[&](QString,QJsonObject p){f.social.lobbyCoordinator()->receive("1","Host",p);});
        QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");QVERIFY(host.invite("2"));
        const auto invitation=f.runtime.invitationId();QVERIFY(f.runtime.invitationBadge());
        finishOrdinary(f);QCOMPARE(f.runtime.invitationId(),invitation);QVERIFY(f.runtime.invitationBadge());
        host.leave();QVERIFY(f.runtime.invitationId().isEmpty());QVERIFY(!f.runtime.invitationBadge());
        f.runtime.openInvitation(invitation);QVERIFY(f.social.face()!="lobby");
    }
    void lobbyReadyRunsBothOwnedChildrenAndReturnsToTheSameRoom_data() {
        QTest::addColumn<QString>("ending");
        for(const auto& name:{"normal","connection-failure","party-left"})QTest::newRow(name)<<QString(name);
    }
    void lobbyReadyRunsBothOwnedChildrenAndReturnsToTheSameRoom() {
        QFETCH(QString,ending);
        Fixture a,b;
        const QJsonObject profile{{"id","runtime.test"},{"content","exact"},{"players",2},{"label","Game"}};
        const auto digest=QString::fromLatin1(QCryptographicHash::hash(QByteArray("runtime.test/exact"),QCryptographicHash::Sha256).toHex());
        for(auto* f:{&a,&b}) {
            const auto id=f==&a?QString("1"):QString("2");
            // The test wires authenticated metadata locally; the signed-out
            // provider worker must not also attempt to deliver those packets.
            disconnect(&f->social,&SocialController::commandRequested,f->social.session_,nullptr);
            disconnect(f->social.session_,nullptr,&f->social,nullptr);
            f->social.snapshot_={{"state","connected"},{"userId",id},{"name",id}};
            f->social.lobbyCoordinator()->configure(id,id,true);f->social.lobbyRuntime()->configure(id,true);
            f->social.lobbyRuntime()->registerService("runtime.test",&f->runtime);
            f->runtime.allowed_=true;f->runtime.games_={{"local",profile}};
            AdventureRegistration record;record.adventure.id="local";f->library.records={record};
            disconnect(&f->runtime.party_,&GameParty::startRequested,&f->runtime,&RuntimeMultiplayer::startParty);
            // Substitute only the emulator; preparation, consent, party wire,
            // launch acknowledgements and the owned child lifecycle are real.
            connect(&f->runtime.party_,&GameParty::startRequested,&f->runtime,[f](bool host,QJsonObject){
                const auto selected=f->runtime.party_.game();
                f->runtime.active_=true;f->runtime.host_=host;f->runtime.descriptor_=selected;f->runtime.game_="local";
                QVERIFY(f->start(false,"local"));
                if(host)f->runtime.party_.ready({{"kind","ready"},{"identity",selected}});
            });
        }
        auto* h=a.social.lobbyCoordinator();auto* g=b.social.lobbyCoordinator();
        QSignalSpy startsA(&a.lifecycle,&AdventureLaunchController::adventureStarted),startsB(&b.lifecycle,&AdventureLaunchController::adventureStarted);
        connect(h,&LobbyCoordinator::outgoing,g,[&](QString,QJsonObject p){g->receive("1","Host",p);});
        connect(g,&LobbyCoordinator::outgoing,h,[&](QString,QJsonObject p){h->receive("2","Guest",p);});
        connect(&a.runtime.party_,&GameParty::outgoing,&b.runtime,[&](QString,QJsonObject p){b.runtime.party_.receive("online:1","Host",p);});
        connect(&b.runtime.party_,&GameParty::outgoing,&a.runtime,[&](QString,QJsonObject p){a.runtime.party_.receive("online:2","Guest",p);});
        QVERIFY(h->create());QVERIFY(h->invite("2"));g->answer(true);const auto room=h->state()["id"].toString();
        QVERIFY(h->selectGame({digest,"Game","test","runtime.test",2,profile}));QVERIFY(h->seat(1));QVERIFY(g->seat(2));
        QVERIFY(a.social.lobbyRuntime()->prepare());QVERIFY(b.social.lobbyRuntime()->prepare());
        QVERIFY(h->ready(true));QVERIFY(!a.process.active());QVERIFY(!b.process.active());QVERIFY(g->ready(true));
        QVERIFY(!a.process.active());QVERIFY(!b.process.active());QVERIFY(!g->start());QVERIFY(h->start());QVERIFY(!h->start());
        QTRY_VERIFY2_WITH_TIMEOUT(h->state()["phase"]=="playing",qPrintable(a.social.lobbyRuntime()->status()+" / "+b.social.lobbyRuntime()->status()+" / "+a.lifecycle.state()+" / "+b.lifecycle.state()),5000);
        QCOMPARE(g->state()["phase"].toString(),QString("playing"));
        QVERIFY(a.process.active());QVERIFY(b.process.active());QVERIFY(a.runtime.party_.pending().isEmpty());QVERIFY(b.runtime.party_.pending().isEmpty());
        if(ending!="normal") {
            if(ending=="party-left")b.runtime.party_.leave();else b.runtime.fail("Connection lost after suspend");
            QTRY_COMPARE(h->state()["phase"].toString(),QString("gathering"));
            QVERIFY(a.process.active());QVERIFY(b.process.active()); // Save/exit remains owned.
            QCOMPARE(g->state()["id"].toString(),room);QVERIFY(!b.runtime.active_);
        }
        finishOrdinary(a);finishOrdinary(b);QTRY_COMPARE(h->state()["phase"].toString(),QString("gathering"));
        QCOMPARE(h->state()["id"].toString(),room);QCOMPARE(g->state()["id"].toString(),room);QCOMPARE(h->state()["members"].toArray().size(),2);
        // Automatic preparation may already have finished again. It must never
        // retain the previous explicit Ready or launch either child by itself.
        for(auto* room:{h,g})for(const auto& member:room->state()["members"].toArray())QVERIFY(!member.toObject()["ready"].toBool());
        QVERIFY(!a.process.active());QVERIFY(!b.process.active());
        auto next=profile;next["content"]="another-game";
        for(auto* f:{&a,&b}) {QVERIFY(QFile::remove(f->dir.filePath("control")));f->runtime.games_["local"]=next;}
        const auto nextId=QString::fromLatin1(QCryptographicHash::hash(QByteArray("runtime.test/another-game"),QCryptographicHash::Sha256).toHex());
        QVERIFY(h->selectGame({nextId,"Another game","test","runtime.test",2,next}));
        QVERIFY(a.social.lobbyRuntime()->prepare());QVERIFY(b.social.lobbyRuntime()->prepare());
        QVERIFY(g->ready(true));QVERIFY(h->ready(true));QVERIFY(h->start());QTRY_COMPARE(h->state()["phase"].toString(),QString("playing"));
        QCOMPARE(startsA.size(),2);QCOMPARE(startsB.size(),2);QCOMPARE(g->state()["id"].toString(),room);
        finishOrdinary(a);finishOrdinary(b);
        QTRY_COMPARE(h->state()["phase"].toString(),QString("gathering"));
    }
    void lobbyPreparationKeepsAnUnrelatedGameUntilExplicitSavedConsent_data() {
        QTest::addColumn<bool>("saveFails");
        QTest::newRow("saved")<<false;QTest::newRow("save-recovery-required")<<true;
    }
    void lobbyPreparationKeepsAnUnrelatedGameUntilExplicitSavedConsent() {
        QFETCH(bool,saveFails);
        Fixture f;f.social.snapshot_={{"state","connected"},{"userId","1"},{"name","Host"}};
        f.runtime.allowed_=true;const QJsonObject profile{{"id","runtime.test"},{"content","exact"},{"players",2},{"label","Game"}};
        f.runtime.games_={{"local",profile}};AdventureRegistration record;record.adventure.id="local";f.library.records={record};
        const auto digest=QString::fromLatin1(QCryptographicHash::hash(QByteArray("runtime.test/exact"),QCryptographicHash::Sha256).toHex());
        const QJsonObject context{{"self","1"},{"owner","1"},{"game",QJsonObject{{"id",digest},{"mode","runtime.test"}}},
            {"members",QJsonArray{QJsonObject{{"id","1"},{"seat",1},{"connected",true}}}}};
        QVERIFY(f.start(saveFails,"ordinary"));QTRY_COMPARE(f.lifecycle.state(),"running");readyMenu(f);
        int callbacks=0;bool prepared=false;
        f.runtime.prepare("operation",context,[&](bool ok,QString){++callbacks;prepared=ok;});
        QCOMPARE(f.view.panel(),QString("lobby-prepare"));QCOMPARE(callbacks,0);QVERIFY(f.process.active());
        f.view.updateInput(f.view.inputGeneration(),{true,true});QVERIFY(f.view.ready());
        f.runtime.action("multiplayer-lobby-saved:stale");QVERIFY(f.process.active());QCOMPARE(callbacks,0);
        f.runtime.action("multiplayer-lobby-saved:operation");QTRY_VERIFY(!f.lifecycle.active());QTRY_COMPARE(callbacks,1);QCOMPARE(prepared,!saveFails);
    }
    void lobbyCancelNeverClosesTheCurrentGameOrAcceptsLatePreparation() {
        Fixture f;f.social.snapshot_={{"state","connected"},{"userId","1"}};f.runtime.allowed_=true;
        const QJsonObject profile{{"id","runtime.test"},{"content","exact"},{"players",2}};f.runtime.games_={{"local",profile}};
        AdventureRegistration record;record.adventure.id="local";f.library.records={record};
        const auto digest=QString::fromLatin1(QCryptographicHash::hash(QByteArray("runtime.test/exact"),QCryptographicHash::Sha256).toHex());
        const QJsonObject context{{"self","1"},{"owner","1"},{"game",QJsonObject{{"id",digest},{"mode","runtime.test"}}},
            {"members",QJsonArray{QJsonObject{{"id","1"},{"seat",1},{"connected",true}}}}};
        QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");readyMenu(f);int calls=0;
        f.runtime.prepare("old",context,[&](bool,QString){++calls;});f.runtime.cancel("old");
        f.runtime.action("multiplayer-lobby-saved:old");QVERIFY(f.process.active());QCOMPARE(calls,0);finishOrdinary(f);QCOMPARE(calls,0);
    }
    void sharedControllerModeIsExplicitAndBoundToTheLiveGame() {
        Fixture f;QVERIFY(f.start(false,"ps1:test"));QTRY_COMPARE(f.lifecycle.state(),"running");
        QJsonObject base{{"id","runtime.retroarch.ps1"},{"players",2},{"controllerModes",QJsonArray{"separate","pass-p1"}}};
        f.runtime.allowed_=true;f.runtime.games_={{"ps1:test",base}};f.runtime.update();
        readyMenu(f);f.runtime.action("multiplayer");
        QVERIFY(!usesSharedController(f.runtime.descriptor_));
        f.runtime.action("multiplayer-mode:ps1:stale:pass-p1");QVERIFY(!usesSharedController(f.runtime.descriptor_));
        f.runtime.action("multiplayer-mode:ps1:test:pass-p1");QVERIFY(usesSharedController(f.runtime.descriptor_));
        QCOMPARE(f.view.menuFocus(),0);
        QCOMPARE(f.view.menuActions().first().toMap()["id"].toString(),QString("multiplayer-mode:ps1:test:separate"));
        for(const auto& row:f.view.menuActions())QVERIFY(row.toMap()["label"]!="No longer available");
        f.runtime.update();QVERIFY(usesSharedController(f.runtime.descriptor_));
        f.runtime.action("multiplayer-mode:ps1:test:unknown");QVERIFY(usesSharedController(f.runtime.descriptor_));
        f.runtime.action("multiplayer-mode:ps1:test:separate");QCOMPARE(f.runtime.descriptor_,base);
        finishOrdinary(f);
    }
    void sharedControllerAbortKeepsOrdinarySaveConsentAndCancelsOwnedPreparation() {
        const QJsonObject profile{{"id","runtime.retroarch.ps1"},{"players",2},{"controllerModes",QJsonArray{"pass-p1"}},{"playMode","pass-p1"}};
        Fixture ordinary;QVERIFY(ordinary.start());QTRY_COMPARE(ordinary.lifecycle.state(),"running");
        requestRestart(ordinary,profile);ordinary.runtime.fail("Connection ended");
        QVERIFY(ordinary.process.active());finishOrdinary(ordinary);

        Fixture preparing;ProcessCommand command;
        command.prepare=[](ProcessCommand&,const std::atomic_bool& cancelled){
            while(!cancelled)QThread::msleep(5);return QString();
        };
        preparing.runtime.active_=true;preparing.runtime.descriptor_=profile;preparing.runtime.game_="ps1";
        QVERIFY(preparing.lifecycle.launch(command,{},"ps1",AdventureSavePolicy::ManualConfirm));
        QTRY_VERIFY(preparing.process.preparing());preparing.runtime.fail("Connection ended");
        QTRY_VERIFY(!preparing.process.active());QVERIFY(!preparing.runtime.active_);
    }
    void sharedControllerBindingsRejectStaleClicksAndEndBothOwnedCopies_data() {
        QTest::addColumn<bool>("nativeAuthority");
        QTest::newRow("retroarch-split-roles")<<false;
        QTest::newRow("dolphin-native-authority")<<true;
    }
    void sharedControllerBindingsRejectStaleClicksAndEndBothOwnedCopies() {
        QFETCH(bool,nativeAuthority);
        Fixture a,b;
        const QJsonObject base{{"id",nativeAuthority?"runtime.dolphin.gc.v1":"runtime.retroarch.ps1"},
            {"players",nativeAuthority?4:2},{"controllerModes",QJsonArray{"separate","pass-p1"}}};
        const auto profile=multiplayerModeProfile(base,"pass-p1");
        auto& host=a.runtime;auto& guest=b.runtime;
        // The child fixture replaces the emulator only; use the actual party,
        // handoff, owned stdin, menu-action and lifecycle bindings end to end.
        disconnect(&host.party_,&GameParty::startRequested,&host,&RuntimeMultiplayer::startParty);
        disconnect(&guest.party_,&GameParty::startRequested,&guest,&RuntimeMultiplayer::startParty);
        // Both fixtures share a host with each other and possibly an installed
        // shell. Their provider transport below is in-memory, without discovery.
        disconnect(&host.nearby_,nullptr,&host,nullptr);
        disconnect(&guest.nearby_,nullptr,&guest,nullptr);
        connect(&host.party_,&GameParty::outgoing,&guest,[&](QString,QJsonObject p){guest.party_.receive("host","Host",p);});
        connect(&guest.party_,&GameParty::outgoing,&host,[&](QString,QJsonObject p){host.party_.receive("guest","Guest",p);});
        host.party_.configure("Host",{base},profile,true);guest.party_.configure("Guest",{base},{},true);
        host.party_.invite("guest");guest.party_.answer(true);host.party_.start();host.party_.ready({{"kind","ready"}});
        QVERIFY2(host.party_.running()&&guest.party_.running(),qPrintable(QString::fromUtf8(QJsonDocument(host.party_.state()).toJson(QJsonDocument::Compact))+" / "+QString::fromUtf8(QJsonDocument(guest.party_.state()).toJson(QJsonDocument::Compact))));
        for(auto* runtime:{&host,&guest}) {
            runtime->allowed_=true;runtime->games_={{"ps1",base}};runtime->descriptor_=profile;
            runtime->active_=true;runtime->partySession_=true;runtime->game_="ps1";
        }
        host.host_=true;guest.host_=false;
        const auto kind=nativeAuthority?"dolphin":"retroarch";
        QVERIFY(a.start(false,"ps1",true,kind));QVERIFY(b.start(false,"ps1",true,kind));
        QTRY_COMPARE(a.lifecycle.state(),"running");QTRY_COMPARE(b.lifecycle.state(),"running");
        const auto session=host.party_.state()["party"].toString();
        const auto observe=[&](int sequence,int owner) {
            if(nativeAuthority)host.output("[TrainerOSNetplay]"+QJsonDocument(QJsonObject{
                {"event","controller"},{"sequence",sequence},{"owner",owner}}).toJson(QJsonDocument::Compact)+'\n');
            else emit guest.client_.hotseatOwnership(sequence,sequence,owner-1,sequence==1);
        };
        if(!nativeAuthority)guest.handoff_.begin(session,2);
        observe(1,1);
        QVERIFY2(host.handoff_.canPass(),qPrintable(host.status_+" / "+guest.status_+" / active "+QString::number(host.active_)+" "+QString::number(guest.active_)+" handoff "+QString::number(host.handoff_.active())+" "+QString::number(guest.handoff_.active())));QVERIFY(!guest.handoff_.canPass());
        const auto first=host.gameMenuActions().first().toMap()["id"].toString();
        connect(&host,&RuntimeMultiplayer::changed,&a.view,[&]{a.view.setExtraActions(host.gameMenuActions());});
        a.view.setExtraActions(host.gameMenuActions());readyMenu(a);
        // The current menu uses two columns: Right enters the multiplayer actions.
        a.view.updateInput(a.view.inputGeneration(),{true,false,false,false,false,false,false,false,true});
        QCOMPARE(a.view.menuActions().value(a.view.menuFocus()).toMap()["id"].toString(),first);
        host.action("multiplayer-pass:old:0");QVERIFY(host.handoff_.canPass());
        host.action(first);QVERIFY(!host.handoff_.canPass());
        QCOMPARE(a.view.menuActions().value(a.view.menuFocus()).toMap()["id"].toString(),QString("continue"));
        QVERIFY(!a.view.ready());
        observe(2,0);observe(3,2);
        QVERIFY(guest.handoff_.canPass());QVERIFY(!host.handoff_.canPass());
        const auto second=guest.gameMenuActions().first().toMap()["id"].toString();
        guest.action(second);observe(4,0);observe(5,1);
        QVERIFY(host.handoff_.canPass());host.action(first);QVERIFY(host.handoff_.canPass());
        host.fail("Connection lost");QTRY_VERIFY(!a.process.active());QTRY_VERIFY(!b.process.active());
        QVERIFY(!host.active_);QVERIFY(!guest.active_);QVERIFY(!host.handoff_.active());QVERIFY(!guest.handoff_.active());
        for(auto* f:{&a,&b}) {
            QFile journal(f->dir.filePath("control"));QVERIFY(journal.open(QIODevice::ReadOnly));
            const auto bytes=journal.readAll();
            if(!nativeAuthority)QCOMPARE(bytes,QByteArray("NETPLAY_GAME_WATCH\nNETPLAY_GAME_WATCH\nQUIT\n"));
            else {
                const auto lines=bytes.trimmed().split('\n');QCOMPARE(lines.size(),f==&a?3:1);
                QCOMPARE(QJsonDocument::fromJson(lines.last()).object(),QJsonObject({{"action","quit"}}));
                if(f==&a)for(int n=0;n<2;++n)QCOMPARE(QJsonDocument::fromJson(lines[n]).object(),
                    QJsonObject({{"action","pass"},{"sequence",1+2*n},{"target",2-n}}));
            }
        }
    }
    void vitaRestartKeepsConsentAndRejectsStaleReady() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        requestRestart(f,{{"id","runtime.vita3k.lan.v1"}});
        QVERIFY(f.process.active());QVERIFY(f.runtime.restarting_);
        f.lifecycle.exitController().cancel();QVERIFY(f.process.active());QVERIFY(!f.runtime.active_);
        f.runtime.frame({{"kind","vita-ready"},{"slot",2},{"identity",f.runtime.descriptor_}});
        QVERIFY(!f.runtime.launchPending_);QVERIFY(!f.runtime.active_);finishOrdinary(f);
    }
    void vitaRejectsChangedGameAndForeignStatus() {
        Fixture f;f.runtime.active_=true;f.runtime.descriptor_={{"id","runtime.vita3k.lan.v1"},{"content","accepted"}};
        f.runtime.frame({{"kind","vita-ready"},{"slot",2},{"identity",QJsonObject{{"id","runtime.vita3k.lan.v1"},{"content","other"}}}});
        QVERIFY(!f.runtime.active_);f.runtime.active_=true;f.runtime.deadline_=1;
        f.runtime.output("[TrainerOSDs] running\n");QCOMPARE(f.runtime.deadline_,1);
        const auto line=QByteArray("TRAINEROS_VITA ")+QJsonDocument(QJsonObject{{"event","running"}}).toJson(QJsonDocument::Compact).toBase64()+"\n";
        f.runtime.output(line.left(5));QCOMPARE(f.runtime.deadline_,1);
        f.runtime.output(line.mid(5));QCOMPARE(f.runtime.deadline_,0);QCOMPARE(f.runtime.status_,"Playing together");
    }
    void azaharRestartKeepsConsentAndRejectsStaleReady() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        requestRestart(f,{{"id","runtime.azahar.3ds.v1"}});
        QVERIFY(f.process.active());QVERIFY(f.runtime.restarting_);
        f.lifecycle.exitController().cancel();QVERIFY(f.process.active());QVERIFY(!f.runtime.active_);
        f.runtime.frame({{"kind","azahar-ready"},{"slot",2},{"identity",f.runtime.descriptor_}});
        QVERIFY(!f.runtime.launchPending_);QVERIFY(!f.runtime.active_);finishOrdinary(f);
    }
    void azaharRejectsChangedGameAndForeignStatus() {
        Fixture f;f.runtime.active_=true;f.runtime.descriptor_={{"id","runtime.azahar.3ds.v1"},{"content","accepted"}};
        f.runtime.frame({{"kind","azahar-ready"},{"slot",2},{"identity",QJsonObject{{"id","runtime.azahar.3ds.v1"},{"content","other"}}}});
        QVERIFY(!f.runtime.active_);f.runtime.active_=true;f.runtime.deadline_=1;
        f.runtime.output("[TrainerOSDs] running\n");QCOMPARE(f.runtime.deadline_,1);
        const auto line=QByteArray("TRAINEROS_3DS ")+QJsonDocument(QJsonObject{{"event","running"}}).toJson(QJsonDocument::Compact).toBase64()+"\n";
        f.runtime.output(line.left(5));QCOMPARE(f.runtime.deadline_,1);
        f.runtime.output(line.mid(5));QCOMPARE(f.runtime.deadline_,0);QCOMPARE(f.runtime.status_,"Playing together");
    }
    void dsRestartKeepsSaveConsentAndCancelledEndpointCannotLaunch() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        requestRestart(f,{{"id","runtime.melonds.nds.v1"},{"privateProgress",true}});
        QVERIFY(f.process.active());QVERIFY(f.runtime.restarting_);
        f.lifecycle.exitController().cancel();QVERIFY(f.process.active());QVERIFY(!f.runtime.active_);
        f.runtime.frame({{"kind","ds-ready"},{"slot",2},{"identity",f.runtime.descriptor_}});
        QVERIFY(!f.runtime.launchPending_);QVERIFY(!f.runtime.active_);finishOrdinary(f);
    }
    void dsRejectsChangedGameAndConsumesOnlyItsOwnStatusMarkers() {
        Fixture f;f.runtime.active_=true;f.runtime.descriptor_={{"id","runtime.melonds.nds.v1"},{"content","accepted"}};
        f.runtime.frame({{"kind","ds-ready"},{"slot",2},{"identity",QJsonObject{{"id","runtime.melonds.nds.v1"},{"content","other"}}}});
        QVERIFY(!f.runtime.active_);f.runtime.active_=true;f.runtime.deadline_=1;
        f.runtime.output("DS_NETWORK_READY delay=4\n");QCOMPARE(f.runtime.deadline_,1);
        f.runtime.output("[TrainerOSDs] run");QCOMPARE(f.runtime.deadline_,1);
        f.runtime.output("ning\n");QCOMPARE(f.runtime.deadline_,0);QCOMPARE(f.runtime.status_,"Playing together");
        f.runtime.output("[TrainerOSDs] error\n");QVERIFY(!f.runtime.active_);
    }
    void dsPrivateEndpointParserRejectsExtraFieldsAndMalformedCredentials() {
        const auto encode=[](QJsonObject value){return QByteArray("TRAINEROS_DS_ENDPOINT ")+QJsonDocument(value).toJson(QJsonDocument::Compact).toBase64();};
        QJsonObject nearby{{"mode","nearby"},{"port",1234},{"fingerprint",QString(64,'a')},{"token",QString(32,'b')}};
        QCOMPARE(melonds::event(encode(nearby))["endpoint"].toObject(),nearby);
        nearby["port"]=1234.5;QVERIFY(melonds::event(encode(nearby)).isEmpty());nearby["port"]=1234;
        nearby["savePath"]="untrusted";QVERIFY(melonds::event(encode(nearby)).isEmpty());
        QVERIFY(melonds::event("TRAINEROS_DS_ENDPOINT !").isEmpty());
        QVERIFY(melonds::event(encode({{"mode","online"},{"code","1-invalid"}})).isEmpty());
        const QJsonObject online{{"mode","online"},{"code","1-one-two-three-four-five-six-seven-eight"}};
        QCOMPARE(melonds::event(encode(online))["endpoint"].toObject(),online);
    }
    void genericPspKeepsSaveConsentAndRejectsChangedInvitation() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        requestRestart(f,{{"id","runtime.ppsspp.adhoc.v1"},{"content","accepted"}});
        QVERIFY(f.process.active());QVERIFY(f.runtime.restarting_);
        f.lifecycle.exitController().cancel();
        QVERIFY(f.process.active());QVERIFY(!f.runtime.active_);
        finishOrdinary(f);
        f.runtime.active_=true;f.runtime.host_=false;
        f.runtime.descriptor_={{"id","runtime.ppsspp.adhoc.v1"},{"content","accepted"}};
        f.runtime.frame({{"kind","psp-ready"},{"identity",QJsonObject{{"id","runtime.ppsspp.adhoc.v1"},{"content","changed"}}}});
        QVERIFY(!f.runtime.active_);QVERIFY(!f.process.active());
    }
    void dreamcastHostSaveCancellationAndStaleMarkers() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        requestRestart(f,{{"id","runtime.flycast-dojo.dc.v1"}});
        QVERIFY(f.process.active());QVERIFY(f.runtime.restarting_);
        f.lifecycle.exitController().cancel();QVERIFY(f.process.active());QVERIFY(!f.runtime.active_);
        f.runtime.output("[TrainerOSFlycast] running\n");QVERIFY(!f.runtime.active_);
        finishOrdinary(f);
    }
    void dreamcastRejectsChangedIdentityAndTracksDisconnect() {
        Fixture f;f.runtime.active_=true;f.runtime.descriptor_={{"id","runtime.flycast-dojo.dc.v1"},{"content","accepted"}};
        f.runtime.frame({{"kind","flycast-ready"},{"slot",2},{"identity",QJsonObject{{"id","runtime.flycast-dojo.dc.v1"},{"content","other"}}}});
        QVERIFY(!f.runtime.active_);
        f.runtime.active_=true;f.runtime.deadline_=1;
        f.runtime.output("[TrainerOSFlycast] run");QCOMPARE(f.runtime.deadline_,1);
        f.runtime.output("ning\n");QCOMPARE(f.runtime.deadline_,0);QCOMPARE(f.runtime.status_,"Playing together");
        f.runtime.output("[TrainerOSFlycast] error\n");QVERIFY(!f.runtime.active_);
    }
    void n64HostRetainsSaveQuestionAndCancellationStopsPreparation() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        requestRestart(f,{{"id","runtime.rmg.n64.v1"}});
        QVERIFY(f.process.active());QVERIFY(f.runtime.restarting_);QVERIFY(!f.runtime.rmgPreparing_);
        f.lifecycle.exitController().cancel();
        QVERIFY(f.process.active());QVERIFY(!f.runtime.active_);QVERIFY(!f.runtime.rmgPreparing_);
        int launches=0;f.runtime.requestRuntimeLaunch=[&](const ProcessCommand&,const QString&){++launches;return AdventureResult{false,{}};};
        emit f.runtime.rmgLobby_.launchReady("127.0.0.1",45001,1);QCOMPARE(launches,0);
        finishOrdinary(f);
    }
    void n64RejectsChangedIdentityBeforeRoomPreparation() {
        for(const auto& kind:{"rmg-ready","rmg-private-ready"}) {
            Fixture f;f.runtime.active_=true;f.runtime.descriptor_={{"id","runtime.rmg.n64.v1"},{"content","accepted"}};
            f.runtime.frame({{"kind",kind},{"identity",QJsonObject{{"id","runtime.rmg.n64.v1"},{"content","other"}}}});
            QVERIFY(!f.runtime.active_);QVERIFY(!f.runtime.rmgPreparing_);
        }
    }
    void n64PrivateNativeReadinessAndLateOutputCannotRestartCancelledGame() {
        Fixture f;f.runtime.active_=true;f.runtime.deadline_=1;
        f.runtime.descriptor_={{"id","runtime.rmg.n64.v1"}};f.runtime.rmgRequest_.privateLan=true;
        const auto event=[](QString kind){return QByteArray("TRAINEROS_NATIVE_LAN ")+
            QJsonDocument(QJsonObject{{"event",kind}}).toJson(QJsonDocument::Compact).toBase64()+"\n";};
        const auto started=event("running");
        f.runtime.output(started.left(12));QCOMPARE(f.runtime.deadline_,1);
        f.runtime.output(started.mid(12));QCOMPARE(f.runtime.deadline_,0);QCOMPARE(f.runtime.status_,"Playing together");
        f.runtime.output(event("error"));QVERIFY(!f.runtime.active_);
        int launches=0;f.runtime.requestRuntimeLaunch=[&](const ProcessCommand&,const QString&){++launches;return AdventureResult{false,{}};};
        f.runtime.output(started);QVERIFY(!f.runtime.active_);QCOMPARE(launches,0);
    }
    void pendingInvitationSurvivesUnrelatedGameExitAndCanBeCollapsed() {
        Fixture f;GameParty host;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        inviteWhilePlaying(f,host);const auto request=f.runtime.invitationId();QVERIFY(!request.isEmpty());
        QVERIFY(f.runtime.invitationBadge());const auto pid=f.process.processId();
        readyMenu(f);f.runtime.openInvitation(request);
        QCOMPARE(f.view.panel(),"multiplayer-request");
        f.runtime.action("multiplayer-later:"+request);
        QVERIFY(!f.view.visible());QCOMPARE(f.process.processId(),pid);QCOMPARE(f.runtime.invitationId(),request);
        finishOrdinary(f);QCOMPARE(f.runtime.invitationId(),request);QVERIFY(f.runtime.invitationBadge());
        f.runtime.openInvitation(request);QVERIFY(f.social.runtimeSurfaceOpen());
        f.runtime.answer(false);QVERIFY(f.runtime.invitationId().isEmpty());
    }
    void acceptingPlayingGuestWaitsForCleanExitBeforeSendingConsent_data() {
        QTest::addColumn<bool>("failure");QTest::newRow("clean")<<false;QTest::newRow("save-failure")<<true;
    }
    void acceptingPlayingGuestWaitsForCleanExitBeforeSendingConsent() {
        QFETCH(bool,failure);Fixture f;GameParty host;QVERIFY(f.start(failure));QTRY_COMPARE(f.lifecycle.state(),"running");
        inviteWhilePlaying(f,host);const auto request=f.runtime.invitationId();readyMenu(f);
        int accepted=0;bool acceptedWhileRunning=false;
        connect(&f.runtime.party_,&GameParty::outgoing,&host,[&](QString,QJsonObject packet){
            if(packet["kind"]=="accept"){++accepted;acceptedWhileRunning|=f.process.active()||f.lifecycle.active();}
        });
        f.runtime.answer(true);QCOMPARE(accepted,0);QTRY_VERIFY(!f.lifecycle.active());
        if(!failure)QTRY_COMPARE(accepted,1);else {QCoreApplication::processEvents();QCOMPARE(accepted,0);QCOMPARE(f.runtime.invitationId(),request);}
        QVERIFY(!acceptedWhileRunning);QVERIFY(f.runtime.exitingInvitation_.isEmpty());
    }
    void cancelledInvitationDuringCaptureKeepsTheGameAndCannotAcceptReplacement() {
        Fixture f;GameParty host;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        inviteWhilePlaying(f,host);const auto request=f.runtime.invitationId();readyMenu(f);
        disconnect(&f.lifecycle.exitController(),&AdventureExitController::captureRequested,&f.view,nullptr);
        f.runtime.answer(true);QCOMPARE(f.lifecycle.exitController().phase(),AdventureExitController::Phase::Capturing);
        host.cancelInvite("guest");QVERIFY(f.runtime.exitingInvitation_.isEmpty());
        QCOMPARE(f.lifecycle.exitController().phase(),AdventureExitController::Phase::Idle);QVERIFY(f.process.active());
        host.invite("guest");QVERIFY(request!=f.runtime.invitationId());
        f.runtime.action("multiplayer-accept:"+request);QVERIFY(f.runtime.exitingInvitation_.isEmpty());
        QVERIFY(!f.runtime.party_.active());finishOrdinary(f);
    }
    void locallySupportedNativeJoinRetainsTheRunningGame() {
        Fixture f;GameParty host;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        const auto game=inviteWhilePlaying(f,host);const auto pid=f.process.processId();int joined=0;
        f.runtime.runningJoin.available=[&](QString id,QJsonObject profile){return id=="ordinary"&&profile==game;};
        f.runtime.runningJoin.join=[&](QString id,QJsonObject profile,QJsonObject endpoint){
            ++joined;return id=="ordinary"&&profile==game&&endpoint["kind"]=="native-ready";
        };
        QSignalSpy closes(&f.lifecycle.exitController(),&AdventureExitController::captureRequested);
        f.runtime.answer(true);QVERIFY(f.runtime.party_.active());host.start();
        host.ready({{"kind","native-ready"},{"identity",game}});
        QCOMPARE(joined,1);QCOMPARE(f.process.processId(),pid);QVERIFY(closes.isEmpty());finishOrdinary(f);
    }
    void remoteNativeHintCannotBypassExitOrAuthorizeAnotherRunningGame() {
        Fixture f;GameParty host;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        auto game=inviteWhilePlaying(f,host);QVERIFY(!f.runtime.canJoinRunning(game));
        f.runtime.runningJoin.available=[](QString,QJsonObject){return true;};
        f.runtime.runningJoin.join=[](QString,QJsonObject,QJsonObject){return true;};
        auto other=game;other["content"]="different";other["joinInPlace"]=true;
        QVERIFY(!f.runtime.canJoinRunning(other));finishOrdinary(f);
    }
    void linkedFinalEndpointWaitsForOwnSeedAndCancellationClearsIt_data() {
        QTest::addColumn<QString>("mode");
        QTest::newRow("gb")<<QString("sameboy-linked-pair-battery-v1");
        QTest::newRow("gba")<<QString("mgba-linked-party-v2");
    }
    void linkedFinalEndpointWaitsForOwnSeedAndCancellationClearsIt() {
        QFETCH(QString,mode);
        Fixture f;f.runtime.active_=true;f.runtime.linkedPreparing_=true;f.runtime.launchPending_=true;
        f.runtime.descriptor_={{"settings",mode},{"content","bound"}};
        f.runtime.request_.ownSram="private";
        const QJsonObject ready{{"kind","ready"},{"identity",f.runtime.descriptor_},{"port",55435},{"machines",2}};
        int launches=0;f.library.inspect=[&]{++launches;};
        f.runtime.frame(ready);QCOMPARE(launches,0);QCOMPARE(f.runtime.linkedEndpoint_,ready);
        auto wrong=ready;wrong["identity"]=QJsonObject{{"content","other"}};f.runtime.frame(wrong);
        QCOMPARE(f.runtime.linkedEndpoint_,ready);
        f.runtime.leaveParty();QVERIFY(!f.runtime.active_);QVERIFY(!f.runtime.linkedPreparing_);
        QVERIFY(f.runtime.linkedEndpoint_.isEmpty());QVERIFY(f.runtime.request_.ownSram.isEmpty());
        emit f.runtime.linkedSave_.completed("late peer bytes");QCOMPARE(launches,0);
        f.runtime.frame(ready);QCOMPARE(launches,0);
    }
    void fourLinkedBatteriesWaitForEverySeatAndCancelTogether() {
        Fixture f;f.runtime.active_=true;f.runtime.host_=true;f.runtime.linkedPreparing_=true;
        f.runtime.request_.linkedPlayers=4;f.runtime.request_.ownSram=QByteArray(512,'a');
        int launches=0;f.library.inspect=[&]{++launches;};
        f.runtime.linkedTransferCompleted(4,QByteArray(512,'d'));
        f.runtime.linkedTransferCompleted(2,QByteArray(512,'b'));
        QCOMPARE(launches,0);QVERIFY(f.runtime.linkedPreparing_);
        QCOMPARE(f.runtime.request_.peerSrams[4],QByteArray(512,'d'));
        f.runtime.linkedTransferCompleted(3,QByteArray(512,'c'));
        QCOMPARE(launches,1); // Missing library record stops the real launch safely.
        QVERIFY(!f.runtime.active_);QVERIFY(f.runtime.request_.peerSrams.isEmpty());
        f.runtime.linkedTransferCompleted(3,QByteArray(512,'z'));QCOMPARE(launches,1);
    }
    void duplicateOrMalformedLinkedSeatCannotCompletePreparation() {
        for(bool duplicate:{false,true}) {
            Fixture f;f.runtime.active_=true;f.runtime.host_=true;f.runtime.linkedPreparing_=true;
            f.runtime.request_.linkedPlayers=4;f.runtime.request_.ownSram=QByteArray(512,'a');
            int launches=0;f.library.inspect=[&]{++launches;};
            f.runtime.linkedTransferCompleted(2,QByteArray(512,'b'));
            f.runtime.linkedTransferCompleted(duplicate?2:3,QByteArray(duplicate?512:511,'c'));
            QCOMPARE(launches,0);QVERIFY(!f.runtime.active_);QVERIFY(f.runtime.request_.peerSrams.isEmpty());
        }
    }
    void leavingDuringLinkedPreparationCancelsAllOwnedTransfers() {
        Fixture f;GameParty guest;
        const QJsonObject game{{"id","test"},{"settings","mgba-linked-party-v2"},{"players",4},{"lateJoin",false}};
        f.runtime.party_.configure("host",{game},game,true);guest.configure("guest",{game},{},true);
        connect(&f.runtime.party_,&GameParty::outgoing,&guest,[&](QString,QJsonObject p){guest.receive("host","host",p);});
        connect(&guest,&GameParty::outgoing,&f.runtime.party_,[&](QString,QJsonObject p){f.runtime.party_.receive("guest","guest",p);});
        f.runtime.party_.invite("guest");guest.answer(true);
        f.runtime.active_=f.runtime.host_=f.runtime.linkedPreparing_=f.runtime.partySession_=true;
        f.runtime.descriptor_=game;f.runtime.linkedPeers_={{1,""},{2,"guest"}};
        f.runtime.request_.ownSram="own";f.runtime.request_.peerSrams[2]="guest";
        f.runtime.linkedTransfers_[2]=new retroarch::LinkedSavePreparation(&f.runtime);
        QVERIFY(f.runtime.linkedRosterMatches());guest.leave();
        QVERIFY(!f.runtime.active_);QVERIFY(f.runtime.linkedTransfers_.isEmpty());
        QVERIFY(f.runtime.request_.ownSram.isEmpty());QVERIFY(f.runtime.request_.peerSrams.isEmpty());
    }
    void verifiedGameIsAvailableWhileOtherGamesAreStillScanning() {
        Fixture f;
        AdventureRegistration first;first.adventure.id="verified";first.adventure.platformId="nes";
        first.integrationConfig["core"]="fceumm";
        auto pending=first;pending.adventure.id="not-yet-checked";
        f.library.records={first,pending};
        // Keep the worker unfinished to exercise result delivery rather than
        // relying on the relative speed of small and large files on this host.
        f.runtime.refresh({},"test-trainer",true);QTRY_VERIFY(!f.runtime.scan_.isRunning());
        QPromise<RuntimeMultiplayer::ScannedGame> scan;
        scan.start();f.runtime.scan_.setFuture(scan.future());
        f.runtime.refresh({},"test-trainer",true);
        QVERIFY(f.runtime.onlineGames().isEmpty());
        const auto profile=retroarch::netplayProfile("nes","fceumm",QString(64,'a'));
        QVERIFY(!profile.isEmpty());
        scan.addResult({first.adventure.id,profile});
        QTRY_COMPARE(f.runtime.onlineGames(),QStringList{"verified"});
        QCOMPARE(f.runtime.nearbyGames(),QStringList{"verified"});
        QCOMPARE(f.runtime.multiplayerGames(),QStringList{"verified"});
        QVERIFY(f.runtime.scan_.isRunning());
        auto nearby=profile;nearby["connections"]=QJsonArray{"nearby"};
        scan.addResult({pending.adventure.id,nearby});
        QTRY_COMPARE(f.runtime.nearbyGames().size(),2);
        QCOMPARE(f.runtime.onlineGames(),QStringList{"verified"});
        QCOMPARE(f.runtime.multiplayerGames().size(),2);
        f.runtime.refresh({},"test-trainer",false);
        QVERIFY(f.runtime.onlineGames().isEmpty());
        QVERIFY(f.runtime.nearbyGames().isEmpty());QVERIFY(f.runtime.multiplayerGames().isEmpty());
        scan.addResult({pending.adventure.id,profile});
        QCoreApplication::processEvents();
        QVERIFY(f.runtime.onlineGames().isEmpty());
        scan.finish();QTRY_VERIFY(!f.runtime.scan_.isRunning());
    }
    void launchedUncheckedGameMovesAheadOfPendingLibraryScan() {
        Fixture f;AdventureRegistration game;game.adventure.id="newly-launched";game.adventure.platformId="nes";
        game.integrationConfig["core"]="fceumm";f.library.records={game};
        f.runtime.refresh({},"test-trainer",true);QTRY_VERIFY(!f.runtime.scan_.isRunning());
        QPromise<RuntimeMultiplayer::ScannedGame> scan;scan.start();f.runtime.scan_.setFuture(scan.future());
        f.runtime.refresh({},"test-trainer",true);QVERIFY(!scan.isCanceled());
        QVERIFY(f.start(false,game.adventure.id));QTRY_COMPARE(f.lifecycle.state(),"running");
        f.runtime.refresh({},"test-trainer",true);QVERIFY(scan.isCanceled());scan.finish();
        QTRY_COMPARE(f.runtime.scanPriority_,game.adventure.id);QTRY_VERIFY(!f.runtime.scan_.isRunning());
        finishOrdinary(f);
    }
    void changedLibraryRetiresObsoleteDiscScan() {
        Fixture f;AdventureRegistration first;first.adventure.id="old";first.adventure.platformId="nes";
        first.integrationConfig["core"]="fceumm";f.library.records={first};
        f.runtime.refresh({},"test-trainer",true);QTRY_VERIFY(!f.runtime.scan_.isRunning());
        const auto revision=f.runtime.scanRevision_;
        QPromise<RuntimeMultiplayer::ScannedGame> scan;scan.start();f.runtime.scan_.setFuture(scan.future());
        f.runtime.refresh({},"test-trainer",true);QVERIFY(!scan.isCanceled());
        auto added=first;added.adventure.id="newly-discovered";f.library.records.append(added);
        f.runtime.refresh({},"test-trainer",true);QVERIFY(scan.isCanceled());
        QVERIFY(!scan.addResult({"old",retroarch::netplayProfile("nes","fceumm",QString(64,'a'))}));
        scan.finish();QTRY_VERIFY(f.runtime.scanRevision_!=revision);
        QTRY_VERIFY(!f.runtime.scan_.isRunning());QVERIFY(f.runtime.multiplayerGames().isEmpty());
    }
    void initTestCase() {
        QVERIFY(settings_.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_.path());
        QCoreApplication::setOrganizationName("TrainerOS test");
        QCoreApplication::setApplicationName("Runtime transition");
    }
    void savePromptRetainsOwnedGameAndCancelDoesNotLaunch() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");
        const auto pid=f.process.processId();requestRestart(f);
        QVERIFY(!f.runtime.timer_.isActive());QCOMPARE(f.runtime.deadline_,0);
        QVERIFY(f.runtime.restarting_);QCOMPARE(f.process.processId(),pid);
        QSignalSpy notice(&f.runtime,&RuntimeMultiplayer::notice);
        QVERIFY(f.lifecycle.exitController().cancel());
        QVERIFY(!f.runtime.active_);QVERIFY(!f.runtime.launchPending_);
        QCOMPARE(f.process.processId(),pid);QVERIFY(f.lifecycle.active());
        QVERIFY(!notice.isEmpty());
        QFile finish(f.dir.filePath("control"));QVERIFY(finish.open(QIODevice::WriteOnly));finish.write("exit");finish.close();
        QTRY_VERIFY(!f.process.active());
    }
    void successfulCloseStartsANewConnectionBudget() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");requestRestart(f);
        qint64 connectionDeadline=0;bool timerRunning=false;
        f.library.inspect=[&]{connectionDeadline=f.runtime.deadline_;timerRunning=f.runtime.timer_.isActive();};
        QVERIFY(f.lifecycle.exitController().confirm());
        QTRY_VERIFY(connectionDeadline>0);
        QVERIFY(timerRunning);QVERIFY(connectionDeadline>=QDateTime::currentSecsSinceEpoch()+70);
        QVERIFY(!f.process.active());
    }
    void failedSaveFinalizationPreventsMultiplayerLaunch() {
        Fixture f;QVERIFY(f.start(true));QTRY_COMPARE(f.lifecycle.state(),"running");requestRestart(f);
        int launches=0;f.library.inspect=[&]{++launches;};
        QSignalSpy notice(&f.runtime,&RuntimeMultiplayer::notice);
        QVERIFY(f.lifecycle.exitController().confirm());QTRY_VERIFY(!f.lifecycle.active());
        QCoreApplication::processEvents();
        QCOMPARE(launches,0);QVERIFY(!f.runtime.active_);QVERIFY(!f.runtime.timer_.isActive());
        QVERIFY(!notice.isEmpty());QCOMPARE(notice.last()[0].toString(),"Save recovery required");
    }
    void queuedLaunchCannotAttachToAReplacementInvitation() {
        Fixture f;QVERIFY(f.start());QTRY_COMPARE(f.lifecycle.state(),"running");requestRestart(f);
        int launches=0;f.library.inspect=[&]{++launches;};
        connect(&f.lifecycle,&AdventureLaunchController::adventureFinished,&f.runtime,[&]{
            // A new invitation can arrive before the old zero-delay callback.
            f.runtime.fail("Cancelled");f.runtime.active_=true;
            ++f.runtime.sessionGeneration_;f.runtime.game_="replacement";
        });
        QVERIFY(f.lifecycle.exitController().confirm());QTRY_VERIFY(!f.lifecycle.active());
        QCoreApplication::processEvents();
        QCOMPARE(launches,0);QVERIFY(f.runtime.active_);QCOMPARE(f.runtime.game_,"replacement");
    }
};
}
QTEST_GUILESS_MAIN(trainer::RuntimeMultiplayerTests)
#include "RuntimeMultiplayerTests.moc"
