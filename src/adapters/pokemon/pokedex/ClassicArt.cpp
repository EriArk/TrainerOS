#include "ClassicArt.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUrl>

namespace trainer {
namespace {
QJsonObject objectFile(const QString& path, qint64 limit) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > limit) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
}
ClassicArt::ClassicArt(const QString& directory, QObject* parent) : QObject(parent), directory_(directory) {
    if (directory.isEmpty()) return;
    const auto document = objectFile(QDir(directory).filePath("bootstrap-index.json"), 16 * 1024 * 1024);
    if (document.isEmpty()) return;
    if (document["version"].toInt() != 1 || document["stage"].toString() != "flip-bootstrap") {
        error_ = "Artwork needs a compatible import"; return;
    }
    const auto root = QFileInfo(directory).canonicalFilePath() + '/';
    const auto rawImages = document["images"].toObject();
    if (rawImages.size() > 5000 || document["targets"].toObject().size() > 3000) {
        error_ = "Artwork import is too large"; return;
    }
    // Resolve and validate bounded derivative headers once, outside list scrolling.
    for (auto it = rawImages.begin(); it != rawImages.end(); ++it) {
        auto record = it.value().toObject();
        auto files = record["files"].toObject();
        QJsonObject urls;
        for (const auto& profile : {QString("pokedexListArt"), QString("pokedexDetailArt"), QString("speciesPickerArt")}) {
            const auto relative = files[profile].toString();
            if (!relative.startsWith("images/") || relative.contains("..") || relative.contains('\\')) continue;
            const QFileInfo info(QDir(directory).filePath(relative));
            const auto path = info.canonicalFilePath();
            if (!path.startsWith(root) || info.size() > 2 * 1024 * 1024) continue;
            QImageReader reader(path);
            const auto dimensions = reader.size();
            const QSize expected = profile == "pokedexListArt" ? QSize(96,96) : profile == "speciesPickerArt" ? QSize(72,72) : QSize(480,400);
            if (reader.format() != "png" || dimensions != expected) continue;
            urls[profile] = QUrl::fromLocalFile(path).toString();
        }
        if (urls.size() == 3) { record["urls"] = urls; images_[it.key()] = record; }
    }
    targets_ = document["targets"].toObject();
    preferences_ = objectFile(QDir(directory).filePath("choices.json"), 256 * 1024);
    countMapped();
}
void ClassicArt::countMapped() {
    mapped_ = 0;
    for (auto it = targets_.begin(); it != targets_.end(); ++it) if (!selected(it.key()).isEmpty()) ++mapped_;
}
QString ClassicArt::selected(const QString& target) const {
    const auto record = targets_[target].toObject();
    const auto preferred = preferences_[target].toString();
    if (record["candidates"].toArray().contains(preferred) && images_.contains(preferred)) return preferred;
    const auto initial = record["selected"].toString();
    return record["candidates"].toArray().contains(initial) && images_.contains(initial) ? initial : QString();
}
QString ClassicArt::coverage() const {
    if (!error_.isEmpty()) return error_;
    if (targets_.isEmpty()) return "Illustrations not installed";
    return QString("Illustrations · %1 / %2 forms").arg(mapped_).arg(targets_.size());
}
QVariantMap ClassicArt::candidate(const QString& id, const QString& profile) const {
    const auto record = images_[id].toObject();
    const auto url = record["urls"].toObject()[profile].toString();
    const auto warning = record["warnings"].toArray();
    return {{"id", id}, {"url", url}, {"available", !url.isEmpty()},
        {"sourceName", record["sourceName"].toString()}, {"source", record["source"].toString()},
        {"credit", record["creator"].toString().isEmpty() ? "Artist not recorded in this source" : record["creator"].toString()},
        {"quality", warning.isEmpty() ? "" : "Small original · detail may look soft"},
        {"review", record["confidence"].toString() == "reviewed" ? "Identity reviewed" : "Identity from source label"}};
}
QVariantMap ClassicArt::image(const QString& target, const QString& profile) const {
    auto result = candidate(selected(target), profile);
    const auto row = targets_[target].toObject();
    result["status"] = result["available"].toBool() ? "Illustration" : row.isEmpty() ? "No illustration installed"
        : row["status"].toString() == "review-required" ? "Illustration awaiting review" : "No confirmed illustration";
    return result;
}
QVariantList ClassicArt::choices(const QString& target) const {
    QVariantList result;
    for (const auto& value : targets_[target].toObject()["candidates"].toArray()) {
        const auto id = value.toString();
        if (!images_.contains(id)) continue;
        auto row = candidate(id, "pokedexDetailArt");
        row["current"] = selected(target) == id;
        result.append(row);
    }
    return result;
}
QString ClassicArt::select(const QString& target, const QString& id) {
    if (!targets_[target].toObject()["candidates"].toArray().contains(id) || !images_.contains(id))
        return "This illustration is not verified for this form.";
    auto next = preferences_; next[target] = id;
    QSaveFile file(QDir(directory_).filePath("choices.json"));
    const auto bytes = QJsonDocument(next).toJson(QJsonDocument::Compact);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return "The illustration choice couldn't be saved. Your previous choice is unchanged.";
    preferences_ = next; countMapped(); emit changed(); return {};
}
}
