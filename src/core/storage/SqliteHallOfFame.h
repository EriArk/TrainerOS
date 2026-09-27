#pragma once
#include "core/repository/HallOfFameRepository.h"
#include <QSqlDatabase>

namespace trainer {
QString migrateChampions(QSqlDatabase&);
QList<ChampionRecord> readChampions(QSqlDatabase&, const QString& owner, QString& error);
QString preserveChampions(QSqlDatabase&, const QString& owner, const QList<ChampionRecord>&);
QString migrateHallOfFame(QSqlDatabase&);
ArchiveResult readHallOfFame(QSqlDatabase&, const QString& owner);
ArchiveWriteResult writeHallOfFame(QSqlDatabase&, const QString& owner, const HallOfFameEntry&, ArchiveResult& snapshot);
}
