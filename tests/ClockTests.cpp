#include "features/settings/ClockController.h"
#include <QtTest>
#include <QSemaphore>
using namespace trainer;
class ClockTests : public QObject {
    Q_OBJECT
private slots:
    void realStateAndControllerEdits() {
        ClockController clock;
        ClockSnapshot state{"Asia/Jerusalem", {}, {"America/New_York", "Asia/Jerusalem", "Asia/Tokyo"}, true, true, true, true};
        QStringList writes;
        clock.read = [&]{ return state; };
        clock.write = [&](const QString& op, const QString& value){
            writes.append(op+":"+value);
            if(op=="automatic")state.automatic=value=="true";
            if(op=="zone")state.zone=value;
            return QString();
        };
        clock.begin(); QTRY_VERIFY(!clock.busy());
        clock.activate(2); QCOMPARE(clock.mode(),"main"); QVERIFY(writes.isEmpty());
        clock.activate(0); QTRY_VERIFY(!clock.busy()); QCOMPARE(writes.size(),1);
        clock.activate(2); QCOMPARE(clock.mode(),"manual");
        clock.adjust(1); clock.dispatch(Action::Back); QCOMPARE(writes.size(),1);
        clock.activate(1); QCOMPARE(clock.mode(),"zones");
        clock.dispatch(Action::Down); clock.dispatch(Action::Confirm); QTRY_VERIFY(!clock.busy());
        QCOMPARE(state.zone,"Asia/Tokyo"); QCOMPARE(clock.mode(),"main");
        clock.activate(2); clock.activate(5); QTRY_VERIFY(!clock.busy()); QVERIFY(writes.last().startsWith("time:"));
        clock.begin(true); QTRY_VERIFY(!clock.busy()); QCOMPARE(clock.focusIndex(),3);
        QSignalSpy continued(&clock,&ClockController::continueRequested);
        clock.dispatch(Action::Confirm); QCOMPARE(continued.size(),1);
    }
    void failureReadbackAndBusyNavigation() {
        ClockController clock; QSemaphore entered, release;
        ClockSnapshot state{"Asia/Jerusalem", {}, {"Asia/Jerusalem"}, true, true, true, true};
        clock.read = [&]{ return state; };
        clock.write = [&](const QString&,const QString&){ entered.release(); release.acquire(); return QString("Permission denied"); };
        clock.begin(); QTRY_VERIFY(!clock.busy()); clock.activate(0);
        QTRY_VERIFY(entered.available()); QSignalSpy back(&clock,&ClockController::backRequested);
        clock.dispatch(Action::Back); clock.activate(1); QCOMPARE(clock.mode(),"main"); QCOMPARE(back.size(),0);
        release.release(); QTRY_VERIFY(!clock.busy()); QCOMPARE(clock.error(),"Permission denied");
        QCOMPARE(clock.rows()[0].toMap()["detail"].toString(),"On");
        clock.read = []{return ClockSnapshot{{},"Service unavailable",{},false,false,false,false};};
        clock.begin(true); QTRY_VERIFY(!clock.busy());
        QVERIFY(!clock.rows()[0].toMap()["enabled"].toBool());
        QSignalSpy continued(&clock,&ClockController::continueRequested); clock.activate(3); QCOMPARE(continued.size(),1);
    }
    void manualDraftCannotOverwriteChangedSystemSettings() {
        ClockController clock;
        ClockSnapshot state{"UTC", {}, {"UTC"}, true, false, false, true};
        int writes=0; clock.read=[&]{return state;};
        clock.write=[&](const QString&,const QString&){++writes;return QString();};
        clock.begin();QTRY_VERIFY(!clock.busy());clock.activate(2);
        state.automatic=true;clock.activate(5);QTRY_VERIFY(!clock.busy());
        QCOMPARE(writes,0);QVERIFY(clock.error().contains("changed"));
        clock.dispatch(Action::Back);QCOMPARE(clock.mode(),"main");
    }
};
QTEST_GUILESS_MAIN(ClockTests)
#include "ClockTests.moc"
