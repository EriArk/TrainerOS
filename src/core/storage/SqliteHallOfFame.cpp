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
ArchiveResult readHallOfFame(QSqlDatabase& db, const QString& owner) {
    ArchiveResult result;
    QSqlQuery q(db);
    q.prepare("SELECT id,adventure_id,title,world,completed_at,playtime_minutes,team,notes,source,revision FROM hall_of_fame WHERE trainer_id=? ORDER BY completed_at DESC,id");q.addBindValue(owner);
    if (!q.exec())
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
ArchiveWriteResult writeHallOfFame(QSqlDatabase& db, const QString& owner, const HallOfFameEntry& candidate, ArchiveResult& snapshot) {
    if(owner.isEmpty())return {false,failure()};
    const auto invalid=validateArchiveEntry(candidate);
    if (!invalid.isEmpty()) return {false,invalid};
    auto entry=candidate;
    if (!db.transaction()) return {false,failure()};
    QString error;
    {
        QSqlQuery q(db);
        QString previousAdventure;
        if (entry.revision>0) {
            q.prepare("SELECT adventure_id,title,world,source FROM hall_of_fame WHERE id=? AND trainer_id=? AND revision=?");
            q.addBindValue(entry.id);q.addBindValue(owner);q.addBindValue(entry.revision);
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
            q.prepare(creating ? "INSERT INTO hall_of_fame(adventure_id,title,world,completed_at,playtime_minutes,team,notes,source,revision,id,trainer_id) VALUES(?,?,?,?,?,?,?,?,?,?,?)"
                : "UPDATE hall_of_fame SET adventure_id=?,title=?,world=?,completed_at=?,playtime_minutes=?,team=?,notes=?,source=?,revision=? WHERE id=? AND trainer_id=? AND revision=?");
            q.addBindValue(entry.adventureId);q.addBindValue(entry.adventureTitle);q.addBindValue(entry.world);
            q.addBindValue(text(entry.completedAt.isValid()?entry.completedAt.toUTC().toString(Qt::ISODateWithMs):QString()));
            q.addBindValue(entry.playtimeMinutes ? QVariant(*entry.playtimeMinutes) : QVariant());
            q.addBindValue(QJsonDocument(team).toJson(QJsonDocument::Compact));q.addBindValue(text(entry.notes.trimmed()));
            q.addBindValue(entry.source==ArchiveSource::Manual ? "manual" : "imported");q.addBindValue(entry.revision+1);q.addBindValue(entry.id);
            q.addBindValue(owner);
            if (!creating) q.addBindValue(entry.revision);
            if (!q.exec() || q.numRowsAffected()!=1) error=failure();
        }
    }
    if (error.isEmpty()) { snapshot=readHallOfFame(db, owner);if(!snapshot.success)error=snapshot.error; }
    if (error.isEmpty() && !db.commit()) error=failure();
    if (!error.isEmpty()) db.rollback();
    return {error.isEmpty(),error,error.isEmpty()?entry.revision+1:entry.revision};
}
QString migrateChampions(QSqlDatabase& db) {
    if(!db.transaction())return failure();
    QSqlQuery q(db);
    const bool ok=q.exec("CREATE TABLE champion_records (trainer_id TEXT NOT NULL REFERENCES trainer_owners(id) ON UPDATE CASCADE, adventure_id TEXT NOT NULL, build TEXT NOT NULL, id TEXT NOT NULL, payload BLOB NOT NULL, PRIMARY KEY(trainer_id,adventure_id,build,id))")
        && q.exec("PRAGMA user_version=14");
    if(ok && db.commit())return {};
    db.rollback();return failure();
}
namespace {
QJsonObject championJson(const ChampionRecord& r) {
    QJsonArray team;
    for(const auto& m:r.team)team.append(QJsonObject{{"species",m.speciesId},{"name",m.name},{"nickname",m.nickname},
        {"number",m.number},{"level",m.level},{"shiny",m.shiny}});
    return {{"lineage",r.lineage},{"victory",r.victory},{"trainer",r.trainerName},{"save",r.saveRevision},
        {"observed",r.observedAt.toUTC().toString(Qt::ISODateWithMs)},{"team",team}};
}
bool validChampion(const ChampionRecord& r) {
    const auto hash=[](const QString& s){if(s.size()!=64)return false;for(auto c:s)if(!QString("0123456789abcdef").contains(c))return false;return true;};
    if(!hash(r.id)||!hash(r.lineage)||!hash(r.build)||!hash(r.saveRevision)||r.adventureId.isEmpty()
        ||!r.observedAt.isValid()||r.victory<1||r.victory>=999||r.team.isEmpty()||r.team.size()>6)return false;
    for(const auto& m:r.team)if(m.speciesId.isEmpty()||m.name.isEmpty()||m.name.size()>80||m.nickname.size()>40||m.level<1||m.level>100||m.number<0||m.number>386)return false;
    return true;
}
}
QList<ChampionRecord> readChampions(QSqlDatabase& db,const QString& owner,QString& error) {
    QSqlQuery q(db);q.prepare("SELECT adventure_id,build,id,payload FROM champion_records WHERE trainer_id=? ORDER BY rowid DESC");q.addBindValue(owner);
    error.clear();QList<ChampionRecord> result;
    if(!q.exec()){error=failure();return {};}
    while(q.next()) {
        ChampionRecord r;r.adventureId=q.value(0).toString();r.build=q.value(1).toString();r.id=q.value(2).toString();
        const auto o=QJsonDocument::fromJson(q.value(3).toByteArray()).object();
        r.lineage=o["lineage"].toString();r.victory=o["victory"].toInt();r.trainerName=o["trainer"].toString();
        r.saveRevision=o["save"].toString();r.observedAt=QDateTime::fromString(o["observed"].toString(),Qt::ISODateWithMs);
        for(const auto& v:o["team"].toArray()) {const auto m=v.toObject();r.team.append({m["species"].toString(),m["name"].toString(),m["nickname"].toString(),m["number"].toInt(),m["level"].toInt(),m["shiny"].toBool()});}
        if(!validChampion(r)){error="Your Champion records need recovery. Existing records have been kept.";return {};}
        result.append(r);
    }
    return result;
}
QString preserveChampions(QSqlDatabase& db,const QString& owner,const QList<ChampionRecord>& records) {
    if(owner.isEmpty()||records.size()>50)return failure();
    for(const auto& r:records)if(!validChampion(r))return failure();
    if(!db.transaction())return failure();
    for(const auto& r:records) {
        QSqlQuery q(db);
        q.prepare("INSERT INTO champion_records(trainer_id,adventure_id,build,id,payload) VALUES(?,?,?,?,?) ON CONFLICT(trainer_id,adventure_id,build,id) DO NOTHING");
        q.addBindValue(owner);q.addBindValue(r.adventureId);q.addBindValue(r.build);q.addBindValue(r.id);q.addBindValue(QJsonDocument(championJson(r)).toJson(QJsonDocument::Compact));
        if(!q.exec()){db.rollback();return failure();}
    }
    if(db.commit())return {};
    db.rollback();return failure();
}

}
