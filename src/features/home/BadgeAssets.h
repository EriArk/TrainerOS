#pragma once
#include <QString>
#include <QVariantList>
#include <optional>

namespace trainer {
// Presentation only. Exact-build readers supply semantic set IDs and bit order;
// catalogue/region labels never select an artwork set.
class BadgeAssets {
public:
    static QVariantList entries(const QString& setId, std::optional<int> earnedMask);
};
}
