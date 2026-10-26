#pragma once
#include "StandaloneAdapter.h"

namespace trainer::dolphinlan {
QString bundleRoot();
bool supportedDisc(const QString& id);
QJsonObject netplayIdentity(const AdventureRegistration&,const StandaloneInstallation&,const std::atomic_bool&);
QString configure(ProcessCommand&,const StandaloneInstallation&,QString content,QString connection,bool host,const QJsonObject& expected);
ProcessCommand command(const AdventureRegistration&,const StandaloneInstallation&,const QJsonObject&,QString connection,bool host);
}
