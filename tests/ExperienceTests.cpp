#include "adapters/pokemon/PokemonExperience.h"
#include "adapters/generic/GenericExperience.h"
#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include "fixtures/adapters/CounterExperience.h"
#include "core/experience/ExperienceNavigation.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

using namespace trainer;
class ExperienceTests : public QObject {
    Q_OBJECT
private slots:
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
        QCOMPARE(shell.primaryNames(),QStringList({"Home","Games","GameHub","Social","Apps"}));
        shell.openGameHubFace("lobby");
        QCOMPARE(shell.primaryId(),"gamehub");QCOMPARE(shell.hubFace(),"lobby");
        QCOMPARE(shell.faceNames(),QStringList({"Overview","RetroAchievements","Lobby"}));
        shell.dispatch(Action::NextFace);QCOMPARE(shell.hubFace(),"overview");
        shell.goToSection("apps");QCOMPARE(shell.faceNames(),QStringList({"Apps","Crates","Browser","Files"}));
        shell.selectFace(3);QCOMPARE(shell.appsFace(),"files");
        shell.goToSection("social");QCOMPARE(shell.faceNames(),QStringList({"Messages","Communities","Discover"}));
    }
    void legacyRoutesMigrateToTheCorrectOwner() {
        MockLibraryRepository library;MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,builtinExperiences(dex,dex),archive,achievements);
        shell.restoreNavigation({{"version",2},{"page","trainer"},{"trainerFace","ra"}});
        QCOMPARE(shell.primaryId(),"gamehub");QCOMPARE(shell.hubFace(),"ra");
        shell.restoreNavigation({{"version",2},{"page","companions"},{"pokemonFace","boxes"}});
        QCOMPARE(shell.primaryId(),"gamehub");QCOMPARE(shell.hubFace(),"adapter");QCOMPARE(shell.adapterScreen(),"boxes");
        shell.restoreNavigation({{"version",2},{"page","social"},{"socialFace","lobby"}});
        QCOMPARE(shell.primaryId(),"gamehub");QCOMPARE(shell.hubFace(),"lobby");
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
        for(const auto& root:{"games","gamehub","social","apps"}) {
            shell.goToSection(root);shell.dispatch(Action::ToggleContinue);QVERIFY(shell.drawerOpen());
            shell.dispatch(Action::Back);QCOMPARE(shell.primaryId(),root);QVERIFY(!shell.drawerOpen());
            shell.dispatch(Action::Secondary);QVERIFY(shell.quickAppsOpen());
            shell.dispatch(Action::Back);QVERIFY(!shell.quickAppsOpen());QCOMPARE(shell.primaryId(),root);
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
        shell.dispatch(Action::NextFace);QCOMPARE(shell.hubFace(),"lobby");QCOMPARE(rpg->lastFace,"second");
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
        rpg->setEnabled(false);QCOMPARE(shell.homeView(),"game-home");QVERIFY(!shell.keyboard()->isOpen());
        rpg->applyText("late");QVERIFY(rpg->lastText.isEmpty());
        rpg->setEnabled(true);QCOMPARE(shell.homeView(),"rpg-home");QCOMPARE(shell.hubFace(),"overview");
        shell.openGameHubFace("adapter");shell.selectAdapterScreen("second");QCOMPARE(shell.focusIndex(),7);
        rpg->version=99;emit rpg->changed();QCOMPARE(shell.homeView(),"game-home");
        QCOMPARE(shell.currentAdventureId(),"rpg");
        shell.openProfile();shell.dispatch(Action::Confirm);QCOMPARE(shell.trainer()->editRows().size(),4);
    }
};
QTEST_GUILESS_MAIN(ExperienceTests)
#include "ExperienceTests.moc"
