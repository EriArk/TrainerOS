#include "features/experience/ExperienceNavigation.h"
#include <QtTest>

using namespace trainer;
class ExperienceTests : public QObject {
    Q_OBJECT
private slots:
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
        Adventure game;game.id="generic";game.domain="multiverse";game.title="Pokemon Emerald";
        game.worldId="pokemon";
        QCOMPARE(ExperienceNavigation::resolve(game).id,"generic");
        game.title="Changed title";game.additionalWorldIds={"pokemon","favorites"};
        QCOMPARE(ExperienceNavigation::resolve(game).id,"generic");
        QCOMPARE(ExperienceNavigation::resolve(std::nullopt).id,"generic");
    }
};
QTEST_GUILESS_MAIN(ExperienceTests)
#include "ExperienceTests.moc"
