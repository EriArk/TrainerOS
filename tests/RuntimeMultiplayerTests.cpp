#include "features/adventure/RuntimeMultiplayer.h"
#include "features/adventure/AdventureExitPresentation.h"
#include "features/social/SocialController.h"
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
        QList<AdventureRegistration> registrations() const override { return records; }
        QList<World> worlds() const override { return {}; }
        QList<Adventure> adventures() const override { return {}; }
        QList<ResumePoint> resumePoints() const override { return {}; }
        HomeSnapshot home() const override { return {}; }
        std::optional<AdventureRegistration> registration(const QString&) const override {
            if(inspect)inspect();return {};
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
        bool start(bool finalizerFails=false,QString game={},bool hotseat=false,QString kind="retroarch") {
#ifdef Q_OS_WIN
            const auto name="trainer_process_probe.exe";
#else
            const auto name="trainer_process_probe";
#endif
            ProcessCommand command{QDir(QCoreApplication::applicationDirPath()).filePath(name),
                {"controlled",dir.filePath("control"),dir.filePath("pid")},{}};
            if(!game.isEmpty())command.runtimeControls={{"game",game}};
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
        a.view.updateInput(a.view.inputGeneration(),{true,false,false,false,false,false,true});QCOMPARE(a.view.menuFocus(),1);
        host.action("multiplayer-pass:old:0");QVERIFY(host.handoff_.canPass());
        host.action(first);QVERIFY(!host.handoff_.canPass());QCOMPARE(a.view.menuFocus(),0);QVERIFY(!a.view.ready());
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
