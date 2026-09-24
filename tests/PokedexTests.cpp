#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
#include <algorithm>

using namespace trainer;
namespace {
QStringList ids(const PokedexController& dex) {
    QStringList result;
    for (const auto& e : dex.entries()) result.append(e.toMap()["id"].toString());
    return result;
}
void filter(PokedexController& dex, int rail, const QString& id) {
    dex.activateControl("rail", rail);
    const auto choices = dex.choices();
    for (int i = 0; i < choices.size(); ++i) if (choices[i].toMap()["id"] == id) { dex.activate(i); return; }
    QFAIL("Expected filter option missing");
}
int keyIndex(TextEntryController& keyboard, const QString& id) {
    const auto keys = keyboard.keys();
    for (int i = 0; i < keys.size(); ++i) if (keys[i].toMap()["id"] == id) return i;
    return -1;
}
class MutableReference : public PokedexReferenceProvider {
public:
    MockPokedexRepository sample;
    PokedexCatalog catalog = sample.load();
    PokedexCatalog load() override { return catalog; }
};
}
class PokedexTests : public QObject {
    Q_OBJECT
private slots:
    void saveProgressIsPrimaryWithoutOverwritingTheJournal() {
        MockPokedexRepository repo; PokedexController dex(repo,repo);
        const auto manual = repo.progress("mudkip"); QVERIFY(manual.caught.value_or(false));
        GameProgress p; p.availability=ProgressAvailability::Available; p.contextRevision="owner-one";
        p.contentRevision="emerald"; p.saveRevision="first";
        p.pokedex=SavePokedex{386,{1,4},{1},{}};
        dex.setSaveProgress("game","Emerald","game",p);
        dex.applySearch("mudkip"); QCOMPARE(dex.detail()["status"],"Not seen");
        QCOMPARE(dex.detail()["journalStatus"],"Caught");
        QCOMPARE(repo.progress("mudkip").caught,manual.caught);
        dex.applySearch(""); filter(dex,3,"caught"); QCOMPARE(ids(dex),(QStringList{"bulbasaur"}));
        filter(dex,3,"seen"); QCOMPARE(ids(dex),(QStringList{"bulbasaur","charmander"}));
        filter(dex,3,""); dex.applySearch("ralts");
        p.saveRevision="second";p.pokedex->seen.insert(280);p.pokedex->caught.insert(280);
        dex.setSaveProgress("game","Emerald","game",p); QCOMPARE(dex.detail()["status"],"Caught");
        p.saveRevision="rollback";p.pokedex->seen.remove(280);p.pokedex->caught.remove(280);
        dex.setSaveProgress("game","Emerald","game",p); QCOMPARE(dex.detail()["status"],"Not seen");
        QCOMPARE(repo.progress("mudkip").caught,manual.caught);
        dex.editJournal(); QVERIFY(dex.journal()->isOpen()); dex.cancelTransient();
    }
    void failedSaveRefreshRetainsOnlySameIdentityAndNeverInventsNewSpecies() {
        MockPokedexRepository repo; MutableReference reference;
        reference.catalog.entries.append({"rowlet",722,"Rowlet",{"Grass","Flying"},{"alola"}});
        PokedexController dex(reference,repo);
        GameProgress p; p.availability=ProgressAvailability::Available; p.contextRevision="owner-one";
        p.contentRevision="emerald";p.saveRevision="first";p.pokedex=SavePokedex{386,{1},{1},{}};
        dex.setSaveProgress("game","Emerald","game",p);dex.applySearch("bulbasaur");
        GameProgress checking;checking.availability=ProgressAvailability::Checking;
        dex.setSaveProgress("game","Emerald","game",checking);QCOMPARE(dex.detail()["status"],"Unknown");
        auto failure=p;failure.availability=ProgressAvailability::Unreadable;failure.pokedex.reset();failure.saveRevision="bad";
        dex.setSaveProgress("game","Emerald","game",failure);QCOMPARE(dex.detail()["status"],"Caught");
        QVERIFY(dex.saveCaption().contains("Last verified"));
        auto damaged=p;damaged.saveRevision="inconsistent";damaged.pokedex->error="Flags disagree";
        dex.setSaveProgress("game","Emerald","game",damaged);QCOMPARE(dex.detail()["status"],"Caught");
        QVERIFY(dex.saveCaption().contains("Last verified"));
        failure.contextRevision="owner-two";dex.setSaveProgress("game","Emerald","game",failure);
        QCOMPARE(dex.detail()["status"],"Unknown");QVERIFY(!dex.saveCaption().contains("Last verified"));
        dex.setSaveProgress("game","Emerald","game",p);dex.applySearch("rowlet");
        QCOMPARE(dex.detail()["status"],"Not in this game");
        dex.setSaveProgress("other","Other","game",p);dex.applySearch("bulbasaur");
        QCOMPARE(dex.detail()["status"],"Unknown");
        p.pokedex.reset();dex.setSaveProgress("game","FireRed","game",p);QCOMPARE(dex.detail()["status"],"Unknown");
    }
    void legacyDetailRestoresTheUnifiedBrowserWithoutChangingProgress() {
        MockPokedexRepository repo; PokedexController dex(repo,repo);
        const auto before=repo.progress("mudkip");
        dex.restoreNavigation({{"entry","mudkip"},{"zone","detail"},{"form",""}});
        QCOMPARE(dex.zone(),"list"); QCOMPARE(dex.detail()["id"],"mudkip");
        QCOMPARE(dex.detail()["stats"].toList().size(),6);
        QCOMPARE(repo.progress("mudkip").favorite,before.favorite);
        dex.dispatch(Action::Confirm);
        QCOMPARE(dex.zone(),"list"); QCOMPARE(repo.progress("mudkip").favorite,!before.favorite);
        dex.dispatch(Action::Back); QCOMPARE(dex.zone(),"rail");
        dex.dispatch(Action::Back); QCOMPARE(dex.zone(),"list");
        dex.editJournal(); QVERIFY(dex.journal()->isOpen()); dex.cancelTransient();
        QCOMPARE(dex.detail()["id"],"mudkip");
    }
    void combinedFiltersAndSearch() {
        MockPokedexRepository repo;
        PokedexController dex(repo, repo);
        QCOMPARE(dex.entries().size(), 14);
        filter(dex, 1, "hoenn"); filter(dex, 2, "Water"); filter(dex, 3, "caught");
        QCOMPARE(ids(dex), QStringList{"mudkip"});
        dex.applySearch("  mUd  ");
        QCOMPARE(ids(dex), QStringList{"mudkip"});
        dex.applySearch("0258");
        QCOMPARE(ids(dex), QStringList{"mudkip"});
        dex.applySearch("#258");
        QCOMPARE(ids(dex), QStringList{"mudkip"});
        dex.applySearch("25"); // Exact numeric lookup, not a prefix match on 258.
        QVERIFY(dex.entries().isEmpty());
        dex.dispatch(Action::Down); QCOMPARE(dex.zone(), "recovery");
        dex.dispatch(Action::Confirm);
        QCOMPARE(dex.entries().size(), 14);
        QVERIFY(dex.query().isEmpty());
        dex.applySearch("PIKA"); QCOMPARE(ids(dex), QStringList{"pikachu"});
    }
    void progressIsNotInferredFromMissingData() {
        MockPokedexRepository repo;
        PokedexController dex(repo, repo);
        filter(dex, 3, "unseen");
        QCOMPARE(ids(dex), QStringList{"squirtle"});
        QCOMPARE(dex.detail()["seen"].toString(), "No");
        filter(dex, 3, "unknown");
        QVERIFY(ids(dex).contains("jigglypuff"));
        QVERIFY(!ids(dex).contains("squirtle"));
        QCOMPARE(dex.detail()["caught"].toString(), "Not recorded");
        filter(dex, 3, "uncaught");
        QCOMPARE(ids(dex), (QStringList{"charmander", "squirtle", "eevee", "ralts"}));
        filter(dex, 3, "seen");
        QVERIFY(ids(dex).contains("pikachu"));
        QVERIFY(ids(dex).contains("charmander"));
        QVERIFY(!ids(dex).contains("squirtle"));
    }
    void stableSelectionSortingAndPickerCancel() {
        MockPokedexRepository repo;
        PokedexController dex(repo, repo);
        for (int i = 0; i < 12; ++i) dex.dispatch(Action::Down);
        QCOMPARE(dex.detail()["id"].toString(), "mudkip");
        dex.activateControl("rail", 4); dex.dispatch(Action::Right); dex.dispatch(Action::Back);
        QCOMPARE(dex.zone(), "rail");
        QCOMPARE(dex.rail()[4].toMap()["value"].toString(), "Number ↑");
        filter(dex, 4, "name");
        QCOMPARE(dex.detail()["id"].toString(), "mudkip");
        auto names = QStringList{};
        for (const auto& e : dex.entries()) names.append(e.toMap()["name"].toString());
        QVERIFY(std::is_sorted(names.begin(), names.end()));
        dex.dispatch(Action::Down); dex.dispatch(Action::Confirm);
        QCOMPARE(dex.zone(), "list");
        dex.dispatch(Action::Back);
        QCOMPARE(dex.detail()["id"].toString(), "mudkip");
        dex.activateControl("rail", 2);
        const int cancel = dex.choices().size() - 1;
        dex.activate(cancel);
        QCOMPARE(dex.rail()[2].toMap()["value"].toString(), "All types");
    }
    void favoriteWriteRetryAndFilteredRemoval() {
        MockPokedexRepository repo;
        PokedexController dex(repo, repo);
        QSignalSpy messages(&dex, &PokedexController::messageRequested);
        const auto before = repo.progress("bulbasaur");
        repo.failNextWrite(); dex.activate(0);
        QVERIFY(!repo.progress("bulbasaur").favorite);
        QCOMPARE(dex.zone(), "list");
        QCOMPARE(messages.size(), 1);
        dex.activate(0);
        QVERIFY(repo.progress("bulbasaur").favorite);
        QCOMPARE(repo.progress("bulbasaur").seen, before.seen);
        QCOMPARE(repo.progress("bulbasaur").caught, before.caught);
        dex.dispatch(Action::Back);
        filter(dex, 3, "favorite");
        dex.applySearch("Bulbasaur"); dex.dispatch(Action::Down);
        dex.activate(0); // Removing the sole matching favorite must leave a usable empty state.
        QVERIFY(dex.entries().isEmpty());
        QCOMPARE(dex.zone(), "recovery");
        dex.dispatch(Action::Confirm);
        QCOMPARE(dex.entries().size(), 14);
    }
    void emptyFailedAndRefreshedReference() {
        MockPokedexRepository progress;
        MutableReference reference;
        reference.catalog = {false, {}, {}, "Unavailable"};
        PokedexController dex(reference, progress);
        QCOMPARE(dex.zone(), "recovery");
        QCOMPARE(dex.recoveryLabel(), "Retry field guide");
        reference.catalog = progress.load(); dex.dispatch(Action::Confirm);
        QCOMPARE(dex.zone(), "list"); QCOMPARE(dex.entries().size(), 14);
        dex.dispatch(Action::Down); dex.dispatch(Action::Confirm);
        std::reverse(reference.catalog.entries.begin(), reference.catalog.entries.end());
        dex.refresh();
        QCOMPARE(dex.detail()["id"].toString(), "charmander");
        reference.catalog.success = false;
        QSignalSpy messages(&dex, &PokedexController::messageRequested);
        dex.refresh();
        QCOMPARE(dex.entries().size(), 14);
        QCOMPARE(messages.size(), 1);
        reference.catalog = {};
        dex.refresh();
        QCOMPARE(dex.zone(), "recovery"); QVERIFY(dex.entries().isEmpty());
    }
    void shellTextConsumersAndGlobalLayers() {
        MockLibraryRepository library;
        MockTrainerRepository profiles;
        MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;
        MockPokedexRepository dex;
        MockHallOfFameRepository shellArchive;
        MockAchievementProvider shellAchievements;
        ShellController shell(library, profiles, adapter, platform, dex, dex, shellArchive, shellAchievements);
        shell.goToPage(2); shell.dispatch(Action::Up); shell.dispatch(Action::Confirm);
        auto* keyboard = shell.keyboard();
        keyboard->activate(keyIndex(*keyboard, "2")); keyboard->activate(keyIndex(*keyboard, "5"));
        shell.dispatch(Action::SystemMenu); shell.dispatch(Action::Back);
        QCOMPARE(keyboard->text(), "25");
        keyboard->activate(keyIndex(*keyboard, "apply"));
        QCOMPARE(ids(*shell.pokedex()), QStringList{"pikachu"});
        QVERIFY(!profiles.load().has_value());
        shell.dispatch(Action::Confirm); keyboard->activate(keyIndex(*keyboard, "A"));
        shell.dispatch(Action::Back);
        QCOMPARE(shell.pokedex()->query(), "25");
        shell.dispatch(Action::Confirm); keyboard->activate(keyIndex(*keyboard, "B"));
        shell.dispatch(Action::NextPage);
        QVERIFY(!keyboard->isOpen()); QCOMPARE(shell.pokedex()->query(), "25");
        shell.dispatch(Action::Confirm); shell.dispatch(Action::Confirm);
        keyboard->activate(keyIndex(*keyboard, "E")); keyboard->activate(keyIndex(*keyboard, "apply"));
        QCOMPARE(shell.trainer()->draftName(), "E");
        QCOMPARE(shell.pokedex()->query(), "25");
        shell.dispatch(Action::PreviousPage);
        shell.pokedex()->activateControl("rail", 2);
        shell.dispatch(Action::Right);
        shell.dispatch(Action::SystemMenu); shell.dispatch(Action::Back);
        QCOMPARE(shell.pokedex()->zone(), "picker");
        shell.dispatch(Action::NextPage); shell.dispatch(Action::PreviousPage);
        QCOMPARE(shell.pokedex()->zone(), "rail");
        QCOMPARE(shell.pokedex()->rail()[2].toMap()["value"].toString(), "All types");
    }
};
QTEST_GUILESS_MAIN(PokedexTests)
#include "PokedexTests.moc"
