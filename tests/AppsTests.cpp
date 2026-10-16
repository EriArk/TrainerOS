#include "features/apps/AppsController.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QSignalSpy>
using namespace trainer;
class AppsTests:public QObject {
    Q_OBJECT
    static void seed(AppsController& apps) {
        apps.accept({{"event","catalogue"},{"rows",QJsonArray{
            QJsonObject{{"id","z.desktop"},{"name","Writer"}},
            QJsonObject{{"id","a.desktop"},{"name","Calculator"}},
            QJsonObject{{"id","a.desktop"},{"name","Duplicate"}}}}});
        apps.accept({{"event","ready"}});
    }
private slots:
    void emptyQuickTrayNeverLaunchesTheUnderlyingGrid() {
        AppsController apps;seed(apps);QSignalSpy commands(&apps,&AppsController::commandRequested);
        apps.focusRail();apps.dispatch(Action::Confirm,true);apps.dispatch(Action::LocalAction,true);
        QVERIFY(commands.isEmpty());QVERIFY(apps.menu().isEmpty());
        apps.accept({{"event","state"},{"running",QJsonArray{"a.desktop","z.desktop"}},{"current",""}});
        apps.focusRail();apps.dispatch(Action::Right,true);QCOMPARE(apps.focusIndex(),1);
        apps.dispatch(Action::Confirm,true);QCOMPARE(commands.last()[0].toJsonObject()["id"],"z.desktop");
        apps.accept({{"event","state"},{"running",QJsonArray{}},{"current",""}});commands.clear();
        apps.dispatch(Action::Confirm,true);QVERIFY(commands.isEmpty());
    }
    void preferencesArePerTrainerAndSurviveReopen() {
        QTemporaryDir dir;AppsController apps;apps.configure(dir.path(),"alice");seed(apps);
        QCOMPARE(apps.rows().size(),2);QCOMPARE(apps.rows()[0].toMap()["name"].toString(),"Calculator");
        apps.options(0);apps.menuAction(0);QCOMPARE(apps.rail().size(),1);
        apps.options(0);apps.menuAction(1);QCOMPARE(apps.rows().size(),1);QVERIFY(apps.rail().isEmpty());
        AppsController reopened;reopened.configure(dir.path(),"alice");seed(reopened);
        QCOMPARE(reopened.rows().size(),1);reopened.showHidden();QCOMPARE(reopened.rows().size(),1);
        QVERIFY(reopened.rows()[0].toMap()["favorite"].toBool());
        reopened.options(0);reopened.menuAction(1);reopened.showHidden();QCOMPARE(reopened.rows().size(),2);
        reopened.configure(dir.path(),"bob");QCOMPARE(reopened.rows().size(),2);QVERIFY(reopened.rail().isEmpty());
    }
    void hiddenRunningAppRemainsReachable() {
        QTemporaryDir dir;AppsController apps;apps.configure(dir.path(),"alice");seed(apps);
        apps.options(0);apps.menuAction(1);
        apps.accept({{"event","state"},{"running",QJsonArray{"a.desktop"}},{"current",""}});
        QCOMPARE(apps.rows().size(),1);QCOMPARE(apps.rail().size(),1);
        QSignalSpy commands(&apps,&AppsController::commandRequested);
        apps.dispatch(Action::ToggleContinue);apps.dispatch(Action::Confirm);
        QCOMPARE(commands.last()[0].toJsonObject()["id"].toString(),"a.desktop");
        apps.accept({{"event","state"},{"running",QJsonArray{}},{"current",""}});
        QVERIFY(apps.rail().isEmpty());QVERIFY(!apps.railFocused());
    }
    void launchGuardAndOrdinaryCloseCommands() {
        AppsController apps;seed(apps);QSignalSpy commands(&apps,&AppsController::commandRequested);
        apps.launchGuard=[] {return QString("Game is running");};apps.open(0);QVERIFY(commands.isEmpty());
        QCOMPARE(apps.status(),"Game is running");apps.launchGuard={};apps.open(0);
        QCOMPARE(commands.last()[0].toJsonObject()["command"].toString(),"launch");
        QSignalSpy hide(&apps,&AppsController::foregroundRequested),show(&apps,&AppsController::shellRequested);
        apps.accept({{"event","foreground"},{"id","a.desktop"}});QCOMPARE(hide.size(),1);
        apps.accept({{"event","controls"}});apps.control(1);
        QCOMPARE(commands.last()[0].toJsonObject()["command"].toString(),"minimize");
        apps.accept({{"event","shell"}});QCOMPARE(show.size(),1);QVERIFY(apps.current().isEmpty());
        apps.accept({{"event","foreground"},{"id","a.desktop"}});apps.control(2);
        QCOMPARE(commands.last()[0].toJsonObject()["command"].toString(),"close");
        QVERIFY(!apps.current().isEmpty()); // A request is not confirmation of exit.
    }
    void failedPreferenceWriteDoesNotPretendSuccess() {
        AppsController apps;seed(apps);apps.options(0);apps.menuAction(1);
        QCOMPARE(apps.rows().size(),2);QVERIFY(!apps.status().isEmpty());
    }
};
QTEST_GUILESS_MAIN(AppsTests)
#include "AppsTests.moc"
