#include "RetroAchievementsApi.h"
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <QSet>
#include <algorithm>

namespace trainer {
namespace {
constexpr qsizetype MaxResponse = 2 * 1024 * 1024;
bool identifier(const QString& id) { static const QRegularExpression re("^[1-9][0-9]{0,9}$"); return re.match(id).hasMatch(); }
bool md5(const QString& hash) { static const QRegularExpression re("^[0-9a-f]{32}$"); return re.match(hash).hasMatch(); }
QString number(const QJsonValue& value) {
    const auto n = value.toDouble(-1);
    return n > 0 && n <= 2147483647 && n == qint64(n) ? QString::number(qint64(n)) : QString();
}
AchievementParameters query(const AchievementAccount& account, const QString& operation, const QString& game) {
    return {{"r", operation}, {"u", account.username}, {"t", account.token}, {"g", game}};
}
bool text(const QString& value, int limit) { return !value.isEmpty() && value.size() <= limit && !value.contains(QChar::Null); }
std::optional<QSet<QString>> unlockIds(const AchievementReply& reply) {
    if (!reply.success || !reply.body["UserUnlocks"].isArray()) return {};
    const auto values = reply.body["UserUnlocks"].toArray(); if (values.size() > 8192) return {};
    QSet<QString> result;
    for (const auto& value : values) { const auto id = number(value); if (id.isEmpty()) return {}; result.insert(id); }
    return result;
}
}
bool AchievementAccount::valid() const {
    static const QRegularExpression user("^[A-Za-z0-9_]{2,32}$");
    static const QRegularExpression credential("^[A-Za-z0-9]{16,128}$");
    return user.match(username).hasMatch() && credential.match(token).hasMatch();
}
AchievementAccount readAchievementAccount(const QString& filename) {
    QFile file(filename);
    if (QFileInfo(filename).isSymLink() || !file.open(QIODevice::ReadOnly) || file.size() > 4096) return {};
#ifdef Q_OS_LINUX
    if (file.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther | QFileDevice::WriteOther)) return {};
#endif
    const auto object = QJsonDocument::fromJson(file.read(4097)).object();
    AchievementAccount account{object["username"].toString(), object["token"].toString()};
    return object["version"].toInt() == 1 && account.valid() ? account : AchievementAccount{};
}
bool writeAchievementAccount(const QString& filename, const AchievementAccount& account) {
    if (!account.valid() || QFileInfo(filename).isSymLink() || !QDir().mkpath(QFileInfo(filename).absolutePath())) return false;
    QSaveFile file(filename); file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) return false;
    const auto bytes = QJsonDocument(QJsonObject{{"version", 1}, {"username", account.username}, {"token", account.token}}).toJson(QJsonDocument::Compact);
    return file.write(bytes) == bytes.size() && file.commit();
}
QByteArray achievementFormBody(const AchievementParameters& parameters) {
    QByteArray body;
    for (auto i = parameters.cbegin(); i != parameters.cend(); ++i) {
        if (!body.isEmpty()) body += '&';
        body += QUrl::toPercentEncoding(i.key()) + '=' + QUrl::toPercentEncoding(i.value());
    }
    return body;
}
AchievementTransport retroAchievementsTransport(std::shared_ptr<std::atomic_bool> cancelled) {
    return [cancelled](const AchievementParameters& query) {
        AchievementReply result; if (cancelled->load()) { result.offline = true; return result; }
        // Constructed in the calling worker thread. Credentials stay in the POST
        // body; redirects, relaxed TLS checks and URL logging are never enabled.
        QNetworkAccessManager network;
        QNetworkRequest request(QUrl("https://retroachievements.org/dorequest.php"));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        request.setHeader(QNetworkRequest::UserAgentHeader, "TrainerOS/0.1.0");
        auto* reply = network.post(request, achievementFormBody(query));
        reply->setReadBufferSize(MaxResponse + 1);
        QEventLoop loop; QTimer deadline, cancellation;
        deadline.setSingleShot(true); deadline.start(15000); cancellation.start(100);
        QByteArray bytes; bool tooLarge = false;
        QObject::connect(reply, &QIODevice::readyRead, &loop, [&] {
            bytes += reply->read(MaxResponse + 1 - bytes.size());
            if (bytes.size() > MaxResponse) { tooLarge = true; reply->abort(); }
        });
        QObject::connect(&deadline, &QTimer::timeout, reply, &QNetworkReply::abort);
        QObject::connect(&cancellation, &QTimer::timeout, reply, [reply, cancelled] { if (cancelled->load()) reply->abort(); });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec(); bytes += reply->read(MaxResponse + 1 - bytes.size());
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.offline = !tooLarge && (status == 0 || status >= 500 || reply->error() == QNetworkReply::TimeoutError
                                      || reply->error() == QNetworkReply::OperationCanceledError);
        if (reply->error() != QNetworkReply::NoError || status != 200 || tooLarge || bytes.size() > MaxResponse) return result;
        QJsonParseError error; const auto document = QJsonDocument::fromJson(bytes, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) return result;
        result.body = document.object(); result.success = result.body["Success"].isBool() && result.body["Success"].toBool();
        return result;
    };
}
QString achievementContentHash(const AdventureRegistration& record, const std::atomic_bool& cancelled) {
    const auto& adventure = record.adventure;
    const QHash<QString, QStringList> formats{{"gb", {"gb"}}, {"gbc", {"gbc"}}, {"gba", {"gba"}}, {"pokemini", {"min"}}};
    QFileInfo before(record.contentPath);
    if (adventure.collectionOnly || !formats.value(adventure.platformId).contains(before.suffix().toLower())
        || !before.isAbsolute() || !before.isFile() || before.size() <= 0 || before.size() > 64LL * 1024 * 1024) return {};
    for (const auto& extension : {"ips", "ups", "bps", "IPS", "UPS", "BPS"})
        if (QFileInfo::exists(before.dir().filePath(before.completeBaseName() + '.' + extension))) return {};
    QFile file(record.contentPath); if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Md5); qint64 total = 0;
    while (!file.atEnd() && !cancelled.load()) {
        const auto bytes = file.read(65536); if (bytes.isEmpty() || (total += bytes.size()) > before.size()) return {};
        hash.addData(bytes);
    }
    QFileInfo after(record.contentPath);
    if (cancelled.load() || file.error() != QFile::NoError || total != before.size() || after.size() != before.size()
        || after.lastModified() != before.lastModified() || after.canonicalFilePath() != before.canonicalFilePath()) return {};
    return QString::fromLatin1(hash.result().toHex());
}
std::optional<LinkedAchievementSet> fetchAchievements(const AchievementAccount& account, const AdventureRegistration& record,
        const QString& world, const QString& hash, const AchievementTransport& transport) {
    if (!account.valid() || !md5(hash)) return {};
    const AchievementParameters identify{{"r", "gameid"}, {"m", hash}};
    const auto found = transport(identify); const auto gameId = number(found.body["GameID"]);
    if (!found.success || gameId.isEmpty()) return {};
    LinkedAchievementSet result; result.contentHash = hash;
    result.set = {gameId, gameId, record.adventure.id, record.adventure.title, world, true};
    result.snapshot = {{"retroAchievements", account.username}, gameId, AchievementState::Error, {}, {}, {}};
    const auto definitions = transport(query(account, "patch", gameId));
    if (!definitions.success) { if (definitions.offline) result.snapshot.state = AchievementState::Offline; return result; }
    const auto patch = definitions.body["PatchData"].toObject();
    if (number(patch["ID"]) != gameId || !text(patch["Title"].toString(), 256) || !patch["Achievements"].isArray()) return result;
    const auto achievements = patch["Achievements"].toArray(); if (achievements.size() > 8192) return result;
    QList<AchievementDefinition> checked; QSet<QString> ids;
    for (const auto& item : achievements) {
        const auto definition = item.toObject(); if (definition["Flags"].toInt(-1) != 3) continue;
        const auto id = number(definition["ID"]), title = definition["Title"].toString(), description = definition["Description"].toString();
        if (id.isEmpty() || ids.contains(id) || !text(title, 256) || !text(description, 4096)) return result;
        ids.insert(id); checked.append({id, title, description});
    }
    auto standardQuery = query(account, "unlocks", gameId); standardQuery.insert("h", "0");
    auto hardcoreQuery = query(account, "unlocks", gameId); hardcoreQuery.insert("h", "1");
    const auto standardReply = transport(standardQuery), hardcoreReply = transport(hardcoreQuery);
    const auto standard = unlockIds(standardReply), hardcore = unlockIds(hardcoreReply);
    if (!standard || !hardcore) {
        if (standardReply.offline || hardcoreReply.offline) result.snapshot.state = AchievementState::Offline;
        return result; // Publish neither partial definitions nor invented locked totals.
    }
    result.set.title = patch["Title"].toString(); result.snapshot.definitions = checked;
    for (const auto& definition : checked) {
        const bool hard = hardcore->contains(definition.id), earned = hard || standard->contains(definition.id);
        result.snapshot.unlocks.append({definition.id, earned,
            earned ? std::optional(hard ? AchievementMode::Hardcore : AchievementMode::Standard) : std::nullopt, {}});
    }
    result.snapshot.state = AchievementState::Ready; result.snapshot.fetchedAt = QDateTime::currentDateTimeUtc(); return result;
}
QJsonObject achievementCache(const LinkedAchievementSet& value) {
    QJsonArray entries;
    for (const auto& definition : value.snapshot.definitions) {
        auto found = std::find_if(value.snapshot.unlocks.cbegin(), value.snapshot.unlocks.cend(), [&](const auto& unlock) { return unlock.achievementId == definition.id; });
        if (found == value.snapshot.unlocks.cend() || !found->unlocked) return {};
        entries.append(QJsonObject{{"id", definition.id}, {"title", definition.title}, {"description", definition.description},
            {"earned", *found->unlocked}, {"hardcore", found->mode == AchievementMode::Hardcore}});
    }
    return {{"version", 1}, {"account", value.snapshot.context.accountId}, {"game", value.set.gameId},
        {"adventure", value.set.adventureId}, {"title", value.set.title}, {"world", value.set.world}, {"hash", value.contentHash},
        {"fetched", value.snapshot.fetchedAt.toUTC().toString(Qt::ISODateWithMs)}, {"entries", entries}};
}
std::optional<LinkedAchievementSet> readAchievementCache(const QJsonObject& object, const AchievementContext& context) {
    if (context.providerId != "retroAchievements" || context.accountId.isEmpty() || object["version"].toInt() != 1
        || object["account"].toString() != context.accountId || !identifier(object["game"].toString())
        || !md5(object["hash"].toString()) || !text(object["adventure"].toString(), 256)
        || !text(object["title"].toString(), 256) || object["world"].toString().size() > 256 || !object["entries"].isArray()) return {};
    LinkedAchievementSet result; const auto id = object["game"].toString();
    result.contentHash = object["hash"].toString();
    result.set = {id, id, object["adventure"].toString(), object["title"].toString(), object["world"].toString(), true};
    result.snapshot = {context, id, AchievementState::Offline, {}, {}, QDateTime::fromString(object["fetched"].toString(), Qt::ISODateWithMs)};
    if (!result.snapshot.fetchedAt.isValid() || object["entries"].toArray().size() > 8192) return {};
    QSet<QString> seen;
    for (const auto& entry : object["entries"].toArray()) {
        const auto value = entry.toObject(); const auto key = value["id"].toString();
        if (!identifier(key) || seen.contains(key) || !text(value["title"].toString(), 256)
            || !text(value["description"].toString(), 4096) || !value["earned"].isBool() || !value["hardcore"].isBool()
            || (value["hardcore"].toBool() && !value["earned"].toBool())) return {};
        seen.insert(key); result.snapshot.definitions.append({key, value["title"].toString(), value["description"].toString()});
        result.snapshot.unlocks.append({key, value["earned"].toBool(), value["earned"].toBool()
            ? std::optional(value["hardcore"].toBool() ? AchievementMode::Hardcore : AchievementMode::Standard) : std::nullopt, {}});
    }
    return result;
}
}
