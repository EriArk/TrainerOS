#pragma once
#include "RetroArchNetplay.h"

namespace trainer::ps2 {
QString bundleRoot();
RetroArchInstallation installation(const RetroArchInstallation& ordinary, const std::atomic_bool& cancel, const QString& root = bundleRoot());
QJsonObject netplayIdentity(const AdventureRegistration&, const RetroArchInstallation& ordinary,
                           const std::atomic_bool& cancel, const QString& root = bundleRoot());
ProcessCommand command(const AdventureRegistration&, const RetroArchInstallation& ordinary,
                       const retroarch::NetplayRequest&, const QString& root = bundleRoot());
}
