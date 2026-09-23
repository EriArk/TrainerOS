#include "SqliteLibrary.h"
#include "core/input/TextEntryController.h"
#include <QSqlQuery>
#include <QJsonDocument>
#include <QFileInfo>
#include <QSet>
#include <algorithm>

namespace trainer {
namespace {
QString sqlFailure() { return "Couldn't save your library. Check storage access or free space, then retry."; }
bool validText(const QString& text, int max) {
    return !text.trimmed().isEmpty() && TextEntryController::characterCount(text) <= max
        && std::none_of(text.begin(), text.end(), [](QChar c) {
            return c.category() == QChar::Other_Control || c.category() == QChar::Separator_Line || c.category() == QChar::Separator_Paragraph;
        });
}
}
QString migrateLibrary(QSqlDatabase& db) {
    // The caller wraps schema migrations in one transaction.
    QSqlQuery q(db);
    const QStringList statements{
        "CREATE TABLE worlds(id TEXT PRIMARY KEY NOT NULL, name TEXT NOT NULL, sort_order INTEGER NOT NULL)",
        "CREATE TABLE adventures(id TEXT PRIMARY KEY NOT NULL, world_id TEXT NOT NULL REFERENCES worlds(id), title TEXT NOT NULL, kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 2), description TEXT NOT NULL, content_path TEXT NOT NULL, adapter_id TEXT NOT NULL, config BLOB NOT NULL, revision INTEGER NOT NULL CHECK(revision>0))",
        "CREATE TABLE adventure_worlds(adventure_id TEXT NOT NULL REFERENCES adventures(id) ON DELETE CASCADE, world_id TEXT NOT NULL REFERENCES worlds(id), PRIMARY KEY(adventure_id,world_id))",
        "CREATE TABLE preferences(slot INTEGER PRIMARY KEY CHECK(slot=1), theme TEXT NOT NULL, reduced_motion INTEGER NOT NULL CHECK(reduced_motion IN(0,1)))"};
    for (const auto& sql : statements) if (!q.exec(sql)) return sqlFailure();
    int order = 0;
    // Region reference names are not personal history or sample Adventures.
    for (const auto& name : QStringList{"Kanto", "Johto", "Hoenn", "Sinnoh", "Unova", "Kalos", "Alola", "Galar", "Paldea"}) {
        q.prepare("INSERT INTO worlds VALUES(?,?,?)"); q.addBindValue(name.toLower()); q.addBindValue(name); q.addBindValue(order++);
        if (!q.exec()) return sqlFailure();
    }
    return {};
}
QString migrateLibraryDomains(QSqlDatabase& db) {
    // SQLite's create/copy/drop/rename procedure. Disable FK enforcement before
    // BEGIN, preserve IDs/rowids and dependent schema, check all FKs before COMMIT.
    QSqlQuery q(db);
    QStringList dependentSchema;
    if (!q.exec("SELECT sql FROM sqlite_master WHERE tbl_name IN ('adventures','exit_media') AND type IN ('index','trigger') AND sql IS NOT NULL")) return sqlFailure();
    while (q.next()) dependentSchema.append(q.value(0).toString());
    if (!q.exec("SELECT sql FROM sqlite_master WHERE type='table' AND name='exit_media'") || !q.next()) return sqlFailure();
    QString mediaSchema = q.value(0).toString(); q.finish();
    if (!mediaSchema.contains("CHECK(domain='pokemon')")) return sqlFailure();
    mediaSchema.replace("CREATE TABLE exit_media", "CREATE TABLE domain_exit_media");
    mediaSchema.replace("CHECK(domain='pokemon')", "CHECK(domain IN ('pokemon','multiverse'))");
    if (!q.exec("PRAGMA foreign_keys=OFF")) return sqlFailure();
    QString error;
    if (!db.transaction()) error = sqlFailure();
    else {
        QStringList statements{
            "CREATE TABLE domain_adventures(id TEXT PRIMARY KEY NOT NULL, world_id TEXT REFERENCES worlds(id), title TEXT NOT NULL, kind INTEGER NOT NULL CHECK(kind BETWEEN 0 AND 2), description TEXT NOT NULL, content_path TEXT NOT NULL, adapter_id TEXT NOT NULL, config BLOB NOT NULL, revision INTEGER NOT NULL CHECK(revision>0), platform_id TEXT NOT NULL DEFAULT '', catalogue_id TEXT NOT NULL DEFAULT '', variant TEXT NOT NULL DEFAULT '', domain TEXT NOT NULL DEFAULT 'pokemon' CHECK(domain IN ('pokemon','multiverse')), CHECK((domain='pokemon' AND world_id IS NOT NULL) OR (domain='multiverse' AND world_id IS NULL AND platform_id<>'' AND catalogue_id='')))",
            "INSERT INTO domain_adventures(rowid,id,world_id,title,kind,description,content_path,adapter_id,config,revision,platform_id,catalogue_id,variant) SELECT rowid,id,world_id,title,kind,description,content_path,adapter_id,config,revision,platform_id,catalogue_id,variant FROM adventures",
            mediaSchema,
            "INSERT INTO domain_exit_media SELECT * FROM exit_media",
            "DROP TABLE exit_media", "DROP TABLE adventures",
            "ALTER TABLE domain_adventures RENAME TO adventures",
            "ALTER TABLE domain_exit_media RENAME TO exit_media"};
        statements.append(dependentSchema);
        statements.append("CREATE INDEX adventures_domain_platform ON adventures(domain,platform_id)");
        statements.append("PRAGMA user_version=11");
        for (const auto& sql : statements) if (!q.exec(sql)) { error = sqlFailure(); break; }
        if (error.isEmpty() && (!q.exec("PRAGMA foreign_key_check") || q.next())) error = sqlFailure();
        q.finish();
        if (error.isEmpty() && !db.commit()) error = sqlFailure();
        if (!error.isEmpty()) db.rollback();
    }
    if (!q.exec("PRAGMA foreign_keys=ON")) error = sqlFailure();
    return error;
}
LibrarySnapshot readLibrary(QSqlDatabase& db) {
    LibrarySnapshot result;
    QSqlQuery q(db);
    const auto fail = [&] { result.error = "Your library couldn't be read. The existing data has been kept."; return result; };
    if (!q.exec("SELECT id,name FROM worlds ORDER BY sort_order,id")) return fail();
    while (q.next()) result.worlds.append({q.value(0).toString(), q.value(1).toString(), {}});
    if (!q.exec("SELECT id,world_id,title,kind,description,content_path,adapter_id,config,revision,platform_id,catalogue_id,variant,domain FROM adventures ORDER BY title COLLATE NOCASE,id")) return fail();
    while (q.next()) {
        AdventureRegistration record;
        record.adventure.id = q.value(0).toString(); record.adventure.worldId = q.value(1).toString();
        record.adventure.title = q.value(2).toString(); record.adventure.kind = AdventureKind(q.value(3).toInt());
        record.adventure.description = q.value(4).toString(); record.contentPath = q.value(5).toString();
        record.adventure.adapterId = q.value(6).toString();
        QJsonParseError error;
        const auto config = QJsonDocument::fromJson(q.value(7).toByteArray(), &error);
        if (error.error != QJsonParseError::NoError || !config.isObject()) return fail();
        record.integrationConfig = config.object(); record.revision = q.value(8).toInt();
        record.adventure.platformId = q.value(9).toString(); record.adventure.catalogueId = q.value(10).toString();
        record.adventure.variant = q.value(11).toString();
        record.adventure.domain = q.value(12).toString();
        const QFileInfo content(record.contentPath);
        record.contentAvailable = content.isFile() && content.isReadable();
        result.registrations.append(record);
    }
    if (!q.exec("SELECT adventure_id,world_id FROM adventure_worlds ORDER BY world_id")) return fail();
    while (q.next()) for (auto& r : result.registrations) if (r.adventure.id == q.value(0).toString())
        r.adventure.additionalWorldIds.append(q.value(1).toString());
    if (!q.exec("SELECT theme,reduced_motion FROM preferences WHERE slot=1")) return fail();
    if (q.next()) result.preferences = {q.value(0).toString(), q.value(1).toBool()};
    if (!QStringList{"turquoise", "red", "green", "blue", "orange"}.contains(result.preferences.theme)) result.preferences.theme = "turquoise";
    if (!q.exec("PRAGMA foreign_key_check") || q.next()) return fail();
    return result;
}
LibraryWriteResult writeAdventure(QSqlDatabase& db, const AdventureRegistration& record) {
    const auto& a = record.adventure;
    if ((a.domain != "pokemon" && a.domain != "multiverse")
        || (a.domain == "pokemon" && a.worldId.isEmpty())
        || (a.domain == "multiverse" && (!a.worldId.isEmpty() || !a.additionalWorldIds.isEmpty()
            || !a.catalogueId.isEmpty() || a.platformId.isEmpty() || record.newWorld || !record.additionalNewWorlds.isEmpty())))
        return {false, "Choose the matching library and platform for this Adventure."};
    if (a.id.isEmpty() || a.collectionOnly || !validText(a.title, 96) || (!a.description.isEmpty() && !validText(a.description, 160))
        || (!a.variant.isEmpty() && !validText(a.variant, 96))
        || int(a.kind) < 0 || int(a.kind) > 2 || record.revision < 0)
        return {false, "Give this Adventure a valid title and edition before saving."};
    // No ROM parsing or execution: validate only a readable regular file reference.
    const QFileInfo content(record.contentPath);
    if (!content.isAbsolute() || !content.isFile() || !content.isReadable())
        return {false, "This file isn't available. Choose a readable local file and retry."};
    if (a.adapterId.isEmpty()) return {false, "This Adventure needs a setup identity."};
    if (!db.transaction()) return {false, sqlFailure()};
    QString error;
    {
        QSqlQuery q(db);
        if (record.newWorld) {
            if (record.newWorld->id.isEmpty() || record.newWorld->id != a.worldId || !validText(record.newWorld->name, 32))
                error = "Give the new World a valid name.";
            else {
                q.prepare("INSERT INTO worlds(id,name,sort_order) SELECT ?,?,COALESCE(MAX(sort_order),-1)+1 FROM worlds");
                q.addBindValue(record.newWorld->id); q.addBindValue(record.newWorld->name.trimmed());
                if (!q.exec()) error = "This World couldn't be created. Retry from the library.";
            }
        }
        for (const auto& world : record.additionalNewWorlds) {
            if (!error.isEmpty()) break;
            if (!a.additionalWorldIds.contains(world.id) || !validText(world.name, 32)) { error = "Choose valid additional Worlds."; break; }
            q.prepare("INSERT OR IGNORE INTO worlds(id,name,sort_order) SELECT ?,?,COALESCE(MAX(sort_order),-1)+1 FROM worlds");
            q.addBindValue(world.id); q.addBindValue(world.name);
            if (!q.exec()) error = sqlFailure();
        }
        if (error.isEmpty()) {
            q.prepare("SELECT revision,domain FROM adventures WHERE id=?"); q.addBindValue(a.id);
            if (!q.exec()) error = sqlFailure();
            else if (q.next() ? q.value(0).toInt() != record.revision || q.value(1).toString() != a.domain : record.revision != 0)
                error = "This Adventure changed since you opened it. Reopen its details before editing.";
            q.finish();
        }
        if (error.isEmpty()) {
            q.prepare(record.revision == 0
                ? "INSERT INTO adventures(world_id,title,kind,description,content_path,adapter_id,config,revision,platform_id,catalogue_id,variant,domain,id) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)"
                : "UPDATE adventures SET world_id=?,title=?,kind=?,description=?,content_path=?,adapter_id=?,config=?,revision=?,platform_id=?,catalogue_id=?,variant=?,domain=? WHERE id=?");
            q.addBindValue(a.domain == "multiverse" ? QVariant() : QVariant(a.worldId)); q.addBindValue(a.title.trimmed()); q.addBindValue(int(a.kind));
            q.addBindValue(a.description.isNull() ? QString("") : a.description.trimmed());
            q.addBindValue(record.contentPath); q.addBindValue(a.adapterId);
            q.addBindValue(QJsonDocument(record.integrationConfig).toJson(QJsonDocument::Compact));
            q.addBindValue(record.revision + 1);
            q.addBindValue(a.platformId.isNull() ? QString("") : a.platformId);
            q.addBindValue(a.catalogueId.isNull() ? QString("") : a.catalogueId);
            q.addBindValue(a.variant.isNull() ? QString("") : a.variant);
            q.addBindValue(a.domain);
            q.addBindValue(a.id);
            if (!q.exec()) error = sqlFailure();
        }
        if (error.isEmpty()) {
            q.prepare("DELETE FROM adventure_worlds WHERE adventure_id=?"); q.addBindValue(a.id);
            if (!q.exec()) error = sqlFailure();
            QSet<QString> unique;
            for (const auto& id : a.additionalWorldIds) {
                if (id == a.worldId || unique.contains(id)) continue;
                unique.insert(id);
                q.prepare("INSERT INTO adventure_worlds VALUES(?,?)"); q.addBindValue(a.id); q.addBindValue(id);
                if (!q.exec()) { error = sqlFailure(); break; }
            }
        }
    }
    if (error.isEmpty() && !db.commit()) error = sqlFailure();
    if (!error.isEmpty()) db.rollback();
    return {error.isEmpty(), error, error.isEmpty() ? record.revision + 1 : record.revision};
}
QString writePreferences(QSqlDatabase& db, const ShellPreferences& value) {
    if (!QStringList{"turquoise", "red", "green", "blue", "orange"}.contains(value.theme)) return "Choose an available color theme.";
    QSqlQuery q(db);
    q.prepare("INSERT INTO preferences VALUES(1,?,?) ON CONFLICT(slot) DO UPDATE SET theme=excluded.theme,reduced_motion=excluded.reduced_motion");
    q.addBindValue(value.theme); q.addBindValue(value.reducedMotion ? 1 : 0);
    return q.exec() ? QString() : "Couldn't save preferences. Check storage access and retry.";
}
}
