#pragma once
#include <QImage>

namespace trainer {
// Presentation-only framing. Keep the original capture intact in storage.
QImage frameExitImage(const QImage& capture);
}
