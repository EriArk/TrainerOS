#pragma once
#include "RetroArchNetplay.h"
namespace trainer::retroarch {
// Source-backed separate-machine protocols; not an all-cartridge capability.
QJsonObject handheldLinkProfile(const AdventureRegistration&);
QString handheldLinkCore(const QJsonObject&);
bool handheldLinkContentCompatible(const QJsonObject&, const QString& sha256);
QByteArray handheldLinkOptions(const QJsonObject&, int playerSlot = 1, int players = 2);
struct LinkedSaveSeed { QByteArray bytes; bool existed = false; QString error; int originalSize = 0; };
LinkedSaveSeed linkedSaveSeed(const AdventureRegistration&, const RetroArchInstallation&);
QString prepareLinkedSave(ProcessCommand&, const AdventureRegistration&, const RetroArchInstallation&,
    const QString& sessionDirectory, const NetplayRequest&, const std::atomic_bool&);
QString recoverHandheldReturn(const AdventureRegistration&, const RetroArchInstallation&);
QString prepareHandheldSave(ProcessCommand&, const AdventureRegistration&,
    const RetroArchInstallation&, const QString& sessionDirectory,
    const std::atomic_bool&);
}
