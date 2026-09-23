#include "LegacyStoreFixture.h"
#include "core/repository/OfflinePokedex.h"
#include "core/storage/LocalStateStore.h"
#include "features/pokedex/PokedexController.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

using namespace trainer;
namespace {
void filter(PokedexController& dex,int rail,const QString& id){
    dex.activateControl("rail",rail);const auto choices=dex.choices();
    for(int i=0;i<choices.size();++i)if(choices[i].toMap()["id"]==id){dex.activate(i);return;}
    QFAIL("Filter missing");
}
class Connection {
public:
    QSqlDatabase db;
    explicit Connection(const QString& path){db=QSqlDatabase::addDatabase("QSQLITE",QUuid::createUuid().toString());db.setDatabaseName(path+"/traineros.sqlite3");db.open();}
    ~Connection(){const auto n=db.connectionName();db.close();db={};QSqlDatabase::removeDatabase(n);}
};
}
class OfflinePokedexTests final : public QObject {
    Q_OBJECT
private slots:
    void completeReferenceFormsAndRegionalMembership() {
        OfflinePokedex reference;const auto catalog=reference.load();QVERIFY2(catalog.success,qPrintable(catalog.error));
        QCOMPARE(catalog.entries.size(),1025);QCOMPARE(catalog.collections.size(),10);
        int forms=0;for(const auto& entry:catalog.entries)forms+=entry.forms.size();QCOMPARE(forms,1579);
        QCOMPARE(catalog.entries.first().id,"bulbasaur");QCOMPARE(catalog.entries.last().id,"pecharunt");
        const auto& bulbasaur=catalog.entries.first();QCOMPARE(bulbasaur.forms.first().stats,QList<int>({45,49,49,65,65,45}));
        QCOMPARE(bulbasaur.forms.first().heightDm,7);QVERIFY(bulbasaur.familyIds.contains("venusaur"));
        const auto& vulpix=catalog.entries[36];QCOMPARE(vulpix.id,"vulpix");QCOMPARE(vulpix.forms.size(),2);
        QVERIFY(vulpix.forms[1].types.contains("Ice"));QVERIFY(vulpix.collectionIds.contains("alola"));
        MockPokedexRepository progress;PokedexController dex(reference,progress);
        dex.applySearch("Flabebe");QCOMPARE(dex.entries().size(),1);QCOMPARE(dex.detail()["id"].toString(),"flabebe");
        dex.applySearch("0037");filter(dex,2,"Ice");QCOMPARE(dex.entries().size(),1);
        dex.activateControl("list",0);QVERIFY(dex.detail()["types"].toString().contains("Ice"));
        dex.cycleForm();QVERIFY(dex.detail()["types"].toString().contains("Fire"));
        const auto nav=dex.navigationState();PokedexController restored(reference,progress);restored.restoreNavigation(nav);
        QCOMPARE(restored.detail()["form"],dex.detail()["form"]);
        dex.dispatch(Action::Back);dex.applySearch("1025");filter(dex,2,"");QCOMPARE(dex.entries().size(),1);
        QCOMPARE(dex.detail()["name"].toString(),"Pecharunt");
    }
    void brokenReferenceRejectedWithoutPartialProjection() {
        OfflinePokedex reference;QVERIFY(reference.load().success);
        QFile file(":/pokedex/pokedex.json");QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();
        auto root=QJsonDocument::fromJson(bytes).object();root["speciesCount"]=1024;
        QVERIFY(!OfflinePokedex::decode(QJsonDocument(root).toJson()).success);
        root=QJsonDocument::fromJson(bytes).object();auto entries=root["entries"].toArray();entries.append(entries.first());root["entries"]=entries;
        QVERIFY(!OfflinePokedex::decode(QJsonDocument(root).toJson()).success);
        QVERIFY(!OfflinePokedex::decode("truncated reference").success);
    }
    void journalMigrationReopenFavoritesAndStaleRevision() {
        QTemporaryDir dir;
        {Connection connection(dir.path());QVERIFY(createLegacyStore(connection.db,5));
            QSqlQuery q(connection.db);QVERIFY(q.exec("INSERT INTO pokedex_favorites VALUES('vulpix')"));}
        PokedexProgress saved;
        {
            LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());QVERIFY(store.progress("vulpix").favorite);
            QVERIFY(!store.progress("vulpix").seen);QVERIFY(!store.progress("vulpix").caught);
            PokedexProgress record;record.seen=true;record.caught=true;record.notes="Found on my own journey";
            bool done=false;store.saveRecordAsync("vulpix",record,this,[&](auto r){QVERIFY2(r.success,qPrintable(r.error));done=true;});
            QVERIFY(!store.progress("vulpix").seen);QTRY_VERIFY(done);saved=store.progress("vulpix");QVERIFY(saved.favorite);QCOMPARE(saved.revision,1);
            auto changed=saved;changed.notes="Another memory";done=false;
            store.saveRecordAsync("vulpix",changed,this,[&](auto r){QVERIFY(r.success);done=true;});QTRY_VERIFY(done);
            bool rejected=false;store.saveRecordAsync("vulpix",saved,this,[&](auto r){rejected=!r.success;});QTRY_VERIFY(rejected);
            QCOMPARE(store.progress("vulpix").notes,"Another memory");
            auto invalid=store.progress("vulpix");invalid.seen.reset();rejected=false;
            store.saveRecordAsync("vulpix",invalid,this,[&](auto r){rejected=!r.success;});QTRY_VERIFY(rejected);
            done=false;store.setFavoriteAsync("vulpix",false,this,[&](auto e){QVERIFY(e.isEmpty());done=true;});QTRY_VERIFY(done);
            QCOMPARE(store.progress("vulpix").revision,2);QCOMPARE(store.progress("vulpix").notes,"Another memory");
        }
        LocalStateStore reopened(dir.path());reopened.open();QTRY_VERIFY(reopened.ready());
        const auto record=reopened.progress("vulpix");QCOMPARE(record.seen,std::optional<bool>(true));QCOMPARE(record.caught,std::optional<bool>(true));
        QCOMPARE(record.notes,"Another memory");QVERIFY(!record.favorite);QVERIFY(!reopened.progress("eevee").seen);
    }
    void journalDraftConsistencyCancelFailedWriteAndFilterRecovery() {
        MockPokedexRepository repository;PokedexController dex(repository,repository);
        dex.applySearch("133");dex.activateControl("list",0);dex.editJournal();auto* editor=dex.journal();QVERIFY(editor->isOpen());
        editor->activate(1);QCOMPARE(editor->fields()[1].toMap()["value"].toString(),"Not recorded"); // Existing No -> unknown.
        editor->activate(1);QCOMPARE(editor->fields()[0].toMap()["value"].toString(),"Yes");
        editor->activate(0);QCOMPARE(editor->fields()[1].toMap()["value"].toString(),"No");
        editor->activate(2);editor->applyNote("Discarded");editor->cancel();QVERIFY(repository.progress("eevee").notes.isEmpty());
        dex.editJournal();editor->activate(2);editor->applyNote("Keep my note");repository.failNextWrite();editor->submit();
        QVERIFY(editor->isOpen());QVERIFY(!editor->error().isEmpty());QVERIFY(repository.progress("eevee").notes.isEmpty());
        editor->submit();QVERIFY(!editor->isOpen());QCOMPARE(repository.progress("eevee").notes,"Keep my note");
        dex.dispatch(Action::Back);filter(dex,3,"uncaught");dex.activateControl("list",0);dex.editJournal();
        editor->activate(1);editor->activate(1);editor->submit();QVERIFY(dex.entries().isEmpty());QCOMPARE(dex.zone(),"recovery");
    }
    void lockedJournalSaveRetainsDraftAndDoesNotFreezeUi() {
        QTemporaryDir dir;LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
        PokedexJournalEditor editor(store);editor.begin("pikachu","Pikachu");editor.applyNote("Keep me");
        Connection connection(dir.path());QSqlQuery q(connection.db);QVERIFY(q.exec("BEGIN IMMEDIATE"));
        int ticks=0;QTimer timer;timer.setInterval(10);connect(&timer,&QTimer::timeout,this,[&]{++ticks;});timer.start();
        editor.submit();QVERIFY(editor.saving());QTRY_VERIFY(!editor.saving());QVERIFY(editor.isOpen());QVERIFY(ticks>2);
        QVERIFY(!editor.error().isEmpty());QVERIFY(store.progress("pikachu").notes.isEmpty());
        QVERIFY(q.exec("ROLLBACK"));editor.submit();QTRY_VERIFY(!editor.saving());QVERIFY(!editor.isOpen());QCOMPARE(store.progress("pikachu").notes,"Keep me");
    }
};
QTEST_GUILESS_MAIN(OfflinePokedexTests)
#include "OfflinePokedexTests.moc"
