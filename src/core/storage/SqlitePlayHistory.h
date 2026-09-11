#pragma once
#include "core/model/Models.h"
#include <QHash>
#include <QSqlDatabase>

namespace trainer {
struct PlayHistorySnapshot {
    QString error;
    QList<PlaySession> recent;
    QHash<QString, qint64> totals;
};
QString migratePlayHistory(QSqlDatabase&);
QString interruptOpenSessions(QSqlDatabase&);
PlayHistorySnapshot readPlayHistory(QSqlDatabase&);
QString writePlaySession(QSqlDatabase&, const PlaySession&);
}
