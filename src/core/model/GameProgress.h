#pragma once
#include <QDateTime>
#include <QString>
#include <optional>

namespace trainer {
enum class ProgressAvailability { Unsupported, Checking, Available, Missing, Unreadable };
// One observation of the ordinary in-game save, never an emulator state or an
// account achievement. External progress does not modify the personal journal.
struct GameProgress {
    ProgressAvailability availability = ProgressAvailability::Unsupported;
    std::optional<int> badgeMask, caught;
    QString provider, contentRevision, saveRevision, message, badgeSet;
    QDateTime observedAt;
};
}
