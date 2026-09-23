#include "SpriteArt.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace trainer {
SpriteArt::SpriteArt(const QString& directory)
    : directory_(QFileInfo(directory).canonicalFilePath()) {
    if (directory.isEmpty() || directory_.isEmpty()) return;
    QFile file(QDir(directory_).filePath("sprite-index.json"));
    if (!file.open(QIODevice::ReadOnly)) return;
    if (file.size() > 12 * 1024 * 1024) { error_ = "Sprite index is too large"; return; }
    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    if (root["version"].toInt() != 1 || root["kind"] != "pmd-detail-preview"
        || !root["targets"].isObject() || !root["assets"].isObject()
        || root["targets"].toObject().size() > 3000 || root["assets"].toObject().size() > 10000) {
        error_ = "Sprites need a compatible local import"; return;
    }
    static const QRegularExpression digest("^[a-f0-9]{64}$");
    const auto assets = root["assets"].toObject();
    for (auto it = assets.begin(); it != assets.end(); ++it) {
        const auto row = it.value().toObject();
        if (!digest.match(it.key()).hasMatch() || row["file"].toString() != "images/" + it.key() + ".png"
            || row["credit"].toString().isEmpty() || row["source"].toString().isEmpty()
            || row["license"].toString().isEmpty()) continue;
        const int frames = row["frames"].toInt(1), frameWidth = row["frameWidth"].toInt(0);
        const auto durations = row["durations"].toArray();
        if (frames < 1 || frames > 32 || (frames > 1 && (frameWidth < 1 || frameWidth > 256 || durations.size() != frames))) continue;
        bool valid = true;
        for (const auto& duration : durations) if (duration.toInt() < 16 || duration.toInt() > 5000) valid = false;
        if (!valid) continue;
        assets_.insert(it.key(), row);
    }
    targets_ = root["targets"].toObject();
}
QString SpriteArt::status() const {
    if (!error_.isEmpty()) return error_;
    return targets_.isEmpty() ? "Sprites not installed" : "No confirmed sprite or portrait for this form";
}
QVariantList SpriteArt::choices(const QString& exactForm) const {
    QVariantList result;
    const auto rows = targets_[exactForm].toArray();
    if (rows.size() > 8) return result;
    for (const auto& value : rows) {
        const auto row = value.toObject(); const auto id = row["asset"].toString();
        if (!assets_.contains(id)) continue;
        auto item = assets_[id].toObject().toVariantMap();
        item["label"] = row["label"].toString();
        item["url"] = "image://sprite-detail/" + id;
        result.append(item);
    }
    return result;
}
QImage SpriteArt::requestImage(const QString& id, QSize* size, const QSize&) {
    QMutexLocker guard(&mutex_);
    QImage image;
    if (const auto* cached = cache_.object(id)) image = *cached;
    else if (assets_.contains(id)) {
        const QFileInfo info(QDir(directory_).filePath(assets_[id].toObject()["file"].toString()));
        // Recheck canonical containment on each uncached read, including symlinks.
        if (info.isFile() && info.canonicalFilePath().startsWith(directory_ + '/') && info.size() <= 1024 * 1024) {
            QFile file(info.canonicalFilePath());
            if (file.open(QIODevice::ReadOnly)) {
                const auto bytes = file.read(1024 * 1024 + 1);
                if (bytes.size() <= 1024 * 1024 && QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()) == id) {
                    QBuffer buffer; buffer.setData(bytes); buffer.open(QIODevice::ReadOnly);
                    QImageReader reader(&buffer,"png"); const auto dimensions = reader.size();
                    if (dimensions.width() > 0 && dimensions.height() > 0 && dimensions.width() <= 8192 && dimensions.height() <= 256
                        && dimensions.width() * dimensions.height() <= 262144)
                    {
                        const auto record = assets_[id].toObject(); const int frames = record["frames"].toInt(1);
                        if ((frames == 1 && dimensions.width() <= 256) ||
                            (frames > 1 && dimensions.width() == record["frameWidth"].toInt() * frames)) image = reader.read();
                    }
                }
            }
        }
        if (!image.isNull()) cache_.insert(id,new QImage(image),int(image.sizeInBytes()));
    }
    if (size) *size = image.size();
    return image;
}
}
