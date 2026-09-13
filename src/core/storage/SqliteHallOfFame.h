#pragma once
#include "core/repository/HallOfFameRepository.h"
#include <QSqlDatabase>

namespace trainer {
QString migrateHallOfFame(QSqlDatabase&);
ArchiveResult readHallOfFame(QSqlDatabase&);
ArchiveWriteResult writeHallOfFame(QSqlDatabase&, const HallOfFameEntry&, ArchiveResult& snapshot);
}
