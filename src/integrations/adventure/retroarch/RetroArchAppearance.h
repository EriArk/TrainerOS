#pragma once
#include "platform/process/ProcessService.h"
#include <QVariantList>
#include <QSize>
namespace trainer::retroarch {
QString prepareAppearance(ProcessCommand&,const QString& id,const QString& baseConfig,const QString& runtimeFile = {},QSize displaySize = {});
QVariantList appearanceActions(const QString& id);
bool changeAppearance(const QString& id,const QString& action);
QVariantList appearanceChoices(const QString& id,const QString& family,const QVariantMap& runtime);
bool chooseAppearance(const QString& id,const QString& action,const QVariantMap& runtime);
}
