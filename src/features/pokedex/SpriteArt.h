#pragma once
#include <QImage>
#include <QJsonObject>
#include <QCache>
#include <QMutex>
#include <QVariantList>

namespace trainer {
// Read-only, optional detail assets. Never shares illustration or Adventure IDs.
// Controller projections do not perform image IO; the app image adapter owns reads.
class SpriteArt final {
public:
    explicit SpriteArt(const QString& directory);
    QVariantList choices(const QString& exactForm) const;
    QString status() const;
    QImage requestImage(const QString& id, QSize* size, const QSize& requested);
private:
    QString directory_, error_;
    QJsonObject targets_, assets_;
    QMutex mutex_;
    QCache<QString,QImage> cache_{4 * 1024 * 1024};
};
}
