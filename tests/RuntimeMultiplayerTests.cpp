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
        bool start(bool finalizerFails=false) {
#ifdef Q_OS_WIN
            const auto name="trainer_process_probe.exe";
#else
            const auto name="trainer_process_probe";
#endif
            ProcessCommand command{QDir(QCoreApplication::applicationDirPath()).filePath(name),
                {"controlled",dir.filePath("control"),dir.filePath("pid")},{}};
            if(finalizerFails)command.finalize=[](const ProcessOutcome&){return QString("Save recovery required");};
            return lifecycle.launch(command,{},"ordinary",AdventureSavePolicy::ManualConfirm);
        }
    };
    void requestRestart(Fixture& f) {
        f.lifecycle.exitController().setAvailable(true);
        QVERIFY(f.view.requestMenu());
        f.runtime.active_=true;f.runtime.host_=true;
        f.runtime.game_="missing-after-close";
        f.runtime.descriptor_={{"transport","netpacket"}};
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
        QPromise<RuntimeMultiplayer::ScannedGame> scan;
        scan.start();f.runtime.scan_.setFuture(scan.future());
        f.runtime.refresh({},"test-trainer",true);
        QVERIFY(f.runtime.onlineGames().isEmpty());
        const auto profile=retroarch::netplayProfile("nes","fceumm",QString(64,'a'));
        QVERIFY(!profile.isEmpty());
        scan.addResult({first.adventure.id,profile});
        QTRY_COMPARE(f.runtime.onlineGames(),QStringList{"verified"});
        QVERIFY(f.runtime.scan_.isRunning());
        f.runtime.refresh({},"test-trainer",false);
        QVERIFY(f.runtime.onlineGames().isEmpty());
        scan.addResult({pending.adventure.id,profile});
        QCoreApplication::processEvents();
        QVERIFY(f.runtime.onlineGames().isEmpty());
        scan.finish();QTRY_VERIFY(!f.runtime.scan_.isRunning());
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
