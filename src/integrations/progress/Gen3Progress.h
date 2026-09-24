#pragma once
#include "core/model/GameProgress.h"
#include "core/model/SaveBackup.h"
#include <QByteArray>

namespace trainer {
enum class Gen3Edition { Emerald, FireRed };
std::optional<Gen3Edition> gen3Edition(const QString& contentSha256);
// Pure, bounded parser. The caller verifies the full ROM fingerprint before
// choosing a layout. No title/header guessing and no writes to game data.
GameProgress readGen3Progress(const QByteArray& save, Gen3Edition edition);
SaveHealing healEmeraldParty(const QByteArray& save, const QString& contentHash);
}
