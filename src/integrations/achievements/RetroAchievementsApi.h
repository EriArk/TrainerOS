#pragma once
#include "AchievementProvider.h"
#include "core/model/Models.h"
#include <QJsonObject>
#include <QMap>
#include <atomic>
#include <functional>
#include <memory>

namespace trainer {
struct AchievementReply { QJsonObject body; bool success = false; bool offline = false; };
using AchievementParameters = QMap<QString, QString>;
using AchievementTransport = std::function<AchievementReply(const AchievementParameters&)>;
QByteArray achievementFormBody(const AchievementParameters&);
struct AchievementAccount { QString username, token; bool valid() const; };
struct LinkedAchievementSet { AchievementSet set; AchievementSnapshot snapshot; QString contentHash; };
AchievementTransport retroAchievementsTransport(std::shared_ptr<std::atomic_bool> cancelled);
QString achievementContentHash(const AdventureRegistration&, const std::atomic_bool& cancelled);
AchievementAccount readAchievementAccount(const QString& filename);
bool writeAchievementAccount(const QString& filename, const AchievementAccount&);
QJsonObject achievementCache(const LinkedAchievementSet&);
std::optional<LinkedAchievementSet> readAchievementCache(const QJsonObject&, const AchievementContext&);
std::optional<LinkedAchievementSet> fetchAchievements(const AchievementAccount&, const AdventureRegistration&,
    const QString& world, const QString& hash, const AchievementTransport&);
}
