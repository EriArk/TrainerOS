#include "SqlitePokedexJournal.h"
#include <QSqlQuery>

namespace trainer {
namespace { QString failure(){return "Couldn't save your field journal. Check storage access and try again.";} }
QString migratePokedexJournal(QSqlDatabase& db) {
    QSqlQuery q(db);
    return q.exec("CREATE TABLE pokedex_records(entry_id TEXT PRIMARY KEY NOT NULL, seen INTEGER CHECK(seen IN (0,1)), caught INTEGER CHECK(caught IN (0,1)), notes TEXT NOT NULL, revision INTEGER NOT NULL CHECK(revision>0), CHECK(caught IS NOT 1 OR seen IS 1), CHECK(seen IS NOT 0 OR caught IS 0))") ? QString() : failure();
}
PokedexJournalSnapshot readPokedexJournal(QSqlDatabase& db) {
    PokedexJournalSnapshot snapshot;QSqlQuery q(db);
    if (!q.exec("SELECT entry_id,seen,caught,notes,revision FROM pokedex_records")) return {{},"Your field journal couldn't be read. Existing records have been kept."};
    while(q.next()) {
        const auto id=q.value(0).toString();PokedexProgress record;
        if(!q.value(1).isNull())record.seen=q.value(1).toBool();
        if(!q.value(2).isNull())record.caught=q.value(2).toBool();
        record.notes=q.value(3).toString();record.revision=q.value(4).toInt();
        if(!validatePokedexRecord(id,record).isEmpty() || record.revision<1) return {{},"Your field journal needs recovery. Existing records have been kept."};
        snapshot.records.insert(id,record);
    }
    return snapshot;
}
PokedexWriteResult writePokedexRecord(QSqlDatabase& db, const QString& id, const PokedexProgress& record, PokedexJournalSnapshot& snapshot) {
    const auto invalid=validatePokedexRecord(id,record);if(!invalid.isEmpty())return {false,invalid};
    if(!db.transaction())return {false,failure()};
    QString error;
    {
        QSqlQuery q(db);const bool creating=record.revision==0;
        q.prepare(creating ? "INSERT INTO pokedex_records(seen,caught,notes,revision,entry_id) VALUES(?,?,?,?,?)"
            : "UPDATE pokedex_records SET seen=?,caught=?,notes=?,revision=? WHERE entry_id=? AND revision=?");
        q.addBindValue(record.seen ? QVariant(*record.seen?1:0) : QVariant());
        q.addBindValue(record.caught ? QVariant(*record.caught?1:0) : QVariant());
        q.addBindValue(record.notes.isNull()?QString(""):record.notes.trimmed());q.addBindValue(record.revision+1);q.addBindValue(id);
        if(!creating)q.addBindValue(record.revision);
        if(!q.exec())error=failure();
        else if(q.numRowsAffected()!=1)error="This journal record changed. Reopen it before editing.";
    }
    if(error.isEmpty()){snapshot=readPokedexJournal(db);error=snapshot.error;}
    if(error.isEmpty()&&!db.commit())error=failure();
    if(!error.isEmpty())db.rollback();
    return {error.isEmpty(),error,error.isEmpty()?record.revision+1:record.revision};
}
}
