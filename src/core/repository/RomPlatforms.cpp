#include "RomPlatforms.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

static void initializeRomPlatforms() { Q_INIT_RESOURCE(rom_platforms); }
namespace trainer {
const QList<RomPlatform>& romPlatforms() {
    static const auto value = [] {
        initializeRomPlatforms();
        QFile file(":/library/rom-platforms.json");
        if (!file.open(QIODevice::ReadOnly)) qFatal("Missing ROM platform registry");
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        if (object["version"].toInt() != 1) qFatal("Invalid ROM platform registry");
        QList<RomPlatform> result;
        for (const auto& entry : object["platforms"].toArray()) {
            const auto p = entry.toObject();
            RomPlatform row{p["id"].toString(), p["folder"].toString(), p["name"].toString(), p["core"].toString(), p["shape"].toString(), {}, {}};
            for (const auto& e : p["extensions"].toArray()) row.extensions.append(e.toString());
            for (const auto& a : p["aliases"].toArray()) row.aliases.append(a.toString());
            result.append(row);
        }
        return result;
    }();
    return value;
}
QString romPlatformId(const QString& folder) {
    for (const auto& p : romPlatforms()) if (p.folder == folder || p.id == folder || p.aliases.contains(folder)) return p.id;
    return folder;
}
QString romCore(const QString& platform) {
    for (const auto& p : romPlatforms()) if (p.id == platform) return p.core;
    return {};
}
bool romContentSupported(const QString& platform, const QString& core, const QString& suffix) {
    for (const auto& p : romPlatforms()) if (p.id == platform) return !core.isEmpty() && p.core == core && p.extensions.contains(suffix.toLower());
    return false;
}
}
