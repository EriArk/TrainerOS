#pragma once
#include "RetroArchNetplay.h"
namespace trainer::retroarch {
// Source-backed separate-machine protocols; not an all-cartridge capability.
QJsonObject handheldLinkProfile(const AdventureRegistration&);
QString handheldLinkCore(const QJsonObject&);
QByteArray handheldLinkOptions(const QJsonObject&);
QString recoverHandheldReturn(const AdventureRegistration&, const RetroArchInstallation&);
QString prepareHandheldSave(ProcessCommand&, const AdventureRegistration&,
    const RetroArchInstallation&, const QString& sessionDirectory,
    const std::atomic_bool&);
}
