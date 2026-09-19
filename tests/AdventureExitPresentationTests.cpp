#include "features/adventure/AdventureExitPresentation.h"
#include <QtTest>

using namespace trainer;
class AdventureExitPresentationTests final : public QObject {
    Q_OBJECT
private slots:
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
