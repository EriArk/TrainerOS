#pragma once
#include "core/model/Models.h"
#include <QSqlDatabase>

namespace trainer {
// Called on the store worker, under its exclusive library lock. Owns the SQL
// transaction and a durable, bounded file-move intent beside the database.
QString moveLibraryFile(QSqlDatabase&, const LibraryEdit&);
QString recoverLibraryFileMove(QSqlDatabase&);
}
