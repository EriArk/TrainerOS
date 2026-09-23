#pragma once
#include "core/storage/SqliteLibrary.h"
#include "core/storage/SqlitePlayHistory.h"
#include "core/storage/SqliteHallOfFame.h"
#include "core/storage/SqlitePokedexJournal.h"
#include "core/storage/SqliteExitMedia.h"
#include <QSqlQuery>

// Construct an actual pre-ownership database, rather than lowering the version
// of the latest schema (which leaves newer constraints and columns behind).
inline bool createLegacyStore(QSqlDatabase& db, int version) {
    if (version < 3 || version > 7) return false;
    QSqlQuery q(db);
    for (const auto& sql : QStringList{
        "CREATE TABLE trainer_profile(slot INTEGER PRIMARY KEY CHECK(slot=1),id TEXT NOT NULL UNIQUE,name TEXT NOT NULL,emblem TEXT NOT NULL,favorite TEXT NOT NULL,created_at TEXT NOT NULL)",
        "CREATE TABLE pokedex_favorites(entry_id TEXT PRIMARY KEY NOT NULL)",
        "CREATE TABLE shell_state(scope TEXT PRIMARY KEY NOT NULL,payload BLOB NOT NULL)"})
        if (!q.exec(sql)) return false;
    if (!trainer::migrateLibrary(db).isEmpty()) return false;
    for (const auto& sql : QStringList{
        "ALTER TABLE adventures ADD COLUMN platform_id TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE adventures ADD COLUMN catalogue_id TEXT NOT NULL DEFAULT ''",
        "ALTER TABLE adventures ADD COLUMN variant TEXT NOT NULL DEFAULT ''",
        "CREATE INDEX adventures_catalogue ON adventures(catalogue_id)"})
        if (!q.exec(sql)) return false;
    if (version >= 4 && !trainer::migratePlayHistory(db).isEmpty()) return false;
    if (version >= 5 && !trainer::migrateHallOfFame(db).isEmpty()) return false;
    if (version >= 6 && !trainer::migratePokedexJournal(db).isEmpty()) return false;
    if (version >= 7 && !trainer::migrateExitMedia(db).isEmpty()) return false;
    return q.exec(QString("PRAGMA user_version=%1").arg(version));
}
