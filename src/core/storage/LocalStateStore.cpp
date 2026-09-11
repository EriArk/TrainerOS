#include "LocalStateStore.h"
#include "SqliteLibrary.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLockFile>
#include <QPointer>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <memory>
#include <algorithm>

namespace trainer {
namespace {
constexpr int SchemaVersion = 3;
QString failedWrite() { return "Couldn't save changes. Check free space or storage access, then try again."; }
struct LoadedState {
    QString error;
    std::optional<TrainerProfile> profile;
    QSet<QString> favorites;
    QJsonObject navigation;
    LibrarySnapshot library;
};
}
class SqliteWorker final : public QObject {
public:
    ~SqliteWorker() override { close(); }
    void close() {
        const QString name = db.connectionName();
        db.close(); db = {};
        if (!name.isEmpty()) QSqlDatabase::removeDatabase(name);
        lock.reset();
    }
    LoadedState open(const QString& directory, const QString& scope) {
        close();
        scope_ = scope;
        LoadedState state;
        const auto fail = [&](const QString& message) {
            state.error = message;
            close();
            return state;
        };
        if (!QDir().mkpath(directory)) return fail("Your data folder isn't available. Check storage access and retry.");
        const auto path = QDir(directory).filePath("traineros.sqlite3");
        lock = std::make_unique<QLockFile>(path + ".lock");
        lock->setStaleLockTime(0);
        if (!lock->tryLock()) return fail("Your data is in use or locked. Close the other TrainerOS instance and retry.");
        db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
        db.setDatabaseName(path);
        if (!db.open()) return fail("Your data couldn't be opened. Check storage access and retry.");
        QString openError;
        // Destroy every query before closing/removing its connection.
        {
            QSqlQuery query(db);
            if (!query.exec("PRAGMA busy_timeout=250") || !query.exec("PRAGMA foreign_keys=ON") || !query.exec("PRAGMA user_version") || !query.next())
                openError = "Your data couldn't be read. The existing file has been kept.";
            else {
                const int version = query.value(0).toInt();
                query.finish();
                if (version > SchemaVersion)
                    openError = "This data was made by a newer TrainerOS. Open it with that version.";
                else if (version == 0) {
                    if (!query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'") || query.next())
                        openError = "This file isn't a recognized TrainerOS store. The existing file has been kept.";
                    else {
                        query.finish();
                        if (!db.transaction()) openError = failedWrite();
                        else {
                            const QStringList migration{
                                "CREATE TABLE trainer_profile (slot INTEGER PRIMARY KEY CHECK(slot=1), id TEXT NOT NULL UNIQUE, name TEXT NOT NULL, emblem TEXT NOT NULL, favorite TEXT NOT NULL, created_at TEXT NOT NULL)",
                                "CREATE TABLE pokedex_favorites (entry_id TEXT PRIMARY KEY NOT NULL)",
                                "CREATE TABLE shell_state (scope TEXT PRIMARY KEY NOT NULL, payload BLOB NOT NULL)",
                                "PRAGMA user_version=1"};
                            for (const auto& sql : migration) if (!query.exec(sql)) { openError = failedWrite(); break; }
                            if (openError.isEmpty() && !db.commit()) openError = failedWrite();
                            if (!openError.isEmpty()) db.rollback();
                        }
                    }
                } else if (version < 0) openError = "This data version isn't supported. The existing file has been kept.";
                if (openError.isEmpty() && version < 2) {
                    if (!db.transaction()) openError = failedWrite();
                    else {
                        openError = migrateLibrary(db);
                        if (openError.isEmpty() && !query.exec("PRAGMA user_version=2")) openError = failedWrite();
                        if (openError.isEmpty() && !db.commit()) openError = failedWrite();
                        if (!openError.isEmpty()) db.rollback();
                    }
                }
                if (openError.isEmpty() && version < 3) {
                    if (!db.transaction()) openError = failedWrite();
                    else {
                        for (const auto& sql : QStringList{
                            "ALTER TABLE adventures ADD COLUMN platform_id TEXT NOT NULL DEFAULT ''",
                            "ALTER TABLE adventures ADD COLUMN catalogue_id TEXT NOT NULL DEFAULT ''",
                            "ALTER TABLE adventures ADD COLUMN variant TEXT NOT NULL DEFAULT ''",
                            "CREATE INDEX adventures_catalogue ON adventures(catalogue_id)",
                            "PRAGMA user_version=3"})
                            if (!query.exec(sql)) { openError = failedWrite(); break; }
                        if (openError.isEmpty() && !db.commit()) openError = failedWrite();
                        if (!openError.isEmpty()) db.rollback();
                    }
                }
            }
            if (openError.isEmpty() && (!query.exec("PRAGMA quick_check") || !query.next() || query.value(0).toString() != "ok"))
                openError = "Your data needs recovery. The existing file has been kept.";
            if (openError.isEmpty() && !query.exec("PRAGMA synchronous=FULL")) openError = failedWrite();
            if (openError.isEmpty()) {
                if (!query.exec("SELECT id,name,emblem,favorite,created_at FROM trainer_profile WHERE slot=1")) openError = "Your Trainer data couldn't be read. The existing file has been kept.";
                else if (query.next()) {
                    TrainerProfile profile;
                    profile.id = query.value(0).toString(); profile.name = query.value(1).toString();
                    profile.emblemId = query.value(2).toString(); profile.favoritePokemonId = query.value(3).toString();
                    profile.createdAt = QDateTime::fromString(query.value(4).toString(), Qt::ISODateWithMs);
                    if (profile.id.isEmpty() || profile.name.trimmed().isEmpty() || !profile.createdAt.isValid())
                        openError = "Your Trainer data needs recovery. The existing file has been kept.";
                    else state.profile = profile;
                }
            }
            if (openError.isEmpty()) {
                if (!query.exec("SELECT entry_id FROM pokedex_favorites")) openError = "Your favorites couldn't be read. The existing file has been kept.";
                else while (query.next()) state.favorites.insert(query.value(0).toString());
            }
            if (openError.isEmpty()) {
                query.prepare("SELECT payload FROM shell_state WHERE scope=?"); query.addBindValue(scope_);
                if (!query.exec()) openError = "Your browsing state couldn't be read. The existing file has been kept.";
                else if (query.next()) {
                    // Invalid optional navigation can fall back without discarding profile/favorites.
                    state.navigation = QJsonDocument::fromJson(query.value(0).toByteArray()).object();
                    if (state.navigation["version"].toInt() > 1)
                        openError = "This browsing state needs a newer TrainerOS. The existing file has been kept.";
                }
            }
        }
        if (openError.isEmpty()) {
            state.library = readLibrary(db);
            openError = state.library.error;
        }
        if (!openError.isEmpty()) return fail(openError);
        return state;
    }
    QString profile(const TrainerProfile& profile) {
        if (!db.transaction()) return failedWrite();
        QString error;
        {
            QSqlQuery query(db);
            if (!query.exec("SELECT id,created_at FROM trainer_profile WHERE slot=1")) error = failedWrite();
            else if (query.next() && (query.value(0).toString() != profile.id
                     || QDateTime::fromString(query.value(1).toString(), Qt::ISODateWithMs) != profile.createdAt))
                error = "This Trainer identity has changed. Reopen TrainerOS before editing.";
            if (error.isEmpty()) {
                query.prepare("INSERT INTO trainer_profile VALUES(1,?,?,?,?,?) ON CONFLICT(slot) DO UPDATE SET name=excluded.name,emblem=excluded.emblem,favorite=excluded.favorite");
                query.addBindValue(profile.id); query.addBindValue(profile.name); query.addBindValue(profile.emblemId);
                query.addBindValue(profile.favoritePokemonId.isNull() ? QString("") : profile.favoritePokemonId);
                query.addBindValue(profile.createdAt.toUTC().toString(Qt::ISODateWithMs));
                if (!query.exec()) error = failedWrite();
            }
        }
        if (error.isEmpty() && !db.commit()) error = failedWrite();
        if (!error.isEmpty()) db.rollback();
        return error;
    }
    QString favorite(const QString& id, bool favorite) {
        QSqlQuery query(db);
        query.prepare(favorite ? "INSERT OR IGNORE INTO pokedex_favorites(entry_id) VALUES(?)" : "DELETE FROM pokedex_favorites WHERE entry_id=?");
        query.addBindValue(id);
        return query.exec() ? QString() : failedWrite();
    }
    QString navigation(const QJsonObject& state) {
        QSqlQuery query(db);
        query.prepare("INSERT INTO shell_state(scope,payload) VALUES(?,?) ON CONFLICT(scope) DO UPDATE SET payload=excluded.payload");
        query.addBindValue(scope_); query.addBindValue(QJsonDocument(state).toJson(QJsonDocument::Compact));
        return query.exec() ? QString() : failedWrite();
    }
    LibraryWriteResult adventure(const AdventureRegistration& record) { return writeAdventure(db, record); }
    QString preferences(const ShellPreferences& value) { return writePreferences(db, value); }
private:
    QSqlDatabase db;
    QString scope_;
    std::unique_ptr<QLockFile> lock;
};

LocalStateStore::LocalStateStore(QString directory, QObject* parent, QString scope)
    : QObject(parent), worker_(new SqliteWorker), directory_(std::move(directory)), scope_(std::move(scope)) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    thread_.start();
}
LocalStateStore::~LocalStateStore() {
    // The application waits asynchronously for pending()==0 before normal exit.
    thread_.quit(); thread_.wait();
}
void LocalStateStore::open() {
    if (opening_ || ready_ || pending_) return;
    opening_ = true; error_.clear();
    QMetaObject::invokeMethod(worker_, [this] {
        auto state = worker_->open(directory_, scope_);
        QMetaObject::invokeMethod(this, [this, state = std::move(state)] {
            opening_ = false; error_ = state.error; ready_ = error_.isEmpty();
            if (ready_) {
                profile_ = state.profile; favorites_ = state.favorites; navigation_ = state.navigation;
                worlds_ = state.library.worlds; registrations_ = state.library.registrations; preferences_ = state.library.preferences;
            }
            emit opened(ready_);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
void LocalStateStore::write(std::function<QString(SqliteWorker&)> operation, std::function<void(QString)> completed) {
    if (!ready_) { completed("Your data isn't open yet. Retry after reopening TrainerOS."); return; }
    ++pending_; emit pendingChanged();
    QMetaObject::invokeMethod(worker_, [this, operation = std::move(operation), completed = std::move(completed)] {
        const auto error = operation(*worker_);
        QMetaObject::invokeMethod(this, [this, error, completed] {
            completed(error);
            --pending_; emit pendingChanged();
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
void LocalStateStore::saveAsync(const TrainerProfile& profile, QObject* context, std::function<void(ProfileWriteResult)> completed) {
    if (profile.id.isEmpty() || profile.name.trimmed().isEmpty() || !profile.createdAt.isValid()) {
        completed({false, "Your Trainer needs a valid identity and name."}); return;
    }
    write([profile](SqliteWorker& worker) { return worker.profile(profile); },
          [this, profile, guard = QPointer<QObject>(context), completed](const QString& error) {
        if (error.isEmpty()) profile_ = profile;
        else emit userWriteFailed();
        if (guard) completed({error.isEmpty(), error});
    });
}
void LocalStateStore::setFavoriteAsync(const QString& id, bool favorite, QObject* context, std::function<void(QString)> completed) {
    if (id.isEmpty()) { completed("Choose a Pokédex entry first."); return; }
    write([id, favorite](SqliteWorker& worker) { return worker.favorite(id, favorite); },
          [this, id, favorite, guard = QPointer<QObject>(context), completed](const QString& error) {
        if (error.isEmpty()) { if (favorite) favorites_.insert(id); else favorites_.remove(id); }
        else emit userWriteFailed();
        if (guard) completed(error);
    });
}
void LocalStateStore::saveNavigation(const QJsonObject& state, QObject* context, std::function<void(QString)> completed) {
    write([state](SqliteWorker& worker) { return worker.navigation(state); },
          [this, state, guard = QPointer<QObject>(context), completed](const QString& error) {
        if (error.isEmpty()) navigation_ = state;
        if (guard) completed(error);
    });
}
QList<Adventure> LocalStateStore::adventures() const {
    QList<Adventure> result;
    for (const auto& record : registrations_) result.append(record.adventure);
    return result;
}
std::optional<AdventureRegistration> LocalStateStore::registration(const QString& id) const {
    for (const auto& record : registrations_) if (record.adventure.id == id) return record;
    return {};
}
void LocalStateStore::saveAdventureAsync(const AdventureRegistration& candidate, QObject* context,
                                       std::function<void(LibraryWriteResult)> completed) {
    auto record = candidate;
    if (record.newWorld) record.newWorld->name = record.newWorld->name.trimmed();
    record.adventure.title = record.adventure.title.trimmed(); record.adventure.description = record.adventure.description.trimmed();
    record.adventure.additionalWorldIds.removeAll(record.adventure.worldId); record.adventure.additionalWorldIds.removeDuplicates();
    auto result = std::make_shared<LibraryWriteResult>();
    write([record, result](SqliteWorker& worker) { *result = worker.adventure(record); return result->error; },
          [this, record, result, guard = QPointer<QObject>(context), completed](const QString& error) mutable {
        if (error.isEmpty()) {
            if (record.newWorld) worlds_.append(*record.newWorld);
            for (const auto& world : record.additionalNewWorlds)
                if (std::none_of(worlds_.begin(), worlds_.end(), [&](const auto& w) { return w.id == world.id; })) worlds_.append(world);
            record.additionalNewWorlds.clear();
            record.newWorld.reset(); record.revision = result->revision;
            bool replaced = false;
            for (auto& existing : registrations_) if (existing.adventure.id == record.adventure.id) { existing = record; replaced = true; break; }
            if (!replaced) registrations_.append(record);
            std::sort(registrations_.begin(), registrations_.end(), [](const auto& a, const auto& b) {
                const int compare = QString::compare(a.adventure.title, b.adventure.title, Qt::CaseInsensitive);
                return compare == 0 ? a.adventure.id < b.adventure.id : compare < 0;
            });
            emit libraryChanged();
        } else emit userWriteFailed();
        if (guard) completed({error.isEmpty(), error, result->revision});
    });
}
void LocalStateStore::savePreferences(const ShellPreferences& value, QObject* context, std::function<void(QString)> completed) {
    write([value](SqliteWorker& worker) { return worker.preferences(value); },
          [this, value, guard = QPointer<QObject>(context), completed](const QString& error) {
        if (error.isEmpty()) { preferences_ = value; emit preferencesChanged(); }
        else emit userWriteFailed();
        if (guard) completed(error);
    });
}
}
