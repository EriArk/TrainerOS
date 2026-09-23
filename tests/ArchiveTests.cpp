#include "LegacyStoreFixture.h"
#include "core/storage/LocalStateStore.h"
#include "features/halloffame/ArchiveEditor.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QUuid>
#include <QFile>

using namespace trainer;
namespace {
class Connection {
public:
    QSqlDatabase db;
    explicit Connection(const QString& path) {
        db=QSqlDatabase::addDatabase("QSQLITE",QUuid::createUuid().toString());
        db.setDatabaseName(path+"/traineros.sqlite3");db.open();
    }
    ~Connection() { const auto name=db.connectionName();db.close();db={};QSqlDatabase::removeDatabase(name); }
};
AdventureRegistration adventure(const QString& path) {
    QFile file(path+"/original.bin");if(file.open(QIODevice::WriteOnly))file.write("Original test fixture; not game data.");
    AdventureRegistration value;
    value.adventure.id="owned-adventure";value.adventure.title="A personal journey";value.adventure.worldId="kanto";
    value.adventure.adapterId="unconfigured";value.contentPath=file.fileName();return value;
}
}
class ArchiveTests final : public QObject {
    Q_OBJECT
private slots:
    void migrationPersistenceUnknownsAndRevision() {
        QTemporaryDir dir;
        {
            Connection connection(dir.path()); QVERIFY(createLegacyStore(connection.db,4));
            const auto record=adventure(dir.path());QSqlQuery q(connection.db);
            q.prepare("INSERT INTO adventures(id,world_id,title,kind,description,content_path,adapter_id,config,revision) VALUES('owned-adventure','kanto','A personal journey',0,'',?,'unconfigured','{}',1)");
            q.addBindValue(record.contentPath);QVERIFY(q.exec());
        }
        HallOfFameEntry saved;
        {
            LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
            QVERIFY(store.loadArchive().success);QVERIFY(store.loadArchive().entries.isEmpty());
            HallOfFameEntry entry;entry.id="memory-1";entry.adventureId="owned-adventure";
            entry.adventureTitle="Untrusted title";entry.world="Wrong world";entry.notes="A remembered journey";
            entry.team={{"Buddy",{}},{"Leaf",42}};
            bool done=false;
            store.saveArchiveAsync(entry,this,[&](auto r){QVERIFY2(r.success,qPrintable(r.error));QCOMPARE(r.revision,1);done=true;});
            QVERIFY(store.loadArchive().entries.isEmpty());QTRY_VERIFY(done);
            saved=store.loadArchive().entries.first();QCOMPARE(saved.adventureTitle,"A personal journey");QCOMPARE(saved.world,"Kanto");
            QVERIFY(!saved.completedAt.isValid());QVERIFY(!saved.playtimeMinutes);QVERIFY(!saved.team.first().level);
            auto registration=*store.registration("owned-adventure");registration.adventure.title="Renamed in Worlds";
            done=false;store.saveAdventureAsync(registration,this,[&](auto r){QVERIFY(r.success);done=true;});QTRY_VERIFY(done);
            saved.notes="Edited memory";done=false;
            store.saveArchiveAsync(saved,this,[&](auto r){QVERIFY(r.success);QCOMPARE(r.revision,2);done=true;});QTRY_VERIFY(done);
            QCOMPARE(store.loadArchive().entries.first().adventureTitle,"A personal journey"); // Historical title snapshot.
            saved.notes="Stale overwrite";bool rejected=false;
            store.saveArchiveAsync(saved,this,[&](auto r){rejected=!r.success;});QTRY_VERIFY(rejected);
            QCOMPARE(store.loadArchive().entries.first().notes,"Edited memory");
        }
        LocalStateStore reopened(dir.path());reopened.open();QTRY_VERIFY(reopened.ready());
        const auto entries=reopened.loadArchive().entries;QCOMPARE(entries.size(),1);QCOMPARE(entries.first().revision,2);
        QCOMPARE(entries.first().team.size(),2);QCOMPARE(entries.first().team[1].level,std::optional<int>(42));
        QVERIFY(!entries.first().completedAt.isValid());QVERIFY(!entries.first().playtimeMinutes);
        QFile file(dir.path()+"/original.bin");QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("Original test fixture; not game data."));
    }
    void invalidAndUnavailableAdventureNeverCreatesMemory() {
        QTemporaryDir dir;LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
        HallOfFameEntry entry;entry.id="missing-memory";entry.adventureId="absent";
        bool rejected=false;store.saveArchiveAsync(entry,this,[&](auto r){rejected=!r.success;});QTRY_VERIFY(rejected);
        QVERIFY(store.loadArchive().entries.isEmpty());
        entry.team={{"",50}};QVERIFY(!validateArchiveEntry(entry).isEmpty());
        entry.team={{"Buddy",101}};QVERIFY(!validateArchiveEntry(entry).isEmpty());
        entry.team={{"Buddy",50}};entry.notes="First\nSecond";QVERIFY(!validateArchiveEntry(entry).isEmpty());
        entry.notes.clear();entry.playtimeMinutes=0;QVERIFY(validateArchiveEntry(entry).isEmpty());
        entry.playtimeMinutes=-1;QVERIFY(!validateArchiveEntry(entry).isEmpty());
    }
    void controllerDraftTeamSearchAndCancel() {
        MockHallOfFameRepository archive;MockLibraryRepository library;ArchiveEditor editor(archive);editor.setLibrary(&library);
        QSignalSpy saved(&editor,&ArchiveEditor::saved);QSignalSpy text(&editor,&ArchiveEditor::textRequested);
        editor.begin();editor.submit();QVERIFY(editor.isOpen());QVERIFY(!editor.error().isEmpty());
        editor.activate(0);QCOMPARE(editor.route(),"adventures");editor.dispatch(Action::Secondary);editor.applyText("no matches");
        QVERIFY(editor.rows().isEmpty());editor.dispatch(Action::Confirm);QCOMPARE(editor.route(),"form");
        editor.activate(0);editor.dispatch(Action::Secondary);editor.applyText("Emerald");QVERIFY(!editor.rows().isEmpty());
        editor.activate(0);editor.activate(1);editor.applyText("20260230");QVERIFY(!editor.error().isEmpty());
        editor.activate(1);editor.applyText("20260913");QVERIFY(editor.error().isEmpty());
        editor.activate(2);editor.applyText("120");editor.activate(4);QCOMPARE(editor.route(),"team");
        editor.activate(0);editor.applyText("Buddy");editor.dispatch(Action::Secondary);editor.applyText("55");
        QCOMPARE(editor.fields().first().toMap()["subtitle"].toString(),"Lv 55");
        editor.dispatch(Action::Right);editor.activate(1);editor.applyText("Leaf");editor.dispatch(Action::Back);
        QCOMPARE(editor.focusIndex(),4);editor.activate(3);editor.applyText("A good day");editor.submit();
        QVERIFY(editor.saving());QTRY_VERIFY(!editor.saving());QCOMPARE(saved.size(),1);QVERIFY(!editor.isOpen());
        auto records=archive.loadArchive().entries;QCOMPARE(records.size(),5);
        const auto memory=records.last();QCOMPARE(memory.team[0].level,std::optional<int>(55));QCOMPARE(memory.notes,"A good day");
        editor.begin(memory);editor.activate(3);editor.applyText("Discard me");editor.cancel();
        QCOMPARE(archive.loadArchive().entries.last().notes,"A good day");QVERIFY(text.size()>=7);
    }
    void lockedWriteKeepsEditorDraftAndAllowsRetry() {
        QTemporaryDir dir;LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
        auto registration=adventure(dir.path());bool done=false;
        store.saveAdventureAsync(registration,this,[&](auto r){QVERIFY(r.success);done=true;});QTRY_VERIFY(done);
        ArchiveEditor editor(store);editor.setLibrary(&store);editor.begin();editor.activate(0);editor.activate(0);
        editor.activate(3);editor.applyText("Keep this draft");
        Connection connection(dir.path());QSqlQuery q(connection.db);QVERIFY(q.exec("BEGIN IMMEDIATE"));
        editor.submit();QVERIFY(editor.saving());QTRY_VERIFY(!editor.saving());QVERIFY(editor.isOpen());QVERIFY(!editor.error().isEmpty());
        QVERIFY(store.loadArchive().entries.isEmpty());QCOMPARE(editor.fields()[3].toMap()["value"].toString(),"Keep this draft");
        QVERIFY(q.exec("ROLLBACK"));editor.submit();QTRY_VERIFY(!editor.saving());QVERIFY(!editor.isOpen());
        QCOMPARE(store.loadArchive().entries.first().notes,"Keep this draft");
    }
};
QTEST_GUILESS_MAIN(ArchiveTests)
#include "ArchiveTests.moc"
