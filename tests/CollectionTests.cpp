#include "core/repository/CollectionRepository.h"
#include "core/storage/LocalStateStore.h"
#include "core/storage/SqliteLibrary.h"
#include "features/library/LibraryManagementController.h"
#include "features/worlds/WorldsController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <algorithm>

using namespace trainer;
class CollectionTests final : public QObject {
    Q_OBJECT
private slots:
    void englishCurationPreservesGamesAndRejectsServiceAndLanguageVariants() {
        const auto all=collectionCatalogue();QSet<QString> ids;
        for(const auto& a:all)ids.insert(a.catalogueId);
        for(const auto& id:{"snap-n64","new-snap-switch","pokken-wiiu","smash-ultimate-switch","ranger-nds","dungeon-explorers-sky-nds"}) QVERIFY(ids.contains(id));
        for(const auto& id:{"green-gb","go-android","box-gc","red-gb-vc-n3ds","studio-red-pc"}) QVERIFY(!ids.contains(id));
        Adventure a;a.title="Pokemon Emerald";a.platformId="gba";a.catalogueId="emerald-gba";
        QVERIFY(!collectionExclusion(a,"Pokemon Emerald (Japan).gba").isEmpty());
        QVERIFY(collectionExclusion(a,"Pokemon Emerald (Europe) (En,Fr,De).gba").isEmpty());
        QVERIFY(!collectionExclusion(a,"Pokemon Emerald (Europe) (Fr,De).gba").isEmpty());
        QVERIFY(!collectionExclusion(a,"Pokemon Emerald (J).gba").isEmpty());
        a.kind=AdventureKind::RomHack;a.catalogueId="";a.title="Pokemon Vega";a.variant="English translation";
        QVERIFY(collectionExclusion(a,"Pokemon Vega (Japan).gba").isEmpty());
        a.variant="Portuguese translation";QVERIFY(!collectionExclusion(a).isEmpty());
        Adventure b=a;b.title="Pokemon Vega Minus";QVERIFY(collectionIdentity(a)!=collectionIdentity(b));
        a.domain="multiverse";QVERIFY(collectionExclusion(a).isEmpty());
    }

