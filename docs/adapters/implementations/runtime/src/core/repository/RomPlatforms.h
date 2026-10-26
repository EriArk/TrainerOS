#pragma once
#include <QList>
#include <QStringList>

namespace trainer {
struct RomPlatform {
    QString id, folder, name, core, shape;
    QStringList extensions, aliases;
};
// ARM64 route catalogue, not a per-title compatibility or save-write claim.
const QList<RomPlatform>& romPlatforms();
QString romPlatformId(const QString& folder);
QString romCore(const QString& platform);
bool romContentSupported(const QString& platform, const QString& core, const QString& suffix);
}
