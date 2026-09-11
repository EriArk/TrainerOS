#include "SqlitePlayHistory.h"
#include <QSqlQuery>
#include <QVariant>

namespace trainer {
namespace {
QString failed() { return "Couldn't save Adventure history. Check free space or storage access. Your game saves are separate."; }
QString outcomeName(PlaySessionOutcome value) {
    switch (value) {
    case PlaySessionOutcome::Running: return "running";
    case PlaySessionOutcome::Returned: return "returned";
    case PlaySessionOutcome::Failed: return "failed";
    case PlaySessionOutcome::Interrupted: return "interrupted";
    }
    return {};
}
}
QString migratePlayHistory(QSqlDatabase& db) {
    QSqlQuery query(db);
    for (const auto& sql : QStringList{
        "CREATE TABLE play_sessions (id TEXT PRIMARY KEY NOT NULL, adventure_id TEXT NOT NULL REFERENCES adventures(id), started_at TEXT NOT NULL, ended_at TEXT, elapsed_seconds INTEGER CHECK(elapsed_seconds >= 0), outcome TEXT NOT NULL CHECK(outcome IN ('running','returned','failed','interrupted')))",
        "CREATE INDEX play_sessions_adventure ON play_sessions(adventure_id)"})
        if (!query.exec(sql)) return failed();
    return {};
}
QString interruptOpenSessions(QSqlDatabase& db) {
    QSqlQuery query(db);
    // An unclean exit provides no reliable end time or duration.
    return query.exec("UPDATE play_sessions SET outcome='interrupted' WHERE outcome='running'") ? QString() : failed();
}
PlayHistorySnapshot readPlayHistory(QSqlDatabase& db) {
    PlayHistorySnapshot result;
    QSqlQuery query(db);
    // Row order remains launch order across system-clock changes. One latest
    // session per Adventure prevents repeat launches crowding out other games.
    if (!query.exec("SELECT id,adventure_id,started_at,ended_at,elapsed_seconds,outcome FROM play_sessions WHERE rowid IN (SELECT MAX(rowid) FROM play_sessions GROUP BY adventure_id) ORDER BY rowid DESC LIMIT 100")) {
        result.error = failed(); return result;
    }
    while (query.next()) {
        PlaySession session;
        session.id = query.value(0).toString(); session.adventureId = query.value(1).toString();
        session.startedAt = QDateTime::fromString(query.value(2).toString(), Qt::ISODateWithMs);
        session.endedAt = QDateTime::fromString(query.value(3).toString(), Qt::ISODateWithMs);
        if (!query.value(4).isNull()) session.elapsedSeconds = query.value(4).toLongLong();
        const auto outcome = query.value(5).toString();
        session.outcome = outcome == "running" ? PlaySessionOutcome::Running : outcome == "returned" ? PlaySessionOutcome::Returned
            : outcome == "failed" ? PlaySessionOutcome::Failed : PlaySessionOutcome::Interrupted;
        result.recent.append(session);
    }
    if (!query.exec("SELECT adventure_id,SUM(elapsed_seconds) FROM play_sessions WHERE elapsed_seconds IS NOT NULL GROUP BY adventure_id")) {
        result.error = failed(); return result;
    }
    while (query.next()) result.totals.insert(query.value(0).toString(), query.value(1).toLongLong());
    return result;
}
QString writePlaySession(QSqlDatabase& db, const PlaySession& session) {
    if (session.id.isEmpty() || session.id.size() > 128 || session.adventureId.isEmpty() || !session.startedAt.isValid()) return failed();
    const bool starting = session.outcome == PlaySessionOutcome::Running;
    if (starting ? session.endedAt.isValid() || session.elapsedSeconds.has_value()
                 : !session.endedAt.isValid() || !session.elapsedSeconds || *session.elapsedSeconds < 0
                    || session.outcome == PlaySessionOutcome::Interrupted) return failed();
    QSqlQuery query(db);
    if (starting) {
        query.prepare("INSERT INTO play_sessions(id,adventure_id,started_at,outcome) VALUES(?,?,?,'running')");
        query.addBindValue(session.id); query.addBindValue(session.adventureId);
        query.addBindValue(session.startedAt.toUTC().toString(Qt::ISODateWithMs));
    } else {
        // Complete only the same running identity; never rewrite another launch.
        query.prepare("UPDATE play_sessions SET ended_at=?,elapsed_seconds=?,outcome=? WHERE id=? AND adventure_id=? AND started_at=? AND outcome='running'");
        query.addBindValue(session.endedAt.toUTC().toString(Qt::ISODateWithMs));
        query.addBindValue(*session.elapsedSeconds); query.addBindValue(outcomeName(session.outcome));
        query.addBindValue(session.id); query.addBindValue(session.adventureId);
        query.addBindValue(session.startedAt.toUTC().toString(Qt::ISODateWithMs));
    }
    return query.exec() && query.numRowsAffected() == 1 ? QString() : failed();
}
}
