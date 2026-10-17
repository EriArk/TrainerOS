#pragma once
#include "integrations/adventure/retroarch/RetroArchNetplay.h"
#include "integrations/adventure/standalone/StandaloneAdapter.h"
namespace trainer {
QJsonObject runtimeIdentity(const AdventureRegistration&,const RetroArchInstallation&,
    const StandaloneInstallation&,const StandaloneInstallation&,const std::atomic_bool&,int,
    retroarch::NetplayDigestCache* = nullptr);
}
