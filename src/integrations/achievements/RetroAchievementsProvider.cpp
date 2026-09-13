#include "RetroAchievementsProvider.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QElapsedTimer>
#include <algorithm>

namespace trainer {
RetroAchievementsProvider::RetroAchievementsProvider(LibraryRepository& library, QString directory, AchievementTransport transport, QObject* parent)
    : AchievementProvider(parent), library_(library), directory_(std::move(directory)),
      account_(readAchievementAccount(QDir(directory_).filePath("integrations/retroachievements-account.json"))),
      transport_(transport ? std::move(transport) : retroAchievementsTransport(cancelled_)), worker_(new QObject) {
    worker_->moveToThread(&thread_); connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater); thread_.start();
    if (account_.valid()) loadCache();
}
RetroAchievementsProvider::~RetroAchievementsProvider() { cancelled_->store(true); thread_.quit(); thread_.wait(); }
QString RetroAchievementsProvider::cacheDirectory() const {
    const auto key = QString::fromLatin1(QCryptographicHash::hash(account_.username.toUtf8(), QCryptographicHash::Sha256).toHex());
    return QDir(directory_).filePath("achievements/" + key);
}
QList<AchievementSet> RetroAchievementsProvider::sets() const {
    QList<AchievementSet> result; if (!account_.valid()) return result;
    for (const auto& record : records_) result.append(record.set);
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.title.localeAwareCompare(b.title) < 0; }); return result;
}
AchievementSnapshot RetroAchievementsProvider::snapshot(const QString& id) const {
    if (account_.valid() && records_.contains(id)) return records_[id].snapshot;
    return {context(), id, account_.valid() ? AchievementState::Unsupported : AchievementState::Disconnected, {}, {}, {}};
}
void RetroAchievementsProvider::loadCache() {
    if (!account_.valid() || busy_) return;
    busy_ = true; const auto folder = cacheDirectory(); const auto identity = context();
    emit snapshotChanged({});
    QMetaObject::invokeMethod(worker_, [this, folder, identity] {
        QList<LinkedAchievementSet> values;
        const auto entries = QDir(folder).entryInfoList({"*.json"}, QDir::Files | QDir::NoSymLinks, QDir::Time);
        for (const auto& entry : entries.mid(0, 128)) {
            if (cancelled_->load()) break;
            QFile file(entry.absoluteFilePath()); if (!file.open(QIODevice::ReadOnly) || file.size() > 2 * 1024 * 1024) continue;
            const auto value = readAchievementCache(QJsonDocument::fromJson(file.read(2 * 1024 * 1024 + 1)).object(), identity);
            if (value && entry.completeBaseName() == value->set.id) values.append(*value);
        }
        QMetaObject::invokeMethod(this, [this, values] {
            busy_ = false; for (const auto& value : values) records_.insert(value.set.id, value);
            message_ = "Saved records are available offline. Play a supported Adventure to add its achievements.";
            emit snapshotChanged({});
            if (pendingRefresh_) { pendingRefresh_ = false; refreshAll(); }
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
void RetroAchievementsProvider::login(const QString& username, const QString& password) {
    if (busy_) return;
    if (username.trimmed().isEmpty() || username.size() > 32 || password.isEmpty() || password.size() > 128) {
        message_ = "Enter your account name and password."; emit snapshotChanged({}); return;
    }
    busy_ = true; message_ = "Connecting…"; emit snapshotChanged({});
    QMetaObject::invokeMethod(worker_, [this, username, password] {
        const AchievementParameters query{{"r", "login2"}, {"u", username.trimmed()}, {"p", password}};
        const auto reply = transport_(query);
        const AchievementAccount account{reply.body["User"].toString(), reply.body["Token"].toString()};
        QMetaObject::invokeMethod(this, [this, reply, account] {
            busy_ = false;
            if (!reply.success || !account.valid()) message_ = reply.offline ? "Could not reach RetroAchievements. Try again when connected."
                                                                                         : "Sign-in was not accepted. Check your account name and password.";
            else if (!writeAchievementAccount(QDir(directory_).filePath("integrations/retroachievements-account.json"), account))
                message_ = "Signed in, but the account could not be saved. Please try again.";
            else {
                account_ = account; records_.clear(); pendingRefresh_ = true; loadCache();
            }
            emit snapshotChanged({});
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
void RetroAchievementsProvider::disconnectAccount() {
    if (busy_) return;
    const auto filename = QDir(directory_).filePath("integrations/retroachievements-account.json");
    if (QFileInfo::exists(filename) && !QFile::remove(filename)) {
        message_ = "The saved sign-in could not be removed. Try again."; emit snapshotChanged({}); return;
    }
    account_ = {}; records_.clear(); pendingRefresh_ = false;
    message_ = "Signed out. Your local archive remains available."; emit snapshotChanged({});
}
void RetroAchievementsProvider::refresh(const QString& id) { sync(id); }
void RetroAchievementsProvider::refreshAll() { if (busy_) { pendingRefresh_ = true; return; } sync({}); }
void RetroAchievementsProvider::publish(LinkedAchievementSet value) {
    if (value.snapshot.context != context()) return;
    if (value.snapshot.state != AchievementState::Ready && records_.contains(value.set.id)) {
        auto cached = records_.value(value.set.id); cached.snapshot.state = value.snapshot.state; value = std::move(cached);
    }
    records_.insert(value.set.id, value); emit snapshotChanged(value.set.id);
}
void RetroAchievementsProvider::sync(const QString& setId) {
    if (!account_.valid() || busy_) return;
    QList<AdventureRegistration> candidates; QSet<QString> used;
    const auto append = [&](const QString& id) {
        if (used.contains(id) || candidates.size() >= 8) return;
        const auto record = library_.registration(id);
        if (!record || record->adventure.collectionOnly || !QStringList{"gb", "gbc", "gba", "pokemini"}.contains(record->adventure.platformId)) return;
        used.insert(id); candidates.append(*record);
    };
    if (!setId.isEmpty()) { if (records_.contains(setId)) append(records_[setId].set.adventureId); }
    else {
        for (const auto& session : library_.recentSessions()) append(session.adventureId);
        for (const auto& record : records_) append(record.set.adventureId);
    }
    if (candidates.isEmpty()) { message_ = "Play a supported Adventure to find its achievements. Saved records stay available."; emit snapshotChanged({}); return; }
    busy_ = true; message_ = "Checking recently played Adventures…";
    for (auto& record : records_) if (setId.isEmpty() || record.set.id == setId) record.snapshot.state = AchievementState::Loading;
    emit snapshotChanged({});
    const auto account = account_; const auto folder = cacheDirectory(); const auto worlds = library_.worlds();
    QMetaObject::invokeMethod(worker_, [this, candidates, account, folder, worlds] {
        int matched = 0, saved = 0; QSet<QString> processed; QElapsedTimer elapsed; elapsed.start();
        for (const auto& candidate : candidates) {
            if (cancelled_->load() || elapsed.elapsed() > 90000) break;
            const auto hash = achievementContentHash(candidate, *cancelled_); if (hash.isEmpty() || processed.contains(hash)) continue;
            processed.insert(hash); QString world;
            for (const auto& item : worlds) if (item.id == candidate.adventure.worldId) { world = item.name; break; }
            auto value = fetchAchievements(account, candidate, world, hash, transport_);
            if (!value) continue;
            ++matched;
            if (value->snapshot.state == AchievementState::Ready && QDir().mkpath(folder)) {
                const auto data = QJsonDocument(achievementCache(*value)).toJson(QJsonDocument::Compact);
                const auto path = QDir(folder).filePath(value->set.id + ".json");
                QSaveFile file(path); file.setDirectWriteFallback(false);
                if (!QFileInfo(path).isSymLink() && file.open(QIODevice::WriteOnly)
                    && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
                    && file.write(data) == data.size() && file.commit()) ++saved;
            }
            QMetaObject::invokeMethod(this, [this, value = *value] { publish(value); }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(this, [this, matched, saved] {
            busy_ = false;
            for (auto& record : records_) if (record.snapshot.state == AchievementState::Loading) record.snapshot.state = AchievementState::Offline;
            message_ = saved > 0 ? "Records refreshed and saved for offline use."
                : matched > 0 ? "Some records could not be refreshed or saved. Previous records have been kept."
                              : "No new supported set was confirmed. Check your connection or try another Adventure.";
            emit snapshotChanged({});
            if (pendingRefresh_) { pendingRefresh_ = false; refreshAll(); }
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
