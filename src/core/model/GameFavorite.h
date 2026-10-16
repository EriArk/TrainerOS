#pragma once
#include <QVariantMap>

namespace trainer {
inline bool gameFavorite(const QVariantMap& metadata) {
    const auto value = metadata.value("favorite").toString().trimmed();
    return value == "1" || value.compare("true", Qt::CaseInsensitive) == 0;
}
}
