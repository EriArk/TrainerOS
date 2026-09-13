#include "SqliteHallOfFame.h"
#include <QSqlQuery>
#include <QJsonArray>
#include <QJsonDocument>

namespace trainer {
namespace {
QString failure() { return "Couldn't save your memory. Check storage access and try again."; }
QString text(const QString& value) { return value.isNull() ? QString("") : value; }
}
QString migrateHallOfFame(QSqlDatabase& db) {
    QSqlQuery q(db);
    for (const auto& sql : QStringList{
        "CREATE TABLE hall_of_fame (id TEXT PRIMARY KEY NOT NULL, adventure_id TEXT NOT NULL, title TEXT NOT NULL, world TEXT NOT NULL, completed_at TEXT NOT NULL, playtime_minutes INTEGER, team BLOB NOT NULL, notes TEXT NOT NULL, source TEXT NOT NULL CHECK(source IN ('manual','imported')), revision INTEGER NOT NULL CHECK(revision>0))",
        "CREATE INDEX hall_of_fame_date ON hall_of_fame(completed_at DESC,id)"})
        if (!q.exec(sql)) return failure();
    return {};
}
ArchiveResult readHallOfFame(QSqlDatabase& db) {
    ArchiveResult result;
    QSqlQuery q(db);
    if (!q.exec("SELECT id,adventure_id,title,world,completed_at,playtime_minutes,team,notes,source,revision FROM hall_of_fame ORDER BY completed_at DESC,id"))
        return {false, {}, "Your Hall of Fame couldn't be read. Existing memories have been kept."};
    while (q.next()) {
        HallOfFameEntry entry;
        entry.id=q.value(0).toString(); entry.adventureId=q.value(1).toString(); entry.adventureTitle=q.value(2).toString(); entry.world=q.value(3).toString();
        const auto date=q.value(4).toString(); entry.completedAt=QDateTime::fromString(date, Qt::ISODateWithMs);
        if (!q.value(5).isNull()) entry.playtimeMinutes=q.value(5).toInt();
        const auto team=QJsonDocument::fromJson(q.value(6).toByteArray());
        for (const auto& value : team.array()) {
            const auto member=value.toObject();
            if (!value.isObject() || !member["name"].isString()
                || (member.contains("level") && (!member["level"].isDouble() || member["level"].toDouble()!=member["level"].toInt())))
                return {false, {}, "Your Hall of Fame needs recovery. Existing memories have been kept."};
            entry.team.append({member["name"].toString(), member["level"].isDouble() ? std::optional<int>(member["level"].toInt()) : std::nullopt});
        }
        entry.notes=q.value(7).toString(); entry.source=q.value(8).toString()=="manual" ? ArchiveSource::Manual : ArchiveSource::Imported;
        entry.revision=q.value(9).toInt();
        if (!team.isArray() || (!date.isEmpty() && !entry.completedAt.isValid()) || !validateArchiveEntry(entry).isEmpty() || entry.revision<1)
            return {false, {}, "Your Hall of Fame needs recovery. Existing memories have been kept."};
        result.entries.append(entry);
    }
    return result;
}
ArchiveWriteResult writeHallOfFame(QSqlDatabase& db, const HallOfFameEntry& candidate, ArchiveResult& snapshot) {
    const auto invalid=validateArchiveEntry(candidate);
    if (!invalid.isEmpty()) return {false,invalid};
    auto entry=candidate;
    if (!db.transaction()) return {false,failure()};
    QString error;
    {
        QSqlQuery q(db);
        QString previousAdventure;
        if (entry.revision>0) {
            q.prepare("SELECT adventure_id,title,world,source FROM hall_of_fame WHERE id=? AND revision=?");
            q.addBindValue(entry.id);q.addBindValue(entry.revision);
            if (!q.exec()) error=failure();
            else if (!q.next()) error="This memory changed. Close the editor and reopen it before saving.";
            else {
                previousAdventure=q.value(0).toString();entry.adventureTitle=q.value(1).toString();entry.world=q.value(2).toString();
                entry.source=q.value(3).toString()=="manual" ? ArchiveSource::Manual : ArchiveSource::Imported;
            }
        } else entry.source=ArchiveSource::Manual;
        if (error.isEmpty() && previousAdventure!=entry.adventureId) {
            q.prepare("SELECT a.title,w.name FROM adventures a JOIN worlds w ON w.id=a.world_id WHERE a.id=?");q.addBindValue(entry.adventureId);
            if (!q.exec()) error=failure();
            else if (!q.next()) error="This Adventure is no longer in your library. Choose another Adventure.";
            else { entry.adventureTitle=q.value(0).toString();entry.world=q.value(1).toString(); }
        }
        if (error.isEmpty()) {
            QJsonArray team;
            for (const auto& member : entry.team) {
                QJsonObject value{{"name",member.name.trimmed()}};
                if (member.level) value["level"]=*member.level;
                team.append(value);
            }
            const bool creating=entry.revision==0;
            q.prepare(creating ? "INSERT INTO hall_of_fame(adventure_id,title,world,completed_at,playtime_minutes,team,notes,source,revision,id) VALUES(?,?,?,?,?,?,?,?,?,?)"
                : "UPDATE hall_of_fame SET adventure_id=?,title=?,world=?,completed_at=?,playtime_minutes=?,team=?,notes=?,source=?,revision=? WHERE id=? AND revision=?");
            q.addBindValue(entry.adventureId);q.addBindValue(entry.adventureTitle);q.addBindValue(entry.world);
            q.addBindValue(text(entry.completedAt.isValid()?entry.completedAt.toUTC().toString(Qt::ISODateWithMs):QString()));
            q.addBindValue(entry.playtimeMinutes ? QVariant(*entry.playtimeMinutes) : QVariant());
            q.addBindValue(QJsonDocument(team).toJson(QJsonDocument::Compact));q.addBindValue(text(entry.notes.trimmed()));
            q.addBindValue(entry.source==ArchiveSource::Manual ? "manual" : "imported");q.addBindValue(entry.revision+1);q.addBindValue(entry.id);
            if (!creating) q.addBindValue(entry.revision);
            if (!q.exec() || q.numRowsAffected()!=1) error=failure();
        }
    }
    if (error.isEmpty()) { snapshot=readHallOfFame(db);if(!snapshot.success)error=snapshot.error; }
    if (error.isEmpty() && !db.commit()) error=failure();
    if (!error.isEmpty()) db.rollback();
    return {error.isEmpty(),error,error.isEmpty()?entry.revision+1:entry.revision};
}
}
