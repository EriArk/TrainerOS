#pragma once
#include "SpriteArt.h"
#include <QQuickImageProvider>
namespace trainer {
class SpriteImages final : public QQuickImageProvider {
public:
    explicit SpriteImages(SpriteArt& source) : QQuickImageProvider(Image,ForceAsynchronousImageLoading), source_(source) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override { return source_.requestImage(id,size,requested); }
private:
    SpriteArt& source_;
};
}
