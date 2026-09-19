#include "BadgeAssets.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

static void initializeBadgeAssets() { Q_INIT_RESOURCE(badge_assets); }

namespace trainer {
QVariantList BadgeAssets::entries(const QString& setId, std::optional<int> earnedMask) {
    static const auto sets = [] {
        initializeBadgeAssets();
        QFile file(":/badges/manifest.json");
        if (!file.open(QIODevice::ReadOnly)) return QJsonObject{};
        return QJsonDocument::fromJson(file.readAll()).object()["sets"].toObject();
    }();
    const auto badges = sets[setId].toObject()["badges"].toArray();
    QVariantList result;
    for (int i = 0; i < badges.size(); ++i) {
        const auto badge = badges[i].toObject();
        const bool known = earnedMask && *earnedMask >= 0 && i < 31;
        const auto image = badge["image"].toString();
        const bool hasAsset = known && QFile::exists(":/badges/" + image);
        result.append(QVariantMap{{"id", badge["id"].toString()}, {"name", badge["name"].toString()},
            {"state", !known ? "unknown" : (*earnedMask & (1 << i)) ? "earned" : "unearned"},
            {"image", hasAsset ? "qrc:/badges/" + image : QString()}});
    }
    return result;
}
}
