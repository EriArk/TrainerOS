#pragma once
#include <QStringList>
#include <QSize>
#include <QRect>
namespace trainer::retroarch {
struct BezelGame { QString platform, contentPath, catalogueTitle; };
struct AutomaticBezel { QString config, source, match; QRect viewport; };
// Reads installed Bezel Project artwork on the launch worker. Never executes its
// ROM overrides. Cached output only paints margins of the full-size game viewport.
AutomaticBezel automaticBezel(const BezelGame&, const QStringList& overlayRoots,
                             const QString& cacheDirectory, QSize display, int ratio);
}
