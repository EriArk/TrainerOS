#pragma once
#include "RetroArchAdapter.h"
#include "core/model/SaveBackup.h"

namespace trainer {
// Worker-only ordinary-save operations; no state directory/protocol required.
SaveTarget resolveRetroArchSave(const AdventureRegistration&, const RetroArchInstallation&);
QString prepareRetroArchLaunch(ProcessCommand&, const AdventureRegistration&,
    const RetroArchInstallation&, const std::atomic_bool& cancelled);
}
