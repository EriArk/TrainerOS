#pragma once
#include "core/model/AchievementMoment.h"
#include <QSqlDatabase>
#include <QVariantList>

namespace trainer {
QString migrateAchievementMoments(QSqlDatabase&);
QString appendAchievementMoment(QSqlDatabase&,const QString& owner,const AchievementMoment&,bool& inserted);
QVariantList readAchievementMoments(QSqlDatabase&,const QString& owner);
}
