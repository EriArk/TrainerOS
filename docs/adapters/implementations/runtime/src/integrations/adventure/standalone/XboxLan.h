// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "StandaloneAdapter.h"
namespace trainer::xboxlan {
QString bundleRoot();
QJsonObject netplayIdentity(const AdventureRegistration&,const std::atomic_bool&);
ProcessCommand command(const AdventureRegistration&,const QJsonObject&,QString connection,bool host,const QStringList& inputPrefix={});
}
