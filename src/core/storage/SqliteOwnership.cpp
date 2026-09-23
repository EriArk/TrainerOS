#include "SqliteOwnership.h"
#include <QSqlQuery>
#include <QVariant>
#include <QUuid>

namespace trainer {
namespace {
QString failure() { return "Your Trainer ownership couldn't be prepared. Existing data has been kept."; }
}
QString localOwner(QSqlDatabase& db) {
    QSqlQuery q(db);
    if (!q.exec("SELECT o.id FROM local_owner l JOIN trainer_owners o ON o.id=l.trainer_id WHERE l.slot=1") || !q.next()) return {};
    return q.value(0).toString();
}
QString migrateOwnership(QSqlDatabase& db) {
    // Caller owns the transaction. Never interpolate a stored identity into SQL.
    QSqlQuery q(db);
    if (!q.exec("SELECT id FROM trainer_profile WHERE slot=1")) return failure();
    QString owner = q.next() ? q.value(0).toString() : QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (owner.isEmpty()) return failure();
    q.finish();
    for (const auto& sql : QStringList{
        "CREATE TABLE trainer_owners(id TEXT PRIMARY KEY NOT NULL CHECK(length(id)>0))",
        "CREATE TABLE local_owner(slot INTEGER PRIMARY KEY CHECK(slot=1), trainer_id TEXT NOT NULL REFERENCES trainer_owners(id) ON UPDATE CASCADE)",
        "CREATE TABLE owned_favorites(trainer_id TEXT NOT NULL REFERENCES trainer_owners(id) ON UPDATE CASCADE, entry_id TEXT NOT NULL, PRIMARY KEY(trainer_id,entry_id))",
        "CREATE TABLE owned_shell_state(trainer_id TEXT NOT NULL REFERENCES trainer_owners(id) ON UPDATE CASCADE, scope TEXT NOT NULL, payload BLOB NOT NULL, PRIMARY KEY(trainer_id,scope))",
        "CREATE TABLE owned_records(trainer_id TEXT NOT NULL REFERENCES trainer_owners(id) ON UPDATE CASCADE, entry_id TEXT NOT NULL, seen INTEGER CHECK(seen IN (0,1)), caught INTEGER CHECK(caught IN (0,1)), notes TEXT NOT NULL, revision INTEGER NOT NULL CHECK(revision>0), PRIMARY KEY(trainer_id,entry_id), CHECK(caught IS NOT 1 OR seen IS 1), CHECK(seen IS NOT 0 OR caught IS 0))",
        "ALTER TABLE play_sessions ADD COLUMN trainer_id TEXT REFERENCES trainer_owners(id) ON UPDATE CASCADE",
        "ALTER TABLE hall_of_fame ADD COLUMN trainer_id TEXT REFERENCES trainer_owners(id) ON UPDATE CASCADE"})
        if (!q.exec(sql)) return failure();
    q.prepare("INSERT INTO trainer_owners VALUES(?)"); q.addBindValue(owner);
    if (!q.exec()) return failure();
    q.prepare("INSERT INTO local_owner VALUES(1,?)"); q.addBindValue(owner);
    if (!q.exec()) return failure();
    for (const auto& sql : QStringList{
        "INSERT INTO owned_favorites SELECT ?,entry_id FROM pokedex_favorites",
        "INSERT INTO owned_shell_state SELECT ?,scope,payload FROM shell_state",
        "INSERT INTO owned_records SELECT ?,entry_id,seen,caught,notes,revision FROM pokedex_records",
        "UPDATE play_sessions SET trainer_id=?", "UPDATE hall_of_fame SET trainer_id=?"}) {
        q.prepare(sql); q.addBindValue(owner); if (!q.exec()) return failure();
    }
    for (const auto& sql : QStringList{
        "DROP TABLE pokedex_favorites", "ALTER TABLE owned_favorites RENAME TO pokedex_favorites",
        "DROP TABLE shell_state", "ALTER TABLE owned_shell_state RENAME TO shell_state",
        "DROP TABLE pokedex_records", "ALTER TABLE owned_records RENAME TO pokedex_records",
        "CREATE INDEX play_sessions_owner ON play_sessions(trainer_id,adventure_id)",
        "CREATE INDEX hall_of_fame_owner ON hall_of_fame(trainer_id,completed_at DESC,id)",
        // ALTER preserves play-session rowids and the existing exit-media FK.
        // SQLite cannot add a non-null FK to populated tables: enforce it on
        // every later write, after assigning all existing rows in this transaction.
        "CREATE TRIGGER sessions_owner_insert BEFORE INSERT ON play_sessions WHEN NEW.trainer_id IS NULL BEGIN SELECT RAISE(ABORT,'Trainer required'); END",
        "CREATE TRIGGER sessions_owner_update BEFORE UPDATE OF trainer_id ON play_sessions WHEN NEW.trainer_id IS NULL BEGIN SELECT RAISE(ABORT,'Trainer required'); END",
        "CREATE TRIGGER hall_owner_insert BEFORE INSERT ON hall_of_fame WHEN NEW.trainer_id IS NULL BEGIN SELECT RAISE(ABORT,'Trainer required'); END",
        "CREATE TRIGGER hall_owner_update BEFORE UPDATE OF trainer_id ON hall_of_fame WHEN NEW.trainer_id IS NULL BEGIN SELECT RAISE(ABORT,'Trainer required'); END",
        "CREATE TRIGGER media_owner_insert BEFORE INSERT ON exit_media WHEN NOT EXISTS(SELECT 1 FROM play_sessions WHERE id=NEW.session_id AND adventure_id=NEW.adventure_id AND trainer_id=NEW.trainer_id) BEGIN SELECT RAISE(ABORT,'Session owner mismatch'); END",
        "CREATE TRIGGER media_owner_update BEFORE UPDATE ON exit_media WHEN NOT EXISTS(SELECT 1 FROM play_sessions WHERE id=NEW.session_id AND adventure_id=NEW.adventure_id AND trainer_id=NEW.trainer_id) BEGIN SELECT RAISE(ABORT,'Session owner mismatch'); END"})
        if (!q.exec(sql)) return failure();
    if (!q.exec("PRAGMA foreign_key_check") || q.next()) return failure();
    return {};
}
QString adoptInitialOwner(QSqlDatabase& db, const QString& previous, const QString& profileId) {
    if (previous.isEmpty() || profileId.isEmpty()) return failure();
    QSqlQuery q(db);
    q.prepare("UPDATE trainer_owners SET id=? WHERE id=?");
    q.addBindValue(profileId); q.addBindValue(previous);
    return q.exec() && q.numRowsAffected()==1 ? QString() : failure();
}
}
