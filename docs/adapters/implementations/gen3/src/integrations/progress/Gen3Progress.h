#pragma once
#include "core/model/GameProgress.h"
#include "core/model/SaveBackup.h"
#include <QByteArray>
#include "core/model/PartyMove.h"

namespace trainer {
enum class Gen3Edition { Emerald, FireRed };
PartyMoveResult moveEmeraldPokemon(const QByteArray&,const QString&,const PartyMove&);
std::optional<Gen3Edition> gen3Edition(const QString& contentSha256);
// Pure, bounded parser. The caller verifies the full ROM fingerprint before
// choosing a layout. No title/header guessing and no writes to game data.
GameProgress readGen3Progress(const QByteArray& save, Gen3Edition edition);
SaveHealing healGen3Party(const QByteArray& save,const QString& contentHash);
SaveHealing healEmeraldParty(const QByteArray& save, const QString& contentHash);
MerchantSnapshot readEmeraldShops(const QByteArray& save, const QString& contentHash);
MerchantWrite buyEmeraldItems(const QByteArray& save, const QString& contentHash, const MerchantPurchase&);
}
