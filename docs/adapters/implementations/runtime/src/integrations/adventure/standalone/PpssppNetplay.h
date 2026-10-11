#pragma once
#include "StandaloneAdapter.h"

namespace trainer::ppsspp {
struct NetplayRequest {
    QJsonObject expected;
    bool host = false, online = false;
    QString address, nickname;
};
QString configFile(const StandaloneInstallation&);
// Read-only, bounded raw UMD probe. A disc ID establishes format/identity,
// never the presence or compatibility of an in-game multiplayer mode.
QString discId(const QString& path, const std::atomic_bool&);
QJsonObject netplayIdentity(const AdventureRegistration&, const StandaloneInstallation&, const std::atomic_bool&);
QByteArray networkSettings(const NetplayRequest&);
// Called only after identity and ordinary launch preparation have passed.
QString configureNetplay(ProcessCommand&, const StandaloneInstallation&, const NetplayRequest&, const std::atomic_bool&);
}
