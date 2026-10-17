#pragma once
#include <QDateTime>
#include <QImage>
#include <QString>

namespace trainer {
// Owned runtime observation, never inferred from an exit photo or account refresh.
struct AchievementMoment {
    QString owner, account, set, achievement, mode, adventure, session, contentHash;
    QString title, badge;
    QDateTime observedAt, capturedAt;
    QImage frame;
};
}
