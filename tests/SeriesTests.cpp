#include "adapters/pokemon/PokemonExperience.h"
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
    QString favoriteError;
    void setFavoriteAsync(const QString& id,bool value,QObject*,std::function<void(QString)> done) override {
        if(favoriteError.isEmpty())metadata[id]["favorite"]=value;
        done(favoriteError);
    }
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
    void favoriteStarAndL3ShareIdentityAndRespectSurfaceGates() {
        SeriesLibrary library;SeriesAdapter adapter;MockTrainerRepository profiles;DevelopmentPlatformService platform;MockPokedexRepository dex;
        MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        shell.goToPage(1);shell.openGamesRootItem("collections","mario");
        const auto home=shell.currentAdventureId();const auto recent=shell.resumePoints();
        shell.focusLibraryGame(1);const auto id=shell.multiverse()->detail()["id"].toString();
        shell.pressButton("L3");QVERIFY(library.metadata[id]["favorite"].toBool());
        QCOMPARE(shell.multiverse()->detail()["id"].toString(),id);QVERIFY(shell.multiverse()->detail()["favorite"].toBool());
        for(const auto& view:QStringList{"list","shelves","grid","wheel"}) {
            shell.libraryAction("view:"+view);QCOMPARE(shell.multiverse()->detail()["id"].toString(),id);
            shell.toggleLibraryFavorite(id);QVERIFY(!shell.multiverse()->detail()["favorite"].toBool());
            shell.dispatch(Action::ToggleFavorite);QVERIFY(shell.multiverse()->detail()["favorite"].toBool());
        }
        shell.toggleLibraryFavorite("z1");QVERIFY(!library.metadata["z1"]["favorite"].toBool()); // Not in current collection.
        shell.dispatch(Action::ToggleContinue);shell.setDrawerSource("favorites");QCOMPARE(shell.drawerGames().size(),1);
        shell.dispatch(Action::ToggleFavorite);QVERIFY(library.metadata[id]["favorite"].toBool());
        shell.dispatch(Action::Back);shell.dispatch(Action::LocalAction);shell.toggleLibraryFavorite(id);
        QVERIFY(library.metadata[id]["favorite"].toBool());shell.dispatch(Action::Back);
        library.favoriteError="Read-only storage";shell.dispatch(Action::ToggleFavorite);
        QVERIFY(library.metadata[id]["favorite"].toBool());QCOMPARE(shell.notice(),"Read-only storage");shell.dispatch(Action::Back);
        library.favoriteError.clear();shell.toggleLibraryFavorite(id);QVERIFY(!library.metadata[id]["favorite"].toBool());
        shell.dispatch(Action::ToggleContinue);shell.setDrawerSource("favorites");QVERIFY(shell.drawerGames().isEmpty());
        QCOMPARE(shell.currentAdventureId(),home);QCOMPARE(shell.resumePoints(),recent);QVERIFY(adapter.launched.isEmpty());QVERIFY(library.sessions.isEmpty());
        shell.dispatch(Action::Back);shell.goToPage(0);shell.toggleLibraryFavorite(id);QVERIFY(!library.metadata[id]["favorite"].toBool());
    }
    void globalFavoritesDrawerPreservesSourcesAndDoesNotLaunch() {
        SeriesLibrary library;library.metadata["m1"]={{"favorite","true"}};library.metadata["z1"]={{"favorite",1}};
        library.sessions={{"one","m2",QDateTime::currentDateTimeUtc()},{"two","m1",QDateTime::currentDateTimeUtc().addSecs(-10)}};
        SeriesAdapter adapter;MockTrainerRepository profiles;DevelopmentPlatformService platform;MockPokedexRepository dex;
        MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        shell.goToPage(1);shell.openGamesRootItem("collections","mario");
        const auto recent=shell.resumePoints();const auto home=shell.currentAdventureId();
        shell.setDrawerSource("favorites");QCOMPARE(shell.drawerSource(),"recent"); // Closed surface cannot steal L2/R2.
        shell.dispatch(Action::ToggleContinue);shell.dispatch(Action::Right);shell.setDrawerScroll(1.25);
        shell.dispatch(Action::NextFace);QVERIFY(shell.drawerOpen());QCOMPARE(shell.drawerSource(),"favorites");
        QCOMPARE(shell.drawerGames().size(),2);QCOMPARE(shell.multiverse()->collection(),"mario");
        shell.dispatch(Action::Right);const auto chosen=shell.drawerGames()[shell.focusIndex()].toMap()["id"].toString();
        shell.setDrawerScroll(2.5);shell.dispatch(Action::PreviousFace);
        QCOMPARE(shell.focusIndex(),1);QCOMPARE(shell.drawerScroll(),1.25);
        shell.dispatch(Action::NextFace);QCOMPARE(shell.focusIndex(),1);QCOMPARE(shell.drawerScroll(),2.5);
        QCOMPARE(shell.currentAdventureId(),home);shell.selectDrawerGame(chosen);QVERIFY(!shell.drawerOpen());
        QCOMPARE(shell.currentAdventureId(),"z1");QVERIFY(adapter.launched.isEmpty());QCOMPARE(shell.resumePoints(),recent);
        QCOMPARE(library.sessions.size(),2);
        for(int page=0;page<5;++page){shell.goToPage(page);shell.dispatch(Action::ToggleContinue);QVERIFY(shell.drawerOpen());shell.dispatch(Action::ToggleContinue);QVERIFY(!shell.drawerOpen());}
    }
    void favoriteRemovalCannotRetargetConfirmAndLiveGameRemainsPinned() {
        SeriesLibrary library;library.metadata["m1"]={{"favorite",true}};library.metadata["z1"]={{"favorite",true}};
        SeriesAdapter adapter;MockTrainerRepository profiles;DevelopmentPlatformService platform;MockPokedexRepository dex;
        MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        shell.dispatch(Action::ToggleContinue);shell.setDrawerSource("favorites");const auto home=shell.currentAdventureId();
        library.metadata["m1"]["favorite"]=false;shell.refreshLibrary();QCOMPARE(shell.focusIndex(),-1);
        shell.dispatch(Action::Confirm);QCOMPARE(shell.currentAdventureId(),home);QVERIFY(shell.drawerOpen());
        shell.selectDrawerGame("favorite:m1");QCOMPARE(shell.currentAdventureId(),home);
        shell.dispatch(Action::Right);QCOMPARE(shell.focusIndex(),0);
        shell.setLiveGame({{"id","m2"},{"title","Running"}});shell.dispatch(Action::Confirm);
        QCOMPARE(shell.currentAdventureId(),"m2");QVERIFY(!shell.notice().isEmpty());QVERIFY(adapter.launched.isEmpty());
        shell.dispatch(Action::Back);shell.setLiveGame({});
        // Revalidate the source on A even before a metadata refresh signal arrives.
        library.metadata["z1"]["favorite"]=false;shell.dispatch(Action::Confirm);QVERIFY(shell.drawerOpen());
        QVERIFY(shell.drawerGames().isEmpty());QVERIFY(adapter.launched.isEmpty());
    }
    void favoritesRestoreStableIdentityAndOwnerNavigationWithoutCaps() {
        SeriesLibrary library;
        for(int i=0;i<105;++i){auto r=library.records[0];r.adventure.id=QString("favorite-%1").arg(i);r.adventure.title=QString("Game %1").arg(i,3,10,QChar('0'));library.records.append(r);library.metadata[r.adventure.id]={{"favorite","true"}};}
        library.records.last().removed=true;library.records[4].contentAvailable=false;
        SeriesAdapter adapter;MockTrainerRepository profiles;DevelopmentPlatformService platform;MockPokedexRepository dex;
        MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        shell.dispatch(Action::ToggleContinue);shell.setDrawerSource("favorites");QCOMPARE(shell.drawerGames().size(),104);
        QCOMPARE(shell.drawerGames().front().toMap()["summary"],"Game file unavailable");
        for(int i=0;i<101;++i)shell.dispatch(Action::Right);
        auto chosen=shell.drawerGames()[shell.focusIndex()].toMap()["id"].toString();shell.setDrawerScroll(98.5);
        const auto state=shell.navigationState();shell.dispatch(Action::Back);shell.restoreNavigation(state);
        shell.dispatch(Action::ToggleContinue);QCOMPARE(shell.drawerGames()[shell.focusIndex()].toMap()["id"].toString(),chosen);QCOMPARE(shell.drawerScroll(),98.5);
        library.records[105].adventure.title="A renamed title";shell.refreshLibrary();
        QCOMPARE(shell.drawerGames()[shell.focusIndex()].toMap()["id"].toString(),chosen);
        auto other=state;other.remove("drawer");other.remove("resume");shell.restoreNavigation(other);
        QVERIFY(!shell.drawerOpen());QCOMPARE(shell.drawerSource(),"recent");QCOMPARE(shell.drawerScroll(),-1.);
        shell.restoreNavigation(state);shell.restoreNavigation({}); // Brand-new Trainer has no saved navigation yet.
        QVERIFY(!shell.drawerOpen());QCOMPARE(shell.drawerSource(),"recent");QCOMPARE(shell.drawerScroll(),-1.);
        shell.restoreNavigation(state);shell.dispatch(Action::ToggleContinue);shell.dispatch(Action::SystemMenu);
        shell.setDrawerSource("recent");QCOMPARE(shell.drawerSource(),"favorites");
    }
    void selectGameOpensCollectionPickerWithoutChangingHomeOrLaunching() {
        SeriesLibrary library;SeriesAdapter adapter;MockTrainerRepository profiles;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        const auto home=shell.currentAdventureId();shell.goToPage(1);shell.openGamesRootItem("platforms","snes");
        shell.dispatch(Action::LocalAction);QCOMPARE(shell.libraryTools()->route(),"game");
        QCOMPARE(shell.libraryTools()->rows()[5].toMap()["label"].toString(),"Add to collection");
        shell.activate(5);QCOMPARE(shell.collectionManager()->route(),"membership");QVERIFY(!shell.libraryTools()->isOpen());
        shell.activate(0);QVERIFY(shell.keyboard()->isOpen());shell.keyboard()->cancel();
        shell.collectionManager()->applyText("Weekend");shell.activate(1);QVERIFY(!shell.collectionManager()->isOpen());
        QVERIFY(shell.collectionManager()->definitions()[0].toObject()["games"].toArray().contains("m1"));
        QCOMPARE(shell.currentAdventureId(),home);QVERIFY(adapter.launched.isEmpty());
    }
    void collectionPickerCreatesAddsIdempotentlyAndCancelsDrafts() {
        SeriesLibrary library;QTemporaryDir dir;GameCollections model(library);model.configure(dir.path(),"one");
        model.beginMembership("m1");QCOMPARE(model.route(),"membership");QCOMPARE(model.rows().size(),2);
        model.activate(0);QCOMPARE(model.route(),"membership-new");model.applyText("  Weekend  ");
        QVERIFY(model.definitions().isEmpty());model.dispatch(Action::Back);
        QCOMPARE(model.route(),"membership");QVERIFY(model.definitions().isEmpty());
        model.activate(0);model.applyText("  ");model.activate(1);QVERIFY(!model.error().isEmpty());QVERIFY(model.definitions().isEmpty());
        model.activate(0);model.applyText("Weekend");model.activate(1);QVERIFY(!model.isOpen());
        QCOMPARE(model.definitions().size(),1);auto id=model.definitions()[0].toObject()["id"].toString();
        QVERIFY(model.contains(id,library.records[0].adventure));
        model.beginMembership("m1");QVERIFY(model.rows()[0].toMap()["member"].toBool());model.activate(0);
        QCOMPARE(model.definitions()[0].toObject()["games"].toArray(),QJsonArray{"m1"});
        model.beginMembership("m2");model.activate(0);QVERIFY(!model.isOpen());
        QCOMPARE(model.definitions()[0].toObject()["games"].toArray(),(QJsonArray{"m1","m2"}));
        model.beginMembership("m1");model.activate(1);model.applyText("Second");model.activate(1);
        QCOMPARE(model.definitions().size(),2);QVERIFY(model.contains(id,library.records[0].adventure));
        GameCollections restored(library);restored.configure(dir.path(),"one");QCOMPARE(restored.definitions(),model.definitions());
        restored.beginMembership("z1");restored.activate(2);restored.configure(dir.path(),"two");
        restored.applyText("Stale keyboard");restored.activate(1);QVERIFY(restored.definitions().isEmpty());
        restored.configure(dir.path(),"one");QCOMPARE(restored.definitions(),model.definitions());
        QCOMPARE(library.records.size(),4);QCOMPARE(library.records[0].contentPath,"/roms/snes/m1");QVERIFY(library.sessions.isEmpty());
    }
    void collectionPickerKeepsUnreadableStorageAndDynamicDefinitions() {
        SeriesLibrary library;QTemporaryDir dir;GameCollections model(library);model.configure(dir.path(),"one");
        model.begin();model.activate(1);model.applyText("Automatic legacy");model.activate(9);
        const auto original=model.definitions();model.beginMembership("m1");QCOMPARE(model.rows().size(),2);
        model.activate(0);model.applyText("Manual");model.activate(1);QCOMPARE(model.definitions()[0],original[0]);
        auto files=QDir(dir.filePath("collections")).entryList({"*.json"},QDir::Files);QCOMPARE(files.size(),1);
        QFile file(dir.filePath("collections/"+files[0]));QVERIFY(file.open(QIODevice::WriteOnly));file.write("future-or-broken");file.close();
        model.configure(dir.path(),"one");model.beginMembership("m1");QVERIFY(!model.error().isEmpty());
        model.activate(0);model.applyText("Do not overwrite");model.activate(1);model.dispatch(Action::Back);
        QVERIFY(!model.isOpen());QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("future-or-broken"));
    }
    void shelvesNavigateRowsPreserveIdentityAndLaunch() {
        SeriesLibrary library;SeriesAdapter adapter;
        for(int i=0;i<21;++i){auto r=library.records[2];r.adventure.id=QString("shelf-%1").arg(i);r.adventure.title=QString("Shelf %1").arg(i,2,10,QChar('0'));library.records.append(r);}
        MultiversePresentation model(library,adapter);model.openSystem("gba");model.setViewStyle("shelves");
        QCOMPARE(model.shelfColumns(),5);model.focusGame(0);
        model.dispatch(Action::Left);model.dispatch(Action::Up);QCOMPARE(model.focusIndex(),0);
        model.dispatch(Action::Down);QCOMPARE(model.focusIndex(),5);
        model.dispatch(Action::Right);QCOMPARE(model.focusIndex(),6);
        model.dispatch(Action::Up);QCOMPARE(model.focusIndex(),1);
        model.focusGame(4);model.dispatch(Action::Right);QCOMPARE(model.focusIndex(),4);
        model.focusGame(19);model.dispatch(Action::Down);QCOMPARE(model.focusIndex(),22);
        model.dispatch(Action::Down);QCOMPARE(model.focusIndex(),22);
        const auto id=model.detail()["id"];const auto games=model.games();
        model.setViewStyle("wheel");model.setViewStyle("list");model.setViewStyle("shelves");
        QCOMPARE(model.detail()["id"],id);QCOMPARE(model.games(),games);
        MultiversePresentation restored(library,adapter);restored.restoreNavigation(model.navigationState());
        QCOMPARE(restored.viewStyle(),"shelves");QCOMPARE(restored.detail()["id"],id);
        restored.setCollection("mario");QCOMPARE(restored.viewStyle(),"wheel");
        restored.openSystem("gba");QCOMPARE(restored.viewStyle(),"shelves");QCOMPARE(restored.detail()["id"],id);
        QVERIFY(adapter.launched.isEmpty());restored.dispatch(Action::Confirm);QCOMPARE(adapter.launched,id.toString());
    }
    void gridUsesResponsiveRowsAndRestoresExactGame() {
        SeriesLibrary library;SeriesAdapter adapter;
        for(int i=0;i<9;++i){auto r=library.records[2];r.adventure.id=QString("grid-%1").arg(i);r.adventure.title=QString("Grid %1").arg(i);library.records.append(r);}
        MultiversePresentation model(library,adapter);model.openSystem("gba");model.setViewStyle("grid");
        model.focusGame(2);model.dispatch(Action::Right);QCOMPARE(model.focusIndex(),2);
        model.dispatch(Action::Down);QCOMPARE(model.focusIndex(),5);
        const auto id=model.detail()["id"];model.setGridColumns(4);QCOMPARE(model.detail()["id"],id);
        model.dispatch(Action::Down);QCOMPARE(model.focusIndex(),9);
        model.dispatch(Action::Right);QCOMPARE(model.focusIndex(),10);
        model.dispatch(Action::Down);QCOMPARE(model.focusIndex(),10);
        const auto last=model.detail()["id"];
        model.setViewStyle("shelves");model.setViewStyle("grid");QCOMPARE(model.detail()["id"],last);
        MultiversePresentation restored(library,adapter);restored.restoreNavigation(model.navigationState());
        QCOMPARE(restored.viewStyle(),"grid");QCOMPARE(restored.detail()["id"],last);
        QVERIFY(adapter.launched.isEmpty());restored.dispatch(Action::Confirm);QCOMPARE(adapter.launched,last.toString());
        model.applySearch("no-matching-grid-game");model.dispatch(Action::Down);QVERIFY(model.games().isEmpty());
    }
    void coversUseGenreAndStableIdWithoutScreenshotSubstitution() {
        SeriesLibrary library;SeriesAdapter adapter;
        library.metadata["z1"]={{"genre","Role Playing Game"},{"screenshot","file:///screen.png"},{"cover","file:///screen.png"}};
        library.metadata["x1"]={{"genre","Racing"},{"boxart","file:///box.png"},{"image","file:///image.png"}};
        MultiversePresentation model(library,adapter);
        const auto fantasy=model.game("z1")["fallbackCover"].toString();
        QVERIFY(fantasy.startsWith("qrc:/cover-fallbacks/fantasy-"));QVERIFY(model.game("z1")["cover"].toString().isEmpty());
        QVERIFY(model.game("x1")["fallbackCover"].toString().startsWith("qrc:/cover-fallbacks/racing-"));
        QCOMPARE(model.game("x1")["cover"].toString(),"file:///box.png");
        library.records[2].adventure.title="Renamed game";model.refresh();QCOMPARE(model.game("z1")["fallbackCover"].toString(),fantasy);
        const QList<QPair<QString,QString>> genres{{"Sports / Football","sports"},{"Puzzle","puzzle"},{"Shoot'em up","scifi"},{"Fighting","action"},{"Unknown","neutral"},{"","neutral"}};
        for(const auto& pair:genres){library.metadata["x1"]["genre"]=pair.first;model.refresh();QVERIFY(model.game("x1")["fallbackCover"].toString().startsWith("qrc:/cover-fallbacks/"+pair.second+"-"));}
        library.metadata["z1"]["image"]="file:///new-cover.png";model.refresh();
        QCOMPARE(model.game("z1")["cover"].toString(),"file:///new-cover.png");QCOMPARE(model.game("z1")["fallbackCover"].toString(),fantasy);
    }
    void gamesRootKeepsIndependentScopesAndStableIdentity() {
        SeriesLibrary library;SeriesAdapter adapter;MockTrainerRepository profiles;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        shell.goToPage(1);const auto home=shell.currentAdventureId();
        for(const auto& row:shell.gamesCollections()) {
            QVERIFY(row.toMap()["id"]!="multiverse");QVERIFY(row.toMap()["id"]!="auto:favorites");
        }
        QCOMPARE(shell.gamesPlatforms().size(),3);
        shell.dispatch(Action::NextFace);QVERIFY(shell.collectionsRoot());QVERIFY(shell.faceNames().isEmpty());
        shell.openGamesRootItem("collections","mario");QCOMPARE(shell.multiverse()->games().size(),2);
        QCOMPARE(shell.gamesPlatforms().size(),3); // Root platforms are not narrowed by the browsed collection.
        shell.dispatch(Action::Back);QCOMPARE(shell.gamesRootZone(),"collections");
        shell.dispatch(Action::Down);QCOMPARE(shell.gamesRootZone(),"platforms");
        shell.openGamesRootItem("platforms","gba");QCOMPARE(shell.multiverse()->route(),"games");
        QCOMPARE(shell.multiverse()->games().size(),2);QCOMPARE(shell.multiverse()->detail()["platformShort"].toString(),QString("GBA"));
        const auto platforms=shell.gamesPlatforms();const auto next=(shell.gamesPlatformIndex()+1)%platforms.size();
        shell.selectFace(next);QCOMPARE(shell.gamesPlatformIndex(),next);
        shell.dispatch(Action::Back);QVERIFY(shell.collectionsRoot());QCOMPARE(shell.gamesRootZone(),"platforms");
        shell.setProperty("collectionScroll",2.0);shell.setProperty("platformScroll",1.0);
        const auto state=shell.navigationState();shell.restoreNavigation(state);
        QCOMPARE(shell.gamesPlatformIndex(),next);QCOMPARE(shell.property("collectionScroll").toDouble(),2.0);
        QCOMPARE(shell.property("platformScroll").toDouble(),1.0);QCOMPARE(shell.currentAdventureId(),home);QVERIFY(adapter.launched.isEmpty());
        shell.focusGamesRootItem("collections","zelda");QVERIFY(shell.collectionsRoot());
        QCOMPARE(shell.gamesCollections()[shell.gamesCollectionIndex()].toMap()["id"].toString(),QString("zelda"));
        QCOMPARE(shell.currentAdventureId(),home);QVERIFY(adapter.launched.isEmpty());
        shell.openGamesRootItem("collections","_create");QVERIFY(shell.collectionManager()->isOpen());
    }
    void browserScrollIsDeliberateAndBlockedByDrawers() {
        SeriesLibrary library;SeriesAdapter adapter;MockTrainerRepository profiles;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        QSignalSpy scroll(shell.multiverse(), &MultiversePresentation::scrollRequested);
        shell.goToPage(1);shell.dispatch(Action::ScrollDown);QCOMPARE(scroll.size(),0);
        shell.openGamesRootItem("platforms","gba");const auto id=shell.multiverse()->detail()["id"];
        shell.dispatch(Action::ScrollDown);QCOMPARE(scroll.size(),1);QCOMPARE(scroll.last()[0].toInt(),1);
        shell.dispatch(Action::Secondary);QVERIFY(shell.quickAppsOpen());
        shell.dispatch(Action::ScrollUp);QCOMPARE(scroll.size(),1);
        shell.dispatch(Action::Back);QVERIFY(!shell.quickAppsOpen());
        shell.dispatch(Action::ScrollUp);QCOMPARE(scroll.size(),2);QCOMPARE(scroll.last()[0].toInt(),-1);
        QCOMPARE(shell.multiverse()->detail()["id"],id);QVERIFY(adapter.launched.isEmpty());
        shell.dispatch(Action::Back);shell.dispatch(Action::ScrollDown);QCOMPARE(scroll.size(),2);
    }
    void listViewIsScopedPersistentAndPreservesIdentity() {
        SeriesLibrary library;SeriesAdapter adapter;MultiversePresentation model(library,adapter);
        model.openSystem("gba");model.focusGame(1);const auto id=model.detail()["id"];
        const auto games=model.games();model.setViewStyle("list");
        QCOMPARE(model.viewStyle(),"list");QCOMPARE(model.detail()["id"],id);QCOMPARE(model.games(),games);
        model.openSystem("snes");QCOMPARE(model.viewStyle(),"wheel");
        model.setCollection("mario");model.setViewStyle("list");model.focusGame(1);
        const auto mario=model.detail()["id"];model.setCollection("zelda");QCOMPARE(model.viewStyle(),"wheel");
        const auto state=model.navigationState();MultiversePresentation restored(library,adapter);restored.restoreNavigation(state);
        restored.setCollection("mario");QCOMPARE(restored.viewStyle(),"list");QCOMPARE(restored.detail()["id"],mario);
        restored.openSystem("gba");QCOMPARE(restored.viewStyle(),"list");QCOMPARE(restored.detail()["id"],id);
        restored.setViewStyle("unknown-view");QCOMPARE(restored.viewStyle(),"list");
        auto extra=library.records[2];extra.adventure.id="new";extra.adventure.title="AAA";library.records.append(extra);
        restored.refresh();QCOMPARE(restored.detail()["id"],id);QCOMPARE(restored.viewStyle(),"list");
        restored.restoreNavigation({});restored.openSystem("gba");QCOMPARE(restored.viewStyle(),"wheel");
        QVERIFY(adapter.launched.isEmpty());
    }
    void listFocusSupportsLargeLibrariesAndKeepsLaunchExplicit() {
        SeriesLibrary library;SeriesAdapter adapter;
        for(int i=0;i<300;++i){auto r=library.records[2];r.adventure.id=QString("many-%1").arg(i);r.adventure.title=QString("Game %1").arg(i,3,10,QChar('0'));library.records.append(r);}
        MultiversePresentation model(library,adapter);model.openSystem("gba");model.setViewStyle("list");
        QCOMPARE(model.listPageSize(),7);
        model.focusGame(0);model.dispatch(Action::Left);QCOMPARE(model.focusIndex(),0);
        model.dispatch(Action::Right);QCOMPARE(model.focusIndex(),7);
        model.dispatch(Action::Down);QCOMPARE(model.focusIndex(),8);
        model.dispatch(Action::Left);QCOMPARE(model.focusIndex(),1);
        model.focusGame(model.games().size()-1);model.dispatch(Action::Right);QCOMPARE(model.focusIndex(),model.games().size()-1);
        model.focusGame(299);const auto id=model.detail()["id"].toString();
        model.focusGame(-1);model.focusGame(10000);QCOMPARE(model.detail()["id"].toString(),id);
        model.setViewStyle("wheel");model.setViewStyle("list");QCOMPARE(model.detail()["id"].toString(),id);
        QVERIFY(adapter.launched.isEmpty());model.dispatch(Action::Confirm);QCOMPARE(adapter.launched,id);
    }
    void listChoiceAndPagingRespectShellModals() {
        SeriesLibrary library;SeriesAdapter adapter;MockTrainerRepository profiles;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        shell.goToPage(1);shell.openGamesRootItem("platforms","gba");const auto home=shell.currentAdventureId();
        shell.focusLibraryGame(1);const auto id=shell.multiverse()->detail()["id"];
        shell.dispatch(Action::ChangeView);QCOMPARE(shell.multiverse()->viewStyle(),"list");
        shell.dispatch(Action::Secondary);shell.dispatch(Action::ChangeView);shell.focusLibraryGame(0);
        QCOMPARE(shell.multiverse()->viewStyle(),"list");QCOMPARE(shell.multiverse()->detail()["id"],id);
        shell.dispatch(Action::Back);shell.openContext();QVERIFY(shell.libraryTools()->isOpen());
        const auto rows=shell.libraryTools()->rows();int wheel=-1;
        for(int i=0;i<rows.size();++i)if(rows[i].toMap()["label"].toString().startsWith("View: Wheel"))wheel=i;
        QVERIFY(wheel>=0);shell.libraryTools()->activate(wheel);
        QVERIFY(!shell.libraryTools()->isOpen());QCOMPARE(shell.multiverse()->viewStyle(),"wheel");
        QCOMPARE(shell.multiverse()->detail()["id"],id);QCOMPARE(shell.currentAdventureId(),home);QVERIFY(adapter.launched.isEmpty());
    }
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
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
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
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        QCOMPARE(shell.primaryNames()[1],"Games");
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
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
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
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
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
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
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
