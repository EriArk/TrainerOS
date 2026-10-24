#pragma once
#include "StandaloneAdapter.h"

namespace trainer::ps2lan {
QString bundleRoot();
QJsonObject netplayIdentity(const AdventureRegistration&,const std::atomic_bool&);
ProcessCommand command(const AdventureRegistration&,const QJsonObject& expected,
                       QString connection,bool host,const QStringList& inputPrefix={});
}
