#include <QtTest>
#include "features/crates/CratesController.h"
using namespace trainer;
class CratesTests: public QObject {
    Q_OBJECT
private slots:
    void authoredIdentity() {
        CratesController c;QCOMPARE(c.crates().size(),6);
        QSet<QString> ids;
        for(const auto& row:c.crates()){const auto v=row.toMap();QVERIFY(!v["title"].toString().isEmpty());ids.insert(v["id"].toString());}
        QCOMPARE(ids.size(),6);QCOMPARE(c.current()["id"].toString(),QString("whats-new"));
    }
    void directionalRowsAndEdges() {
        CratesController c;c.dispatch(Action::Right);QCOMPARE(c.focusIndex(),1);
        c.dispatch(Action::Right);QCOMPARE(c.focusIndex(),1);
        c.dispatch(Action::Down);QCOMPARE(c.focusIndex(),4);
        c.dispatch(Action::Right);QCOMPARE(c.focusIndex(),5);
        c.dispatch(Action::Up);QCOMPARE(c.focusIndex(),1);
        c.select(2);c.dispatch(Action::Left);QCOMPARE(c.focusIndex(),2);
        c.select(-1);c.open(99);QCOMPARE(c.focusIndex(),2);QVERIFY(!c.opening());
    }
    void backAndSkipRestoreFocus() {
        CratesController c;c.open(4);QVERIFY(c.opening());
        c.dispatch(Action::Confirm);QVERIFY(c.opened());QVERIFY(!c.opening());
        c.dispatch(Action::Right);QCOMPARE(c.focusIndex(),4);
        QVERIFY(c.dispatch(Action::Back));QVERIFY(!c.opened());QCOMPARE(c.focusIndex(),4);
        QVERIFY(!c.dispatch(Action::Back));
    }
    void interruptedOpenDoesNotNavigateLater() {
        CratesController c;c.open(3);c.leave();QTest::qWait(260);
        QVERIFY(!c.opened());QVERIFY(!c.opening());QCOMPARE(c.focusIndex(),3);
        c.open(3);c.back();QTest::qWait(260);QVERIFY(!c.opened());
    }
    void reducedMotionAndRouteRetention() {
        CratesController c;c.setProperty("reducedMotion",true);c.dispatch(Action::Confirm);
        QVERIFY(c.opened());QVERIFY(!c.opening());c.leave();QVERIFY(c.opened());
        c.back();c.open(5);QVERIFY(c.opened());QCOMPARE(c.focusIndex(),5);
    }
};
QTEST_GUILESS_MAIN(CratesTests)
#include "CratesTests.moc"
