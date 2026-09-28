#pragma once
#include <QList>
#include <QString>

namespace trainer {
struct LibraryLocation {
    QString label, path, mount, device;
    qint64 available = 0;
};
// Mounted destinations only. Never format, move content or replace aliases.
QList<LibraryLocation> libraryLocations(const QString& currentRoot);
QString prepareLibraryLocation(const LibraryLocation& location);
}