    void chronologyUsesEditionAndRuntimePlatformNotAvailabilityOrTitle() {
        const auto reference = collectionCatalogue();
        auto edition = [&](const QString& id) {
            return *std::find_if(reference.cbegin(), reference.cend(), [&](const auto& a) { return a.catalogueId == id; });
        };
        auto hack = edition("ruby-gba"); hack.id = "hack"; hack.kind = AdventureKind::RomHack;
        hack.title = "1990 - A very early sounding hack"; hack.collectionOnly = false;
        auto unknown = hack; unknown.id = "unknown"; unknown.platformId = "future-system";
        auto mismatched = edition("red-gb"); mismatched.id = "mismatch"; mismatched.platformId = "gba";
        auto ruby = edition("ruby-gba"); ruby.collectionOnly = false;
        QList<Adventure> entries{unknown, edition("sword-switch"), hack, edition("x-n3ds"),
            edition("diamond-nds"), edition("emerald-gba"), edition("leafgreen-gba"),
            edition("firered-gba"), ruby, edition("gold-gbc"), edition("stadium-n64"), edition("red-gb"), mismatched};
        const auto platforms = collectionPlatforms(); const auto chronology = collectionChronology();
        sortWorldAdventures(entries, platforms, chronology);
        QStringList ids; for (const auto& a : entries) ids.append(a.id);
        QCOMPARE(ids, QStringList({"catalogue:red-gb", "catalogue:stadium-n64", "catalogue:gold-gbc",
            "catalogue:ruby-gba", "catalogue:firered-gba", "catalogue:leafgreen-gba", "catalogue:emerald-gba",
            "hack", "mismatch", "catalogue:diamond-nds", "catalogue:x-n3ds", "catalogue:sword-switch", "unknown"}));
        auto expanded = platforms;
        expanded.append({"future-system", "Renamed system", "F", "console", 65});
        sortWorldAdventures(entries, expanded, chronology);
        QCOMPARE(entries[9].id, "unknown"); // New metadata inserts between GBA and DS without UI code.
        for (auto& a : entries) a.collectionOnly = !a.collectionOnly;
        const auto expected = entries;
        std::reverse(entries.begin(), entries.end());
        sortWorldAdventures(entries, expanded, chronology);
        for (int i = 0; i < entries.size(); ++i) QCOMPARE(entries[i].id, expected[i].id);
    }
    void chronologyHasDeterministicUnknownAndSameYearFallbacks() {
        auto make = [](QString id, QString title, QString platform, QString catalogue) {
            Adventure a; a.id=id; a.title=title; a.platformId=platform; a.catalogueId=catalogue; return a;
        };
        QList<Adventure> rows{make("z", "same", "new", ""), make("b", "Same", "new", ""),
            make("date", "Z", "known", "dated"), make("order", "A", "known", "ordered"),
            make("undated", "A", "known", "unknown-date"), make("unlisted", "B", "known", "")};
        sortWorldAdventures(rows, {{"known", "Name", "K", "console", 10}},
            {{"dated", "known", 2000, 1}, {"ordered", "known", 2000, 2}, {"unknown-date", "known", {}, 3}});
        QStringList ids; for (const auto& a : rows) ids.append(a.id);
        QCOMPARE(ids, QStringList({"date", "order", "undated", "unlisted", "b", "z"}));
    }
    void chronologicalControllerKeepsIdentityThroughFiltersRefreshAndRestore() {
        QTemporaryDir dir; LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
        CollectionRepository collection(store); UnconfiguredAdventureAdapter adapter;
        WorldsController worlds(collection, adapter); worlds.activate(0);
        const auto rows = worlds.adventures();
        int chosen = -1;
        for (int i = 0; i < rows.size(); ++i)
            if (rows[i].toMap()["id"] == "catalogue:firered-gba") chosen = i;
        QVERIFY(chosen >= 0); worlds.activate(chosen); worlds.dispatch(Action::Back);
        const auto state = worlds.navigationState();
        worlds.refresh(); QCOMPARE(worlds.detail()["id"].toString(), "catalogue:firered-gba");
        WorldsController restored(collection, adapter); restored.restoreNavigation(state);
        QCOMPARE(restored.detail()["id"].toString(), "catalogue:firered-gba");
        restored.applySearch("fire"); QCOMPARE(restored.detail()["id"].toString(), "catalogue:firered-gba");
        restored.dispatch(Action::ToggleContinue); QVERIFY(restored.adventures().isEmpty()); // Linked
        restored.dispatch(Action::ToggleContinue); // Missing: preserves relative chronology.
        auto filtered = restored.adventures(); int previous = -1;
        for (const auto& item : filtered) {
            const auto id = item.toMap()["id"].toString(); int position = -1;
            for (int i = 0; i < rows.size(); ++i) if (rows[i].toMap()["id"] == id) position = i;
            QVERIFY(position > previous); previous = position;
        }
        QVERIFY(store.adventures().isEmpty()); // Read-only projection, no catalogue migration into personal data.
    }
    void versionTwoMigrationPreservesExistingIdentityAndConfiguration() {
        QTemporaryDir dir;
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", "collection-v2"); db.setDatabaseName(dir.filePath("traineros.sqlite3")); QVERIFY(db.open());
            QSqlQuery q(db);
            QVERIFY(q.exec("CREATE TABLE trainer_profile(slot INTEGER PRIMARY KEY,id TEXT,name TEXT,emblem TEXT,favorite TEXT,created_at TEXT)"));
            QVERIFY(q.exec("INSERT INTO trainer_profile VALUES(1,'identity','ERI','leaf','eevee','2026-09-11T00:00:00.000Z')"));
            QVERIFY(q.exec("CREATE TABLE pokedex_favorites(entry_id TEXT PRIMARY KEY)")); QVERIFY(q.exec("INSERT INTO pokedex_favorites VALUES('eevee')"));
            QVERIFY(q.exec("CREATE TABLE shell_state(scope TEXT PRIMARY KEY,payload BLOB)"));
            QVERIFY(migrateLibrary(db).isEmpty());
            QVERIFY(q.exec("INSERT INTO adventures VALUES('owned','hoenn','Personal title',0,'Keep notes','/absent/card/game.gba','retroarch','{\"core\":\"mgba\",\"future\":true}',7)"));
            QVERIFY(q.exec("INSERT INTO adventure_worlds VALUES('owned','kanto')"));
            QVERIFY(q.exec("PRAGMA user_version=2"));
        }
        QSqlDatabase::removeDatabase("collection-v2");
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
        QCOMPARE(store.load()->id, "identity"); QCOMPARE(store.load()->name, "ERI"); QVERIFY(store.progress("eevee").favorite);
        const auto record = store.registration("owned"); QVERIFY(record);
        QCOMPARE(record->revision, 7); QCOMPARE(record->adventure.title, "Personal title");
        QCOMPARE(record->contentPath, "/absent/card/game.gba"); QVERIFY(record->integrationConfig["future"].toBool());
        QCOMPARE(record->adventure.additionalWorldIds, QStringList{"kanto"}); QVERIFY(record->adventure.catalogueId.isEmpty());
        QVERIFY(record->adventure.platformId.isEmpty()); QVERIFY(!record->adventure.collectionOnly);
    }
    void referenceIntegrityAndAvailability() {
        QSet<QString> worlds, platforms, identities;
        for (const auto& w : collectionWorlds()) { QVERIFY(!worlds.contains(w.id)); worlds.insert(w.id); }
        for (const auto& p : collectionPlatforms()) { QVERIFY(!platforms.contains(p.id)); platforms.insert(p.id); }
        const auto catalogue = collectionCatalogue(); QCOMPARE(catalogue.size(), 92);
        for (const auto& a : catalogue) {
            QVERIFY2(!identities.contains(a.catalogueId), qPrintable(a.title)); identities.insert(a.catalogueId);
            QVERIFY(worlds.contains(a.worldId)); QVERIFY(platforms.contains(a.platformId));
            for (const auto& w : a.additionalWorldIds) QVERIFY(worlds.contains(w));
            QVERIFY(a.collectionOnly); QVERIFY(!a.status); QVERIFY(!a.badges); QVERIFY(!a.caught);
            QVERIFY(a.title.size() <= 96);
        }
        QVERIFY(identities.contains("red-gb")); QVERIFY(identities.contains("colosseum-gc"));
        QVERIFY(identities.contains("conquest-nds")); QVERIFY(identities.contains("friends-switch"));
        QVERIFY(!identities.contains("rumble-rush")); // Closed service, not a local checklist target.
    }
    void attachReopenVariantsAndNeverRewriteExternalFile() {
        QTemporaryDir dir; const auto path = dir.filePath("fixture.gba");
        { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("Original non-game fixture"); }
        const auto all = collectionCatalogue();
        auto found = std::find_if(all.begin(), all.end(), [](const auto& a) { return a.catalogueId == "ruby-gba"; }); QVERIFY(found != all.end());
        AdventureRegistration record; record.adventure = *found; record.adventure.collectionOnly = false; record.contentPath = path;
        record.adventure.variant = "European revision";
        {
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready()); CollectionRepository collection(store);
            QCOMPARE(collection.adventures().size(), all.size()); QVERIFY(store.adventures().isEmpty());
            UnconfiguredAdventureAdapter adapter; WorldsController worlds(collection, adapter);
            worlds.activate(2); worlds.activate(0);
            QCOMPARE(worlds.actions().first().toMap()["id"].toString(), "setup");
            QVERIFY(worlds.actions().first().toMap()["enabled"].toBool());
            QSignalSpy setup(&worlds, &WorldsController::setupRequested); worlds.dispatch(Action::Confirm); QCOMPARE(setup.size(), 1);
            bool done = false;
            collection.saveAdventureAsync(record, this, [&](auto r) { QVERIFY2(r.success, qPrintable(r.error)); done = true; }); QTRY_VERIFY(done);
            QCOMPARE(collection.adventures().size(), all.size()); QCOMPARE(store.adventures().size(), 1);
            auto duplicate = *store.registration(record.adventure.id); duplicate.adventure.id = "second-revision"; duplicate.revision = 0;
            duplicate.adventure.title = "My renamed Ruby"; duplicate.adventure.variant = "Another revision"; done = false;
            collection.saveAdventureAsync(duplicate, this, [&](auto r) { QVERIFY(r.success); done = true; }); QTRY_VERIFY(done);
            QCOMPARE(collection.adventures().size(), all.size());
        }
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready()); CollectionRepository collection(store);
        const auto saved = store.registration(record.adventure.id); QVERIFY(saved);
        QCOMPARE(saved->adventure.catalogueId, "ruby-gba"); QCOMPARE(saved->adventure.platformId, "gba");
        QCOMPARE(saved->adventure.variant, "European revision");
        QVERIFY(QFile::rename(path, path + ".moved"));
        QCOMPARE(collection.adventures().size(), all.size()); // Missing removable content never deletes ownership metadata.
        auto wrong = *saved; wrong.adventure.platformId = "n64";
        bool failed = false; collection.saveAdventureAsync(wrong, this, [&](auto r) { failed = !r.success; }); QVERIFY(failed);
        QFile external(path + ".moved"); QVERIFY(external.open(QIODevice::ReadOnly)); QCOMPARE(external.readAll(), QByteArray("Original non-game fixture"));
    }
    void newReferenceWorldsAndControllerCancel() {
        QTemporaryDir dir; const auto path = dir.filePath("fixture.nds"); { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("fixture"); }
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready()); CollectionRepository collection(store);
        LibraryManagementController manager(collection, nullptr);
        manager.beginEdit("catalogue:conquest-nds"); QCOMPARE(manager.route(), "edit"); QCOMPARE(manager.focusIndex(), 4);
        QCOMPARE(manager.fields().first().toMap()["value"].toString(), "Pokémon Conquest");
        QSignalSpy closed(&manager, &LibraryManagementController::closeRequested); manager.dispatch(Action::Back); QCOMPARE(closed.size(), 1);
        QVERIFY(store.adventures().isEmpty());
        auto all = collectionCatalogue(); auto a = *std::find_if(all.begin(), all.end(), [](const auto& e) { return e.catalogueId == "conquest-nds"; });
        a.collectionOnly = false; a.additionalWorldIds = {"lental"};
        AdventureRegistration record; record.adventure = a; record.contentPath = path; bool done = false;
        collection.saveAdventureAsync(record, this, [&](auto r) { QVERIFY2(r.success, qPrintable(r.error)); done = true; }); QTRY_VERIFY(done);
        QCOMPARE(store.adventures().size(), 1); QCOMPARE(store.worlds().size(), 11);
    }
};
QTEST_GUILESS_MAIN(CollectionTests)
#include "CollectionTests.moc"
