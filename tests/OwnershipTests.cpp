#include "LegacyStoreFixture.h"
#include "core/storage/LocalStateStore.h"
#include "core/storage/SqliteOwnership.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSqlError>
#include <QUuid>

using namespace trainer;
namespace {
struct Connection {
    QSqlDatabase db;
    explicit Connection(const QString& path) {
        db=QSqlDatabase::addDatabase("QSQLITE",QUuid::createUuid().toString());
        db.setDatabaseName(path+"/traineros.sqlite3"); db.open();
        QSqlQuery q(db); q.exec("PRAGMA foreign_keys=ON");
    }
    ~Connection() { const auto name=db.connectionName();db.close();db={};QSqlDatabase::removeDatabase(name); }
};
bool seed(QSqlDatabase& db) {
    if(!createLegacyStore(db,7))return false;
    QSqlQuery q(db);
    for(const auto& sql:QStringList{
        "INSERT INTO trainer_profile VALUES(1,'legacy-owner','Old Trainer','compass','eevee','2026-01-01T00:00:00.000Z')",
        "INSERT INTO pokedex_favorites VALUES('eevee')",
        "INSERT INTO pokedex_records VALUES('eevee',1,NULL,'An old note',4)",
        "INSERT INTO shell_state VALUES('user-library-v1','{\"version\":1,\"page\":\"pokedex\"}')",
        "INSERT INTO shell_state VALUES('other-library','opaque navigation bytes')",
        "INSERT INTO adventures VALUES('game','kanto','A game',0,'','/not-read-by-migration','unconfigured','{}',1,'gba','','')",
        "INSERT INTO play_sessions VALUES('session','game','2026-01-01T00:00:00.000Z','2026-01-01T00:01:00.000Z',60,'returned')",
        "INSERT INTO hall_of_fame VALUES('memory','game','Historical title','Kanto','',NULL,'[]','An old memory','manual',3)"})
        if(!q.exec(sql))return false;
    return true;
}
}
class OwnershipTests final:public QObject {
    Q_OBJECT
private slots:
    void migrationAndRestartPreserveLegacyData() {
        QTemporaryDir dir;
        {Connection c(dir.path());QVERIFY(seed(c.db));}
        for(int pass=0;pass<2;++pass) {
            LocalStateStore store(dir.path());store.open();QTRY_VERIFY_WITH_TIMEOUT(!store.opening(),5000);
            QVERIFY2(store.ready(),qPrintable(store.error()));QCOMPARE(store.ownerId(),"legacy-owner");
            QCOMPARE(store.load()->name,"Old Trainer");QCOMPARE(store.load()->id,store.ownerId());
            QVERIFY(store.progress("eevee").favorite);QVERIFY(store.progress("eevee").seen.value());
            QVERIFY(!store.progress("eevee").caught);QCOMPARE(store.progress("eevee").notes,"An old note");
            QCOMPARE(store.progress("eevee").revision,4);QCOMPARE(store.navigation()["page"].toString(),"pokedex");
            QCOMPARE(store.adventures().size(),1);QCOMPARE(store.recordedSeconds("game").value(),60);
            QCOMPARE(store.recentSessions().first().id,"session");QCOMPARE(store.loadArchive().entries.first().notes,"An old memory");
        }
        Connection c(dir.path());QSqlQuery q(c.db);
        QVERIFY(q.exec("SELECT payload FROM shell_state WHERE scope='other-library'"));QVERIFY(q.next());
        QCOMPARE(q.value(0).toByteArray(),QByteArray("opaque navigation bytes"));
        QVERIFY(q.exec("SELECT rowid,trainer_id FROM play_sessions"));QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(),1);QCOMPARE(q.value(1).toString(),"legacy-owner");
        QVERIFY(q.exec("PRAGMA user_version"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),10);
        QVERIFY(q.exec("PRAGMA foreign_key_check"));QVERIFY(!q.next());
    }
    void failedMigrationRollsBackAndCanRetry() {
        QTemporaryDir dir;
        {Connection c(dir.path());QVERIFY(seed(c.db));QSqlQuery q(c.db);
            // Fail after several DDL statements, not just at BEGIN.
            QVERIFY(q.exec("CREATE TABLE owned_records(sentinel TEXT)"));}
        {LocalStateStore store(dir.path());store.open();QTRY_VERIFY(!store.opening());QVERIFY(!store.ready());}
        {Connection c(dir.path());QSqlQuery q(c.db);
            QVERIFY(q.exec("PRAGMA user_version"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),7);
            QVERIFY(q.exec("SELECT name FROM sqlite_master WHERE name IN ('trainer_owners','local_owner','owned_favorites')"));QVERIFY(!q.next());
            QVERIFY(q.exec("SELECT entry_id,notes,revision FROM pokedex_records"));QVERIFY(q.next());
            QCOMPARE(q.value(0).toString(),"eevee");QCOMPARE(q.value(1).toString(),"An old note");QCOMPARE(q.value(2).toInt(),4);
            QVERIFY(q.exec("DROP TABLE owned_records"));}
        LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());QCOMPARE(store.ownerId(),"legacy-owner");
    }
    void unnamedOwnerAdoptsFirstProfileAtomically() {
        QTemporaryDir dir;QString unnamed;
        {
            LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
            unnamed=store.ownerId();QVERIFY(!unnamed.isEmpty());QVERIFY(!store.load());
            store.setFavoriteAsync("eevee",true,this,[](auto e){QVERIFY(e.isEmpty());});
            store.saveNavigation({{"version",1},{"page","worlds"}},this,[](auto e){QVERIFY(e.isEmpty());});
            PokedexProgress record;record.seen=true;record.notes="Before registration";
            store.saveRecordAsync("eevee",record,this,[](auto r){QVERIFY(r.success);});QTRY_COMPARE(store.pending(),0);
        }
        LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());QCOMPARE(store.ownerId(),unnamed);
        TrainerProfile profile{"owner'with-quote","New Trainer","compass","eevee",QDateTime::currentDateTimeUtc()};
        {
            Connection c(dir.path());QSqlQuery q(c.db);
            QVERIFY(q.exec("CREATE TRIGGER reject_profile BEFORE INSERT ON trainer_profile BEGIN SELECT RAISE(ABORT,'fixture failure'); END"));
            bool done=false;store.saveAsync(profile,this,[&](auto r){QVERIFY(!r.success);done=true;});QTRY_VERIFY(done);
            QCOMPARE(store.ownerId(),unnamed);QCOMPARE(localOwner(c.db),unnamed);QVERIFY(!store.load());
            QVERIFY(q.exec("DROP TRIGGER reject_profile"));
        }
        bool done=false;store.saveAsync(profile,this,[&](auto r){QVERIFY2(r.success,qPrintable(r.error));done=true;});QTRY_VERIFY(done);
        QCOMPARE(store.ownerId(),profile.id);QVERIFY(store.progress("eevee").favorite);
        QCOMPARE(store.progress("eevee").notes,"Before registration");
        Connection c(dir.path());QCOMPARE(localOwner(c.db),profile.id);QSqlQuery q(c.db);
        QVERIFY(q.exec("SELECT COUNT(*) FROM trainer_owners"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);
        QVERIFY(q.exec("PRAGMA foreign_key_check"));QVERIFY(!q.next());
    }
    void twoOwnerReadsAndWritesAreIsolated() {
        QTemporaryDir dir;
        {Connection c(dir.path());QVERIFY(seed(c.db));}
        {LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());}
        Connection c(dir.path());QSqlQuery q(c.db);QVERIFY(q.exec("INSERT INTO trainer_owners VALUES('second')"));
        QVERIFY(q.exec("INSERT INTO pokedex_favorites VALUES('second','pikachu')"));
        QVERIFY(q.exec("INSERT INTO shell_state VALUES('second','user-library-v1','{\"version\":1,\"page\":\"hall\"}')"));
        auto own=readPokedexJournal(c.db,"legacy-owner");QCOMPARE(own.records.size(),1);
        QVERIFY(readPokedexJournal(c.db,"second").records.isEmpty());
        PokedexJournalSnapshot journal;auto stolen=own.records.value("eevee");stolen.notes="Overwrite";
        QVERIFY(!writePokedexRecord(c.db,"second","eevee",stolen,journal).success);
        stolen.revision=0;QVERIFY(writePokedexRecord(c.db,"second","eevee",stolen,journal).success);
        QCOMPARE(readPokedexJournal(c.db,"legacy-owner").records.value("eevee").notes,"An old note");
        QCOMPARE(readPokedexJournal(c.db,"second").records.value("eevee").notes,"Overwrite");
        QVERIFY(readHallOfFame(c.db,"second").entries.isEmpty());ArchiveResult archive;
        auto memory=readHallOfFame(c.db,"legacy-owner").entries.first();memory.notes="Overwrite";
        QVERIFY(!writeHallOfFame(c.db,"second",memory,archive).success);
        memory.id="second-memory";memory.revision=0;QVERIFY(writeHallOfFame(c.db,"second",memory,archive).success);
        QCOMPARE(readHallOfFame(c.db,"legacy-owner").entries.first().notes,"An old memory");
        PlaySession session{"second-session","game",QDateTime::currentDateTimeUtc(),{},{},PlaySessionOutcome::Running};
        QVERIFY(writePlaySession(c.db,"second",session).isEmpty());
        session.endedAt=QDateTime::currentDateTimeUtc();session.elapsedSeconds=12;session.outcome=PlaySessionOutcome::Returned;
        QVERIFY(!writePlaySession(c.db,"legacy-owner",session).isEmpty());
        QVERIFY(writePlaySession(c.db,"second",session).isEmpty());
        QCOMPARE(readPlayHistory(c.db,"second").totals.value("game"),12);
        QCOMPARE(readPlayHistory(c.db,"legacy-owner").totals.value("game"),60);
        QVERIFY(!q.exec("UPDATE play_sessions SET trainer_id=NULL WHERE id='session'"));
        QVERIFY(!q.exec("UPDATE hall_of_fame SET trainer_id='missing' WHERE id='memory'"));
        LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
        QVERIFY(!store.progress("pikachu").favorite);QCOMPARE(store.progress("eevee").notes,"An old note");
        QCOMPARE(store.navigation()["page"].toString(),"pokedex");QCOMPARE(store.loadArchive().entries.size(),1);
        QCOMPARE(store.recordedSeconds("game").value(),60);QCOMPARE(store.adventures().size(),1);
    }
};
QTEST_GUILESS_MAIN(OwnershipTests)
#include "OwnershipTests.moc"
