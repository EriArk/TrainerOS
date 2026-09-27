#pragma once
#include "core/model/PartySnapshot.h"
#include <QByteArray>

namespace trainer {
enum class Gen3Edition;
PartySnapshot readGen3Party(const QByteArray& world,const QByteArray& storage,Gen3Edition);
// Called only after full ROM identification and selection of a complete,
// checksum-valid save slot. These bytes are immutable and never written back.
PokemonRecord readEmeraldPartyMember(const QByteArray& record);
PartySnapshot readEmeraldParty(const QByteArray& world, const QByteArray& storage);
}
