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
        shell.goToPage(3);shell.dispatch(Action::Confirm);
        auto* persona=pokemonModule(shell).persona();persona->setDraftName("Trainer");persona->activate(2);
        persona->picker()->applySearch("Treecko");persona->dispatch(Action::Confirm);persona->activate(3);
        QCOMPARE(profiles.load()->favoritePokemonId,"treecko");
        shell.goToPage(0);shell.openProfile();shell.dispatch(Action::Confirm);shell.trainer()->setDraftName("New name");shell.trainer()->activate(2);
        QCOMPARE(profiles.load()->name,"New name");QCOMPARE(profiles.load()->favoritePokemonId,"treecko");
        shell.goToPage(3);QCOMPARE(persona->profile()["name"].toString(),"New name");
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
        QVERIFY(navigation.show(0,"boxes"));
        QVERIFY(navigation.show(1,"hall"));
        const auto generation=navigation.generation();
        navigation.select("alice","racer",genericExperienceDescriptor(),2);
        QCOMPARE(navigation.face(0),"details");
        QVERIFY(!navigation.show(0,"boxes"));
        QVERIFY(!navigation.current("alice","emerald",generation));
        navigation.select("alice","emerald",pokemonExperienceDescriptor(),5);
        QCOMPARE(navigation.face(0),"boxes");
        QCOMPARE(navigation.face(1),"hall");
        const auto saved=navigation.state();
        ExperienceNavigation reopened;
        reopened.select("alice","emerald",pokemonExperienceDescriptor(),5);
        reopened.restore(saved);
        QCOMPARE(reopened.face(0),"boxes");
        reopened.select("bob","emerald",pokemonExperienceDescriptor(),5);
        reopened.restore(saved);
        QCOMPARE(reopened.face(0),"dex");
        QCOMPARE(reopened.face(1),"profile");
    }
    void materiallyDifferentFixturesDriveContentWithoutTitleRules() {
        const ExperienceDescriptor rpg{"fixture-rpg",1,"fixture-character-home",
            {"Hero",{{"character","Character","fixture-character"},{"skills","Skills","fixture-skills"}}},
            {"Chronicle",{{"records","Records","fixture-records"}}}};
        const ExperienceDescriptor racing{"fixture-racing",1,"fixture-car-home",
            {"Garage",{{"cars","Cars","fixture-cars"}}},
            {"Career",{{"races","Races","fixture-races"}}}};
        ExperienceNavigation navigation;
        navigation.select("alice","one",rpg);
        QVERIFY(navigation.cycle(0,1));
        QCOMPARE(navigation.view(0),"fixture-skills");
        navigation.select("alice","two",racing);
        QCOMPARE(navigation.descriptor().homeView,"fixture-car-home");
        QCOMPARE(navigation.labels(0),QStringList{"Cars"});
        QCOMPARE(navigation.view(1),"fixture-races");
        navigation.select("alice","one",rpg);
        QCOMPARE(navigation.face(0),"skills");
    }
    void removedPartialAndFuturePackagesFallBackWithoutReinterpretingFaces() {
        ExperienceNavigation navigation;
        auto package=pokemonExperienceDescriptor();
        navigation.select("alice","game",package);
        navigation.show(0,"boxes");
        auto prior=navigation.generation();
        package.first.faces={{"dex","Guide","pokemon-guide"}};
        QVERIFY(navigation.select("alice","game",package));
        QVERIFY(navigation.generation()>prior);
        QCOMPARE(navigation.face(0),"dex");
        package.version=99;
        navigation.select("alice","game",package);
        QCOMPARE(navigation.descriptor().id,"generic");
        QCOMPARE(navigation.face(0),"details");
        QCOMPARE(navigation.adventure(),"game");
        auto saved=navigation.state();saved["version"]=99;
        navigation.show(1,"ra");navigation.restore(saved);
        QCOMPARE(navigation.face(1),"sessions");
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
        shell.goToPage(2);shell.dispatch(Action::Confirm);QCOMPARE(rpg->count,1);
        shell.activate(7);QCOMPARE(shell.home()["fixtureValue"].toInt(),7);
        shell.dispatch(Action::NextFace);QCOMPARE(rpg->lastFace,"second");
        const auto oldToken=rpg->token();
        auto state=shell.navigationState();state["homeAdventure"]="racing";shell.restoreNavigation(state);
        QCOMPARE(shell.homeView(),"racing-home");QCOMPARE(shell.primaryNames()[2],"racing tools");
        QCOMPARE(shell.home()["fixtureGame"].toString(),"racing");QCOMPARE(shell.focusIndex(),0);
        rpg->requestFace();QCOMPARE(shell.experienceView(),"racing-counter");
        const auto live=shell.liveExperienceActions("rpg","123:1");QCOMPARE(live.size(),1);
        const auto liveId=live.first().toMap()["id"].toString();
        QVERIFY(shell.invokeLiveExperienceAction(liveId,"rpg","123:1"));QCOMPARE(rpg->liveCalls,1);QCOMPARE(rpg->lastLiveGame,"rpg");
        QVERIFY(shell.invokeLiveExperienceAction(liveId,"rpg","123:2"));QCOMPARE(rpg->liveCalls,1);
        state=shell.navigationState();state["homeAdventure"]="rpg";shell.restoreNavigation(state);
        QCOMPARE(shell.focusIndex(),7);QCOMPARE(rpg->lastFace,"second");
        emit rpg->faceRequested(0,"counter",false,oldToken);QCOMPARE(rpg->lastFace,"second");
        rpg->requestText();QVERIFY(shell.keyboard()->isOpen());
        rpg->setEnabled(false);QCOMPARE(shell.homeView(),"game-home");QVERIFY(!shell.keyboard()->isOpen());
        rpg->applyText("late");QVERIFY(rpg->lastText.isEmpty());
        rpg->setEnabled(true);QCOMPARE(shell.homeView(),"rpg-home");QCOMPARE(shell.focusIndex(),7);
        rpg->version=99;emit rpg->changed();QCOMPARE(shell.homeView(),"game-home");
        QCOMPARE(shell.currentAdventureId(),"rpg");
        shell.openProfile();shell.dispatch(Action::Confirm);QCOMPARE(shell.trainer()->editRows().size(),4);
    }
};
QTEST_GUILESS_MAIN(ExperienceTests)
#include "ExperienceTests.moc"
