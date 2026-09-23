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
