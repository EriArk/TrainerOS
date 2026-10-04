#include "core/model/SeriesCatalog.h"
#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
using namespace trainer;

class SeriesLibrary final: public LibraryRepository {
public:
    QList<AdventureRegistration> records;
    int reads=0;
    SeriesLibrary() {
        for(const auto& row:QList<QStringList>{{"m1","Super Mario World","snes"},{"m2","Mario Kart 64","n64"},
                {"z1","The Legend of Zelda - The Minish Cap","gba"},{"x1","Advance Wars","gba"}}) {
            AdventureRegistration r;r.adventure.id=row[0];r.adventure.title=row[1];r.adventure.platformId=row[2];
            r.adventure.domain="multiverse";r.contentAvailable=true;r.contentPath="/roms/"+row[2]+"/"+row[0];records.append(r);
        }
    }
    QList<World> worlds()const override{return {};}
    QList<Adventure> adventures()const override{QList<Adventure> result;for(const auto& r:records)result.append(r.adventure);return result;}
    QList<ResumePoint> resumePoints()const override{return {};}
    HomeSnapshot home()const override{return {};}
    bool editable()const override{return true;}
    std::optional<AdventureRegistration> registration(const QString& id)const override{for(const auto& r:records)if(r.adventure.id==id)return r;return {};}
};
class SeriesAdapter final: public AdventureAdapter {
public:
    QString launched;
    QString id()const override{return "fixture";}
    AdventureCapabilities capabilities(const Adventure&)const override{return {true,false,false};}
    AdventureResult launch(const Adventure& a)override{launched=a.id;return {true,{},true};}
    AdventureResult resume(const Adventure&,const ResumePoint&)override{return {false,{}};}
};
class SeriesTests: public QObject {
    Q_OBJECT
private slots:
    void classificationAndFallback() {
        QCOMPARE(seriesForTitle("Sonic Advance 2 (USA)"),"sonic");
        QCOMPARE(seriesForTitle("Dragon Ball Z - Supersonic Warriors"),"multiverse");
        QCOMPARE(seriesForTitle("Mario & Sonic at the Olympic Games"),"multiverse");
        QCOMPARE(seriesForTitle("Final Fantasy VII"),"final-fantasy");
        QCOMPARE(seriesForTitle("Super Metroid"),"metroid");
        QCOMPARE(seriesDisplayTitle("Sonic Advance (USA) (En,Ja)"),"Sonic Advance");
        QCOMPARE(seriesDisplayTitle("Metroid (My Hack) (Rev 2) (USA)"),"Metroid (My Hack) (Rev 2)");
    }
    void collectionViewsDoNotDuplicateOrChangeIdentity() {
        SeriesLibrary library;SeriesAdapter adapter;MultiversePresentation model(library,adapter);
        QCOMPARE(model.collections().size(),4); // Pokemon, Mario, Zelda, fallback; no empty series.
        QCOMPARE(model.choices().size(),1);QCOMPARE(model.choices()[0].toMap()["id"],"x1");
        model.setCollection("mario");QCOMPARE(model.route(),"games");QCOMPARE(model.games().size(),2);
        model.activate(1);QCOMPARE(adapter.launched,"m1");
        QCOMPARE(library.records.size(),4);QCOMPARE(library.records[0].contentPath,"/roms/snes/m1");
        model.setCollection("zelda");QCOMPARE(model.games().size(),1);QCOMPARE(model.detail()["id"],"z1");
    }
    void choicesSearchAndFocusSurviveRestartAndStayOwnerScoped() {
        SeriesLibrary library;SeriesAdapter adapter;MultiversePresentation model(library,adapter);
        model.setCollection("mario");model.select("m2");model.dispatch(Action::Down);model.applySearch("Mario");
        model.dispatch(Action::Down);const auto marioFocus=model.detail()["id"];
        model.setCollection("zelda");model.select("z1");model.applySearch("Minish");
        const auto state=model.navigationState();
        MultiversePresentation restored(library,adapter);restored.restoreNavigation(state);
        QCOMPARE(restored.collection(),"zelda");QCOMPARE(restored.query(),"Minish");QCOMPARE(restored.selected()["id"],"z1");
        restored.setCollection("mario");QCOMPARE(restored.query(),"Mario");QCOMPARE(restored.selected()["id"],"m2");QCOMPARE(restored.detail()["id"],marioFocus);
        restored.restoreNavigation({});restored.setCollection("mario");QVERIFY(restored.query().isEmpty());
        QCOMPARE(restored.navigationState()["selected"].toString(),QString());
        restored.restoreNavigation({{"selected","m1"}});restored.setCollection("mario");QCOMPARE(restored.selected()["id"],"m1");
    }
    void shellRootBackHomeCyclingAndDirectLaunch() {
        SeriesLibrary library;SeriesAdapter adapter;MockTrainerRepository profiles;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        shell.goToPage(1);QVERIFY(shell.collectionsRoot());shell.activate(1);
        QVERIFY(!shell.collectionsRoot());QCOMPARE(shell.multiverse()->collection(),"mario");
        shell.dispatch(Action::Confirm);QVERIFY(!adapter.launched.isEmpty());
        shell.dispatch(Action::Back);QVERIFY(shell.collectionsRoot());QCOMPARE(shell.focusIndex(),1);
        shell.activate(2);shell.goToPage(0);shell.goToPage(1);QVERIFY(shell.collectionsRoot());QCOMPARE(shell.focusIndex(),2);
        shell.goToPage(0);shell.dispatch(Action::NextFace);QVERIFY(shell.multiverseHome());QCOMPARE(shell.multiverse()->collection(),"mario");
        shell.multiverse()->select("m1");shell.dispatch(Action::NextFace);QCOMPARE(shell.multiverse()->collection(),"zelda");
        shell.dispatch(Action::PreviousFace);QCOMPARE(shell.multiverse()->selected()["id"],"m1");
        shell.dispatch(Action::Confirm);QCOMPARE(adapter.launched,"m1");
        const auto state=shell.navigationState();shell.restoreNavigation(state);QCOMPARE(shell.multiverse()->selected()["id"],"m1");
        shell.dispatch(Action::PreviousFace);QVERIFY(!shell.multiverseHome());shell.dispatch(Action::PreviousFace);QCOMPARE(shell.multiverse()->collection(),"multiverse");
    }
};
QTEST_MAIN(SeriesTests)
#include "SeriesTests.moc"
