#include "ExitImage.h"
#include <algorithm>
#include <cmath>

namespace trainer {
QImage frameExitImage(const QImage& capture) {
    if (capture.isNull() || capture.width() < 32 || capture.height() < 32) return capture;
    const auto image = capture.convertToFormat(QImage::Format_RGB32);
    // Full-length, paired dark margins are strong evidence of emulator bars.
    // Tolerate JPEG ringing, but keep one-sided shadows/dark scenes intact.
    const auto margin = [&](bool vertical, bool trailing) {
        const int extent = vertical ? image.width() : image.height();
        const int length = vertical ? image.height() : image.width();
        for (int offset = 0; offset <= extent / 3; ++offset) {
            const int position = trailing ? extent - offset - 1 : offset;
            int bright = 0;
            for (int along = 0; along < length; ++along) {
                const auto pixel = image.pixel(vertical ? position : along, vertical ? along : position);
                if (std::max({qRed(pixel), qGreen(pixel), qBlue(pixel)}) > 24) ++bright;
            }
            if (bright > length / 200) return offset;
        }
        return 0; // All-dark or implausibly narrow content: do not guess.
    };
    const auto paired = [&](bool vertical) {
        const int first = margin(vertical, false), last = margin(vertical, true);
        return first >= 2 && last >= 2 && std::abs(first - last) <= 2 ? std::min(first, last) : 0;
    };
    const int x = paired(true), y = paired(false);
    return x || y ? capture.copy(x, y, capture.width() - 2 * x, capture.height() - 2 * y) : capture;
}
}
