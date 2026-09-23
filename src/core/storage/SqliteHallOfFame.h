#pragma once
#include "core/repository/HallOfFameRepository.h"
#include <QSqlDatabase>

namespace trainer {
QString migrateHallOfFame(QSqlDatabase&);
ArchiveResult readHallOfFame(QSqlDatabase&, const QString& owner);
ArchiveWriteResult writeHallOfFame(QSqlDatabase&, const QString& owner, const HallOfFameEntry&, ArchiveResult& snapshot);
}
