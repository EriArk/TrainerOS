#pragma once
#include "core/repository/PokedexRepository.h"
#include <QSqlDatabase>

namespace trainer {
struct PokedexJournalSnapshot { QHash<QString,PokedexProgress> records; QString error; };
QString migratePokedexJournal(QSqlDatabase&);
PokedexJournalSnapshot readPokedexJournal(QSqlDatabase&);
PokedexWriteResult writePokedexRecord(QSqlDatabase&, const QString&, const PokedexProgress&, PokedexJournalSnapshot&);
}
