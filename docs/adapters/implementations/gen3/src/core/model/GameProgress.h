#pragma once
#include <QDateTime>
#include <QString>
#include <optional>
#include <QSet>
#include "PartySnapshot.h"

namespace trainer {
enum class ProgressAvailability { Unsupported, Checking, Available, Missing, Unreadable };
// National species flags, never individual/form ownership or manual journal data.
struct SavePokedex {
    int speciesCount = 0;
    QSet<int> seen, caught;
    QString error;
};
// One observation of the ordinary in-game save, never an emulator state or an
// account achievement. External progress does not modify the personal journal.
struct GameProgress {
    ProgressAvailability availability = ProgressAvailability::Unsupported;
    std::optional<int> badgeMask, caught;
    QString provider, contentRevision, saveRevision, message, badgeSet, contextRevision;
    QDateTime observedAt;
    std::optional<PartySnapshot> party;
    std::optional<SavePokedex> pokedex;
};
}
