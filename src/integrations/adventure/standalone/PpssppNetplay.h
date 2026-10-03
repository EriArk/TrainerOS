#pragma once
#include "StandaloneAdapter.h"

namespace trainer::ppsspp {
struct NetplayRequest {
    QJsonObject expected;
    bool host = false, online = false;
    QString address, nickname;
};
QString configFile(const StandaloneInstallation&);
QJsonObject netplayIdentity(const AdventureRegistration&, const StandaloneInstallation&, const std::atomic_bool&);
// Called only after identity and ordinary launch preparation have passed.
QString configureNetplay(ProcessCommand&, const StandaloneInstallation&, const NetplayRequest&, const std::atomic_bool&);
}
