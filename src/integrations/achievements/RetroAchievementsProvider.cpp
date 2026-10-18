#include "RetroAchievementsProvider.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <algorithm>

namespace trainer {
RetroAchievementsProvider::RetroAchievementsProvider(LibraryRepository& library, QString directory, AchievementTransport transport, QObject* parent)
    : AchievementProvider(parent), library_(library), directory_(std::move(directory)),
      account_(readAchievementAccount(QDir(directory_).filePath("integrations/retroachievements-account.json"))),
      transport_(transport ? std::move(transport) : retroAchievementsTransport(cancelled_)), worker_(new QObject) {
    worker_->moveToThread(&thread_); connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater); thread_.start();
    webAccount_=readAchievementAccount(QDir(directory_).filePath("integrations/retroachievements-web-account.json"));
    if (account_.valid()) loadCache();
}
RetroAchievementsProvider::~RetroAchievementsProvider() { cancelled_->store(true); thread_.quit(); thread_.wait(); }
void RetroAchievementsProvider::clearHistory() {
    ++historyGeneration_;history_.clear();historyBusy_=false;historyMore_=false;historyMessage_.clear();emit historyChanged();
}
QString RetroAchievementsProvider::historyStatus() const {
    if(!hasHistoryKey())return "Account-wide history uses your Web API key from retroachievements.org/settings. Add it in RA account.";
    return historyMessage_;
}
void RetroAchievementsProvider::setHistoryKey(const QString& input) {
    if(!account_.valid())return;
    const QString path=QDir(directory_).filePath("integrations/retroachievements-web-account.json");
    AchievementAccount web{account_.username,input.trimmed()};
    if(input.trimmed().isEmpty()) {
        if(QFileInfo::exists(path) && !QFile::remove(path)){message_="Could not remove the saved history key.";emit snapshotChanged({});return;}
        webAccount_={};clearHistory();message_="History key removed.";
    } else if(!web.valid() || !writeAchievementAccount(path,web))message_="The Web API key could not be saved. Check the key and try again.";
    else {webAccount_=web;clearHistory();message_="History key saved privately on this device.";}
    emit snapshotChanged({});emit historyChanged();
}
void RetroAchievementsProvider::fetchHistory(bool more) {
    if(historyBusy_ || !hasHistoryKey()) {emit historyChanged();return;}
    if(more && !historyMore_)return;
    const int offset=more?int(history_.size()):0,generation=historyGeneration_;
    const auto web=webAccount_;
    historyBusy_=true;historyMessage_="Loading account history...";emit historyChanged();
    QMetaObject::invokeMethod(worker_,[this,web,offset,generation] {
        const auto reply=transport_({{"r","account-history"},{"u",web.username},{"y",web.token},{"c","100"},{"o",QString::number(offset)}});
        const auto page=reply.success?readAchievementHistoryPage(reply.body):std::optional<QVariantList>{};
        QMetaObject::invokeMethod(this,[this,web,offset,generation,reply,page] {
            if(generation!=historyGeneration_ || web.username!=account_.username || web.token!=webAccount_.token)return;
            historyBusy_=false;
            if(!page)historyMessage_=reply.offline ? "Offline. Previously loaded history is kept." : "History could not be loaded. Check your Web API key or try again.";
            else {
                // Do not advance offsets on duplicate/moving pages: retry from the start.
                QSet<QString> ids; if(offset)for(const auto& row:history_)ids.insert(row.toMap()["id"].toString());
                bool duplicate=false;for(const auto& row:*page)if(ids.contains(row.toMap()["id"].toString()))duplicate=true;
                if(duplicate) {historyMore_=false;historyMessage_="History changed while loading. Refresh to update the list.";}
                else {
                    if(!offset)history_.clear();history_.append(*page);
                    const int total=reply.body["Total"].toInt();historyMore_=!page->isEmpty() && history_.size()<total && history_.size()<10000;
                    historyMessage_=history_.isEmpty()?"No games in this account's achievement history.":QString("%1 of %2 games").arg(history_.size()).arg(total);
                }
            }
            emit historyChanged();
        },Qt::QueuedConnection);
    },Qt::QueuedConnection);
}
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
            if(!pendingAdventure_.isEmpty()){const auto id=pendingAdventure_;const bool notify=pendingNotify_;pendingAdventure_.clear();pendingNotify_=false;refreshAdventure(id,notify);}
            else if (pendingRefresh_) { pendingRefresh_ = false; refreshAll(); }
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
                account_ = account; clearHistory(); records_.clear(); runtimeIdentities_.clear(); liveAcknowledged_.clear(); pendingRefresh_ = true; loadCache();
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
    account_ = {}; clearHistory(); webAccount_={}; QFile::remove(QDir(directory_).filePath("integrations/retroachievements-web-account.json")); records_.clear(); runtimeIdentities_.clear();liveAcknowledged_.clear(); pendingRefresh_ = false; pendingAdventure_.clear();pendingNotify_=false;
    message_ = "Signed out. Your local archive remains available."; emit snapshotChanged({});
}
bool RetroAchievementsProvider::disconnectForRemoval() {
    if(busy_)return false;
    disconnectAccount();
    return !QFileInfo::exists(QDir(directory_).filePath("integrations/retroachievements-account.json"));
}
void RetroAchievementsProvider::refresh(const QString& id) { sync(id); }
void RetroAchievementsProvider::observeRuntimeGame(const QString& id,const QString& hash,const QString& game) {
    static const QRegularExpression md5("^[0-9a-f]{32}$"),number("^[1-9][0-9]{0,9}$");
    const auto record=library_.registration(id);
    if(!account_.valid()||!record||record->removed||!md5.match(hash).hasMatch()||!number.match(game).hasMatch())return;
    QFileInfo file(record->contentPath);if(!file.isFile())return;
    runtimeIdentities_[id]={hash,game,record->revision,file.size(),file.lastModified().toMSecsSinceEpoch()};
    refreshAdventure(id);
}
void RetroAchievementsProvider::refreshAll() { if (busy_) { pendingRefresh_ = true; return; } sync({}); }
void RetroAchievementsProvider::refreshAdventure(const QString& id,bool notify) {
    if(id.isEmpty())return;
    if(busy_){pendingNotify_=(pendingAdventure_==id&&pendingNotify_)||notify;pendingAdventure_=id;return;}
    sync({},id,notify);
}
void RetroAchievementsProvider::publish(LinkedAchievementSet value,bool notify) {
    if (value.snapshot.context != context()) return;
    if (value.snapshot.state != AchievementState::Ready && records_.contains(value.set.id)) {
        auto cached = records_.value(value.set.id); cached.snapshot.state = value.snapshot.state; value = std::move(cached);
    }
    QStringList earned;
    if(notify && value.snapshot.state==AchievementState::Ready && records_.contains(value.set.id)) {
        const auto& previous=records_[value.set.id];
        if(previous.contentHash==value.contentHash && previous.snapshot.context==value.snapshot.context)
            for(const auto& unlock:value.snapshot.unlocks)if(unlock.unlocked==true) {
                if(liveAcknowledged_.contains(value.set.id+":"+unlock.achievementId))continue;
                const auto old=std::find_if(previous.snapshot.unlocks.cbegin(),previous.snapshot.unlocks.cend(),[&](const auto& u){return u.achievementId==unlock.achievementId;});
                if(old==previous.snapshot.unlocks.cend()||old->unlocked!=false)continue;
                for(const auto& definition:value.snapshot.definitions)if(definition.id==unlock.achievementId)earned.append(definition.title);
            }
    }
    records_.insert(value.set.id, value); emit snapshotChanged(value.set.id);
    if(!earned.isEmpty())emit achievementsEarned(value.set.title,earned);
}
void RetroAchievementsProvider::sync(const QString& setId,const QString& adventureId,bool notify) {
    if (!account_.valid() || busy_) return;
    QList<AdventureRegistration> candidates; QSet<QString> used;
    const auto append = [&](const QString& id) {
        if (used.contains(id) || candidates.size() >= 8) return;
        const auto record = library_.registration(id);
        if (!record || record->adventure.collectionOnly || (!runtimeIdentities_.contains(id) && !QStringList{"gb", "gbc", "gba", "pokemini"}.contains(record->adventure.platformId))) return;
        used.insert(id); candidates.append(*record);
    };
    if(!adventureId.isEmpty()) append(adventureId);
    else if (!setId.isEmpty()) { if (records_.contains(setId)) append(records_[setId].set.adventureId); }
    else {
        for (const auto& session : library_.recentSessions()) append(session.adventureId);
        for (const auto& record : records_) append(record.set.adventureId);
    }
    if (candidates.isEmpty()) { message_ = "Play a supported Adventure to find its achievements. Saved records stay available."; emit snapshotChanged({}); return; }
    busy_ = true; message_ = "Checking recently played Adventures…";
    for (auto& record : records_)
        if ((!adventureId.isEmpty() && record.set.adventureId == adventureId)
            || (adventureId.isEmpty() && (setId.isEmpty() || record.set.id == setId)))
            record.snapshot.state = AchievementState::Loading;
    emit snapshotChanged({});
    const auto account = account_; const auto folder = cacheDirectory(); const auto worlds = library_.worlds();const auto identities=runtimeIdentities_;
    QMetaObject::invokeMethod(worker_, [this, candidates, account, folder, worlds, notify,identities] {
        int matched = 0, saved = 0; QSet<QString> processed; QElapsedTimer elapsed; elapsed.start();
        for (const auto& candidate : candidates) {
            if (cancelled_->load() || elapsed.elapsed() > 90000) break;
            QString hash;
            const auto runtime=identities.constFind(candidate.adventure.id);
            if(runtime!=identities.cend()) {
                QFileInfo file(candidate.contentPath);
                if(candidate.revision!=runtime->revision||file.size()!=runtime->size||file.lastModified().toMSecsSinceEpoch()!=runtime->modified)continue;
                hash=runtime->hash;
            } else hash=achievementContentHash(candidate, *cancelled_);
            if (hash.isEmpty() || processed.contains(hash)) continue;
            processed.insert(hash); QString world;
            for (const auto& item : worlds) if (item.id == candidate.adventure.worldId) { world = item.name; break; }
            auto value = fetchAchievements(account, candidate, world, hash, transport_);
            if (!value) continue;
            if(runtime!=identities.cend() && value->set.gameId!=runtime->game)continue;
            ++matched;bool persisted=false;
            if (value->snapshot.state == AchievementState::Ready && QDir().mkpath(folder)) {
                const auto data = QJsonDocument(achievementCache(*value)).toJson(QJsonDocument::Compact);
                const auto path = QDir(folder).filePath(value->set.id + ".json");
                QSaveFile file(path); file.setDirectWriteFallback(false);
                if (!QFileInfo(path).isSymLink() && file.open(QIODevice::WriteOnly)
                    && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
                    && file.write(data) == data.size() && file.commit()) {++saved;persisted=true;}
            }
            QMetaObject::invokeMethod(this, [this, value = *value, notify, persisted] { publish(value,notify&&persisted); }, Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(this, [this, matched, saved] {
            busy_ = false;
            for (auto& record : records_) if (record.snapshot.state == AchievementState::Loading) record.snapshot.state = AchievementState::Offline;
            message_ = saved > 0 ? "Records refreshed and saved for offline use."
                : matched > 0 ? "Some records could not be refreshed or saved. Previous records have been kept."
                              : "No new supported set was confirmed. Check your connection or try another Adventure.";
            emit snapshotChanged({});
            if(!pendingAdventure_.isEmpty()){const auto id=pendingAdventure_;const bool notify=pendingNotify_;pendingAdventure_.clear();pendingNotify_=false;refreshAdventure(id,notify);}
            else if (pendingRefresh_) { pendingRefresh_ = false; refreshAll(); }
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
