#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

using namespace trainer;

namespace {
int keyIndex(const TextEntryController& keyboard, const QString& id) {
    const auto keys = keyboard.keys();
    for (int i = 0; i < keys.size(); ++i) if (keys[i].toMap()["id"] == id) return i;
    return -1;
}
QString focusedKey(const TextEntryController& keyboard) {
    return keyboard.keys().at(keyboard.focusIndex()).toMap()["id"].toString();
}
void tap(TextEntryController& keyboard, Action action, int count = 1) {
    for (int i = 0; i < count; ++i) keyboard.dispatch(action);
}
}

class InteractionTests : public QObject {
    Q_OBJECT
private slots:
    void centerActivitiesAreAnIsolatedRehearsal() {
        PartyPresentation party(true);
        party.setAdventure("one", "First");
        party.dispatch(Action::Down); party.dispatch(Action::Down); party.dispatch(Action::Down);
        QVERIFY(party.activitiesFocused());
        party.dispatch(Action::Confirm); QCOMPARE(party.section(), "activities");
        auto* activities = party.activities();
        activities->activate(0); activities->dispatch(Action::Right); activities->dispatch(Action::Confirm);
        QCOMPARE(activities->focusIndex(), 1); QVERIFY(!activities->reaction().isEmpty());
        activities->dispatch(Action::Secondary); QVERIFY(activities->reaction().contains("unchanged"));
        party.setAdventure("one", "Renamed"); QCOMPARE(activities->route(), "playroom");
        party.setAdventure("two", "Other"); QCOMPARE(activities->route(), "menu"); QVERIFY(activities->reaction().isEmpty());
        activities->activate(1); activities->dispatch(Action::Confirm); QCOMPARE(activities->stage(), "preview");
        activities->dispatch(Action::Back); QCOMPARE(activities->stage(), "setup");
        activities->dispatch(Action::Back); activities->activate(2);
        activities->dispatch(Action::Confirm); QCOMPARE(activities->stage(), "review");
        activities->dispatch(Action::Confirm); QCOMPARE(activities->stage(), "interrupted");
        activities->dispatch(Action::Back); activities->dispatch(Action::Back); activities->dispatch(Action::Back);
        QCOMPARE(party.section(), "party"); QVERIFY(party.activitiesFocused());
        party.dispatch(Action::Up); QCOMPARE(party.focusIndex(), 0);
        PartyPresentation personal(false);
        personal.dispatch(Action::Down); QVERIFY(personal.activitiesFocused());
        personal.dispatch(Action::Confirm);
        for (int i = 0; i < 3; ++i) {
            personal.activities()->activate(i);
            personal.activities()->dispatch(Action::Secondary);
            QVERIFY(personal.activities()->reaction().isEmpty());
            QCOMPARE(personal.activities()->stage(), "setup");
            personal.activities()->dispatch(Action::Confirm);
            QCOMPARE(personal.activities()->route(), "menu");
        }
        QVERIFY(personal.entries().isEmpty());
    }
    void partyPresentationStaysReadOnlyAndContextBound() {
        PartyPresentation sample(true);
        sample.setAdventure("one", "First Adventure");
        QCOMPARE(sample.entries().size(),6);
        sample.activate(1); QCOMPARE(sample.detail()["hp"],"0 / 38");
        sample.dispatch(Action::Secondary); QCOMPARE(sample.section(),"party"); // Detail owns local input.
        sample.dispatch(Action::Back); QCOMPARE(sample.focusIndex(),1);
        sample.dispatch(Action::Secondary); QCOMPARE(sample.entries().size(),12);
        sample.dispatch(Action::Right); sample.dispatch(Action::Right); sample.dispatch(Action::Right);
        sample.dispatch(Action::Right); QCOMPARE(sample.box(),1);
        sample.dispatch(Action::Down); sample.dispatch(Action::Right); sample.activate(5);
        QCOMPARE(sample.detail()["kind"],"unreadable");
        sample.dispatch(Action::Back); sample.openSaves(); sample.returnFromSaves();
        QCOMPARE(sample.section(),"storage"); QCOMPARE(sample.focusIndex(),5);
        sample.setAdventure("one", "Same Adventure"); QCOMPARE(sample.focusIndex(),5);
        sample.setAdventure("two", "Other Adventure"); QCOMPARE(sample.focusIndex(),0); QCOMPARE(sample.box(),0); QVERIFY(!sample.detailOpen());
        PartyPresentation personal(false); personal.setAdventure("one", "Real Adventure");
        QVERIFY(personal.entries().isEmpty()); QVERIFY(personal.detail().isEmpty());
        personal.activate(0); QCOMPARE(personal.section(),"saves");
        personal.returnFromSaves(); personal.dispatch(Action::Secondary);
        QVERIFY(personal.entries().isEmpty()); QVERIFY(!personal.sample());
        personal.setAdventure("missing", {}); QVERIFY(personal.status().contains("no longer linked"));
    }
    void multiverseIsolationAndModalPriority() {
        MockLibraryRepository library; MockTrainerRepository profiles;
        MockAdventureAdapter adapter; DevelopmentPlatformService platform;
        MockPokedexRepository dex; MockHallOfFameRepository archive; MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        QSignalSpy launches(&shell, &ShellController::homeLaunchPressed);
        const auto pokemon = shell.currentAdventureId();
        shell.dispatch(Action::Secondary); QVERIFY(shell.multiverseHome());
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::Right); shell.dispatch(Action::Confirm);
        QCOMPARE(shell.multiverse()->selected()["id"], "sample-orbit");
        QCOMPARE(shell.currentAdventureId(), pokemon); QCOMPARE(launches.size(), 0);
        shell.dispatch(Action::Confirm); QVERIFY(shell.notice().contains("No game"));
        shell.dispatch(Action::Secondary); QVERIFY(shell.multiverseHome()); // Notice traps X.
        shell.dispatch(Action::Back); shell.dispatch(Action::Secondary); QVERIFY(!shell.multiverseHome());
        QCOMPARE(shell.currentAdventureId(), pokemon);
        shell.goToPage(1); const auto route = shell.worlds()->navigationState();
        shell.dispatch(Action::NextFace); QVERIFY(shell.multiverseFace());
        shell.dispatch(Action::Confirm); QCOMPARE(shell.multiverse()->route(), "games");
        shell.dispatch(Action::Secondary); QVERIFY(shell.keyboard()->isOpen());
        shell.dispatch(Action::NextFace); QVERIFY(shell.multiverseFace());
        shell.dispatch(Action::Back); QVERIFY(!shell.keyboard()->isOpen());
        shell.dispatch(Action::Down); shell.dispatch(Action::Confirm);
        QCOMPARE(shell.multiverse()->focusIndex(), 1); // Missing file cannot be selected.
        shell.dispatch(Action::Left); QCOMPARE(shell.multiverse()->focusIndex(), 1);
        shell.dispatch(Action::Back); QCOMPARE(shell.multiverse()->route(), "games");
        shell.dispatch(Action::PreviousFace); QVERIFY(!shell.multiverseFace());
        QCOMPARE(shell.worlds()->navigationState(), route);
        shell.dispatch(Action::NextFace); QCOMPARE(shell.multiverse()->focusIndex(), 1);
        shell.dispatch(Action::Up); shell.dispatch(Action::Confirm); shell.dispatch(Action::Confirm);
        QCOMPARE(shell.page(), 0); QVERIFY(shell.multiverseHome());
        QCOMPARE(shell.multiverse()->selected()["id"], "sample-courier");
        QCOMPARE(shell.currentAdventureId(), pokemon); QCOMPARE(launches.size(), 0);
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::Secondary); QVERIFY(shell.multiverseHome());
        shell.dispatch(Action::Back); shell.dispatch(Action::SystemMenu); shell.dispatch(Action::Secondary);
        QVERIFY(shell.multiverseHome()); shell.dispatch(Action::Back);
        shell.goToPage(2); QVERIFY(shell.resumePoints() != shell.multiverse()->choices());
        shell.dispatch(Action::Home); QVERIFY(shell.multiverseHome());
    }
    void realMultiverseSelectionLaunchAndMissingContent() {
        class Library final:public LibraryRepository {
        public:
            QList<AdventureRegistration> records;
            QList<World> worlds()const override{return {{"hoenn","Hoenn",{}}};}
            QList<Adventure> adventures()const override{QList<Adventure> r;for(const auto& x:records)r.append(x.adventure);return r;}
            QList<ResumePoint> resumePoints()const override{return {};}
            HomeSnapshot home()const override{return {"pokemon",{},{},{}};}
            bool editable()const override{return true;}
            std::optional<AdventureRegistration> registration(const QString& id)const override{for(const auto& r:records)if(r.adventure.id==id)return r;return {};}
        } library;
        class Adapter final:public AdventureAdapter {
        public:
            QString launched;
            QString id()const override{return "fixture";}
            AdventureCapabilities capabilities(const Adventure&)const override{return {true,false,false};}
            AdventureResult launch(const Adventure& a)override{launched=a.id;return {true,{},true};}
            AdventureResult resume(const Adventure&,const ResumePoint&)override{return {false,{}};}
        } adapter;
        QTemporaryDir dir;const auto path=dir.filePath("fixture.gba");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("original fixture");file.close();
        AdventureRegistration pokemon;pokemon.adventure.id="pokemon";pokemon.adventure.worldId="hoenn";pokemon.adventure.title="Pokemon fixture";pokemon.contentPath=path;library.records.append(pokemon);
        for(int i=0;i<2;++i){auto r=pokemon;r.adventure.id="multi"+QString::number(i);r.adventure.title="General "+QString::number(i);r.adventure.domain="multiverse";r.adventure.worldId.clear();r.adventure.platformId=i?"snes":"gba";r.contentPath=i?dir.filePath("missing.sfc"):path;r.contentAvailable=!i;library.records.append(r);}
        MockTrainerRepository profiles;DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        QCOMPARE(shell.multiverse()->systems().size(),1);shell.goToPage(1);shell.dispatch(Action::NextFace);shell.dispatch(Action::Confirm);
        shell.multiverse()->applySearch("absent");QVERIFY(shell.multiverse()->games().isEmpty());QCOMPARE(shell.multiverse()->systems().size(),1);
        shell.dispatch(Action::Confirm);shell.dispatch(Action::Confirm);shell.dispatch(Action::Confirm);
        QCOMPARE(shell.page(),0);QVERIFY(shell.multiverseHome());QVERIFY(adapter.launched.isEmpty());QCOMPARE(shell.currentAdventureId(),"pokemon");
        const auto state=shell.navigationState();shell.dispatch(Action::Confirm);QCOMPARE(adapter.launched,"multi0");
        ShellController restored(library,profiles,adapter,platform,dex,dex,archive,achievements);restored.restoreNavigation(state);
        QVERIFY(restored.multiverseHome());QCOMPARE(restored.multiverse()->selected()["id"],"multi0");QCOMPARE(restored.currentAdventureId(),"pokemon");
        QVERIFY(QFile::remove(path));library.records[1].contentAvailable=false;restored.refreshLibrary();QVERIFY(restored.multiverse()->systems().isEmpty());
        QCOMPARE(restored.multiverse()->selected()["id"],"multi0");QVERIFY(!restored.multiverse()->selected()["linked"].toBool());
        restored.restoreNavigation(QJsonObject{{"version",1}});QVERIFY(restored.multiverse()->selected().isEmpty());QVERIFY(!restored.multiverseHome());
    }
    void multiverseFilteringAndEmptyProduction() {
        MultiversePresentation sample(true);
        sample.activate(0); sample.applySearch("lantern"); QCOMPARE(sample.games().size(), 1);
        sample.dispatch(Action::ToggleContinue); QVERIFY(sample.games().isEmpty());
        sample.dispatch(Action::Confirm); QCOMPARE(sample.games().size(), 2); QVERIFY(sample.query().isEmpty());
        sample.applySearch("courier"); sample.dispatch(Action::Back);
        sample.activate(1); QVERIFY(sample.query().isEmpty());
        sample.dispatch(Action::Back); sample.activate(0); QCOMPARE(sample.query(), "courier");
        sample.select("sample-lantern"); QVERIFY(sample.selected().isEmpty());
        MultiversePresentation personal(false);
        QVERIFY(personal.systems().isEmpty());
        for (const auto action : {Action::Down, Action::Up, Action::Left, Action::Right}) personal.dispatch(action);
        QCOMPARE(personal.focusIndex(), 0);
        QVERIFY(personal.choices().isEmpty()); personal.select("sample-courier"); QVERIFY(personal.selected().isEmpty());
        personal.activate(0); QVERIFY(personal.games().isEmpty());
        personal.dispatch(Action::Confirm); QCOMPARE(personal.route(), "systems");
    }
    void personalLibraryCannotEnterSetupPreview() {
        class Library final : public LibraryRepository {
        public:
            QList<World> worlds() const override { return {}; }
            QList<Adventure> adventures() const override { return {}; }
            QList<ResumePoint> resumePoints() const override { return {}; }
            HomeSnapshot home() const override { return {}; }
            bool editable() const override { return true; }
        } library;
        MockTrainerRepository profiles; MockAdventureAdapter adapter; DevelopmentPlatformService platform;
        MockPokedexRepository dex; MockHallOfFameRepository archive; MockAchievementProvider achievements;
        achievements.enableAccountPreview();
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        QVERIFY(!shell.multiverse()->sample());
        shell.dispatch(Action::Secondary); QVERIFY(shell.multiverseHome());
        shell.dispatch(Action::ToggleContinue); QVERIFY(shell.resumePoints().isEmpty());
        shell.dispatch(Action::Confirm); QVERIFY(!shell.drawerOpen());
        shell.dispatch(Action::Confirm); QCOMPARE(shell.page(), 1); QVERIFY(shell.multiverseFace());
        shell.dispatch(Action::Confirm); QVERIFY(shell.multiverse()->games().isEmpty());
        shell.dispatch(Action::Home); shell.dispatch(Action::Secondary); QVERIFY(!shell.multiverseHome());
        shell.dispatch(Action::Confirm); QCOMPARE(shell.page(), 1); QVERIFY(!shell.multiverseFace());
        shell.dispatch(Action::Home);
        shell.dispatch(Action::SystemMenu); shell.activate(0); shell.activate(4);
        shell.activate(2); QCOMPARE(shell.service(),"settings"); QVERIFY(!shell.notice().isEmpty());
        shell.dispatch(Action::Back); shell.activate(1); QVERIFY(shell.hall()->account()->isOpen());
        shell.dispatch(Action::Back); QVERIFY(!shell.hall()->account()->isOpen());
        QCOMPARE(shell.service(),"settings"); QCOMPARE(shell.focusIndex(),1);
        shell.dispatch(Action::Back); QCOMPARE(shell.service(),"settings"); QCOMPARE(shell.settings()->category(),4); QCOMPARE(shell.focusIndex(),4);
        // Editing stays inside Settings, with vertical Save / Cancel navigation.
        shell.activate(4); shell.activate(0);
        QVERIFY(shell.trainer()->editing()); QCOMPARE(shell.service(), "settings");
        const auto before = shell.trainer()->profile();
        shell.trainer()->setDraftName("Discard this draft");
        for(int i=0;i<4;++i) shell.dispatch(Action::Down);
        QCOMPARE(shell.focusIndex(),4); shell.dispatch(Action::Confirm);
        QVERIFY(!shell.trainer()->editing()); QCOMPARE(shell.trainer()->profile(),before);
        QCOMPARE(shell.settings()->rowFocus(),0);
        shell.dispatch(Action::Back);
        achievements.setAccount({});
        shell.activate(4); shell.activate(1); shell.activate(0); QVERIFY(shell.keyboard()->isOpen());
        shell.dispatch(Action::NextPage); QVERIFY(!shell.keyboard()->isOpen());
        QVERIFY(!shell.hall()->account()->isOpen()); QVERIFY(!shell.serviceOpen());
    }
    void trainerSetupRehearsalNeverWritesProfiles() {
        MockLibraryRepository library; MockTrainerRepository profiles;
        MockAdventureAdapter adapter; DevelopmentPlatformService platform;
        MockPokedexRepository dex; MockHallOfFameRepository archive; MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        const auto original = shell.trainer()->profile();
        shell.dispatch(Action::SystemMenu); shell.activate(0); shell.activate(4);
        QCOMPARE(shell.service(), "settings"); shell.activate(2);
        QCOMPARE(shell.service(), "trainer-setup");
        auto* flow = shell.trainerSetup();
        flow->activate(0); flow->activate(0); flow->activate(3);
        QVERIFY(!flow->error().isEmpty()); QCOMPARE(flow->stage(), "identity");
        shell.activate(0); QVERIFY(shell.keyboard()->isOpen());
        shell.keyboard()->activate(keyIndex(*shell.keyboard(), "R"));
        shell.keyboard()->activate(keyIndex(*shell.keyboard(), "apply"));
        // Direct text completion below also exercises validation independently of key geometry.
        shell.keyboard()->cancel(); flow->applyName(" River ");
        flow->activate(3); QCOMPARE(flow->stage(), "pin");
        flow->activate(12); QVERIFY(!flow->error().isEmpty());
        for (int i=0;i<4;++i) flow->activate(i);
        QCOMPARE(flow->pinMask().size(),4); QVERIFY(!flow->pinMask().contains("1234"));
        flow->activate(12); QCOMPARE(flow->stage(), "repeat");
        for (int i=0;i<4;++i) flow->activate(0);
        flow->activate(12); QVERIFY(!flow->error().isEmpty()); QVERIFY(flow->pinMask().isEmpty());
        for (int i=0;i<4;++i) flow->activate(i);
        flow->activate(12); QCOMPARE(flow->stage(), "review");
        QCOMPARE(flow->pinChoice(), "PIN chosen for preview");
        flow->activate(1); QCOMPARE(flow->name(), "River");
        flow->activate(3); flow->activate(13); QCOMPARE(flow->pinChoice(), "No PIN chosen");
        flow->activate(0); QCOMPARE(flow->stage(), "done");
        QCOMPARE(shell.trainer()->profile(),original);
        shell.dispatch(Action::NextPage); QVERIFY(!shell.serviceOpen());
        QVERIFY(flow->name().isEmpty()); QVERIFY(flow->pinMask().isEmpty());
    }
    void trainerSetupUnlockAndGlobalCancellation() {
        TrainerSetupPresentation flow; flow.begin(); flow.activate(1); flow.activate(1);
        QCOMPARE(flow.stage(), "unlock");
        for (int i=0;i<8;++i) flow.activate(0);
        QCOMPARE(flow.pinMask().size(),6); flow.activate(12);
        QVERIFY(!flow.error().isEmpty()); QVERIFY(flow.pinMask().isEmpty());
        for (int i=0;i<4;++i) flow.activate(i);
        flow.activate(12); QCOMPARE(flow.stage(),"done");
        flow.close(); flow.activate(1); flow.activate(1); flow.activate(0);
        flow.dispatch(Action::Back); QCOMPARE(flow.stage(),"chooser");
        QVERIFY(flow.pinMask().isEmpty()); QCOMPARE(flow.focusIndex(),1);
    }
    void sharedAdventureSelectionAndPairedCenter() {
        class Library final : public LibraryRepository {
        public:
            MockLibraryRepository sample;
            QString missing, latest = "crystal-demo";
            QList<World> worlds() const override { return sample.worlds(); }
            QList<Adventure> adventures() const override {
                auto rows=sample.adventures(); rows.removeIf([&](const auto& a){return a.id==missing;}); return rows;
            }
            QList<ResumePoint> resumePoints() const override { return sample.resumePoints(); }
            HomeSnapshot home() const override { auto h=sample.home();h.activeAdventureId=latest;return h; }
            std::optional<AdventureRegistration> registration(const QString& id) const override {
                for(const auto& a:adventures())if(a.id==id){AdventureRegistration r;r.adventure=a;r.revision=1;return r;}return {};
            }
        } library;
        class Service final : public SaveBackupService {
        public:
            bool working=false;
            QString inspected;
            std::function<void(SaveBackupSnapshot)> pending;
            bool busy() const override { return working; }
            bool supports(const AdventureRegistration&) const override { return true; }
            void inspect(const AdventureRegistration& r,QObject*,std::function<void(SaveBackupSnapshot)> done) override {
                inspected=r.adventure.id;pending=std::move(done);working=true;emit busyChanged();
            }
            void finish() {
                auto done=std::move(pending);working=false;
                done({true,true,"token",{},{{inspected,"revision",QDateTime::currentDateTimeUtc(),32,true,true,false}}});
                emit busyChanged();
            }
            void create(const AdventureRegistration&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override {}
            void restore(const AdventureRegistration&,const SaveBackup&,const QString&,QObject*,std::function<void(SaveBackupResult)>) override {}
        } service;
        MockTrainerRepository profiles; MockAdventureAdapter adapter; DevelopmentPlatformService platform;
        MockPokedexRepository dex; MockHallOfFameRepository archive; MockAchievementProvider achievements;
        ShellController shell(library,profiles,adapter,platform,dex,dex,archive,achievements);
        shell.center()->configure(&service);
        QSignalSpy launched(&shell,&ShellController::homeLaunchPressed);
        shell.goToPage(2); shell.dispatch(Action::Down);
        const auto dexState=shell.pokedex()->navigationState();
        shell.dispatch(Action::ToggleContinue); QVERIFY(shell.drawerOpen());
        shell.dispatch(Action::Right); QCOMPARE(shell.focusIndex(),1);
        shell.dispatch(Action::Back); QCOMPARE(shell.pokedex()->navigationState(),dexState);
        QCOMPARE(shell.currentAdventureId(),library.latest);
        shell.dispatch(Action::ToggleContinue); shell.activate(0);
        QCOMPARE(shell.currentAdventureId(),QString("emerald-demo")); QVERIFY(launched.isEmpty());
        QVERIFY(!shell.drawerOpen()); QCOMPARE(shell.page(),2);
        shell.dispatch(Action::NextFace); QVERIFY(shell.centerFace());
        QCOMPARE(service.inspected,shell.currentAdventureId());
        // A different choice while the previous game's read is in flight must
        // never show the old shelf under the new game's name.
        shell.dispatch(Action::ToggleContinue); shell.activate(1);
        const auto chosen=shell.currentAdventureId(); QVERIFY(chosen!="emerald-demo");
        service.finish(); QTRY_VERIFY(service.busy()); QCOMPARE(service.inspected,chosen);
        QVERIFY(shell.center()->rows().isEmpty()); service.finish();
        QCOMPARE(shell.center()->rows().first().toMap()["id"].toString(),chosen);
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::SystemMenu); shell.activate(0);
        QCOMPARE(shell.service(),QString("settings")); QVERIFY(!shell.drawerOpen());
        shell.dispatch(Action::Back); QVERIFY(shell.menuOpen()); shell.dispatch(Action::Back);
        QVERIFY(shell.centerFace()); QVERIFY(service.busy()); service.finish();
        shell.dispatch(Action::LocalAction); // Party/Storage opens the existing save shelf.
        shell.dispatch(Action::Confirm); QVERIFY(shell.center()->confirming());
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::NextFace);
        QVERIFY(!shell.drawerOpen()); QVERIFY(shell.centerFace()); QVERIFY(shell.center()->confirming());
        shell.dispatch(Action::Back); QVERIFY(!shell.center()->confirming());
        shell.dispatch(Action::Back); QVERIFY(shell.centerFace()); // B never flips a pair.
        shell.dispatch(Action::PreviousFace); QVERIFY(!shell.centerFace());
        QCOMPARE(shell.pokedex()->navigationState(),dexState);
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::NextPage);
        QVERIFY(!shell.drawerOpen()); QCOMPARE(shell.currentAdventureId(),chosen);
        shell.dispatch(Action::ToggleContinue); QVERIFY(shell.drawerOpen()); shell.dispatch(Action::Back);
        shell.goToPage(4); shell.dispatch(Action::ToggleContinue); QVERIFY(shell.drawerOpen());
        shell.dispatch(Action::SystemMenu); shell.dispatch(Action::ToggleContinue); QVERIFY(shell.menuOpen());
        shell.dispatch(Action::Back); shell.dispatch(Action::Back);
        shell.goToPage(2); shell.pokedex()->activateControl("rail",1);
        shell.dispatch(Action::ToggleContinue); shell.dispatch(Action::NextFace);
        QVERIFY(!shell.drawerOpen()); QVERIFY(!shell.centerFace());
        shell.dispatch(Action::Back); shell.dispatch(Action::Secondary);
        QVERIFY(shell.keyboard()->isOpen()); shell.dispatch(Action::NextFace);
        QVERIFY(!shell.centerFace()); shell.dispatch(Action::Back);
        const auto state=shell.navigationState();
        library.missing=chosen; library.latest="emerald-demo"; shell.refreshLibrary();
        QCOMPARE(shell.currentAdventureId(),chosen); QVERIFY(shell.home()["adventureId"].toString().isEmpty());
        shell.restoreNavigation(state); QCOMPARE(shell.currentAdventureId(),chosen);
        shell.dispatch(Action::NextFace); QVERIFY(shell.centerFace()); QVERIFY(shell.center()->rows().isEmpty());
        QVERIFY(shell.center()->message().contains("no longer linked")); QVERIFY(launched.isEmpty());
        const auto pairedState=shell.navigationState();
        shell.goToPage(0); shell.restoreNavigation(pairedState);
        QVERIFY(shell.centerFace()); QCOMPARE(shell.currentAdventureId(),chosen);
        QVERIFY(shell.center()->rows().isEmpty()); // Restart state does not invent a fallback shelf.
    }
    void caseSymbolsAndSecretEntry() {
        TextEntryController keyboard;
        QSignalSpy accepted(&keyboard, &TextEntryController::accepted);
        keyboard.begin("Password", "", 128, true);
        tap(keyboard, Action::Confirm);
        tap(keyboard, Action::Secondary); tap(keyboard, Action::Confirm);
        QCOMPARE(keyboard.text(), "aA");
        QCOMPARE(keyboard.displayText(), QString(2, QChar(0x2022)));
        // All printable ASCII punctuation is available, including quote/backslash.
        QString entered;
        for (int page = 0; page < 2; ++page) {
            tap(keyboard, Action::ToggleContinue);
            const auto keys = keyboard.keys();
            for (int i = 0; i < 26; ++i) {
                if (!keys[i].toMap()["visible"].toBool()) continue;
                entered += keys[i].toMap()["label"].toString();
                keyboard.activate(i);
            }
        }
        QString expected;
        for (int code = 33; code <= 126; ++code)
            if (!QChar(code).isLetterOrNumber()) expected += QChar(code);
        QCOMPARE(entered, expected);
        keyboard.activate(keyIndex(keyboard, "9"));
        QCOMPARE(keyboard.text(), "aA" + expected + "9");
        QCOMPARE(keyboard.displayText(), QString(keyboard.count(), QChar(0x2022)));
        const QString result = keyboard.text();
        keyboard.activate(keyIndex(keyboard, "apply"));
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(accepted.first().first().toString(), result);
        QVERIFY(keyboard.text().isEmpty()); QVERIFY(keyboard.displayText().isEmpty());
        keyboard.begin("Password", "synthetic secret", 64, true);
        keyboard.cancel();
        QVERIFY(keyboard.text().isEmpty()); QVERIFY(keyboard.displayText().isEmpty());
        keyboard.begin("Name", "", 24);
        tap(keyboard, Action::Confirm);
        QCOMPARE(keyboard.displayText(), "A"); // Other forms retain the existing default.
        QVERIFY(keyboard.metaObject()->indexOfProperty("text") < 0);
    }
    void symbolsKeepEveryVisibleKeyReachable() {
        TextEntryController keyboard;
        for (int page = 1; page <= 2; ++page) {
            keyboard.begin("Search", "", 128);
            tap(keyboard, Action::ToggleContinue, page);
            const auto keys = keyboard.keys();
            QSet<int> visited{0}; QList<QList<Action>> pending{{}};
            while (!pending.isEmpty()) {
                const auto path = pending.takeFirst();
                for (const auto direction : {Action::Up, Action::Down, Action::Left, Action::Right}) {
                    keyboard.begin("Search", "", 128);
                    tap(keyboard, Action::ToggleContinue, page);
                    for (const auto step : path) tap(keyboard, step);
                    tap(keyboard, direction);
                    const int next = keyboard.focusIndex();
                    QVERIFY(keys[next].toMap()["visible"].toBool());
                    if (!visited.contains(next)) { visited.insert(next); auto nextPath = path; nextPath.append(direction); pending.append(nextPath); }
                }
            }
            for (int i = 0; i < keys.size(); ++i)
                if (keys[i].toMap()["visible"].toBool()) QVERIFY2(visited.contains(i), qPrintable(keys[i].toMap()["id"].toString()));
        }
        keyboard.begin("Search", "", 128);
        keyboard.activate(keyIndex(keyboard, "Z"));
        tap(keyboard, Action::ToggleContinue, 2);
        QCOMPARE(focusedKey(keyboard), "A");
        const QString before = keyboard.text();
        keyboard.activate(keyIndex(keyboard, "Z")); // Hidden slots cannot insert characters.
        QCOMPARE(keyboard.text(), before);
    }
    void sessionChoicesRequireConfirmationAndCancelCleanly() {
        class Platform final : public PlatformService {
        public:
            bool canSwitchSession() const override { return true; }
            bool dedicatedSession() const override { return true; }
            QString sessionStatus() const override { return {}; }
        } platform;
        MockLibraryRepository library;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        MockPokedexRepository dex;
        MockHallOfFameRepository archive;
        MockAchievementProvider achievements;
        ShellController shell(library, profiles, adapter, platform, dex, dex, archive, achievements);
        QSignalSpy requested(&shell, &ShellController::modeRequested);
        QSignalSpy exited(&shell, &ShellController::exitRequested);
        shell.dispatch(Action::SystemMenu);
        shell.activate(4);
        QVERIFY(shell.modeConfirmation());
        QVERIFY(requested.isEmpty());
        shell.dispatch(Action::Back);
        QVERIFY(!shell.modeConfirmation());
        shell.activate(5);
        shell.dispatch(Action::NextPage);
        QVERIFY(!shell.modeConfirmation());
        QVERIFY(requested.isEmpty());
        shell.dispatch(Action::SystemMenu);
        shell.activate(5);
        shell.dispatch(Action::Confirm);
        QCOMPARE(requested.size(), 1);
        QCOMPARE(requested.first().first().toString(), "steam");
        shell.activate(6);
        QVERIFY(shell.powerMenu()); QVERIFY(!shell.modeConfirmation());
        QCOMPARE(shell.focusIndex(), 3); // Fresh Power menu defaults to Cancel.
        shell.dispatch(Action::Confirm);
        QVERIFY(!shell.powerMenu()); QCOMPARE(requested.size(), 1);
        shell.activate(4); // Maintenance remains explicit, never a Power default.
        QVERIFY(shell.modeConfirmation()); shell.dispatch(Action::Confirm);
        QCOMPARE(requested.last().first().toString(), "desktop");
        QVERIFY(exited.isEmpty());
    }
    void everyLetterAndNumber() {
        TextEntryController keyboard;
        keyboard.begin("Search", "", 80);
        const auto row = [&](int count) {
            for (int i = 0; i < count; ++i) {
                keyboard.dispatch(Action::Confirm);
                if (i + 1 < count) keyboard.dispatch(Action::Right);
            }
        };
        row(10); // A-J
        tap(keyboard, Action::Down); tap(keyboard, Action::Left, 9);
        row(10); // K-T
        tap(keyboard, Action::Down); tap(keyboard, Action::Left, 7);
        QCOMPARE(focusedKey(keyboard), "U");
        row(6); // U-Z
        QCOMPARE(keyboard.text(), "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        tap(keyboard, Action::Right, 3); tap(keyboard, Action::Up, 2);
        QCOMPARE(focusedKey(keyboard), "1");
        row(3); // 123
        tap(keyboard, Action::Down);
        for (int i = 0; i < 3; ++i) {
            tap(keyboard, Action::Confirm);
            if (i < 2) tap(keyboard, Action::Left);
        }
        tap(keyboard, Action::Down); row(3); tap(keyboard, Action::Down); tap(keyboard, Action::Confirm);
        QCOMPARE(keyboard.text(), "ABCDEFGHIJKLMNOPQRSTUVWXYZ1236547890");
    }
    void spatialEdgesAndWideKeys() {
        TextEntryController keyboard;
        keyboard.begin("Name", "", 24);
        tap(keyboard, Action::Up); tap(keyboard, Action::Left);
        QCOMPARE(focusedKey(keyboard), "A");
        tap(keyboard, Action::Right, 5); tap(keyboard, Action::Down, 3);
        QCOMPARE(focusedKey(keyboard), "space");
        tap(keyboard, Action::Up, 3);
        QCOMPARE(focusedKey(keyboard), "F"); // Preserve F's column through Space.
        tap(keyboard, Action::Right, 3); tap(keyboard, Action::Down, 3);
        QCOMPARE(focusedKey(keyboard), "apply");
        tap(keyboard, Action::Up, 3);
        QCOMPARE(focusedKey(keyboard), "I"); // Through Clear and Apply.
        tap(keyboard, Action::Right, 2);
        QCOMPARE(focusedKey(keyboard), "1"); // Across the physical gap.
        tap(keyboard, Action::Left);
        QCOMPARE(focusedKey(keyboard), "J");
        tap(keyboard, Action::Right, 3);
        QCOMPARE(focusedKey(keyboard), "3");
        tap(keyboard, Action::Right);
        QCOMPARE(focusedKey(keyboard), "3");
        tap(keyboard, Action::Down, 3);
        QCOMPARE(focusedKey(keyboard), "0");
        tap(keyboard, Action::Down);
        QCOMPARE(focusedKey(keyboard), "0");
        tap(keyboard, Action::Up, 3);
        QCOMPARE(focusedKey(keyboard), "3"); // 0 remembers the third numeric column.
        tap(keyboard, Action::Down, 3); tap(keyboard, Action::Left); tap(keyboard, Action::Up);
        QCOMPARE(focusedKey(keyboard), "clear"); // Enter Apply horizontally at its middle column.
    }
    void bufferApplyCancelAndLimit() {
        TextEntryController keyboard;
        QSignalSpy accepted(&keyboard, &TextEntryController::accepted);
        keyboard.begin("Name", "ER", 3);
        keyboard.activate(keyIndex(keyboard, "I"));
        keyboard.activate(keyIndex(keyboard, "2"));
        QCOMPARE(keyboard.text(), "ERI");
        QVERIFY(!keyboard.hint().isEmpty());
        keyboard.activate(keyIndex(keyboard, "delete"));
        QCOMPARE(keyboard.text(), "ER");
        keyboard.activate(keyIndex(keyboard, "space"));
        QCOMPARE(keyboard.text(), "ER ");
        keyboard.activate(keyIndex(keyboard, "clear"));
        QVERIFY(keyboard.text().isEmpty());
        keyboard.activate(keyIndex(keyboard, "0"));
        keyboard.activate(keyIndex(keyboard, "apply"));
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(accepted.first().first().toString(), "0");
        QVERIFY(!keyboard.isOpen());
        keyboard.begin("Name", "ORIGINAL", 24);
        keyboard.activate(keyIndex(keyboard, "clear"));
        keyboard.dispatch(Action::Back);
        QCOMPARE(accepted.size(), 1);
        QVERIFY(!keyboard.isOpen());
        keyboard.activate(0);
        QCOMPARE(accepted.size(), 1);
    }
    void existingUnicodeAndOversizedInput() {
        TextEntryController keyboard;
        const QString name = QString::fromUtf8("Aé🇯🇵é");
        keyboard.begin("Name", name, 4);
        QCOMPARE(keyboard.count(), 4);
        keyboard.activate(keyIndex(keyboard, "delete"));
        QCOMPARE(keyboard.text(), QString::fromUtf8("Aé🇯🇵"));
        keyboard.activate(keyIndex(keyboard, "delete"));
        QCOMPARE(keyboard.text(), QString::fromUtf8("Aé"));
        keyboard.begin("Name", "TOO LONG", 4);
        QCOMPARE(keyboard.text(), "TOO LONG");
        keyboard.activate(keyIndex(keyboard, "apply"));
        QVERIFY(keyboard.isOpen());
        QVERIFY(!keyboard.hint().isEmpty());
    }
    void profileCreateEditAndCancel() {
        MockTrainerRepository repository;
        TrainerController trainer(repository);
        MockPokedexRepository guide;
        trainer.picker()->setReference(&guide);
        QVERIFY(!trainer.exists());
        trainer.beginEdit();
        trainer.setDraftName("  ERI 2  ");
        trainer.activate(1); trainer.activate(2);
        trainer.picker()->applySearch("Treecko"); trainer.dispatch(Action::Confirm);
        QVERIFY(!repository.load());
        trainer.activate(3);
        QVERIFY(trainer.exists());
        QVERIFY(!trainer.editing());
        const auto saved = repository.load().value();
        QCOMPARE(saved.name, "ERI 2");
        QCOMPARE(saved.emblemId, "leaf");
        QCOMPARE(saved.favoritePokemonId, "treecko");
        QVERIFY(!saved.id.isEmpty());
        QVERIFY(saved.createdAt.isValid());
        trainer.beginEdit();
        trainer.setDraftName("DISCARD"); trainer.activate(1); trainer.activate(2);
        trainer.dispatch(Action::Back); trainer.activate(4);
        QCOMPARE(repository.load()->name, saved.name);
        QCOMPARE(repository.load()->emblemId, saved.emblemId);
        QCOMPARE(repository.load()->favoritePokemonId, saved.favoritePokemonId);
        trainer.beginEdit();
        trainer.setDraftName("ERI 3"); trainer.activate(3);
        QCOMPARE(repository.load()->name, "ERI 3");
        QCOMPARE(repository.load()->id, saved.id);
        QCOMPARE(repository.load()->createdAt, saved.createdAt);
        TrainerController reopened(repository);
        QCOMPARE(reopened.profile()["name"].toString(), "ERI 3");
    }
    void profileValidationAndFailedWrite() {
        MockTrainerRepository repository;
        TrainerController trainer(repository);
        trainer.beginEdit(); trainer.activate(3);
        QVERIFY(!trainer.error().isEmpty());
        QCOMPARE(trainer.focusIndex(), 0);
        QVERIFY(!repository.load());
        trainer.setDraftName(" \t "); trainer.activate(3);
        QVERIFY(!repository.load());
        trainer.setDraftName(QString(25, 'A')); trainer.activate(3);
        QVERIFY(!repository.load());
        trainer.setDraftName("A\nB"); trainer.activate(3);
        QVERIFY(!repository.load());
        trainer.setDraftName("ERI"); trainer.activate(3);
        const auto id = repository.load()->id;
        trainer.beginEdit(); trainer.setDraftName("ERI 2");
        repository.failNextSave(); trainer.activate(3);
        QVERIFY(trainer.editing());
        QVERIFY(!trainer.error().isEmpty());
        QCOMPARE(trainer.draftName(), "ERI 2");
        QCOMPARE(trainer.profile()["name"].toString(), "ERI");
        QCOMPARE(repository.load()->name, "ERI");
        QCOMPARE(trainer.focusIndex(), 3); // Retry stays reachable.
        trainer.activate(3);
        QCOMPARE(repository.load()->name, "ERI 2");
        QCOMPARE(repository.load()->id, id);
        QVERIFY(!trainer.editing());
    }
    void shellLayersAndProgressIsolation() {
        MockLibraryRepository library;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;
        MockPokedexRepository dex;
        MockHallOfFameRepository shellArchive;
        MockAchievementProvider shellAchievements;
        ShellController shell(library, profiles, adapter, platform, dex, dex, shellArchive, shellAchievements);
        const auto badges = shell.home()["badges"];
        const auto caught = shell.home()["caught"];
        shell.goToPage(3); shell.dispatch(Action::Confirm); shell.dispatch(Action::Confirm);
        auto* keyboard = shell.keyboard();
        keyboard->activate(keyIndex(*keyboard, "E"));
        shell.dispatch(Action::SystemMenu);
        for (int i = 0; i < 4; ++i) shell.dispatch(Action::Down);
        shell.dispatch(Action::Confirm); // Unavailable Desktop mode overlays the keyboard; Center is now usable.
        shell.dispatch(Action::Back); // Notice -> menu.
        QVERIFY(shell.menuOpen());
        shell.dispatch(Action::Back); // Menu -> keyboard.
        QCOMPARE(keyboard->text(), "E");
        QCOMPARE(focusedKey(*keyboard), "E");
        shell.dispatch(Action::ToggleContinue);
        QCOMPARE(keyboard->text(), "E");
        keyboard->activate(keyIndex(*keyboard, "apply"));
        QCOMPARE(shell.trainer()->draftName(), "E");
        shell.trainer()->activate(3);
        QCOMPARE(shell.home()["trainer"].toString(), "E");
        QCOMPARE(shell.home()["badges"], badges);
        QCOMPARE(shell.home()["caught"], caught);
        shell.dispatch(Action::Confirm); shell.dispatch(Action::Confirm); shell.dispatch(Action::Confirm);
        shell.dispatch(Action::PreviousPage);
        QCOMPARE(shell.page(), 2);
        QVERIFY(!keyboard->isOpen());
        QVERIFY(!shell.trainer()->editing());
        QCOMPARE(profiles.load()->name, "E");
        shell.dispatch(Action::NextPage);
        QCOMPARE(shell.page(), 3);
        QCOMPARE(shell.focusIndex(), 0);
    }
};
QTEST_GUILESS_MAIN(InteractionTests)
#include "InteractionTests.moc"
