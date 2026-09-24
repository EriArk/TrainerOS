#pragma once
#include <QJsonObject>
#include <QMap>
#include <QUrl>
#include <QElapsedTimer>
#include <atomic>
#include <functional>
#include <memory>

namespace trainer::scraper {
using Cancellation = std::shared_ptr<std::atomic_bool>;
struct Credentials {
    QString developerId, developerPassword, username, password;
    bool valid() const;
};
Credentials readCredentials(const QString& file);
bool writeCredentials(const QString& file, const Credentials& credentials);
int systemId(const QString& platform);
QStringList platforms();
struct Fingerprint {
    QString path, md5, sha1, crc;
    qint64 size = 0, modified = 0;
    bool unchanged() const;
    bool valid() const { return size > 0 && md5.size() == 32 && sha1.size() == 40 && crc.size() == 8; }
};
// Hashes the supplied file bytes, not unpacked archives or reconstructed discs.
// Never use this as a RetroAchievements hash without that provider's checks.
Fingerprint fingerprint(const QString& path, const Cancellation& cancel);
struct Reply { int status = 0; QByteArray bytes; int retryAfter = 0; };
using Transport = std::function<Reply(const QUrl&, qint64, const Cancellation&)>;
using Delay = std::function<bool(qint64, const Cancellation&)>;
Transport httpsTransport();
enum class Status { Ready, MissingCredentials, Cancelled, Offline, InvalidResponse, NotFound, Busy, Quota, Denied };
struct Quota {
    int threads = 0, perMinute = 0, daily = 0, today = 0, failedDaily = 0, failedToday = 0;
    bool exhausted() const;
};
struct Game {
    QString id;
    int system = 0;
    QMap<QString,QString> fields;
    QMap<QString,QUrl> media;
    bool exactFile = false;
};
struct Result {
    Status status = Status::InvalidResponse;
    QList<Game> games;
    Quota quota;
    int retryAfter = 0;
};
// Synchronous worker API. No network during construction, discovery or rendering.
// Results of search (and lookup without matching returned ROM hashes) require
// explicit choice; no silent filename-based binding or catalogue-ID replacement.
class Client {
public:
    explicit Client(Credentials credentials, Transport transport = httpsTransport(), Delay delay = {});
    Result account(const Cancellation& cancel);
    Result lookup(const QString& platform, const Fingerprint& file, const Cancellation& cancel);
    Result search(const QString& platform, const QString& title, const Cancellation& cancel);
    Reply media(const QUrl& url, bool video, const Cancellation& cancel);
private:
    QUrl url(const QString& operation, const QMap<QString,QString>& fields) const;
    Result request(const QString& operation, const QMap<QString,QString>& fields, int system, const Fingerprint*, const Cancellation&);
    Credentials credentials_;
    Transport transport_;
    Delay delay_;
    Quota quota_;
    QElapsedTimer spacing_;
    int backoffSeconds_ = 0;
    bool accountKnown_ = false;
    bool reserve(const Cancellation& cancel);
};
bool allowedUrl(const QUrl& url);
// Content-addressed media never overwrites existing unrelated user files.
// Returns an absolute local filename, or empty on invalid bytes/cancellation.
QString storeMedia(const QString& systemDirectory, const QString& gameId, const QByteArray& bytes,
                   bool video, const Cancellation& cancel);
// Atomic Batocera update. Caller must serialize this with library edits/rescans.
// Local media paths must already exist inside this system directory. Name and
// unknown XML fields are preserved; refresh explicitly replaces other fields.
QString writeGamelist(const QString& systemDirectory, const Fingerprint& file,
                      const Game& game, const QMap<QString,QString>& localMedia,
                      bool refresh, const Cancellation& cancel);
}
