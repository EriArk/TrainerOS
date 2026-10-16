#pragma once
#include "core/repository/LibraryRepository.h"
#include "integrations/achievements/AchievementProvider.h"
#include <QVariantList>

namespace trainer {
// Read-only presentation of existing owner/account-scoped caches. Independent
// of Home selection, adapters and launches; no network requests during browsing.
class PlayerStory {
public:
    PlayerStory(LibraryRepository& library, AchievementProvider& achievements)
        : library_(library), achievements_(achievements) {}
    QVariantMap summary() const;
    QVariantList history() const;
    QVariantList media() const;
private:
    LibraryRepository& library_;
    AchievementProvider& achievements_;
};
}
