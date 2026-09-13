#include "core/storage/SessionState.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QUuid>

using namespace trainer;
namespace {
void fixtureFile(const QString& path) { QFile f(path); if (f.open(QIODevice::WriteOnly)) f.write("Original test data. Not a game or save.\n"); }
AdventureRegistration candidate(const QString& path) {
    AdventureRegistration value;
    value.adventure.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    value.adventure.worldId = "hoenn"; value.adventure.title = "A new journey";
    value.adventure.adapterId = "unconfigured"; value.adventure.kind = AdventureKind::RomHack;
    value.adventure.additionalWorldIds = {"kanto", "johto"}; value.contentPath = path;
    return value;
}
class Connection {
public:
    QSqlDatabase db;
    explicit Connection(const QString& directory) {
        db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
        db.setDatabaseName(directory + "/traineros.sqlite3"); db.open();
    }
    ~Connection() { const auto name = db.connectionName(); db.close(); db = {}; QSqlDatabase::removeDatabase(name); }
};
}
class LibraryTests final : public QObject {
    Q_OBJECT
private slots:
    void libraryIdentityRelationshipsAndReopening() {
        QTemporaryDir dir; const auto file = dir.path() + "/original fixture.bin"; fixtureFile(file);
        auto value = candidate(file); value.integrationConfig = {{"future-adapter-field", "literal `name` $(value)"}};
        {
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
            QCOMPARE(store.worlds().size(), 9); QVERIFY(store.adventures().isEmpty()); QVERIFY(store.resumePoints().isEmpty());
            QVERIFY(store.home().activeAdventureId.isEmpty()); QVERIFY(!store.home().badges.has_value());
            bool done = false;
            store.saveAdventureAsync(value, this, [&](auto result) { QVERIFY2(result.success, qPrintable(result.error)); QCOMPARE(result.revision, 1); done = true; });
            QVERIFY(store.adventures().isEmpty()); QTRY_VERIFY(done);
            auto saved = store.registration(value.adventure.id); QVERIFY(saved); QCOMPARE(saved->revision, 1);
            QVERIFY(!saved->adventure.status); QVERIFY(!saved->adventure.badges); QVERIFY(!saved->adventure.caught);
            UnconfiguredAdventureAdapter adapter;
            WorldsController worlds(store, adapter); worlds.activate(0); // Kanto contains a secondary relationship.
            QCOMPARE(worlds.adventures().size(), 1); worlds.activate(0); QCOMPARE(worlds.detail()["id"].toString(), value.adventure.id);
            QVERIFY(!worlds.actions().first().toMap()["enabled"].toBool()); // Not a falsely playable sample.
            const auto state = worlds.navigationState(); WorldsController restored(store, adapter); restored.restoreNavigation(state);
            QCOMPARE(restored.region()["id"].toString(), "kanto"); QCOMPARE(restored.detail()["id"].toString(), value.adventure.id);
            saved->adventure.title = "Renamed journey"; saved->adventure.additionalWorldIds = {"kanto", "kanto", "hoenn"};
            done = false; store.saveAdventureAsync(*saved, this, [&](auto result) { QVERIFY(result.success); done = true; }); QTRY_VERIFY(done);
            QCOMPARE(store.registration(value.adventure.id)->revision, 2);
        }
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
        QCOMPARE(store.adventures().size(), 1); const auto saved = *store.registration(value.adventure.id);
        QCOMPARE(saved.adventure.title, "Renamed journey"); QCOMPARE(saved.contentPath, file);
        QCOMPARE(saved.integrationConfig, value.integrationConfig); QCOMPARE(saved.adventure.additionalWorldIds, QStringList{"kanto"});
        QFile original(file); QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), QByteArray("Original test data. Not a game or save.\n"));
    }
    void staleEditMissingFileAndWorldRollback() {
        QTemporaryDir dir; const auto file = dir.path() + "/fixture.bin"; fixtureFile(file);
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
        auto value = candidate(file); bool done = false;
        store.saveAdventureAsync(value, this, [&](auto r) { QVERIFY(r.success); done = true; }); QTRY_VERIFY(done);
        value = *store.registration(value.adventure.id);
        auto stale = value; value.adventure.title = "Current title";
        done = false; store.saveAdventureAsync(value, this, [&](auto r) { QVERIFY(r.success); done = true; }); QTRY_VERIFY(done);
        stale.newWorld = World{"custom-island", "New Island", {}}; stale.adventure.worldId = "custom-island";
        bool failed = false; store.saveAdventureAsync(stale, this, [&](auto r) { failed = !r.success; }); QTRY_VERIFY(failed);
        QCOMPARE(store.worlds().size(), 9); QCOMPARE(store.registration(value.adventure.id)->adventure.title, "Current title");
        value = *store.registration(value.adventure.id); value.adventure.additionalWorldIds = {"missing-world"};
        failed = false; store.saveAdventureAsync(value, this, [&](auto r) { failed = !r.success; }); QTRY_VERIFY(failed);
        QCOMPARE(store.registration(value.adventure.id)->adventure.additionalWorldIds, QStringList({"kanto", "johto"}));
        value = *store.registration(value.adventure.id);
        QVERIFY(QFile::rename(file, file + ".moved"));
        failed = false; store.saveAdventureAsync(value, this, [&](auto r) { failed = !r.success; }); QTRY_VERIFY(failed);
        value.contentPath = file + ".moved"; value.newWorld = World{"custom-island", "New Island", {}}; value.adventure.worldId = "custom-island";
        done = false; store.saveAdventureAsync(value, this, [&](auto r) { QVERIFY2(r.success, qPrintable(r.error)); done = true; }); QTRY_VERIFY(done);
        QCOMPARE(store.worlds().size(), 10); QCOMPARE(store.registration(value.adventure.id)->adventure.worldId, "custom-island");
    }
    void migrationFromVersionOnePreservesPersonalData() {
        QTemporaryDir dir;
        {
            Connection connection(dir.path()); QSqlQuery q(connection.db);
            QVERIFY(q.exec("CREATE TABLE trainer_profile(slot INTEGER PRIMARY KEY,id TEXT,name TEXT,emblem TEXT,favorite TEXT,created_at TEXT)"));
            QVERIFY(q.exec("INSERT INTO trainer_profile VALUES(1,'trainer-id','ERI','leaf','eevee','2026-09-06T12:00:00.000Z')"));
            QVERIFY(q.exec("CREATE TABLE pokedex_favorites(entry_id TEXT PRIMARY KEY)")); QVERIFY(q.exec("INSERT INTO pokedex_favorites VALUES('eevee')"));
            QVERIFY(q.exec("CREATE TABLE shell_state(scope TEXT PRIMARY KEY,payload BLOB)"));
            QVERIFY(q.exec("INSERT INTO shell_state VALUES('prototype-library-v1','{\"version\":1,\"page\":\"worlds\"}')"));
            QVERIFY(q.exec("PRAGMA user_version=1"));
        }
        {
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
            QCOMPARE(store.load()->id, "trainer-id"); QCOMPARE(store.load()->name, "ERI"); QVERIFY(store.progress("eevee").favorite);
            QCOMPARE(store.worlds().size(), 9); QVERIFY(store.adventures().isEmpty()); QVERIFY(store.navigation().isEmpty());
        }
        Connection connection(dir.path()); QSqlQuery q(connection.db);
        QVERIFY(q.exec("PRAGMA user_version")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 6);
        QVERIFY(q.exec("SELECT payload FROM shell_state WHERE scope='prototype-library-v1'")); QVERIFY(q.next()); QVERIFY(!q.value(0).toString().isEmpty());
    }
    void filePagingCancellationAndUnavailableDirectory() {
        QTemporaryDir dir; QVERIFY(QDir().mkpath(dir.path() + "/folder"));
        for (int i = 0; i < 90; ++i) fixtureFile(dir.path() + QString("/entry-%1.bin").arg(i, 3, 10, QChar('0')));
        LocalFileCatalog catalog; FilePickerController picker(&catalog);
        picker.begin(dir.path()); QTRY_VERIFY(!picker.busy()); QCOMPARE(picker.rows().size(), 80);
        QVERIFY(picker.rows().first().toMap()["title"].toString() == "folder");
        for (int i = 0; i < 80; ++i) picker.dispatch(Action::Down);
        QCOMPARE(picker.zone(), QString("actions"));
        picker.dispatch(Action::Up);
        QCOMPARE(picker.zone(), QString("list")); QCOMPARE(picker.rowIndex(), 79);
        picker.activate(3, "actions"); QTRY_VERIFY(!picker.busy()); QCOMPARE(picker.rows().size(), 11);
        picker.activate(2, "actions"); picker.cancel(); QTest::qWait(100); QVERIFY(!picker.isOpen());
        picker.begin(dir.path() + "/missing"); QTRY_VERIFY(!picker.busy()); QVERIFY(!picker.error().isEmpty()); QCOMPARE(picker.zone(), "actions");
        picker.dispatch(Action::Back); QVERIFY(!picker.isOpen());
    }
    void settingsAreCommittedAndRetryable() {
        QTemporaryDir dir;
        {
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
            SettingsController settings; settings.setRepository(&store);
            settings.activate(0); QVERIFY(settings.saving()); QCOMPARE(settings.theme(), "turquoise");
            QTRY_VERIFY(!settings.saving()); QCOMPARE(settings.theme(), "red");
            Connection connection(dir.path()); QSqlQuery q(connection.db); QVERIFY(q.exec("BEGIN IMMEDIATE"));
            settings.activate(1); QTRY_VERIFY(!settings.saving()); QVERIFY(!settings.error().isEmpty()); QVERIFY(!settings.reducedMotion());
            QVERIFY(q.exec("ROLLBACK")); settings.activate(1); QTRY_VERIFY(!settings.saving()); QVERIFY(settings.reducedMotion());
        }
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
        QCOMPARE(store.preferences().theme, "red"); QVERIFY(store.preferences().reducedMotion);
    }
};
QTEST_GUILESS_MAIN(LibraryTests)
#include "LibraryTests.moc"
