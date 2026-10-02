#pragma once
#include "platform/process/ProcessService.h"
#include <QVariantList>
namespace trainer::retroarch {
QString prepareAppearance(ProcessCommand&,const QString& id,const QString& baseConfig);
QVariantList appearanceActions(const QString& id);
bool changeAppearance(const QString& id,const QString& action);
}
