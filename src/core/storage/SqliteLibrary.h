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
LibrarySnapshot readLibrary(QSqlDatabase&);
LibraryWriteResult writeAdventure(QSqlDatabase&, const AdventureRegistration&);
QString writePreferences(QSqlDatabase&, const ShellPreferences&);
}
