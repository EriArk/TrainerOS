#include "core/model/SeriesCatalog.h"
#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
using namespace trainer;

class SeriesLibrary final: public LibraryRepository {
public:
    QList<AdventureRegistration> records;
    int reads=0;
    QList<PlaySession> sessions;
    QList<ResumePoint> moments;
    QHash<QString,QVariantMap> metadata;
    QList<PlaySession> recentSessions()const override{return sessions;}
    QVariantMap artwork(const QString& id)const override{return metadata.value(id);}
    SeriesLibrary() {
        for(const auto& row:QList<QStringList>{{"m1","Super Mario World","snes"},{"m2","Mario Kart 64","n64"},
                {"z1","The Legend of Zelda - The Minish Cap","gba"},{"x1","Advance Wars","gba"}}) {
            AdventureRegistration r;r.adventure.id=row[0];r.adventure.title=row[1];r.adventure.platformId=row[2];
            r.adventure.domain="multiverse";r.contentAvailable=true;r.contentPath="/roms/"+row[2]+"/"+row[0];records.append(r);
        }
    }
    QList<World> worlds()const override{return {};}
    QList<Adventure> adventures()const override{QList<Adventure> result;for(const auto& r:records)result.append(r.adventure);return result;}
    QList<ResumePoint> resumePoints()const override{return moments;}
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
        QCOMPARE(model.collections().size(),8); // Series plus four automatic views.
        QCOMPARE(model.choices().size(),4); // All games includes games also present in series.
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
        shell.goToPage(0);const auto before=shell.navigationState();
        shell.dispatch(Action::NextFace);shell.dispatch(Action::PreviousFace);
        QCOMPARE(shell.navigationState(),before);QVERIFY(shell.faceNames().isEmpty());
    }
    void globalRecentsAndPokemonUseTheSameLibrary() {
        SeriesLibrary library;SeriesAdapter adapter;
        auto p=library.records[0];p.adventure.id="p1";p.adventure.title="Pokemon Gold";p.adventure.domain="pokemon";p.adventure.platformId="gbc";library.records.append(p);
        const auto time=QDateTime::currentDateTimeUtc();
        library.sessions={{"s1","m1",time},{"s2","p1",time.addSecs(-10)},{"s3","m1",time.addSecs(-20)}};
        MockTrainerRepository profiles;DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        QCOMPARE(shell.primaryNames()[1],"Collections");
        shell.goToPage(1);shell.activate(0);QVERIFY(shell.multiverseFace());
        QCOMPARE(shell.multiverse()->route(),"games");QCOMPARE(shell.multiverse()->games().size(),1);
        QCOMPARE(shell.multiverse()->detail()["id"],"p1");
        shell.dispatch(Action::Confirm);QCOMPARE(adapter.launched,"p1");
        shell.goToPage(0);shell.dispatch(Action::ToggleContinue);QCOMPARE(shell.resumePoints().size(),2);
        QCOMPARE(shell.resumePoints()[0].toMap()["title"],"Super Mario World");
        shell.activate(0);QCOMPARE(shell.currentAdventureId(),"m1");QVERIFY(shell.multiverseHome());
        shell.dispatch(Action::Confirm);QCOMPARE(adapter.launched,"m1");
        shell.dispatch(Action::ToggleContinue);shell.activate(1);QCOMPARE(shell.currentAdventureId(),"p1");QVERIFY(!shell.multiverseHome());
        shell.dispatch(Action::Confirm);QCOMPARE(adapter.launched,"p1");
        const auto selected=shell.currentAdventureId();shell.goToPage(1);shell.activate(1);shell.goToPage(0);
        QCOMPARE(shell.currentAdventureId(),selected);
        const auto state=shell.navigationState();shell.restoreNavigation(state);QCOMPARE(shell.currentAdventureId(),selected);
    }
    void recentSelectionKeepsLatestResumeIdentityWithoutLaunching() {
        SeriesLibrary library;SeriesAdapter adapter;const auto now=QDateTime::currentDateTimeUtc();
        library.sessions={{"launch","m1",now}};
        ResumePoint old;old.id="old";old.adventureId="m1";old.observedAt=now.addSecs(-30);
        auto latest=old;latest.id="latest";latest.observedAt=now;
        library.moments={old,latest};
        MockTrainerRepository profiles;DevelopmentPlatformService platform;MockPokedexRepository dex;
        MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        shell.dispatch(Action::ToggleContinue);QCOMPARE(shell.resumePoints().size(),1);shell.activate(0);
        QCOMPARE(shell.currentAdventureId(),"m1");QCOMPARE(shell.navigationState()["homeResume"].toString(),"latest");
        QVERIFY(adapter.launched.isEmpty());
    }
    void manualCollectionsPersistIndependentlyAndNeverMoveGames() {
        SeriesLibrary library;QTemporaryDir dir;GameCollections model(library);model.configure(dir.path(),"one");
        model.begin();model.activate(0);model.applyText("Weekend");model.activate(1);
        model.activate(2);model.activate(4);model.activate(1);QVERIFY(!model.isOpen());
        QCOMPARE(model.definitions().size(),1);const auto id=model.definitions()[0].toObject()["id"].toString();
        QCOMPARE(model.definitions()[0].toObject()["games"].toArray().size(),2);
        QCOMPARE(library.records[0].contentPath,"/roms/snes/m1");
        GameCollections restored(library);restored.configure(dir.path(),"one");QCOMPARE(restored.definitions(),model.definitions());
        restored.configure(dir.path(),"two");QVERIFY(restored.definitions().isEmpty());
        restored.configure(dir.path(),"one");restored.begin(id);restored.activate(0);restored.applyText("Discard this");restored.dispatch(Action::Back);
        QCOMPARE(restored.definitions()[0].toObject()["name"].toString(),"Weekend");
        restored.begin(id);restored.activate(3);restored.activate(1);QVERIFY(restored.definitions().isEmpty());
        restored.configure(dir.path(),"one");QVERIFY(restored.definitions().isEmpty());QCOMPARE(library.records.size(),4);
    }
    void legacyHomeChoiceMigratesOnceAndBrokenCollectionsAreKept() {
        SeriesLibrary library;SeriesAdapter adapter;MockTrainerRepository profiles;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        const QJsonObject legacy{{"version",2},{"page","home"},{"homeDomain","multiverse"},{"homeCollection","mario"},
            {"multiverse",QJsonObject{{"collection","zelda"},{"selected","z1"},{"collections",QJsonObject{{"mario",QJsonObject{{"selected","m2"}}}}}}}};
        shell.restoreNavigation(legacy);QCOMPARE(shell.currentAdventureId(),"m2");
        const auto state=shell.navigationState();shell.restoreNavigation(state);QCOMPARE(shell.currentAdventureId(),"m2");
        QTemporaryDir dir;GameCollections editor(library);editor.configure(dir.path(),"one");
        editor.begin();editor.activate(0);editor.applyText("Keep");editor.activate(2);
        const auto files=QDir(dir.filePath("collections")).entryList({"*.json"},QDir::Files);QCOMPARE(files.size(),1);
        const auto path=dir.filePath("collections/"+files[0]);QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("broken-data");file.close();
        editor.configure(dir.path(),"one");QVERIFY(!editor.error().isEmpty());editor.begin();editor.activate(0);editor.applyText("Replace");editor.activate(2);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("broken-data"));
    }
    void automaticRulesRefreshWithMetadataAndUnknownPlayersDoNotMatch() {
        SeriesLibrary library;SeriesAdapter adapter;MultiversePresentation model(library,adapter);auto* editor=model.collectionManager();
        editor->begin();editor->activate(1);editor->applyText("Mario multiplayer");
        editor->activate(1);editor->applyText("Mario");editor->activate(6);editor->activate(9);
        const auto id=editor->definitions()[0].toObject()["id"].toString();model.setCollection(id);QVERIFY(model.games().isEmpty());
        library.metadata["m1"]={{"players","1-2"},{"favorite","true"}};model.refresh();
        QCOMPARE(model.games().size(),1);QCOMPARE(model.detail()["id"],"m1");
        library.metadata["m2"]={{"players","4"}};model.refresh();QCOMPARE(model.games().size(),2);
        model.setCollection("auto:favorites");QCOMPARE(model.games().size(),1);
        model.setCollection("auto:unplayed");QCOMPARE(model.games().size(),4);
        library.sessions={{"s","m1",QDateTime::currentDateTimeUtc()}};model.refresh();QCOMPARE(model.games().size(),3);
        model.setCollection("auto:recent");QCOMPARE(model.games().size(),1);QCOMPARE(model.detail()["id"],"m1");
    }
    void collectionModalKeepsStartAndKeyboardAccessible() {
        SeriesLibrary library;SeriesAdapter adapter;MockTrainerRepository profiles;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        shell.goToPage(1);shell.dispatch(Action::LocalAction);QVERIFY(shell.collectionManager()->isOpen());
        shell.dispatch(Action::SystemMenu);QVERIFY(shell.menuOpen());shell.activate(0);QCOMPARE(shell.service(),"settings");
        QVERIFY(!shell.collectionManager()->isOpen());
        shell.goToPage(1);shell.manageCollection(true);shell.activate(0);QVERIFY(shell.keyboard()->isOpen());
        shell.dispatch(Action::Back);QVERIFY(!shell.keyboard()->isOpen());QVERIFY(shell.collectionManager()->isOpen());
        shell.dispatch(Action::Back);QVERIFY(!shell.collectionManager()->isOpen());

    }
};
QTEST_MAIN(SeriesTests)
#include "SeriesTests.moc"
