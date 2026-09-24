#pragma once
#include "core/model/Models.h"
#include <QSqlDatabase>

namespace trainer {
struct LibrarySnapshot {
    QList<World> worlds;
    QList<AdventureRegistration> registrations;
    ShellPreferences preferences;
    QString error;
};
// Called exclusively by the connection's worker thread.
QString migrateLibrary(QSqlDatabase&);
QString migrateLibraryDomains(QSqlDatabase&); // Owns its FK-safe transaction.
LibrarySnapshot readLibrary(QSqlDatabase&);
LibraryWriteResult writeAdventure(QSqlDatabase&, const AdventureRegistration&);
QString writePreferences(QSqlDatabase&, const ShellPreferences&);
QString migrateLibraryEditing(QSqlDatabase&);
QString editLibrary(QSqlDatabase&, const LibraryEdit&);
}
