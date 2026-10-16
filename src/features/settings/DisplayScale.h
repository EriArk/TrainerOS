#pragma once
#include <QString>
#include <algorithm>
#include <cmath>

namespace trainer {
// All geometry is in Qt device-independent pixels. Qt applies the display DPR
// when rasterizing; applying it here again would double-scale high-DPI screens.
inline double displayDensity(double screenWidth, double screenHeight,
                             double windowWidth, double windowHeight,
                             const QString& profile, int percent) {
    const auto shortEdge = std::min(screenWidth, screenHeight);
    const auto base = profile == "handheld" ? shortEdge / 540.0
                    : profile == "tv" ? shortEdge / 480.0 : 1.0;
    const auto requested = std::max(0.5, base) * std::clamp(percent, 75, 150) / 100.0;
    // Keep navigation and recovery controls reachable in small windows. The
    // available layout remains fluid; this is not a fixed-aspect render target.
    const auto fitting = std::min(windowWidth / 800.0, windowHeight / 450.0);
    return std::max(0.25, std::min(requested, fitting));
}
}
