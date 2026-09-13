#pragma once
#include "StandaloneAdapter.h"
#include "core/model/SaveBackup.h"

namespace trainer {
bool supportsMelonDsSave(const AdventureRegistration&, const StandaloneInstallation&);
SaveTarget resolveMelonDsSave(const AdventureRegistration&, const StandaloneInstallation&);
}
