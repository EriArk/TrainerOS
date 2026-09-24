#pragma once
#include "core/model/PartySnapshot.h"
#include <QByteArray>

namespace trainer {
// Called only after full ROM identification and selection of a complete,
// checksum-valid save slot. These bytes are immutable and never written back.
PartySnapshot readEmeraldParty(const QByteArray& world, const QByteArray& storage);
}
