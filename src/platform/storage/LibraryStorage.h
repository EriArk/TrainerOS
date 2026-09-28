#pragma once
#include <QList>
#include <QString>
#include <QVariantMap>

namespace trainer {
struct LibraryLocation {
    QString label, path, mount, device;
    qint64 available = 0;
    qint64 total = 0;
    QString kind;
};
// Mounted destinations only. Never format, move content or replace aliases.
QList<LibraryLocation> libraryLocations(const QString& currentRoot);
QString prepareLibraryLocation(const LibraryLocation& location);
QVariantMap libraryLocationRow(const LibraryLocation&, const QString& currentRoot);
QString readLibraryRoot(const QString& directory, const QString& fallback);
QString saveLibraryRoot(const QString& directory, const QString& root);
}
