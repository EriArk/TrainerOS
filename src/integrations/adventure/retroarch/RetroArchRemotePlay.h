#pragma once
#include "RetroArchAdapter.h"

namespace trainer::retroarch {
// Host-local request. Never deserialize this from public lobby metadata.
struct RemotePlayRequest {
    QString ownerDevice; // Exact local controller identity, not a device index.
    int players=2;
    bool snesMultitap=false;
};
QByteArray remotePlayPortSettings(const RemotePlayRequest&);
QString prepareRemotePlay(ProcessCommand&,const RetroArchInstallation&,
                          const RemotePlayRequest&,const std::atomic_bool&);
}
