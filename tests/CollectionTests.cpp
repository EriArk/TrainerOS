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
        const auto catalogue = collectionCatalogue(); QVERIFY(catalogue.size() > 100);
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
            duplicate.adventure.variant = "Another revision"; done = false;
            collection.saveAdventureAsync(duplicate, this, [&](auto r) { QVERIFY(r.success); done = true; }); QTRY_VERIFY(done);
            QCOMPARE(collection.adventures().size(), all.size() + 1);
        }
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready()); CollectionRepository collection(store);
        const auto saved = store.registration(record.adventure.id); QVERIFY(saved);
        QCOMPARE(saved->adventure.catalogueId, "ruby-gba"); QCOMPARE(saved->adventure.platformId, "gba");
        QCOMPARE(saved->adventure.variant, "European revision");
        QVERIFY(QFile::rename(path, path + ".moved"));
        QCOMPARE(collection.adventures().size(), all.size() + 1); // Missing removable content never deletes ownership metadata.
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
