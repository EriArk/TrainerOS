#include "adapters/pokemon/PokemonExperience.h"
#include "adapters/generic/GenericExperience.h"
#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include "fixtures/adapters/CounterExperience.h"
#include "core/experience/ExperienceNavigation.h"
#include <QtTest>
#include "core/model/GamePlayers.h"
#include <QTemporaryDir>
#include <QFile>
#include <QBuffer>
#include "fixtures/HomeSocialFixture.h"

using namespace trainer;
class ExperienceTests : public QObject {
    Q_OBJECT
private slots:
    void currentNavigationRoundTripDoesNotInventLegacySlots() {
        ExperienceNavigation navigation;
        navigation.select("owner","game",genericExperienceDescriptor());
        navigation.rememberNavigation({{"selection","exact-item"}});
        const auto saved=navigation.state();navigation.restore(saved);
        QCOMPARE(navigation.state(),saved);
    }
    void homeAchievementsAvoidRepeatedSnapshotsAndInvalidate() {
        class Library final : public LibraryRepository {
        public:
            std::optional<quint64> revision=1;
            QList<Adventure> games;
            std::optional<quint64> libraryRevision()const override{return revision;}
            QList<Adventure> adventures()const override{return games;}
            QList<World> worlds()const override{return {};}
            QList<ResumePoint> resumePoints()const override{return {};}
            HomeSnapshot home()const override{return {};}
        } library;
        Adventure game;game.id="game";library.games={game};
        class Provider final : public AchievementProvider {
        public:
            mutable int reads=0;
            AchievementContext identity{"fixture","owner"};
            AchievementSnapshot data;
            AchievementContext context()const override{return identity;}
            QList<AchievementSet> sets()const override{return {{"set","game","game","Game","",true}};}
            AchievementSnapshot snapshot(const QString&)const override{++reads;return data;}
            void refresh(const QString&)override{emit snapshotChanged("set");}
        } provider;
        provider.data.context=provider.identity;provider.data.setId="set";provider.data.state=AchievementState::Ready;
        for(int i=0;i<1000;++i) {
            const auto id=QString::number(i);
            provider.data.definitions.append({id,"Achievement "+id,"","",5});
            provider.data.unlocks.append({id,true,AchievementMode::Standard,QDateTime::fromSecsSinceEpoch(1000+i)});
        }
        SocialController social;HomeDashboardController dashboard(library,provider,social);
        dashboard.setContext("game",true);const auto rows=dashboard.achievements();QCOMPARE(rows.size(),16);
        const int reads=provider.reads;QElapsedTimer elapsed;elapsed.start();
        for(int i=0;i<100;++i){dashboard.reset();QCOMPARE(dashboard.achievements(),rows);}
        qInfo()<<"100 Home trophy reads:"<<elapsed.nsecsElapsed()/1e6<<"ms, provider snapshots:"<<provider.reads-reads;
        QCOMPARE(provider.reads,reads);
        provider.data.unlocks.clear();provider.refresh("set");QVERIFY(dashboard.achievements().isEmpty());
        provider.data.unlocks.append({"1",true,AchievementMode::Standard,QDateTime::currentDateTimeUtc()});provider.refresh("set");
        QCOMPARE(dashboard.achievements().size(),1);
        library.games.clear();++*library.revision;QVERIFY(dashboard.achievements().isEmpty());
        library.games={game};++*library.revision;QCOMPARE(dashboard.achievements().size(),1);
        dashboard.setContext("other",true);QVERIFY(dashboard.achievements().isEmpty());
        dashboard.setContext("game",true);QCOMPARE(dashboard.achievements().size(),1);
        provider.identity.accountId="another";QVERIFY(dashboard.achievements().isEmpty());
        provider.identity=provider.data.context;QCOMPARE(dashboard.achievements().size(),1);
        library.revision.reset();library.games.clear();QVERIFY(dashboard.achievements().isEmpty());
        library.games={game};QCOMPARE(dashboard.achievements().size(),1);
    }
    void albumKeepsIdentityFiltersAndOwnerScopedStars() {
        QTemporaryDir temp;ScreensController album;
        album.configure(temp.path(),"one");
        QVariantList media;
        for(int i=0;i<9;++i)media.append(QVariantMap{{"id",QString::number(i)},{"adventure",i%2?"b":"a"},{"game",i%2?"Second":"First"},{"day",i<4?"17 Oct 2026":"16 Oct 2026"},{"source","fixture"}});
        album.setMedia(media);QCOMPARE(album.items().size(),9);
        album.select(2);album.dispatch(Action::Right);QCOMPARE(album.index(),3);
        QCOMPARE(album.current()["id"].toString(),"3");
        album.dispatch(Action::Left);QCOMPARE(album.index(),2);
        album.select(5);album.dispatch(Action::Confirm);QCOMPARE(album.index(),5);
        album.toggleFavorite();QCOMPARE(album.count(),8);
        album.dispatch(Action::ChangeView);QVERIFY(album.favoritesView());QCOMPARE(album.count(),1);QCOMPARE(album.current()["id"].toString(),"5");
        album.select(0);album.toggleFavorite();QCOMPARE(album.count(),0);QVERIFY(album.current().isEmpty());
        album.switchLibrary();album.select(5);album.toggleFavorite();
        ScreensController reopened;reopened.configure(temp.path(),"one");reopened.setMedia(media);reopened.switchLibrary();
        QCOMPARE(reopened.count(),1);QCOMPARE(reopened.current()["id"].toString(),"5");
        reopened.configure(temp.path(),"two");reopened.setMedia(media);reopened.switchLibrary();QCOMPARE(reopened.count(),0);
        reopened.configure(temp.path(),"one");reopened.setMedia(media);reopened.switchLibrary();QCOMPARE(reopened.count(),1);
        // Replacing a game's final image does not transfer a star to the new image.
        auto replacement=media;auto row=replacement[5].toMap();row["id"]="new-capture";replacement[5]=row;
        reopened.setMedia(replacement);QCOMPARE(reopened.count(),0);
        album.forGame("b");QCOMPARE(album.count(),3);album.select(2);const auto id=album.current()["id"];
        const auto navigation=album.navigationState();
        album.chooseFilter(0);album.select(0);album.switchLibrary();
        album.restoreNavigation(navigation);QCOMPARE(album.navigationState(),navigation);QCOMPARE(album.current()["id"],id);
        media.prepend(QVariantMap{{"id","later"},{"adventure","b"},{"game","Second"},{"day","18 Oct 2026"}});
        album.setMedia(media);QCOMPARE(album.current()["id"],id);
        auto deleted=media;for(int i=deleted.size()-1;i>=0;--i)if(deleted[i].toMap()["id"]==id)deleted.removeAt(i);
        album.setMedia(deleted);QVERIFY(album.current()["id"]!=id);
        album.forGame("missing");QCOMPARE(album.count(),0);QVERIFY(album.current().isEmpty());
        album.showFilters();album.dispatch(Action::Down);album.dispatch(Action::Confirm);QVERIFY(!album.filtering());
        album.configure(temp.path()+"/blocked","three");QFile blocked(temp.path()+"/blocked");QVERIFY(blocked.open(QIODevice::WriteOnly));blocked.close();
        album.setMedia(media);album.select(0);album.toggleFavorite();QVERIFY(!album.error().isEmpty());QVERIFY(!album.current()["favorite"].toBool());
        QVariantList many;for(int i=0;i<300;++i)many.append(QVariantMap{{"id",QString::number(i)},{"day","17 Oct 2026"}});
        album.setMedia(many);QCOMPARE(album.items().size(),300);
        album.select(299);album.dispatch(Action::Right);QCOMPARE(album.index(),299);
        album.dispatch(Action::Left);QCOMPARE(album.index(),298);
    }
    void adapterBrowsingRestoresThroughItsOwnNavigationContract() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockHallOfFameRepository archive;MockAchievementProvider achievements;
        auto factory=[&](const ExperienceServices& services) {
            ExperienceModules modules;auto module=std::make_unique<test::CounterExperience>("test");
            module->matchGame="emerald-demo";module->resetOnLeave=true;
            modules.push_back(std::move(module));modules.push_back(std::make_unique<GenericExperience>(services));return modules;
        };
        ShellController shell(library,profiles,adapter,platform,factory,archive,achievements);
        shell.openGameHubFace("adapter");shell.selectAdapterScreen("second");shell.activate(7);
        shell.goToSection("apps");shell.goToSection("gamehub");
        QCOMPARE(shell.hubFace(),"adapter");QCOMPARE(shell.adapterScreen(),"second");QCOMPARE(shell.focusIndex(),7);
        shell.openGameHubFace("overview");shell.openGameHubFace("adapter");
        QCOMPARE(shell.adapterScreen(),"second");QCOMPARE(shell.focusIndex(),7);
    }
    void overviewTimelineRunsLeftToRightAndRestoresSession() {
        class HistoryLibrary : public LibraryRepository {
            MockLibraryRepository base;
        public:
            QList<World> worlds() const override { return base.worlds(); }
            QList<Adventure> adventures() const override { return base.adventures(); }
            QList<ResumePoint> resumePoints() const override { return base.resumePoints(); }
            HomeSnapshot home() const override { return base.home(); }
            QList<PlaySession> gameSessions(const QString& id) const override {
                const auto now=QDateTime::fromString("2026-10-16T10:00:00Z",Qt::ISODate);
                return {{"new",id,now,now.addSecs(60),60,PlaySessionOutcome::Returned},
                    {"old",id,now.addDays(-1),{},{},PlaySessionOutcome::Interrupted}};
            }
            qint64 gameSessionCount(const QString&) const override { return 87; }
            PlayerHistory playerHistory() const override {
                auto rows=gameSessions("emerald-demo");rows[1].adventureId="crystal-demo";
                return {rows,90,2,1234};
            }
        } library;
        MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        shell.openGameHubFace("overview");QCOMPARE(shell.home()["sessionCount"].toLongLong(),87);
        QCOMPARE(shell.story()["sessionCount"].toLongLong(),90);
        QCOMPARE(shell.story()["gamesPlayed"].toLongLong(),2);
        QCOMPARE(shell.storyHistory()[1].toMap()["adventure"].toString(),"crystal-demo");
        const auto originalGame=shell.currentAdventureId();
        QSignalSpy launches(&shell,&ShellController::homeLaunchPressed);
        shell.dispatch(Action::LocalAction);QVERIFY(!shell.menuOpen());
        shell.showOverview("timeline");QCOMPARE(shell.focusIndex(),0);
        shell.dispatch(Action::Left);QCOMPARE(shell.focusIndex(),1);
        const auto story=shell.storyHistory();
        shell.dispatch(Action::SystemMenu);shell.dispatch(Action::Back);
        QCOMPARE(shell.service(),"profile");QCOMPARE(shell.overviewFace(),"timeline");QCOMPARE(shell.focusIndex(),1);
        QCOMPARE(shell.primaryId(),"home");
        shell.setLiveGame({{"id","crystal-demo"}});
        QCOMPARE(shell.focusIndex(),1);QCOMPARE(shell.storyHistory(),story);
        shell.setLiveGame({{"id",originalGame}});shell.setLiveGame({});
        shell.dispatch(Action::Left);QCOMPARE(shell.focusIndex(),1);
        QCOMPARE(shell.gameHistory()[1].toMap()["outcome"].toString(),"interrupted");
        QVERIFY(shell.gameHistory()[1].toMap()["ended"].toString().isEmpty());
        shell.dispatch(Action::Back);QCOMPARE(shell.overviewFace(),"summary");
        shell.showOverview("timeline");QCOMPARE(shell.focusIndex(),1);
        shell.dispatch(Action::Right);QCOMPARE(shell.focusIndex(),0);
        shell.showOverview("screens");shell.dispatch(Action::Back);QCOMPARE(shell.focusIndex(),1);
        QCOMPARE(shell.currentAdventureId(),originalGame);QVERIFY(launches.isEmpty());
    }
    void storyAchievementsAreAccountScopedAndDeduplicateEditions() {
        MockLibraryRepository library;
        class Provider : public AchievementProvider {
        public:
            AchievementContext identity{"fixture","owner"};
            AchievementContext snapshotOwner=identity;
            AchievementState state=AchievementState::Ready;
            AchievementContext context() const override { return identity; }
            QList<AchievementSet> sets() const override {
                return {{"first","same-provider-game","emerald-demo","First",{},true},
                    {"edition","same-provider-game","emerald-demo","Second",{},true},
                    {"uninstalled","foreign","missing-game","Absent",{},true}};
            }
            AchievementSnapshot snapshot(const QString& id) const override {
                return {snapshotOwner,id,state,{{"award","Award",{},{}},{"locked","Locked",{},{}}},
                    {{"award",true,AchievementMode::Standard,{}},{"award",true,AchievementMode::Hardcore,{}},
                     {"locked",false,{},{}},{"no-definition",true,{},{}}},{}};
            }
            void refresh(const QString&) override {}
        } provider;
        PlayerStory story(library,provider);
        QCOMPARE(story.summary()["achievementCount"].toString(),"1");
        const auto badges=story.profile()["badges"].toList();QCOMPARE(badges.size(),1);
        QCOMPARE(badges.first().toMap()["achievement"].toString(),"award");
        QCOMPARE(badges.first().toMap()["date"].toString(),"Date unavailable");
        provider.state=AchievementState::Offline;
        QCOMPARE(story.summary()["achievementCount"].toString(),"1");
        provider.state=AchievementState::Loading;
        QCOMPARE(story.summary()["achievementCount"].toString(),"1");
        provider.identity.accountId="another-owner";
        QVERIFY(story.profile()["badges"].toList().isEmpty());
        QVERIFY(story.summary()["achievementCount"].toString()!="1");
        provider.identity.accountId.clear();
        QCOMPARE(story.summary()["achievementNote"].toString(),"Connect RetroAchievements");
    }
    void storyCollageUsesDistinctGamesAndRejectsWrongMediaIdentity() {
        class Library : public LibraryRepository {
            MockLibraryRepository base;
        public:
            QList<World> worlds() const override { return base.worlds(); }
            QList<Adventure> adventures() const override { return base.adventures(); }
            QList<ResumePoint> resumePoints() const override { return {}; }
            HomeSnapshot home() const override { return base.home(); }
            std::optional<ExitMedia> exitMedia(const QString& id) const override {
                ExitMedia media;media.sessionId="capture-"+id;media.adventureId=id;
                media.capturedAt=QDateTime::fromString("2026-10-16T10:00:00Z",Qt::ISODate);
                if(id=="emerald-demo")media.capturedAt=media.capturedAt.addSecs(60);
                else if(id!="crystal-demo")media.adventureId="wrong-game";
                QImage image(id=="emerald-demo"?QSize(160,144):QSize(320,180),QImage::Format_RGB32);image.fill(Qt::darkGreen);
                if(id=="crystal-demo")for(int y=0;y<image.height();++y)for(int x=0;x<40;++x) {
                    image.setPixelColor(x,y,Qt::black);image.setPixelColor(image.width()-1-x,y,Qt::black);
                }
                QBuffer buffer(&media.jpeg);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"JPEG");
                return media;
            }
            PlayerHistory playerHistory() const override {
                return {{{"past-session","removed-game",QDateTime::currentDateTimeUtc(),{},{},PlaySessionOutcome::Interrupted}},1,1,{}};
            }
            std::optional<AdventureRegistration> registration(const QString& id) const override {
                if(id!="removed-game")return {};
                AdventureRegistration record;record.adventure.id=id;record.adventure.title="A remembered game";record.removed=true;return record;
            }
        } library;
        DisconnectedAchievementProvider provider;PlayerStory story(library,provider);
        QCOMPARE(story.history().first().toMap()["game"].toString(),"A remembered game");
        const auto media=story.media();QCOMPARE(media.size(),2);
        QCOMPARE(media[0].toMap()["adventure"].toString(),"emerald-demo");
        QCOMPARE(media[1].toMap()["adventure"].toString(),"crystal-demo");
        QCOMPARE(media[1].toMap()["source"].toString(),"image://exit-media/capture-crystal-demo");
        QVERIFY(qAbs(media[0].toMap()["ratio"].toDouble()-160.0/144)<.001);
        QVERIFY(qAbs(media[1].toMap()["ratio"].toDouble()-4.0/3)<.025);
        QCOMPARE(story.media(),media); // Cached dimensions retain the cropped provider ratio.
        MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,provider);
        QSignalSpy launches(&shell,&ShellController::homeLaunchPressed);
        shell.openGameHubFace("overview");shell.showOverview("screens");
        shell.dispatch(Action::Right);QCOMPARE(shell.focusIndex(),1);
        shell.dispatch(Action::Right);QCOMPARE(shell.focusIndex(),1);
        shell.dispatch(Action::Confirm);QCOMPARE(shell.overviewFace(),"screens");
        shell.dispatch(Action::ChangeView);QVERIFY(shell.album()->favoritesView());
        shell.dispatch(Action::ChangeView);QVERIFY(!shell.album()->favoritesView());QCOMPARE(shell.focusIndex(),1);
        shell.dispatch(Action::SystemMenu);shell.dispatch(Action::Back);QCOMPARE(shell.service(),"profile");QCOMPARE(shell.overviewFace(),"screens");QCOMPARE(shell.focusIndex(),1);
        shell.dispatch(Action::LocalAction);QVERIFY(shell.album()->filtering());
        shell.dispatch(Action::Back);QVERIFY(!shell.album()->filtering());
        shell.dispatch(Action::ToggleFavorite);QVERIFY(!shell.album()->error().isEmpty()); // No storage configured for this fixture; L3 reached album.
        shell.dispatch(Action::Back);shell.showOverview("screens");QCOMPARE(shell.focusIndex(),1);
        shell.goToSection("home");shell.activateDashboard("screens");
        QCOMPARE(shell.focusIndex(),0); // Home deep link overrides the album's old selection.
        QCOMPARE(shell.album()->count(),1);
        shell.dispatch(Action::Confirm);QCOMPARE(shell.overviewFace(),"screens");
        shell.dispatch(Action::ChangeView);QVERIFY(shell.album()->favoritesView());
        shell.dispatch(Action::ChangeView);QVERIFY(!shell.album()->favoritesView());QCOMPARE(shell.focusIndex(),0);
        shell.dispatch(Action::Back);QCOMPARE(shell.primaryId(),"home");
        QVERIFY(launches.isEmpty());
    }
    void dashboardFocusDoesNotRetargetOnUnreadUpdates() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        QSignalSpy launches(&shell,&ShellController::homeLaunchPressed);
        QSignalSpy commands(shell.social(),&SocialController::commandRequested);
        auto chat=HomeSocialFixture::chat("one","peer-one","First");
        QVariantMap snapshot{{"state","connected"},{"userId","self"},{"chats",QVariantList{chat}}};
        HomeSocialFixture::publish(*shell.social(),snapshot);QCOMPARE(shell.dashboard()->people().size(),1);
        shell.dispatch(Action::Up);QCOMPARE(shell.dashboard()->focus(),"lobbies");
        shell.dispatch(Action::Down);QCOMPARE(shell.dashboard()->focus(),"person:peer-one");
        shell.dispatch(Action::Confirm);QCOMPARE(shell.primaryId(),"online");QCOMPARE(shell.onlineFace(),"chats");QVERIFY(launches.isEmpty());
        QCOMPARE(commands.last()[0].toString(),"conversation");QCOMPARE(commands.last()[1].toMap()["id"],"one");
        shell.dispatch(Action::Back);QCOMPARE(shell.primaryId(),"home");QCOMPARE(shell.dashboard()->focus(),"person:peer-one");
        chat=HomeSocialFixture::chat("two","peer-two","Second");snapshot["chats"]=QVariantList{chat};HomeSocialFixture::publish(*shell.social(),snapshot);commands.clear();
        shell.dispatch(Action::Confirm);QCOMPARE(shell.primaryId(),"home");QVERIFY(commands.isEmpty());QVERIFY(launches.isEmpty());
        shell.dispatch(Action::Left);QCOMPARE(shell.dashboard()->focus(),"person:peer-two");
        shell.dispatch(Action::ToggleContinue);shell.dispatch(Action::Back);QCOMPARE(shell.dashboard()->focus(),"person:peer-two");
    }
    void homePeopleMergeRecencyAndTriggersSkipEmptyBlocks() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        auto* social=shell.social();auto* dashboard=shell.dashboard();
        auto first=HomeSocialFixture::chat("one","11","Ren",2000);
        auto second=HomeSocialFixture::chat("two","22","Iris",4000);
        auto group=HomeSocialFixture::chat("group","33","Group",9000);group["kind"]="groups";
        auto blocked=HomeSocialFixture::chat("blocked","44","Blocked",10000);
        auto muted=HomeSocialFixture::chat("muted","55","Muted",11000);muted["muted"]=true;
        QVariantMap snapshot{{"userId","99"},{"state","connected"},{"chats",QVariantList{first,second,group,blocked,muted}},
            {"friends",QVariantList{QVariantMap{{"id","44"},{"type",2}}}}};
        HomeSocialFixture::publish(*social,snapshot);
        HomeSocialFixture::encounter(*social,"99","11",6);
        HomeSocialFixture::encounter(*social,"99","66",5);
        auto people=dashboard->people();QCOMPARE(people.size(),3);
        QCOMPARE(people[0].toMap()["id"],"11");QCOMPARE(people[1].toMap()["id"],"66");
        QCOMPARE(people[0].toMap()["conversation"],"one");
        dashboard->setContext("emerald-demo",true);
        QSignalSpy launches(&shell,&ShellController::homeLaunchPressed);
        shell.dispatch(Action::NextFace);QCOMPARE(dashboard->focus(),"lobbies");
        shell.dispatch(Action::Down);QCOMPARE(dashboard->focus(),"person:11");
        shell.dispatch(Action::NextFace);QVERIFY(dashboard->focus().startsWith("event:"));
        shell.dispatch(Action::NextFace);QCOMPARE(dashboard->focus(),"play");
        shell.dispatch(Action::PreviousFace);QVERIFY(dashboard->focus().startsWith("event:"));
        shell.dispatch(Action::PreviousFace);QCOMPARE(dashboard->focus(),"lobbies");
        shell.dispatch(Action::Down);QCOMPARE(dashboard->focus(),"person:11");QVERIFY(launches.isEmpty());
        // Recency updates do not replace the selected person with another row.
        second["lastInteraction"]=9000;snapshot["chats"]=QVariantList{first,second};HomeSocialFixture::publish(*social,snapshot);
        QCOMPARE(dashboard->focus(),"person:11");QCOMPARE(dashboard->target(dashboard->focus())["conversation"],"one");
        first["unread"]=0;second["unread"]=0;snapshot["chats"]=QVariantList{first,second};HomeSocialFixture::publish(*social,snapshot);
        shell.dispatch(Action::NextFace);QCOMPARE(dashboard->focus(),"play");
        shell.dispatch(Action::PreviousFace);QCOMPARE(dashboard->focus(),"lobbies");
        shell.dispatch(Action::Down);QCOMPARE(dashboard->focus(),"person:22");
        snapshot={{"userId","other"},{"state","connected"}};HomeSocialFixture::publish(*social,snapshot);
        QVERIFY(dashboard->people().isEmpty());QCOMPARE(dashboard->focus(),"play");
        shell.dispatch(Action::NextFace);QCOMPARE(dashboard->focus(),"lobbies"); // Persistent Lobby entry remains available without people.
        shell.dispatch(Action::NextFace);QCOMPARE(dashboard->focus(),"play"); // Empty activity block is skipped.
    }
    void dashboardAchievementsUseVerifiedAccountSnapshotsNotToasts() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        const auto rows=shell.dashboard()->achievements();QVERIFY(rows.size()>=4);
        QSet<QString> sets;for(const auto& row:rows)sets.insert(row.toMap()["set"].toString());QCOMPARE(sets.size(),2);
        shell.showAchievements("Unrelated toast",{"Not a trophy"});QCOMPARE(shell.dashboard()->achievements(),rows);
        const auto key=rows.first().toMap()["key"].toString();shell.activateDashboard(key);
        QCOMPARE(shell.service(),"profile");QCOMPARE(shell.profileRoute(),"ra");QCOMPARE(shell.hall()->route(),"achievement-detail");
        QCOMPARE(shell.hall()->detail()["title"].toString(),rows.first().toMap()["name"].toString());
        shell.dispatch(Action::Back);QCOMPARE(shell.primaryId(),"home");QCOMPARE(shell.dashboard()->focus(),key);
        achievements.setAccount({});QVERIFY(shell.dashboard()->achievements().isEmpty());
        shell.dispatch(Action::Confirm);QCOMPARE(shell.primaryId(),"home");
    }
    void homeScreensOpensExistingRouteWithoutLaunchAndRestoresFocus() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        QSignalSpy launches(&shell,&ShellController::homeLaunchPressed);
        const auto game=shell.currentAdventureId();
        shell.dispatch(Action::Left);QCOMPARE(shell.dashboard()->focus(),"screens");
        shell.dispatch(Action::Confirm);QCOMPARE(shell.primaryId(),"home");
        QCOMPARE(shell.service(),"profile");QCOMPARE(shell.profileRoute(),"story");QCOMPARE(shell.overviewFace(),"screens");
        QCOMPARE(shell.currentAdventureId(),game);QVERIFY(launches.isEmpty());
        shell.dispatch(Action::Back);QCOMPARE(shell.primaryId(),"home");QCOMPARE(shell.dashboard()->focus(),"screens");
        shell.dispatch(Action::Right);QCOMPARE(shell.dashboard()->focus(),"play");
        shell.activateDashboard("screens");QCOMPARE(shell.overviewFace(),"screens");
        shell.dispatch(Action::Back);QCOMPARE(shell.dashboard()->focus(),"screens");QVERIFY(launches.isEmpty());
    }
    void genericHomeScopesAchievementsAndSpatialFocus() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        HomeDashboardController projection(library,achievements,*shell.social());
        auto* dashboard=&projection;dashboard->setContext("emerald-demo",false);
        const auto adapterRows=dashboard->achievements();QVERIFY(adapterRows.size()>3);
        dashboard->setContext("emerald-demo",true);
        const auto emerald=dashboard->achievements();QCOMPARE(emerald.size(),3);
        for(const auto& row:emerald)QCOMPARE(row.toMap()["adventure"].toString(),"emerald-demo");
        dashboard->move(Action::Up);QCOMPARE(dashboard->focus(),"screens");
        dashboard->move(Action::Down);QCOMPARE(dashboard->focus(),"play");
        dashboard->move(Action::Left);QCOMPARE(dashboard->focus(),emerald.last().toMap()["key"].toString());
        dashboard->move(Action::Right);QCOMPARE(dashboard->focus(),"play");
        const auto oldKey=emerald.first().toMap()["key"].toString();dashboard->select(oldKey);
        dashboard->setContext("crystal-demo",true);QCOMPARE(dashboard->focus(),"play");
        QVERIFY(dashboard->target(oldKey).isEmpty());
        for(const auto& row:dashboard->achievements())QCOMPARE(row.toMap()["adventure"].toString(),"crystal-demo");
        dashboard->setContext("unknown",true);QVERIFY(dashboard->achievements().isEmpty());
        dashboard->setContext({},true);QVERIFY(dashboard->achievements().isEmpty());
        dashboard->setContext("emerald-demo",false);QCOMPARE(dashboard->achievements(),adapterRows);
        dashboard->setContext("emerald-demo",true);
        HomeSocialFixture::publish(*shell.social(),{{"state","connected"},{"userId","self"},{"chats",QVariantList{
            HomeSocialFixture::chat("one","peer-one","First"),
            HomeSocialFixture::chat("two","peer-two","Second")}}});
        const auto chats=dashboard->people();QCOMPARE(chats.size(),2);
        dashboard->select(chats.first().toMap()["key"].toString());dashboard->move(Action::Right);
        QCOMPARE(dashboard->focus(),chats.last().toMap()["key"].toString());
        dashboard->move(Action::Right);QCOMPARE(dashboard->focus(),"screens");
    }
    void homeModeIconsRequireAdmittedRuntimeAndConnection() {
        QVERIFY(multiplayerPresentation({}).isEmpty());
        QVERIFY(multiplayerPresentation({{"id","future.runtime"},{"players",4}}).isEmpty());
        auto profile=QJsonObject{{"id","runtime.retroarch.snes.snes9x.v1"},{"connections",QJsonArray{"nearby"}}};
        const auto mode=multiplayerPresentation(profile);
        QCOMPARE(mode["id"].toString(),"netplay");QVERIFY(mode["nearby"].toBool());QVERIFY(!mode["online"].toBool());
        profile["connections"]=QJsonArray{"unknown"};QVERIFY(multiplayerPresentation(profile).isEmpty());
        profile={{"id","runtime.handheld.gba-gen3-en.v1"},{"transport","netpacket"}};
        QCOMPARE(multiplayerPresentation(profile)["id"].toString(),"link");
        profile.remove("transport");QVERIFY(multiplayerPresentation(profile).isEmpty());
        QCOMPARE(multiplayerPresentation({{"id","runtime.ppsspp.adhoc.v1"}})["id"].toString(),"wireless");
    }
    void dashboardScrollHasAnExclusiveShellInputOwner() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        QSignalSpy scroll(shell.dashboard(),&HomeDashboardController::scrollRequested);
        shell.dispatch(Action::ScrollDown);QCOMPARE(scroll.size(),1);QCOMPARE(shell.dashboard()->focus(),"play");
        shell.keyboard()->begin("Text","draft",20);shell.dispatch(Action::ScrollDown);QCOMPARE(scroll.size(),1);shell.keyboard()->cancel();
        shell.dispatch(Action::ToggleContinue);shell.dispatch(Action::ScrollUp);QCOMPARE(scroll.size(),1);shell.dispatch(Action::Back);
        shell.goToSection("social");shell.dispatch(Action::ScrollUp);QCOMPARE(scroll.size(),1);
    }
    void manifestAlternativesConstraintsAndAssets() {
        Adventure game;game.id="unknown";game.domain="multiverse";game.platformId="gba";
        ExperienceManifest manifest{"fixture",1,{
            {"screen","gba",{{"catalog.screenscraper","42"}}},
            {"other","gba",{{"catalog.other","abc"}}},
            {"digest","gba",{{"rom.sha256","verified-fixture-digest"}}},
            {"save","gba",{{"save.format","fixture-v2"},{"save.edition","edition-a"}}}},
            {{"sprites","fixture.entities",1,{}}}};
        QCOMPARE(manifest.match(game),0);
        for(const auto& rule:manifest.alternatives){QVariantMap evidence;for(auto i=rule.required.cbegin();i!=rule.required.cend();++i)evidence[i.key()]=i.value();QCOMPARE(manifest.match(game,evidence),2);}
        QCOMPARE(manifest.match(game,{{"save.format","fixture-v2"}}),0);
        game.platformId="nds";QCOMPARE(manifest.match(game,{{"catalog.screenscraper","42"}}),0);
        game.platformId="gba";manifest.version=99;QCOMPARE(manifest.match(game,{{"catalog.screenscraper","42"}}),0);
        manifest.version=1;QTemporaryDir dir;const auto legacy=dir.filePath("old-sprites");
        QCOMPARE(manifest.assetRoot(dir.path(),"sprites",legacy),legacy);
        const auto root=dir.filePath("adapters/fixture/packs/sprites");QVERIFY(QDir().mkpath(root));
        QCOMPARE(manifest.assetRoot(dir.path(),"sprites",legacy),root);
        QVERIFY(manifest.assetRoot(dir.path(),"../sprites").isEmpty());
    }
    void pokemonSelectsWithoutScrapingOrFilenames() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        auto* module=shell.module("pokemon");Adventure game;game.domain="multiverse";game.platformId="gba";game.title="renamed";
        QCOMPARE(module->match(game,{{"catalog.screenscraper","84406"}}),2);
        game.platformId="nds";QCOMPARE(module->match(game,{{"catalog.screenscraper","84406"}}),0);game.platformId="gba";
        QTemporaryDir dir;const auto path=dir.filePath("arbitrary.gba");
        QByteArray bytes(0xc0,0);bytes.replace(0xac,4,"BPEE");bytes[0xb2]=char(0x96);
        quint8 sum=0x19;for(int i=0xa0;i<0xbd;++i)sum+=quint8(bytes[i]);bytes[0xbd]=char(-sum);
        QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(bytes);file.close();
        QCOMPARE(module->match(game,observePokemonIdentity("gba",path)),2);
        QVERIFY(observePokemonIdentity("nds",path).isEmpty());
        bytes[0xbd]=char(quint8(bytes[0xbd])+1);QVERIFY(file.open(QIODevice::WriteOnly));file.write(bytes);file.close();
        QVERIFY(observePokemonIdentity("gba",path).isEmpty());
    }
    void universalProfileEditsPreserveAdapterPersonaFields() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        shell.goToTrainerFace("profile");shell.dispatch(Action::Confirm);
        auto* persona=pokemonModule(shell).persona();persona->setDraftName("Trainer");persona->activate(2);
        persona->picker()->applySearch("Treecko");persona->dispatch(Action::Confirm);persona->activate(3);
        QCOMPARE(profiles.load()->favoritePokemonId,"treecko");
        shell.goToPage(0);shell.openProfile();shell.dispatch(Action::Confirm);shell.trainer()->setDraftName("New name");shell.trainer()->activate(2);
        QCOMPARE(profiles.load()->name,"New name");QCOMPARE(profiles.load()->favoritePokemonId,"treecko");
        shell.goToTrainerFace("profile");QCOMPARE(persona->profile()["name"].toString(),"New name");
    }
    void ambiguousAdaptersNeverDependOnRegistrationOrder() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockHallOfFameRepository archive;MockAchievementProvider achievements;
        for(bool reverse:{false,true}) {
            ExperienceFactory factory=[&](ExperienceServices services){
                ExperienceModules modules;
                for(const auto& id:reverse?QStringList{"racing","rpg"}:QStringList{"rpg","racing"}) {
                    auto module=std::make_unique<test::CounterExperience>(id);module->matchGame=library.home().activeAdventureId;
                    modules.push_back(std::move(module));
                }
                modules.push_back(std::make_unique<GenericExperience>(services));return modules;
            };
            ShellController shell(library,profiles,adapter,platform,factory,archive,achievements);
            QCOMPARE(shell.homeView(),"game-home");
            shell.module("rpg")->setEnabled(false);QCOMPARE(shell.homeView(),"racing-home");
        }
    }
    void selectionRestoresOnlyMatchingOwnerGameAndModule() {
        ExperienceNavigation navigation;
        navigation.select("alice","emerald",pokemonExperienceDescriptor(),5);
        QVERIFY(navigation.show("boxes"));
        navigation.rememberNavigation({{"nested","retained"}});
        const auto generation=navigation.generation();
        navigation.select("alice","racer",genericExperienceDescriptor(),2);
        QVERIFY(!navigation.hasAdapter());QVERIFY(navigation.screen().isEmpty());
        QVERIFY(!navigation.show("boxes"));
        QVERIFY(!navigation.current("alice","emerald",generation));
        navigation.select("alice","emerald",pokemonExperienceDescriptor(),5);
        QCOMPARE(navigation.screen(),"boxes");
        QCOMPARE(navigation.navigation()["nested"].toString(),"retained");
        const auto saved=navigation.state();
        ExperienceNavigation reopened;
        reopened.select("alice","emerald",pokemonExperienceDescriptor(),5);reopened.restore(saved);
        QCOMPARE(reopened.screen(),"boxes");
        reopened.select("bob","emerald",pokemonExperienceDescriptor(),5);reopened.restore(saved);
        QCOMPARE(reopened.screen(),"dex");QVERIFY(reopened.navigation().isEmpty());
    }
    void materiallyDifferentFixturesDriveContentWithoutTitleRules() {
        const ExperienceDescriptor rpg{"fixture-rpg",2,"fixture-character-home",
            {"Hero","hero",{{"character","Character","fixture-character","person"},{"skills","Skills","fixture-skills","skills"}}}};
        const ExperienceDescriptor racing{"fixture-racing",2,"fixture-car-home",
            {"Garage","car",{{"cars","Cars","fixture-cars","car"},{"races","Races","fixture-races","flag"}}}};
        ExperienceNavigation navigation;
        navigation.select("alice","one",rpg);QVERIFY(navigation.show("skills"));
        QCOMPARE(navigation.view(),"fixture-skills");
        navigation.select("alice","two",racing);
        QCOMPARE(navigation.descriptor().homeView,"fixture-car-home");
        QCOMPARE(navigation.screenIds(),QStringList({"cars","races"}));
        QVERIFY(navigation.show("races"));QCOMPARE(navigation.view(),"fixture-races");
        navigation.select("alice","one",rpg);QCOMPARE(navigation.screen(),"skills");
    }
    void removedPartialAndFuturePackagesFallBackWithoutReinterpretingScreens() {
        ExperienceNavigation navigation;
        auto package=pokemonExperienceDescriptor();
        navigation.select("alice","game",package);navigation.show("boxes");
        const auto prior=navigation.generation();
        package.adapter.screens={{"dex","Guide","pokemon-guide","guide"}};
        QVERIFY(navigation.select("alice","game",package));QVERIFY(navigation.generation()>prior);
        QCOMPARE(navigation.screen(),"dex");
        package.adapter.screens.append(package.adapter.screens.first());
        QVERIFY(!ExperienceNavigation::valid(package));
        package.version=99;navigation.select("alice","game",package);
        QCOMPARE(navigation.descriptor().id,"generic");QVERIFY(!navigation.hasAdapter());
        QCOMPARE(navigation.adventure(),"game");
        auto saved=navigation.state();saved["version"]=99;navigation.restore(saved);
        QVERIFY(navigation.screen().isEmpty());QVERIFY(navigation.view().isEmpty());
    }
    void oldSlotStateMigratesWithoutCrossOwnerOrOpaqueStateLoss() {
        ExperienceNavigation navigation;
        navigation.select("alice","emerald",pokemonExperienceDescriptor());
        const QJsonObject entry{{"0","boxes"},{"1","hall"},{"navigation",QJsonObject{{"module",QJsonObject{{"focus",7}}}}}};
        navigation.restore({{"version",1},{"owner","alice"},{"entries",QJsonObject{{R"(["emerald","pokemon",1])",entry}}}});
        QCOMPARE(navigation.screen(),"boxes");QCOMPARE(navigation.legacyScreen(1),"hall");
        QCOMPARE(navigation.navigation()["module"].toObject()["focus"].toInt(),7);
        QVERIFY(navigation.show(navigation.legacyScreen(1)));QCOMPARE(navigation.screen(),"hall");
        QCOMPARE(navigation.state()["version"].toInt(),2);
    }
    void fiveRootsAndGameFreeHubDoNotDependOnAnAdapter() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ExperienceFactory factory=[](ExperienceServices services){ExperienceModules result;result.push_back(std::make_unique<GenericExperience>(services));return result;};
        ShellController shell(library,profiles,adapter,platform,factory,archive,achievements);
        QCOMPARE(shell.primaryNames(),QStringList({"Home","Games","GameHub","Online","Apps"}));
        shell.openGameHubFace("lobby");
        QCOMPARE(shell.primaryId(),"online");QCOMPARE(shell.onlineFace(),"lobby");
        shell.restoreNavigation({{"version",3},{"page","home"},{"socialFace","lobby"}});
        QCOMPARE(shell.primaryId(),"home");QCOMPARE(shell.hubFace(),"spaces");
        shell.goToSection("social");QCOMPARE(shell.socialSurface(),"chats");
        QCOMPARE(shell.faceNames(),QStringList({"Lobby","Bazaar","Crates","Chats"}));
        shell.goToSection("gamehub");QCOMPARE(shell.faceNames(),QStringList({"Game spaces"}));
        shell.goToSection("apps");QCOMPARE(shell.faceNames(),QStringList({"Apps","Browser","Files"}));
        shell.selectFace(2);QCOMPARE(shell.appsFace(),"files");
        shell.goToSection("social");QCOMPARE(shell.onlineFace(),"chats");
    }
    void legacyRoutesMigrateToTheCorrectOwner() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        shell.restoreNavigation({{"version",2},{"page","trainer"},{"trainerFace","ra"}});
        QCOMPARE(shell.primaryId(),"home");QCOMPARE(shell.service(),"profile");QCOMPARE(shell.profileRoute(),"ra");
        shell.restoreNavigation({{"version",2},{"page","companions"},{"pokemonFace","boxes"}});
        QCOMPARE(shell.primaryId(),"gamehub");QCOMPARE(shell.hubFace(),"adapter");QCOMPARE(shell.adapterScreen(),"boxes");
        shell.restoreNavigation({{"version",2},{"page","social"},{"socialFace","lobby"}});
        QCOMPARE(shell.primaryId(),"online");QCOMPARE(shell.onlineFace(),"lobby");
        shell.restoreNavigation({{"version",2},{"page",5}});
        QCOMPARE(shell.primaryId(),"apps");
        shell.restoreNavigation({{"version",1},{"page",4},{"hall",QJsonObject{{"route","achievement-sets"}}}});
        QCOMPARE(shell.primaryId(),"gamehub");
        shell.restoreNavigation({{"version",99},{"page","apps"}});
        QCOMPARE(shell.primaryId(),"home");
    }
    void globalDrawersRespectKeyboardAndKeepTheirReturnRoute() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        for(const auto& root:{"games","gamehub","online","apps"}) {
            shell.goToSection(root);shell.dispatch(Action::ToggleContinue);QVERIFY(shell.drawerOpen());
            shell.dispatch(Action::Back);QCOMPARE(shell.primaryId(),root);QVERIFY(!shell.drawerOpen());
            shell.dispatch(Action::Secondary);QVERIFY(shell.quickAppsOpen());
            shell.dispatch(Action::Back);QVERIFY(!shell.quickAppsOpen());QCOMPARE(shell.primaryId(),root);
            shell.dispatch(Action::Secondary);shell.dispatch(Action::ToggleContinue);
            QVERIFY(shell.drawerOpen());QVERIFY(!shell.quickAppsOpen());
            shell.dispatch(Action::Secondary);QVERIFY(shell.quickAppsOpen());QVERIFY(!shell.drawerOpen());
            shell.closeDrawers();QVERIFY(!shell.quickAppsOpen());QCOMPARE(shell.primaryId(),root);
            shell.dispatch(Action::Secondary);shell.dispatch(Action::NextFace);QVERIFY(!shell.quickAppsOpen());
            shell.dispatch(Action::Secondary);shell.apps()->accept({{"event","foreground"},{"id","fixture.desktop"}});
            QVERIFY(!shell.quickAppsOpen());shell.apps()->accept({{"event","shell"}});
        }
        shell.keyboard()->begin("Text","draft",32);
        shell.dispatch(Action::Secondary);QVERIFY(!shell.quickAppsOpen());QVERIFY(shell.keyboard()->isOpen());
        shell.dispatch(Action::ToggleContinue);QVERIFY(!shell.drawerOpen());QVERIFY(shell.keyboard()->isOpen());
    }
    void namesAndCollectionGroupingCannotGrantAnExperience() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        auto* module=shell.module("pokemon");
        Adventure game;game.id="generic";game.domain="multiverse";game.title="Pokemon Emerald";game.worldId="pokemon";
        QVERIFY(!module->supports(game));
        game.title="Changed title";game.additionalWorldIds={"pokemon","favorites"};
        QVERIFY(!module->supports(game));QVERIFY(!module->supports(std::nullopt));
        game.domain="pokemon";QVERIFY(module->supports(game));
    }
    void independentlyRegisteredModulesRunThroughTheActualShell() {
        class Library final:public LibraryRepository {
        public:
            MockLibraryRepository sample;
            QList<Adventure> adventures()const override {auto list=sample.adventures();for(const auto& id:{"rpg","racing"}){auto game=list.first();game.id=id;game.domain=id;list.append(game);}return list;}
            std::optional<AdventureRegistration> registration(const QString& id)const override { for(const auto& a:adventures())if(a.id==id){AdventureRegistration r;r.adventure=a;r.revision=1;return r;}return {}; }
            QList<World> worlds()const override{return sample.worlds();}
            QList<ResumePoint> resumePoints()const override{return {};}
            HomeSnapshot home()const override {auto home=sample.home();home.activeAdventureId="rpg";return home;}
        } library;
        MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ExperienceFactory factory=[](ExperienceServices services){
            ExperienceModules modules;
            modules.push_back(std::make_unique<test::CounterExperience>("rpg"));
            modules.push_back(std::make_unique<test::CounterExperience>("racing"));
            modules.push_back(std::make_unique<GenericExperience>(services));return modules;
        };
        ShellController shell(library,profiles,adapter,platform,factory,archive,achievements);
        QCOMPARE(shell.homeView(),"rpg-home");QVERIFY(!shell.module("pokemon"));
        auto* rpg=static_cast<test::CounterExperience*>(shell.module("rpg"));
        shell.activate(4,"experience-home:counter");QCOMPARE(rpg->count,4);QVERIFY(shell.notice().isEmpty());rpg->count=0;
        shell.openGameHubFace("adapter");shell.selectAdapterScreen("counter");
        shell.dispatch(Action::Confirm);QCOMPARE(rpg->count,1);
        shell.activate(7);QCOMPARE(shell.home()["fixtureValue"].toInt(),7);
        shell.selectAdapterScreen("second");QCOMPARE(rpg->lastFace,"second");
        shell.dispatch(Action::NextFace);QCOMPARE(shell.hubFace(),"spaces");QCOMPARE(rpg->lastFace,"second");
        shell.openGameHubFace("adapter");
        const auto oldToken=rpg->token();
        auto state=shell.navigationState();state["homeAdventure"]="racing";shell.restoreNavigation(state);
        QCOMPARE(shell.homeView(),"racing-home");QCOMPARE(shell.primaryNames()[2],"GameHub");
        QCOMPARE(shell.home()["fixtureGame"].toString(),"racing");QCOMPARE(shell.focusIndex(),0);
        rpg->requestFace();QCOMPARE(shell.experienceView(),"racing-counter");
        const auto live=shell.liveExperienceActions("rpg","123:1");QCOMPARE(live.size(),1);
        const auto liveId=live.first().toMap()["id"].toString();
        QVERIFY(shell.invokeLiveExperienceAction(liveId,"rpg","123:1"));QCOMPARE(rpg->liveCalls,1);QCOMPARE(rpg->lastLiveGame,"rpg");
        QVERIFY(shell.invokeLiveExperienceAction(liveId,"rpg","123:2"));QCOMPARE(rpg->liveCalls,1);
        state=shell.navigationState();state["homeAdventure"]="rpg";shell.restoreNavigation(state);
        QCOMPARE(shell.focusIndex(),7);QCOMPARE(rpg->lastFace,"second");
        emit rpg->screenRequested("counter",false,oldToken);QCOMPARE(rpg->lastFace,"second");
        rpg->requestText();QVERIFY(shell.keyboard()->isOpen());
        rpg->setEnabled(false);QCOMPARE(shell.homeView(),"game-home");QVERIFY(shell.dashboard()->genericLayout());QVERIFY(!shell.keyboard()->isOpen());
        rpg->applyText("late");QVERIFY(rpg->lastText.isEmpty());
        rpg->setEnabled(true);QCOMPARE(shell.homeView(),"rpg-home");QVERIFY(!shell.dashboard()->genericLayout());QCOMPARE(shell.hubFace(),"spaces");
        shell.openGameHubFace("adapter");shell.selectAdapterScreen("second");QCOMPARE(shell.focusIndex(),7);
        rpg->version=99;emit rpg->changed();QCOMPARE(shell.homeView(),"game-home");
        QCOMPARE(shell.currentAdventureId(),"rpg");
        shell.openProfile();shell.dispatch(Action::Confirm);QCOMPARE(shell.trainer()->editRows().size(),4);
    }
};
QTEST_GUILESS_MAIN(ExperienceTests)
#include "ExperienceTests.moc"
