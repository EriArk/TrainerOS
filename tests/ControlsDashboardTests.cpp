#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QCryptographicHash>
#include "features/device/ControlsDashboard.h"
using namespace trainer;
class ControlsDashboardTests: public QObject {
    Q_OBJECT
    QTemporaryDir directory;
private slots:
    void initTestCase(){QCoreApplication::setOrganizationName("TrainerControlsTest");QCoreApplication::setApplicationName("Dashboard");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());}
    void init(){QSettings().clear();}
    void cancelledMoveRestoresWholeLayout(){ControlsDashboard b;b.setOwner("one");b.begin();const auto original=b.widgets();b.edit();b.activate("volume");b.dispatch(Action::Right);QVERIFY(b.widgets()!=original);b.back();QCOMPARE(b.widgets(),original);QVERIFY(b.editing());}
    void ownerIsolationAndHiddenRestore(){ControlsDashboard b;b.setOwner("one");b.edit();b.select("downloads");b.hide();QCOMPARE(b.widgets().size(),9);b.setOwner("two");QCOMPARE(b.widgets().size(),10);b.setOwner("one");QCOMPARE(b.widgets().size(),9);b.edit();b.pick();b.restore("downloads");QCOMPARE(b.widgets().size(),10);QVERIFY(!b.picking());}
    void pagesAndCancel(){ControlsDashboard b;b.setOwner("one");const auto original=b.widgets();b.edit();b.activate("downloads");b.changePage(1);QCOMPARE(b.page(),1);QCOMPARE(b.pages(),2);b.back();QCOMPARE(b.page(),0);QCOMPARE(b.widgets(),original);}
    void editNeverTriggersRadio(){ControlsDashboard b;b.setOwner("one");QSignalSpy actions(&b,&ControlsDashboard::actionRequested);b.edit();b.activate("wifi");QCOMPARE(actions.size(),0);b.back();b.edit();b.activate("wifi");QCOMPARE(actions.size(),1);}
    void sliderRequiresActivation(){ControlsDashboard b;b.setOwner("one");QSignalSpy levels(&b,&ControlsDashboard::adjustRequested);b.dispatch(Action::Up);QCOMPARE(levels.size(),0);b.activate("volume");b.dispatch(Action::Up);QCOMPARE(levels.size(),1);QCOMPARE(levels[0][1].toInt(),5);b.back();b.dispatch(Action::Up);QCOMPARE(levels.size(),1);}
    void focusDoesNotRebuildWidgets(){ControlsDashboard b;b.setOwner("one");QSignalSpy layout(&b,&ControlsDashboard::layoutChanged);b.select("wifi");b.dispatch(Action::Right);QCOMPARE(layout.size(),0);b.edit();b.hide();QCOMPARE(layout.size(),1);}
    void switchNeedsExplicitChoice(){ControlsDashboard b;b.setOwner("one");QSignalSpy action(&b,&ControlsDashboard::actionRequested);b.activate("switch");QVERIFY(b.switching());QVERIFY(action.isEmpty());b.back();QVERIFY(action.isEmpty());b.activate("switch");b.dispatch(Action::Down);b.dispatch(Action::Confirm);QCOMPARE(action.last()[0].toString(),QString("steam"));}
    void corruptLayoutFallsBack(){const auto key="controls/v1/"+QString::fromLatin1(QCryptographicHash::hash("bad",QCryptographicHash::Sha256).toHex());QSettings().setValue(key,QByteArray(9000,'x'));ControlsDashboard b;b.setOwner("bad");QCOMPARE(b.widgets().size(),10);QCOMPARE(b.pages(),1);}
};
QTEST_GUILESS_MAIN(ControlsDashboardTests)
#include "ControlsDashboardTests.moc"
