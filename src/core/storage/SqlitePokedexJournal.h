#pragma once
#include "core/repository/PokedexRepository.h"
#include <QSqlDatabase>

namespace trainer {
struct PokedexJournalSnapshot { QHash<QString,PokedexProgress> records; QString error; };
QString migratePokedexJournal(QSqlDatabase&);
PokedexJournalSnapshot readPokedexJournal(QSqlDatabase&, const QString& owner);
PokedexWriteResult writePokedexRecord(QSqlDatabase&, const QString& owner, const QString&, const PokedexProgress&, PokedexJournalSnapshot&);
}
