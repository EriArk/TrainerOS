#pragma once
#include <QByteArray>
#include <QString>
#include <functional>

namespace trainer {
struct BoxNameChange { int box=0; QString name,saveRevision; };
struct BoxNameResult { QByteArray data; QString error,message; };
using BoxNameWriter = std::function<BoxNameResult(const QByteArray&,const QString&,const BoxNameChange&)>;
// -1 denotes Party; nonnegative boxes and all positions are zero-based.
struct PokemonPosition { int box = -1, slot = 0; };
// itemId 0 returns the held item to the Bag; otherwise give/swap exactly one.
struct HeldItemChange { PokemonPosition pokemon; int itemId=0; QString saveRevision; };
struct HeldItemResult { QByteArray data; QString error,message; };
using HeldItemWriter = std::function<HeldItemResult(const QByteArray&,const QString&,const HeldItemChange&)>;
struct PartyMove {
    PokemonPosition from, to;
    QString saveRevision; // SHA-256 of the observation shown to the user.
    bool exchangeOccupied = false; // Explicit two-member confirmation, never implicit replacement.
};
struct PokemonRelease { PokemonPosition from; QString saveRevision; };
struct PokemonReleaseResult { QByteArray data; QString error, message; };
using PokemonReleaser = std::function<PokemonReleaseResult(const QByteArray&, const QString&, const PokemonRelease&)>;
struct PartyMoveResult { QByteArray data; QString error, message; };
using PartyMover = std::function<PartyMoveResult(const QByteArray&, const QString&, const PartyMove&)>;
}
