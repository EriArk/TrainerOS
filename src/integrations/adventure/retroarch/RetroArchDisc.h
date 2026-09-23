#pragma once
#include "RetroArchAdapter.h"

namespace trainer::retroarch {
bool discPlatform(const QString& platform);
bool verifiedDiscFirmware(const RetroArchInstallation&, const QString& platform, const std::atomic_bool&);
QString validateDiscContent(const QString& path, const std::atomic_bool&);
}
