#include "features/adventure/AdventureExitPresentation.h"
#include <QtTest>

using namespace trainer;
class AdventureExitPresentationTests final : public QObject {
    Q_OBJECT
private slots:
    void appearancePickerBackRetainsMenuAndHomeStillReturnsToGame() {
        AdventureExitController exit;AdventureExitPresentation view(exit);
        exit.beginSession(AdventureSavePolicy::ManualConfirm);exit.setAvailable(true);
        QSignalSpy preview(&view,&AdventureExitPresentation::menuCaptureRequested),actions(&view,&AdventureExitPresentation::menuActionRequested);
        QVERIFY(view.requestMenu());view.menuCaptureCompleted(preview.last()[0].toULongLong(),{});
        view.setInputIsolated(true);view.setWindowFocused(true);
        view.setPanel("display-choice","Choose shader",{QVariantMap{{"id","shader:off"},{"label","Off"}}},"display");
        view.updateInput(view.inputGeneration(),{true,true});view.cancel();
        QCOMPARE(actions.last()[0].toString(),QString("display"));QVERIFY(view.menuOpen());QCOMPARE(exit.phase(),AdventureExitController::Phase::Idle);
        view.updateInput(view.inputGeneration(),{true,true});view.updateInput(view.inputGeneration(),{true,false,false,false,true});
        QVERIFY(!view.menuOpen());QCOMPARE(exit.phase(),AdventureExitController::Phase::Idle);
    }
    void incomingCallActionsPreserveSelectionAndNeverRedirectConfirm() {
        AdventureExitController exit;AdventureExitPresentation view(exit);
        exit.beginSession(AdventureSavePolicy::ManualConfirm);exit.setAvailable(true);
        const QVariantMap notifications{{"id","notifications"},{"label","Notifications"}};
        const QVariantMap answer{{"id","answer-call:friend"},{"label","Answer"}};
        view.setExtraActions({notifications});
        QSignalSpy preview(&view,&AdventureExitPresentation::menuCaptureRequested);
        QSignalSpy requested(&view,&AdventureExitPresentation::menuActionRequested);
        QVERIFY(view.requestMenu());view.menuCaptureCompleted(preview.last()[0].toULongLong(),{});
        view.setInputIsolated(true);view.setWindowFocused(true);
        auto neutral=[&]{view.updateInput(view.inputGeneration(),{true,true});};
        auto down=[&]{neutral();view.updateInput(view.inputGeneration(),{true,false,false,false,false,false,true});};
        down();down();QCOMPARE(view.menuFocus(),2);
        view.setExtraActions({answer,notifications});QCOMPARE(view.menuFocus(),3);
        neutral();view.activateMenu(view.menuFocus());QCOMPARE(requested.last()[0].toString(),QString("notifications"));
        neutral();view.updateInput(view.inputGeneration(),{true,false,false,false,false,true,false});QCOMPARE(view.menuFocus(),2);
        view.setExtraActions({notifications});QCOMPARE(view.menuFocus(),0);
        QCOMPARE(exit.phase(),AdventureExitController::Phase::Idle);
        view.setPanel("call","Friend",{QVariantMap{{"id","mute"},{"label","Mute"}}});
        view.setExtraActions({answer,notifications});QCOMPARE(view.menuFocus(),0);QCOMPARE(view.panel(),QString("call"));
    }
    void extendedMenuRetainsProcessAndBackReturnsToRoot() {
        AdventureExitController exit;AdventureExitPresentation view(exit);
        exit.beginSession(AdventureSavePolicy::ManualConfirm);exit.setAvailable(true);
        view.setExtraActions({QVariantMap{{"id","display"},{"label","Display"}}});
        QSignalSpy preview(&view,&AdventureExitPresentation::menuCaptureRequested);
        QSignalSpy requested(&view,&AdventureExitPresentation::menuActionRequested);
        QVERIFY(view.requestMenu());view.menuCaptureCompleted(preview.last()[0].toULongLong(),{});
        view.setInputIsolated(true);view.setWindowFocused(true);
        auto neutral=[&]{view.updateInput(view.inputGeneration(),{true,true});};neutral();view.activateMenu(2);
        QCOMPARE(requested.last()[0].toString(),QString("display"));QCOMPARE(exit.phase(),AdventureExitController::Phase::Idle);
        view.setPanel("display","Next launch",{QVariantMap{{"id","ratio"},{"label","Screen"}}});
        QVERIFY(!view.ready());neutral();view.cancel();QCOMPARE(view.menuActions().size(),3);QVERIFY(view.menuOpen());
        view.setPanel("notifications","Unread",{QVariantMap{{"id","notice:1"},{"label","Friend"},{"readOnly",true}}});
        neutral();const auto actions=requested.size();view.activateMenu(0);
        QCOMPARE(requested.size(),actions);QVERIFY(!view.menuCanSelect());QVERIFY(view.menuOpen());
        view.cancel();neutral();
        neutral();view.cancel();QVERIFY(!view.menuOpen());QCOMPARE(exit.phase(),AdventureExitController::Phase::Idle);
    }
    void menuKeepsSessionAndRequiresExplicitFreshExit() {
        AdventureExitController exit; AdventureExitPresentation view(exit);
        QSignalSpy captures(&exit, &AdventureExitController::captureRequested);
        QSignalSpy previews(&view, &AdventureExitPresentation::menuCaptureRequested);
        QSignalSpy dismissed(&view, &AdventureExitPresentation::menuDismissed);
        QSignalSpy completed(&exit, &AdventureExitController::completed);
        exit.beginSession(AdventureSavePolicy::ManualConfirm); exit.setAvailable(true);
        auto feed = [&](ExitInputSnapshot s) { view.updateInput(view.inputGeneration(),s); };
        auto open = [&] {
            QVERIFY(view.requestMenu()); QVERIFY(!view.requestMenu());
            view.setInputIsolated(true);
            QImage image(60,40,QImage::Format_RGB32); image.fill(Qt::green);
            view.menuCaptureCompleted(previews.last()[0].toULongLong(),image);
            view.setWindowFocused(true);
        };
        open(); QVERIFY(view.menuOpen()); QVERIFY(view.visible()); QVERIFY(captures.isEmpty());
        QCOMPARE(exit.phase(), AdventureExitController::Phase::Idle);
        feed({true,false,true}); QVERIFY(!view.ready());
        feed({true,true}); feed({true,false,true}); // Continue
        QVERIFY(!view.visible()); QCOMPARE(dismissed.size(),1); QVERIFY(captures.isEmpty());
        open(); feed({true,true}); feed({true,false,false,false,true}); // Home again
        QVERIFY(!view.visible()); QCOMPARE(dismissed.size(),2);
        open(); feed({true,true});
        feed({true,false,true,false,false,false,true}); // Down+A only selects Exit.
        QCOMPARE(view.menuFocus(),1); QVERIFY(captures.isEmpty());
        feed({true,true}); feed({true,false,true});
        QCOMPARE(captures.size(),1); QVERIFY(!view.visible());
        QVERIFY(view.frame().isNull()); // Menu preview never becomes exit media.
        exit.captureCompleted(captures.last()[0].toULongLong(),{},"No capture");
        view.setInputIsolated(true); feed({true,false,true}); QVERIFY(!view.ready());
        feed({true,true}); feed({true,false,false,false,true});
        QCOMPARE(exit.phase(),AdventureExitController::Phase::Idle); QVERIFY(completed.isEmpty());
    }
    void menuLeaseLossAndOldPreviewCannotOpenAnotherSession() {
        AdventureExitController exit; AdventureExitPresentation view(exit);
        QSignalSpy previews(&view,&AdventureExitPresentation::menuCaptureRequested);
        exit.beginSession(AdventureSavePolicy::Unknown); exit.setAvailable(true);
        QVERIFY(view.requestMenu()); const auto old=previews.last()[0].toULongLong();
        exit.endSession(false); view.menuCaptureCompleted(old,{}); QVERIFY(!view.visible());
        exit.beginSession(AdventureSavePolicy::Unknown); exit.setAvailable(true);
        QVERIFY(view.requestMenu()); view.menuCaptureCompleted(old,{}); QVERIFY(!view.visible());
        view.menuCaptureCompleted(previews.last()[0].toULongLong(),{}); QVERIFY(view.menuOpen());
        exit.setAvailable(false); QVERIFY(!view.visible());
    }
    void isolatedFocusedNeutralAndFreshAreAllRequired() {
        AdventureExitController exit;
        AdventureExitPresentation view(exit);
        QSignalSpy captures(&exit, &AdventureExitController::captureRequested);
        QSignalSpy close(&exit, &AdventureExitController::gracefulExitRequested);
        QSignalSpy resume(&exit, &AdventureExitController::returnToGameRequested);
        exit.beginSession(AdventureSavePolicy::Unknown); exit.setAvailable(true);
        QVERIFY(exit.requestExit()); QVERIFY(!view.visible());
        const auto attempt = captures.last()[0].toULongLong();
        exit.captureCompleted(attempt, {}, "Fixture capture failure");
        QVERIFY(view.visible()); QVERIFY(view.captureFailed());
        const auto feed = [&](ExitInputSnapshot sample) { view.updateInput(view.inputGeneration(), sample); };
        feed({true, true}); view.confirm(); QVERIFY(close.isEmpty());
        view.setInputIsolated(true); feed({true, true}); QVERIFY(!view.ready());
        view.setWindowFocused(true);
        feed({true, false, true}); QVERIFY(!view.ready()); // Opening A is still held.
        feed({true, true, true}); QVERIFY(!view.ready()); // Inconsistent provider cannot arm A.
        feed({true, true}); QVERIFY(view.ready());
        const auto stale = view.inputGeneration();
        view.setWindowFocused(false); view.setWindowFocused(true);
        view.updateInput(stale, {true, true}); QVERIFY(!view.ready());
        feed({true, false, true}); QVERIFY(close.isEmpty());
        feed({true, true}); feed({true, false, true, true}); // A+B chooses Back.
        QCOMPARE(resume.size(), 1); QVERIFY(close.isEmpty()); QVERIFY(!view.visible());
        QVERIFY(exit.requestExit()); exit.captureCompleted(captures.last()[0].toULongLong(), {}, "Failed");
        feed({true, true}); QVERIFY(!view.ready()); // Each attempt needs a new lease.
        view.setInputIsolated(true); feed({true, true}); QVERIFY(view.ready());
        feed({false}); QVERIFY(!view.ready()); // Disconnect requires release on reconnect.
        feed({true, false, true}); QVERIFY(close.isEmpty());
        feed({true, true}); feed({true, false, true});
        QCOMPARE(close.size(), 1); QVERIFY(view.visible()); QVERIFY(!view.confirming());
        view.cancel(); view.confirm(); feed({true, false, false, true});
        QCOMPARE(resume.size(), 1); QCOMPARE(close.size(), 1);
        exit.endSession(false); QVERIFY(!view.visible()); // A crash cannot leave a stranded question.
    }
    void revokedLeaseAndRejectedCloseRequireFreshInput() {
        AdventureExitController exit; AdventureExitPresentation view(exit);
        QSignalSpy captures(&exit, &AdventureExitController::captureRequested);
        QSignalSpy closes(&exit, &AdventureExitController::gracefulExitRequested);
        exit.beginSession(AdventureSavePolicy::ManualConfirm); exit.setAvailable(true);
        auto prompt = [&] {
            QVERIFY(exit.requestExit());
            exit.captureCompleted(captures.last()[0].toULongLong(), {}, "Failed");
            view.setInputIsolated(true); view.setWindowFocused(true);
            view.updateInput(view.inputGeneration(), {true, true});
        };
        prompt(); QVERIFY(view.ready());
        auto old = view.inputGeneration(); view.setInputIsolated(false); view.confirm();
        QVERIFY(closes.isEmpty()); view.setInputIsolated(true);
        view.updateInput(old, {true, true}); QVERIFY(!view.ready());
        view.updateInput(view.inputGeneration(), {true, true}); view.confirm();
        QCOMPARE(closes.size(), 1);
        exit.gracefulExitFailed(closes.last()[0].toULongLong(), "Rejected");
        QVERIFY(!view.visible());
        prompt(); QVERIFY(view.ready()); view.cancel(); QVERIFY(!view.visible());
    }
};
QTEST_GUILESS_MAIN(AdventureExitPresentationTests)
#include "AdventureExitPresentationTests.moc"
