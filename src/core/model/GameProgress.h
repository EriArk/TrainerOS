#pragma once
#include <QDateTime>
#include <QString>
#include <optional>
#include <QSet>
#include <QHash>
#include "PartySnapshot.h"
#include "JourneySnapshot.h"

namespace trainer {
// Game-adapter reference capability, independent of whether a save exists.
// Absent means the full reference catalogue remains available.
struct PokedexGameScope {
    QString family;
    int nationalLimit=0;
    QHash<int,QStringList> forms,types;
    QHash<int,QList<int>> stats;
};
enum class ProgressAvailability { Unsupported, Checking, Available, Missing, Unreadable };
// National species flags, never individual/form ownership or manual journal data.
struct SavePokedex {
    int speciesCount = 0;
    QSet<int> seen, caught;
    QString error;
    // Verified current hatched individuals, keyed by species/form identity.
    // Missing maps mean incomplete/unavailable reads; absent keys in a complete
    // map mean known zero. These are not historical Seen/Caught form flags.
    std::optional<QHash<QString,int>> partyForms, boxForms;
};
// One observation of the ordinary in-game save, never an emulator state or an
// account achievement. External progress does not modify the personal journal.
struct GameProgress {
    std::optional<PokedexGameScope> pokedexScope;
    ProgressAvailability availability = ProgressAvailability::Unsupported;
    std::optional<int> badgeMask, caught;
    QString provider, contentRevision, saveRevision, message, badgeSet, contextRevision;
    QDateTime observedAt;
    std::optional<PartySnapshot> party;
    std::optional<SavePokedex> pokedex;
    std::optional<JourneySnapshot> journey;
};
}
