#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "features/device/ControlsDashboard.h"
using namespace trainer;
class ControlsDashboardTests: public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    static QVariantMap row(const ControlsDashboard& b,const QString& id){for(const auto& v:b.widgets())if(v.toMap()["id"]==id)return v.toMap();return {};}
private slots:
    void initTestCase(){QCoreApplication::setOrganizationName("TrainerControlsTest");QCoreApplication::setApplicationName("Dashboard");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());}
    void init(){
        QSettings().clear();
        // Editing regressions use the supported compact layout. Migration to the
        // informational scraper widget has a separate test below.
        for(const auto& owner:QList<QByteArray>{"one","two","bad"}){
            const auto key="controls/v1/"+QString::fromLatin1(QCryptographicHash::hash(owner,QCryptographicHash::Sha256).toHex());
            QSettings().setValue(key+"/layoutRevision",4);
        }
    }
    void scraperInformationMigrationPreservesOtherWidgetsAndSavedLayout(){
        ControlsDashboard b;b.setOwner("fresh");QCOMPARE(b.pages(),2);
        const auto first=b.widgets();QCOMPARE(first.size(),9);QCOMPARE(row(b,"downloads")["w"].toInt(),4);QCOMPARE(row(b,"storage")["h"].toInt(),4);
        b.changePage(1);QCOMPARE(row(b,"scraper")["w"].toInt(),8);QCOMPARE(row(b,"scraper")["h"].toInt(),4);
        ControlsDashboard restored;restored.setOwner("fresh");QCOMPARE(restored.widgets(),first);
        restored.changePage(1);QCOMPARE(restored.widgets(),b.widgets());
    }
    void scraperMigrationUsesSpaceBesideAnExistingWidget(){
        ControlsDashboard b;b.setOwner("one");b.edit();b.drop("clock",12,0);b.edit();
        const auto key="controls/v1/"+QString::fromLatin1(QCryptographicHash::hash("one",QCryptographicHash::Sha256).toHex());
        QSettings().setValue(key+"/layoutRevision",2);
        ControlsDashboard restored;restored.setOwner("one");QCOMPARE(restored.pages(),2);restored.changePage(1);
        QCOMPARE(row(restored,"clock")["x"].toInt(),0);QCOMPARE(row(restored,"scraper")["x"].toInt(),2);
        QCOMPARE(row(restored,"scraper")["w"].toInt(),8);
    }
    void outputPickerDoesNotReplaceSliderOrEditor(){
        ControlsDashboard b;b.setOwner("one");QSignalSpy actions(&b,&ControlsDashboard::actionRequested);
        b.activate("volume");b.dispatch(Action::ToggleContinue);QVERIFY(b.adjusting());QCOMPARE(actions.last()[0].toString(),"audio-output");
        b.dispatch(Action::Secondary);QCOMPARE(actions.last()[0].toString(),"volume");
        actions.clear();b.edit();b.audioOutputs();QVERIFY(actions.isEmpty());
        b.edit();b.select("brightness");b.audioOutputs();QVERIFY(actions.isEmpty());
    }
    void cancelledMoveRestoresWholeLayout(){ControlsDashboard b;b.setOwner("one");b.begin();const auto original=b.widgets();b.edit();b.activate("volume");b.dispatch(Action::Right);QVERIFY(b.widgets()!=original);b.back();QCOMPARE(b.widgets(),original);QVERIFY(b.editing());}
    void ownerIsolationAndHiddenRestore(){ControlsDashboard b;b.setOwner("one");b.edit();b.select("downloads");b.hide();QCOMPARE(b.widgets().size(),9);b.setOwner("two");QCOMPARE(b.widgets().size(),10);b.setOwner("one");QCOMPARE(b.widgets().size(),9);b.edit();b.pick();b.restore("downloads");QCOMPARE(b.widgets().size(),10);QVERIFY(!b.picking());}
    void pagesAndCancel(){ControlsDashboard b;b.setOwner("one");const auto original=b.widgets();b.edit();b.activate("downloads");b.changePage(1);QCOMPARE(b.page(),1);QCOMPARE(b.pages(),2);b.back();QCOMPARE(b.page(),0);QCOMPARE(b.widgets(),original);}
    void editNeverTriggersRadio(){ControlsDashboard b;b.setOwner("one");QSignalSpy actions(&b,&ControlsDashboard::actionRequested);b.edit();b.activate("wifi");QCOMPARE(actions.size(),0);b.back();b.edit();b.activate("wifi");QCOMPARE(actions.size(),1);}
    void sliderRequiresActivation(){ControlsDashboard b;b.setOwner("one");QSignalSpy levels(&b,&ControlsDashboard::adjustRequested);b.dispatch(Action::Up);QCOMPARE(levels.size(),0);b.activate("volume");b.dispatch(Action::Up);QCOMPARE(levels.size(),1);QCOMPARE(levels[0][1].toInt(),5);b.back();b.dispatch(Action::Up);QCOMPARE(levels.size(),1);}
    void focusDoesNotRebuildWidgets(){ControlsDashboard b;b.setOwner("one");QSignalSpy layout(&b,&ControlsDashboard::layoutChanged);b.select("wifi");b.dispatch(Action::Right);QCOMPARE(layout.size(),0);b.edit();b.hide();QCOMPARE(layout.size(),1);}
    void switchNeedsExplicitChoice(){ControlsDashboard b;b.setOwner("one");QSignalSpy action(&b,&ControlsDashboard::actionRequested);b.activate("switch");QVERIFY(b.switching());QVERIFY(action.isEmpty());b.back();QVERIFY(action.isEmpty());b.activate("switch");b.dispatch(Action::Down);b.dispatch(Action::Confirm);QCOMPARE(action.last()[0].toString(),QString("steam"));}
    void corruptLayoutFallsBack(){const auto key="controls/v1/"+QString::fromLatin1(QCryptographicHash::hash("bad",QCryptographicHash::Sha256).toHex());QSettings().setValue(key,QByteArray(9000,'x'));ControlsDashboard b;b.setOwner("bad");QCOMPARE(b.widgets().size(),10);QCOMPARE(b.pages(),1);}
    void touchSwapPersistsAndInvalidDropIsAtomic(){
        ControlsDashboard b;b.setOwner("one");b.edit();b.drop("volume",1,0);
        QCOMPARE(row(b,"volume")["x"].toInt(),1);QCOMPARE(row(b,"brightness")["x"].toInt(),0);
        const auto swapped=b.widgets();b.drop("volume",4,1);QCOMPARE(b.widgets(),swapped);QVERIFY(!b.status().isEmpty());
        ControlsDashboard restored;restored.setOwner("one");QCOMPARE(restored.widgets(),swapped);
    }
    void touchMoveAcrossPageAndBack(){
        ControlsDashboard b;b.setOwner("one");b.edit();b.drop("downloads",12,0);
        QCOMPARE(b.page(),1);QCOMPARE(b.pages(),2);QCOMPARE(b.selected(),QString("downloads"));
        b.drop("downloads",-1,0);QCOMPARE(b.page(),0);QCOMPARE(b.pages(),2); // exchanges with equal-size storage
        QCOMPARE(row(b,"downloads")["x"].toInt(),8);QCOMPARE(b.widgets().size(),9);
        b.changePage(1);QCOMPARE(b.selected(),QString("storage"));b.hide();
        QCOMPARE(b.pages(),1);QCOMPARE(b.page(),0);
    }
    void movingCannotOpenPickerOrSystemActions(){
        ControlsDashboard b;b.setOwner("one");QSignalSpy actions(&b,&ControlsDashboard::actionRequested);
        b.edit();b.activate("volume");b.pick();QVERIFY(!b.picking());b.activate("settings");QVERIFY(b.moving());QVERIFY(actions.isEmpty());
        b.changePage(1);b.leave();QCOMPARE(b.page(),0);QCOMPARE(b.pages(),1);QCOMPARE(b.selected(),QString("volume"));
    }
    void pageChangeEndsSliderAdjustment(){
        ControlsDashboard b;b.setOwner("one");b.edit();b.drop("clock",12,0);b.edit();b.changePage(-1);
        b.activate("volume");QVERIFY(b.adjusting());b.changePage(1);QVERIFY(!b.adjusting());
        QSignalSpy levels(&b,&ControlsDashboard::adjustRequested);b.dispatch(Action::Up);QVERIFY(levels.isEmpty());
    }
    void resizePrefersCurrentLocation(){
        ControlsDashboard b;b.setOwner("one");b.edit();b.drop("clock",12,0);b.drop("clock",4,2);
        b.resize();const auto w=row(b,"clock");QCOMPARE(w["x"].toInt(),4);QCOMPARE(w["y"].toInt(),2);QCOMPARE(w["w"].toInt(),2);QCOMPARE(w["h"].toInt(),2);
        QCOMPARE(b.selectedTitle(),QString("Clock"));QVERIFY(b.widgetSelected());
    }
    void unknownOrHiddenActionsAreIgnored(){
        ControlsDashboard b;b.setOwner("one");QSignalSpy actions(&b,&ControlsDashboard::actionRequested);
        b.edit();b.select("wifi");b.hide();b.edit();b.activate("wifi");b.activate("missing");QVERIFY(actions.isEmpty());
    }
    void legacyLayoutAndHiddenWidgetsSurvive(){
        const QList<QRect> positions{{0,0,1,4},{1,0,1,4},{2,0,2,2},{4,0,1,2},{5,0,1,2},{6,0,6,2},{2,2,2,2},{4,2,2,2},{6,2,4,2},{10,2,2,2}};
        const QStringList ids{"volume","brightness","wifi","bluetooth","airplane","downloads","battery","clock","storage","scraper"};QJsonArray rows;
        for(int i=0;i<ids.size();++i){const auto r=positions[i];rows.append(QJsonObject{{"id",ids[i]},{"x",r.x()},{"y",r.y()},{"w",r.width()},{"h",r.height()},{"page",0},{"hidden",i==7}});}
        const auto key="controls/v1/"+QString::fromLatin1(QCryptographicHash::hash("legacy",QCryptographicHash::Sha256).toHex());QSettings().setValue(key,QJsonDocument(rows).toJson());
        ControlsDashboard b;b.setOwner("legacy");QCOMPARE(b.pages(),2);QCOMPARE(row(b,"volume")["w"].toInt(),1);QCOMPARE(row(b,"downloads")["w"].toInt(),4);QCOMPARE(row(b,"downloads")["h"].toInt(),4);QCOMPARE(row(b,"wifi")["w"].toInt(),1);QCOMPARE(row(b,"wifi")["h"].toInt(),1);QCOMPARE(b.available().size(),1);
        b.edit();b.edit();ControlsDashboard reopened;reopened.setOwner("legacy");QCOMPARE(reopened.widgets(),b.widgets());
    }
    void existingExternalPlacementReflowsWithoutHiding(){
        const auto key="controls/v1/"+QString::fromLatin1(QCryptographicHash::hash("one",QCryptographicHash::Sha256).toHex());
        QSettings().setValue(key+"/extensions",QVariantMap{{"sdk:test",QVariantMap{{"x",0},{"y",0},{"w",4},{"h",2},{"page",0},{"hidden",false}}}});
        ControlsDashboard b;b.setOwner("one");b.setExternalWidgets({QVariantMap{{"id","sdk:test"},{"title","Existing widget"},{"external",true}}});
        QCOMPARE(b.pages(),2);QVERIFY(b.available().isEmpty());b.changePage(1);QCOMPARE(row(b,"sdk:test")["w"].toInt(),4);
    }
};
QTEST_GUILESS_MAIN(ControlsDashboardTests)
#include "ControlsDashboardTests.moc"
