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
    void browserUsesManagedLaunchAndReportsFailure() {
        QTemporaryDir dir;AppsController apps;apps.configure(dir.path(),"alice");
        QSignalSpy commands(&apps,&AppsController::commandRequested),errors(&apps,&AppsController::webOpenFailed);
        QVERIFY(!apps.openWebUrl(QUrl("https://example.com/login")));seed(apps);commands.clear();
        for(const auto& url:{"file:///tmp/test","http://example.com/login","https://user:secret@example.com/"})QVERIFY(!apps.openWebUrl(QUrl(url)));
        apps.launchGuard=[]{return QString("Game owns input");};QVERIFY(!apps.openWebUrl(QUrl("https://example.com/login")));
        apps.launchGuard={};QVERIFY(apps.openWebUrl(QUrl("https://example.com/login?a=1")));
        QCOMPARE(commands.size(),1);QCOMPARE(commands[0][0].toJsonObject()["command"].toString(),"open-url");
        QVERIFY(!apps.openWebUrl(QUrl("https://example.com/duplicate")));
        apps.accept({{"event","error"},{"message","Browser unavailable"}});QCOMPARE(errors.size(),1);
        QVERIFY(apps.openWebUrl(QUrl("https://example.com/retry")));
        apps.accept({{"event","foreground"},{"id","browser.desktop"},{"pid","123"}});
        apps.control(1);QCOMPARE(commands.last()[0].toJsonObject()["command"].toString(),"minimize");
        apps.accept({{"event","shell"}});QVERIFY(apps.current().isEmpty());
        apps.accept({{"event","error"},{"message","Unrelated error"}});QCOMPARE(errors.size(),1);
    }
    void browserRestartsStoppedHelperAndWaitsForReadiness() {
        AppsController apps;QSignalSpy commands(&apps,&AppsController::commandRequested),errors(&apps,&AppsController::webOpenFailed);
        apps.start("traineros-missing-test-executable","fixture");QTRY_VERIFY(apps.status().contains("stopped"));
        QVERIFY(apps.openWebUrl(QUrl("https://example.com/recovery")));QVERIFY(commands.isEmpty());
        apps.accept({{"event","ready"}});QCOMPARE(commands.size(),1);
        QCOMPARE(commands.last()[0].toJsonObject()["url"],"https://example.com/recovery");
        QTRY_COMPARE(errors.size(),1);
        QVERIFY(apps.openWebUrl(QUrl("https://example.com/retry")));
        apps.launchGuard=[]{return QString("Game started while connecting");};apps.accept({{"event","ready"}});
        QCOMPARE(commands.size(),1);QCOMPARE(errors.size(),2);
    }
    void leavingForAnotherPrimaryPreservesBrowsingModeAndFocus() {
        QTemporaryDir dir;AppsController apps;apps.configure(dir.path(),"alice");seed(apps);
        apps.select(1);apps.leave(true);QCOMPARE(apps.focusIndex(),1);
        apps.options(1);apps.menuAction(0);apps.select(0,true);
        apps.leave(true);QVERIFY(apps.railFocused());QCOMPARE(apps.focusIndex(),0);
        apps.showHidden();apps.leave(true);QVERIFY(apps.hiddenView());
        apps.leave();QVERIFY(!apps.hiddenView());QVERIFY(!apps.railFocused());
    }
    void pinnedAndRunningAreSeparateWithoutDuplicates() {
        QTemporaryDir dir;AppsController apps;apps.configure(dir.path(),"alice");seed(apps);
        apps.options(1);apps.menuAction(0); // Writer sorts after Calculator but is pinned first.
        apps.accept({{"event","state"},{"running",QJsonArray{"a.desktop","z.desktop"}},{"current",""}});
        const auto rail=apps.rail();QCOMPARE(rail.size(),2);
        QCOMPARE(rail[0].toMap()["id"].toString(),"z.desktop");
        QVERIFY(rail[0].toMap()["railPinned"].toBool());QVERIFY(rail[0].toMap()["running"].toBool());
        QCOMPARE(rail[1].toMap()["id"].toString(),"a.desktop");QVERIFY(!rail[1].toMap()["railPinned"].toBool());
        apps.select(0,true);apps.options(0,true);apps.menuAction(0);
        QCOMPARE(apps.rail()[apps.focusIndex()].toMap()["id"].toString(),"z.desktop");
        apps.accept({{"event","state"},{"running",QJsonArray{"z.desktop"}},{"current",""}});
        QCOMPARE(apps.focusIndex(),0);QCOMPARE(apps.rail().size(),1);
    }
    void favoriteCapacityBlocksNewPinsWithoutDeletingSavedFavorites() {
        QTemporaryDir dir;AppsController apps;apps.configure(dir.path(),"alice");seed(apps);
        apps.setFavoriteCapacity(1);apps.options(0);apps.menuAction(0);
        apps.options(1);apps.menuAction(0);
        QCOMPARE(apps.rail().size(),1);QVERIFY(!apps.rows()[1].toMap()["favorite"].toBool());
        QVERIFY(apps.status().contains("full"));QVERIFY(!apps.menu().isEmpty());
        apps.setFavoriteCapacity(2);apps.menuAction(0);QCOMPARE(apps.rail().size(),2);
        apps.setFavoriteCapacity(1);QCOMPARE(apps.rail().size(),1);
        QVERIFY(apps.rows()[1].toMap()["favorite"].toBool());
        AppsController reopened;reopened.configure(dir.path(),"alice");seed(reopened);
        QCOMPARE(reopened.rail().size(),2);
    }
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
