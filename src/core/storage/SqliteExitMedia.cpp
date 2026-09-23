#include "SqliteExitMedia.h"
#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonDocument>
#include <QSqlQuery>

namespace trainer {
namespace {
constexpr qint64 MaximumContent = 128 * 1024 * 1024;
constexpr int MaximumImage = 512 * 1024;
QString failure() { return "The exit picture couldn't be saved. Your previous picture has been kept."; }
QString hash(const QByteArray& data) { return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()); }
bool currentSource(QSqlDatabase& db, const ExitMediaSource& source) {
    if (source.trainerId.isEmpty() || (source.domain != "pokemon" && source.domain != "multiverse")
        || source.domain != source.registration.adventure.domain) return false;
    const auto& r = source.registration;
    QSqlQuery q(db);
    q.prepare("SELECT a.revision,a.content_path,a.adapter_id,a.config,a.domain FROM adventures a,trainer_profile p WHERE a.id=? AND p.id=?");
    q.addBindValue(r.adventure.id); q.addBindValue(source.trainerId);
    return q.exec() && q.next() && q.value(0).toInt() == r.revision
        && q.value(1).toString() == r.contentPath && q.value(2).toString() == r.adventure.adapterId
        && QJsonDocument::fromJson(q.value(3).toByteArray()).object() == r.integrationConfig
        && q.value(4).toString() == source.domain;
}
QString contentHash(const QString& path, qint64 size, qint64 modified) {
    const QFileInfo before(path);
    if (!before.isFile() || before.size() != size || before.lastModified().toMSecsSinceEpoch() != modified
        || size <= 0 || size > MaximumContent) return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash digest(QCryptographicHash::Sha256);
    // All I/O runs on the store worker. Bound work to the first supported
    // cartridge routes; larger/disc builds keep an honest image placeholder.
    qint64 read = 0;
    while (!file.atEnd() && read <= MaximumContent) {
        const auto bytes = file.read(256 * 1024);
        if (bytes.isEmpty()) return {};
        digest.addData(bytes); read += bytes.size();
    }
    const QFileInfo after(path);
    if (file.error() != QFileDevice::NoError || read != size || after.size() != size
        || after.lastModified().toMSecsSinceEpoch() != modified) return {};
    return QString::fromLatin1(digest.result().toHex());
}
}
QString migrateExitMedia(QSqlDatabase& db) {
    QSqlQuery q(db);
    return q.exec("CREATE TABLE exit_media (trainer_id TEXT NOT NULL, domain TEXT NOT NULL CHECK(domain='pokemon'), adventure_id TEXT NOT NULL REFERENCES adventures(id), session_id TEXT NOT NULL UNIQUE REFERENCES play_sessions(id), registration_revision INTEGER NOT NULL, content_path TEXT NOT NULL, content_size INTEGER NOT NULL, content_modified INTEGER NOT NULL, build_sha256 TEXT NOT NULL, captured_at TEXT NOT NULL, width INTEGER NOT NULL CHECK(width BETWEEN 1 AND 960), height INTEGER NOT NULL CHECK(height BETWEEN 1 AND 540), jpeg BLOB NOT NULL CHECK(length(jpeg) BETWEEN 1 AND 524288), image_sha256 TEXT NOT NULL, PRIMARY KEY(trainer_id,domain,adventure_id))") ? QString() : failure();
}
std::optional<PreparedExitMedia> prepareExitMedia(QSqlDatabase& db, const ExitMediaSource& source) {
    if (!currentSource(db, source)) return {};
    const QFileInfo file(source.registration.contentPath);
    PreparedExitMedia result{source, {}, file.size(), file.lastModified().toMSecsSinceEpoch()};
    result.buildSha256 = contentHash(file.filePath(), result.contentSize, result.contentModified);
    if (result.buildSha256.isEmpty()) return {};
    return result;
}
QString writeExitMedia(QSqlDatabase& db, const PlaySession& session, const PreparedExitMedia& prepared, const ExitCapture& capture) {
    const auto& source = prepared.source;
    if (session.outcome != PlaySessionOutcome::Returned || session.adventureId != source.registration.adventure.id
        || capture.frame.isNull() || capture.frame.sizeInBytes() > 64 * 1024 * 1024 || !capture.capturedAt.isValid()
        || !currentSource(db, source) || contentHash(source.registration.contentPath, prepared.contentSize, prepared.contentModified) != prepared.buildSha256)
        return failure();
    const auto frame = capture.frame.scaled(960, 540, Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB32);
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
    if (!frame.save(&buffer, "JPEG", 85) || bytes.isEmpty() || bytes.size() > MaximumImage) return failure();
    QSqlQuery q(db);
    q.prepare("INSERT INTO exit_media VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(trainer_id,domain,adventure_id) DO UPDATE SET session_id=excluded.session_id, registration_revision=excluded.registration_revision, content_path=excluded.content_path, content_size=excluded.content_size, content_modified=excluded.content_modified, build_sha256=excluded.build_sha256, captured_at=excluded.captured_at, width=excluded.width, height=excluded.height, jpeg=excluded.jpeg, image_sha256=excluded.image_sha256");
    for (const auto& value : QVariantList{source.trainerId, source.domain, session.adventureId, session.id, source.registration.revision,
             source.registration.contentPath, prepared.contentSize, prepared.contentModified, prepared.buildSha256,
             capture.capturedAt.toUTC().toString(Qt::ISODateWithMs), frame.width(), frame.height(), bytes, hash(bytes)}) q.addBindValue(value);
    return q.exec() ? QString() : failure();
}
QList<ExitMedia> readExitMedia(QSqlDatabase& db, const QString& owner) {
    QList<ExitMedia> result;
    QSqlQuery q(db);
    // Exact owner and registration gates also apply after restart. Media is
    // optional: damage never prevents the library/history from opening.
    q.prepare("SELECT m.session_id,m.trainer_id,m.domain,m.adventure_id,m.registration_revision,m.build_sha256,m.captured_at,m.jpeg,m.image_sha256,m.content_path,m.content_size,m.content_modified,m.width,m.height FROM exit_media m JOIN trainer_profile p ON p.id=m.trainer_id JOIN adventures a ON a.id=m.adventure_id AND a.revision=m.registration_revision AND a.content_path=m.content_path JOIN play_sessions s ON s.id=m.session_id AND s.adventure_id=m.adventure_id AND s.outcome='returned' AND s.trainer_id=m.trainer_id WHERE m.trainer_id=? AND length(m.jpeg) BETWEEN 1 AND 524288 ORDER BY s.rowid DESC LIMIT 100"); q.addBindValue(owner);
    if (!q.exec()) return result;
    qint64 budget = 16 * 1024 * 1024;
    qint64 contentBudget = 256 * 1024 * 1024;
    while (q.next()) {
        const auto bytes = q.value(7).toByteArray();
        if (bytes.size() > budget || hash(bytes) != q.value(8).toString()) continue;
        const QFileInfo content(q.value(9).toString());
        if (!content.isFile() || content.size() != q.value(10).toLongLong() || content.lastModified().toMSecsSinceEpoch() != q.value(11).toLongLong()) continue;
        if (content.size() <= 0 || content.size() > contentBudget) continue;
        contentBudget -= content.size();
        if (contentHash(content.filePath(), content.size(), content.lastModified().toMSecsSinceEpoch()) != q.value(5).toString()) continue;
        QBuffer buffer; buffer.setData(bytes); buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "JPEG");
        const QSize expected(q.value(12).toInt(), q.value(13).toInt());
        if (reader.size() != expected || expected.width() < 1 || expected.height() < 1 || expected.width() > 960 || expected.height() > 540 || reader.read().isNull()) continue;
        const auto at = QDateTime::fromString(q.value(6).toString(), Qt::ISODateWithMs);
        if (!at.isValid()) continue;
        result.append({q.value(0).toString(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString(), q.value(4).toInt(), q.value(5).toString(), at, bytes});
        budget -= bytes.size();
    }
    return result;
}
}
