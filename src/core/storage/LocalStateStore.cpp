#include "LocalStateStore.h"
#include "SqliteLibrary.h"
#include "SqliteExitMedia.h"
#include "SqliteOwnership.h"
#include <QDir>
#include <QTimer>
#include <QUuid>
#include <QCryptographicHash>
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
constexpr int SchemaVersion = 10;
QString failedWrite() { return "Couldn't save changes. Check free space or storage access, then try again."; }
struct LoadedState {
    QString error;
    QString ownerId, accountOwner;
    bool gated=false, familyProtected=false;
    QSet<QString> protectedIds;
    QList<TrainerProfile> profiles;
    std::optional<TrainerProfile> profile;
    QSet<QString> favorites;
    QJsonObject navigation;
    LibrarySnapshot library;
    PlayHistorySnapshot history;
    QList<ExitMedia> exitMedia;
    QList<HallOfFameEntry> archive;
    QHash<QString,PokedexProgress> journal;
};
}
class SqliteWorker final : public QObject {
public:
    ~SqliteWorker() override { close(); }
    void close() {
        pendingMedia.clear();verified_.clear();
        const QString name = db.connectionName();
        db.close(); db = {};
        if (!name.isEmpty()) QSqlDatabase::removeDatabase(name);
        lock.reset();
    }
    LoadedState open(const QString& directory, const QString& scope, bool enforce=false, const QString& grant={}, bool choose=false) {
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
            if (openError.isEmpty() && (!query.exec("PRAGMA user_version") || !query.next())) openError = failedWrite();
            if (openError.isEmpty() && query.value(0).toInt() < 4) {
                query.finish();
                if (!db.transaction()) openError = failedWrite();
                else {
                    openError = migratePlayHistory(db);
                    if (openError.isEmpty() && !query.exec("PRAGMA user_version=4")) openError = failedWrite();
                    if (openError.isEmpty() && !db.commit()) openError = failedWrite();
                    if (!openError.isEmpty()) db.rollback();
                }
            }
            if (openError.isEmpty() && (!query.exec("PRAGMA user_version") || !query.next())) openError = failedWrite();
            if (openError.isEmpty() && query.value(0).toInt() < 5) {
                query.finish();
                if (!db.transaction()) openError = failedWrite();
                else {
                    openError = migrateHallOfFame(db);
                    if (openError.isEmpty() && !query.exec("PRAGMA user_version=5")) openError = failedWrite();
                    if (openError.isEmpty() && !db.commit()) openError = failedWrite();
                    if (!openError.isEmpty()) db.rollback();
                }
            }
            if (openError.isEmpty() && (!query.exec("PRAGMA user_version") || !query.next())) openError = failedWrite();
            if (openError.isEmpty() && query.value(0).toInt() < 6) {
                query.finish();
                if (!db.transaction()) openError = failedWrite();
                else {
                    openError = migratePokedexJournal(db);
                    if (openError.isEmpty() && !query.exec("PRAGMA user_version=6")) openError = failedWrite();
                    if (openError.isEmpty() && !db.commit()) openError = failedWrite();
                    if (!openError.isEmpty()) db.rollback();
                }
            }
            if (openError.isEmpty() && (!query.exec("PRAGMA user_version") || !query.next())) openError = failedWrite();
            if (openError.isEmpty() && query.value(0).toInt() < 7) {
                query.finish();
                if (!db.transaction()) openError = failedWrite();
                else {
                    openError = migrateExitMedia(db);
                    if (openError.isEmpty() && !query.exec("PRAGMA user_version=7")) openError = failedWrite();
                    if (openError.isEmpty() && !db.commit()) openError = failedWrite();
                    if (!openError.isEmpty()) db.rollback();
                }
            }
            if (openError.isEmpty() && (!query.exec("PRAGMA user_version") || !query.next())) openError = failedWrite();
            if (openError.isEmpty() && query.value(0).toInt() < 8) {
                query.finish();
                if (!db.transaction()) openError = failedWrite();
                else {
                    openError = migrateOwnership(db);
                    if (openError.isEmpty() && !query.exec("PRAGMA user_version=8")) openError = failedWrite();
                    if (openError.isEmpty() && !db.commit()) openError = failedWrite();
                    if (!openError.isEmpty()) db.rollback();
                }
            }
            if (openError.isEmpty() && (!query.exec("PRAGMA user_version") || !query.next())) openError = failedWrite();
            if (openError.isEmpty() && query.value(0).toInt() < 9) {
                query.finish();
                if (!db.transaction()) openError = failedWrite();
                else {
                    openError = migrateProfiles(db);
                    if (openError.isEmpty() && !query.exec("PRAGMA user_version=9")) openError = failedWrite();
                    if (openError.isEmpty() && !db.commit()) openError = failedWrite();
                    if (!openError.isEmpty()) db.rollback();
                }
            }
            if (openError.isEmpty() && (!query.exec("PRAGMA user_version") || !query.next())) openError = failedWrite();
            if (openError.isEmpty() && query.value(0).toInt() < 10) {
                query.finish();
                if (!db.transaction()) openError = failedWrite();
                else {
                    openError = migrateTrainerPins(db);
                    if (openError.isEmpty() && !query.exec("PRAGMA user_version=10")) openError = failedWrite();
                    if (openError.isEmpty() && !db.commit()) openError = failedWrite();
                    if (!openError.isEmpty()) db.rollback();
                }
            }
            if (openError.isEmpty()) {
                ownerId_ = localOwner(db);
                if (ownerId_.isEmpty()) openError = "Your Trainer ownership needs recovery. Existing data has been kept.";
                state.ownerId = ownerId_;
            }
            if (openError.isEmpty() && (!query.exec("PRAGMA quick_check") || !query.next() || query.value(0).toString() != "ok"))
                openError = "Your data needs recovery. The existing file has been kept.";
            if (openError.isEmpty() && !query.exec("PRAGMA synchronous=FULL")) openError = failedWrite();
            if (openError.isEmpty()) {
                if (!query.exec("SELECT id,name,emblem,favorite,created_at FROM trainer_profile ORDER BY created_at,id")) openError = "Your Trainer data couldn't be read. The existing file has been kept.";
                else while (query.next()) {
                    TrainerProfile profile;
                    profile.id = query.value(0).toString(); profile.name = query.value(1).toString();
                    profile.emblemId = query.value(2).toString(); profile.favoritePokemonId = query.value(3).toString();
                    profile.createdAt = QDateTime::fromString(query.value(4).toString(), Qt::ISODateWithMs);
                    if (profile.id.isEmpty() || profile.name.trimmed().isEmpty() || !profile.createdAt.isValid())
                        openError = "Your Trainer data needs recovery. The existing file has been kept.";
                    else { state.profiles.append(profile); if(profile.id==ownerId_)state.profile=profile; }
                }
            }
            if (openError.isEmpty()) {
                if (!state.profiles.isEmpty() && !state.profile) openError = "The selected Trainer is unavailable. Existing records have been kept.";
                if (!query.exec("SELECT trainer_id FROM legacy_account_owner WHERE slot=1") || !query.next()) openError = failedWrite();
                else state.accountOwner = query.value(0).toString();
            }
            if(openError.isEmpty()) {
                if(!query.exec("SELECT p.id,a.version,length(a.salt),length(a.verifier) FROM trainer_profile p LEFT JOIN trainer_access a ON a.trainer_id=p.id"))openError=failedWrite();
                else while(query.next()) {
                    const int v=query.value(1).toInt();
                    if(query.value(1).isNull() || (v!=0 && v!=1) || query.value(2).toInt()!=(v?16:0) || query.value(3).toInt()!=(v?32:0))openError="Trainer PIN access needs recovery. Your records have been kept.";
                    if(v)state.protectedIds.insert(query.value(0).toString());
                }
                if(!query.exec("SELECT version,length(salt),length(verifier) FROM family_access WHERE trainer_id='family'") || !query.next())openError=failedWrite();
                else {
                    const int v=query.value(0).toInt();
                    if((v!=0 && v!=1) || query.value(1).toInt()!=(v?16:0) || query.value(2).toInt()!=(v?32:0))openError=failedWrite();
                    state.familyProtected=v==1;
                }
                if(openError.isEmpty() && enforce && grant!=ownerId_
                    && (choose || state.profiles.size()!=1 || state.protectedIds.contains(ownerId_))) {
                    state.gated=true;state.profile.reset();return state;
                }
                if(openError.isEmpty())verified_.insert(ownerId_);
            }
            if (openError.isEmpty()) {
                query.prepare("SELECT entry_id FROM pokedex_favorites WHERE trainer_id=?"); query.addBindValue(ownerId_);
                if (!query.exec()) openError = "Your favorites couldn't be read. The existing file has been kept.";
                else while (query.next()) state.favorites.insert(query.value(0).toString());
            }
            if (openError.isEmpty()) {
                query.prepare("SELECT payload FROM shell_state WHERE scope=? AND trainer_id=?"); query.addBindValue(scope_); query.addBindValue(ownerId_);
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
        openError = interruptOpenSessions(db);
        if (openError.isEmpty()) { state.history = readPlayHistory(db, ownerId_); openError = state.history.error; }
        if (!openError.isEmpty()) return fail(openError);
        const auto archive = readHallOfFame(db, ownerId_);
        if (!archive.success) return fail(archive.error);
        state.archive = archive.entries;
        const auto journal = readPokedexJournal(db, ownerId_);
        if (!journal.error.isEmpty()) return fail(journal.error);
        state.journal = journal.records;
        state.exitMedia = readExitMedia(db, ownerId_);
        return state;
    }
    ArchiveWriteResult memory(const HallOfFameEntry& entry, ArchiveResult& snapshot) {
        return writeHallOfFame(db, ownerId_, entry, snapshot);
    }
    PokedexWriteResult journal(const QString& id, const PokedexProgress& record, PokedexJournalSnapshot& snapshot) {
        return writePokedexRecord(db, ownerId_, id, record, snapshot);
    }
    QString profile(const TrainerProfile& profile, const SecretPin& pin = {}) {
        if(const auto e=requireFamilyCode(pin);!e.isEmpty())return e;
        PinRecord record;auto pinError=makePinRecord(pin,record);if(!pinError.isEmpty())return pinError;
        if (!db.transaction()) return failedWrite();
        QString error;
        bool creating = false;
        {
            QSqlQuery query(db);
            query.prepare("SELECT id,created_at FROM trainer_profile WHERE id=?");query.addBindValue(ownerId_);
            if (!query.exec()) error = failedWrite();
            else if (query.next()) {
                if (query.value(0).toString() != profile.id || profile.id != ownerId_
                    || QDateTime::fromString(query.value(1).toString(), Qt::ISODateWithMs) != profile.createdAt)
                    error = "This Trainer identity has changed. Reopen TrainerOS before editing.";
            } else creating = true;
            query.finish();
            if (error.isEmpty() && creating) error = adoptInitialOwner(db, ownerId_, profile.id);
            if (error.isEmpty()) {
                query.prepare("INSERT INTO trainer_profile VALUES(?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET name=excluded.name,emblem=excluded.emblem,favorite=excluded.favorite");
                query.addBindValue(profile.id); query.addBindValue(profile.name); query.addBindValue(profile.emblemId);
                query.addBindValue(profile.favoritePokemonId.isNull() ? QString("") : profile.favoritePokemonId);
                query.addBindValue(profile.createdAt.toUTC().toString(Qt::ISODateWithMs));
                if (!query.exec()) error = failedWrite();
            }
        }
        if(error.isEmpty() && creating)error=writePinRecord(db,profile.id,record);
        if (error.isEmpty() && !db.commit()) error = failedWrite();
        if (!error.isEmpty()) db.rollback();
        else if (creating) { ownerId_ = profile.id;verified_.insert(profile.id); }
        return error;
    }
    QString createTrainer(const TrainerProfile& profile, const SecretPin& pin = {}) {
        if(const auto e=requireFamilyCode(pin);!e.isEmpty())return e;
        PinRecord record;auto pinError=makePinRecord(pin,record);if(!pinError.isEmpty())return pinError;
        if(!db.transaction())return failedWrite();
        QString error;
        QSqlQuery q(db);
        if(!q.exec("SELECT COUNT(*) FROM trainer_profile") || !q.next() || q.value(0).toInt()>=8) error="Up to eight Trainers can share this handheld.";
        q.finish();
        if(error.isEmpty()) {
            q.prepare("INSERT INTO trainer_owners(id) VALUES(?)");q.addBindValue(profile.id);
            if(!q.exec())error=failedWrite();
        }
        if(error.isEmpty()) {
            q.prepare("INSERT INTO trainer_profile VALUES(?,?,?,?,?)");
            for(const auto& value:QStringList{profile.id,profile.name,profile.emblemId,profile.favoritePokemonId.isNull()?QString(""):profile.favoritePokemonId,profile.createdAt.toUTC().toString(Qt::ISODateWithMs)})q.addBindValue(value);
            if(!q.exec())error=failedWrite();
        }
        q.finish();
        if(error.isEmpty())error=writePinRecord(db,profile.id,record);
        if(error.isEmpty()&&!db.commit())error=failedWrite();
        if(!error.isEmpty())db.rollback();else verified_.insert(profile.id);
        return error;
    }
    QString authenticate(const QString& id, const SecretPin& pin, bool family) {
        auto result=verifyTrainerPin(db,family?QString("family"):id,pin,QDateTime::currentSecsSinceEpoch(),family);
        if(result.success() && !family)verified_.insert(id);
        return result.error;
    }
    QString changePin(const QString& id,const SecretPin& old,const SecretPin& next,bool family) {
        if(!family)if(const auto e=requireFamilyCode(next);!e.isEmpty())return e;
        auto error=authenticate(id,old,family);
        if(!error.isEmpty())return error;
        if(family && (!next || next->size()!=6))return "Use six digits for the family code.";
        PinRecord record;error=makePinRecord(next,record);
        if(!error.isEmpty())return error;
        return writePinRecord(db,family?QString("family"):id,record,family);
    }
    QString resetPin(const QString& id,const SecretPin& pin) {
        QSqlQuery q(db);
        if(!q.exec("SELECT version FROM family_access WHERE trainer_id='family'") || !q.next() || q.value(0).toInt()!=1)
            return "Ask a parent to set a family code in Settings first.";
        q.finish();
        auto error=authenticate(id,pin,true);
        if(error.isEmpty())error=writePinRecord(db,id,{});
        if(error.isEmpty())verified_.insert(id);
        return error;
    }
    QString stageTrainer(const QString& id) {
        if(!verified_.contains(id)) {
            QSqlQuery access(db);access.prepare("SELECT version FROM trainer_access WHERE trainer_id=?");access.addBindValue(id);
            if(!access.exec() || !access.next() || access.value(0).toInt()!=0)return "Enter this Trainer's PIN first.";
        }
        QSqlQuery q(db);
        q.prepare("UPDATE local_owner SET trainer_id=? WHERE slot=1 AND trainer_id=? AND EXISTS(SELECT 1 FROM trainer_profile WHERE id=?)");
        q.addBindValue(id);q.addBindValue(ownerId_);q.addBindValue(id);
        return q.exec()&&q.numRowsAffected()==1 ? QString() : "This Trainer is unavailable. Choose again.";
    }
    QString removeCurrentTrainer(const SecretPin& familyCode) {
        QSqlQuery q(db);
        if(!q.exec("SELECT version FROM family_access WHERE trainer_id='family'") || !q.next())return failedWrite();
        const int familyVersion=q.value(0).toInt();q.finish();
        if(familyVersion!=0 && familyVersion!=1)return failedWrite();
        if(familyVersion==1)if(const auto e=authenticate(ownerId_,familyCode,true);!e.isEmpty())return e;
        if(!q.exec("SELECT id FROM play_sessions WHERE outcome='running' LIMIT 1") || q.next())return "Close the current game first.";
        q.finish();
        if(!db.transaction())return failedWrite();
        QString error;
        q.prepare("SELECT id FROM trainer_profile WHERE id=?");q.addBindValue(ownerId_);
        if(!q.exec() || !q.next())error="This Trainer is no longer available.";
        q.finish();
        // All personal rows disappear together. Shared library and save files
        // never participate. Retired owner IDs remain reserved, including the
        // legacy root-account association, so a new Trainer cannot inherit it.
        for(const auto& table:QStringList{"exit_media","hall_of_fame","play_sessions","pokedex_records","pokedex_favorites","shell_state","trainer_access"}) {
            if(!error.isEmpty())break;
            q.prepare("DELETE FROM "+table+" WHERE trainer_id=?");q.addBindValue(ownerId_);
            if(!q.exec())error=failedWrite();
        }
        if(error.isEmpty()) {
            q.prepare("DELETE FROM trainer_profile WHERE id=?");q.addBindValue(ownerId_);
            if(!q.exec() || q.numRowsAffected()!=1)error=failedWrite();
        }
        QString next;
        if(error.isEmpty()) {
            if(!q.exec("SELECT id FROM trainer_profile ORDER BY created_at,id LIMIT 1"))error=failedWrite();
            else if(q.next())next=q.value(0).toString();
            q.finish();
            if(error.isEmpty() && next.isEmpty()) {
                next=QUuid::createUuid().toString(QUuid::WithoutBraces);
                q.prepare("INSERT INTO trainer_owners(id) VALUES(?)");q.addBindValue(next);
                if(!q.exec())error=failedWrite();
            }
        }
        if(error.isEmpty()) {
            q.prepare("UPDATE local_owner SET trainer_id=? WHERE slot=1 AND trainer_id=?");q.addBindValue(next);q.addBindValue(ownerId_);
            if(!q.exec() || q.numRowsAffected()!=1)error=failedWrite();
        }
        if(error.isEmpty() && !db.commit())error=failedWrite();
        if(!error.isEmpty())db.rollback();else {verified_.clear();pendingMedia.clear();}
        return error;
    }
    QString favorite(const QString& id, bool favorite) {
        QSqlQuery query(db);
        query.prepare(favorite ? "INSERT OR IGNORE INTO pokedex_favorites(entry_id,trainer_id) VALUES(?,?)" : "DELETE FROM pokedex_favorites WHERE entry_id=? AND trainer_id=?");
        query.addBindValue(id); query.addBindValue(ownerId_);
        return query.exec() ? QString() : failedWrite();
    }
    QString navigation(const QJsonObject& state) {
        QSqlQuery query(db);
        query.prepare("INSERT INTO shell_state(scope,payload,trainer_id) VALUES(?,?,?) ON CONFLICT(trainer_id,scope) DO UPDATE SET payload=excluded.payload");
        query.addBindValue(scope_); query.addBindValue(QJsonDocument(state).toJson(QJsonDocument::Compact)); query.addBindValue(ownerId_);
        return query.exec() ? QString() : failedWrite();
    }
    LibraryWriteResult adventure(const AdventureRegistration& record) { return writeAdventure(db, record); }
    QString preferences(const ShellPreferences& value) { return writePreferences(db, value); }
    QString playSession(const PlaySession& value, const std::optional<ExitMediaSource>& source,
                        const std::optional<ExitCapture>& capture, PlayHistorySnapshot& snapshot,
                        QList<ExitMedia>& media, QString& warning) {
        // Source is fixed at process start, never resolved from current Home at
        // completion. Hashing/encoding and all SQLite work stay off the UI thread.
        std::optional<PreparedExitMedia> prepared;
        if (value.outcome == PlaySessionOutcome::Running && source && source->trainerId == ownerId_ && source->registration.adventure.id == value.adventureId)
            prepared = prepareExitMedia(db, *source);
        if (!db.transaction()) return failedWrite();
        auto error = writePlaySession(db, ownerId_, value);
        if (error.isEmpty() && value.outcome != PlaySessionOutcome::Running && capture) {
            QSqlQuery savepoint(db);
            if (!savepoint.exec("SAVEPOINT optional_exit_media")) error = failedWrite();
            else {
                if (pendingMedia.contains(value.id)) warning = writeExitMedia(db, value, pendingMedia.value(value.id), *capture);
                else warning = "The exit picture isn't available for this game build. Your previous picture has been kept.";
                if (!warning.isEmpty() && !savepoint.exec("ROLLBACK TO optional_exit_media")) error = failedWrite();
                if (!savepoint.exec("RELEASE optional_exit_media")) error = failedWrite();
            }
        }
        if (error.isEmpty()) { snapshot = readPlayHistory(db, ownerId_); error = snapshot.error; }
        if (error.isEmpty() && !db.commit()) error = failedWrite();
        if (!error.isEmpty()) db.rollback();
        else {
            if (prepared) pendingMedia.insert(value.id, *prepared);
            if (value.outcome != PlaySessionOutcome::Running) pendingMedia.remove(value.id);
            media = readExitMedia(db, ownerId_);
        }
        return error;
    }
private:
    QString requireFamilyCode(const SecretPin& pin) {
        if(!pin || !pin->size())return {};
        QSqlQuery q(db);
        if(q.exec("SELECT version FROM family_access WHERE trainer_id='family'") && q.next() && q.value(0).toInt()==1)return {};
        return "First ask a parent to set the family code in Settings > Trainer.";
    }
    QSet<QString> verified_;
    QHash<QString, PreparedExitMedia> pendingMedia;
    QSqlDatabase db;
    QString scope_, ownerId_;
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
        auto state = worker_->open(directory_, scope_, enforceAccess_, grant_, chooseOnOpen_);
        QMetaObject::invokeMethod(this, [this, state = std::move(state)] {
            opening_ = false; error_ = state.error; ready_ = error_.isEmpty() && !state.gated;
            accessRequired_=state.gated;
            if(error_.isEmpty()) {
                ownerId_=state.ownerId;profiles_=state.profiles;accountOwner_=state.accountOwner;
                protected_=state.protectedIds;familyProtected_=state.familyProtected;
            }
            if(state.gated){emit accessNeeded();return;}
            if (ready_) {
                ownerId_ = state.ownerId; accountOwner_ = state.accountOwner; profiles_ = state.profiles; profile_ = state.profile; favorites_ = state.favorites; navigation_ = state.navigation;
                worlds_ = state.library.worlds; registrations_ = state.library.registrations; preferences_ = state.library.preferences;
                history_ = state.history; archive_ = state.archive; journal_ = state.journal; exitMedia_ = state.exitMedia;
            }
            emit opened(ready_);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
void LocalStateStore::write(std::function<QString(SqliteWorker&)> operation, std::function<void(QString)> completed) {
    if (!ready_ || staged_) { completed("Your data isn't open yet. Retry after reopening TrainerOS."); return; }
    accessWrite(std::move(operation),std::move(completed));
}
void LocalStateStore::accessWrite(std::function<QString(SqliteWorker&)> operation,std::function<void(QString)> completed) {
    if((!ready_ && !accessRequired_) || staged_) {completed("Your data isn't available. Try again.");return;}
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
        if (error.isEmpty()) {
            if(accountOwner_==ownerId_)accountOwner_=profile.id;
            profile_ = profile; ownerId_ = profile.id;
            bool found=false;for(auto& item:profiles_)if(item.id==profile.id){item=profile;found=true;break;}
            if(!found)profiles_.append(profile);
            emit trainersChanged();
        }
        else emit userWriteFailed();
        if (guard) completed({error.isEmpty(), error});
    });
}
QString LocalStateStore::accountDirectory() const {
    if(!ready_)return {};
    return ownerId_==accountOwner_ ? directory_ : QDir(directory_).filePath("trainers/"+QString::fromLatin1(QCryptographicHash::hash(ownerId_.toUtf8(),QCryptographicHash::Sha256).toHex()));
}
void LocalStateStore::createTrainerAsync(const TrainerProfile& profile, QObject* context, std::function<void(ProfileWriteResult)> completed) {
    if(profile.id.isEmpty() || profile.name.trimmed().isEmpty() || profile.name.size()>96 || !profile.createdAt.isValid()) {completed({false,"Choose a valid Trainer name."});return;}
    if(!profile_) {saveAsync(profile,context,std::move(completed));return;}
    write([profile](SqliteWorker& worker){return worker.createTrainer(profile);},
        [this,profile,guard=QPointer<QObject>(context),completed](const QString& error){
            if(error.isEmpty()){profiles_.append(profile);emit trainersChanged();}else emit userWriteFailed();
            if(guard)completed({error.isEmpty(),error});
        });
}
void LocalStateStore::stageTrainerAsync(const QString& id, QObject* context, std::function<void(QString)> completed) {
    if(!ready_ || pending_ || staged_){completed("Finish saving before switching Trainers.");return;}
    write([id](SqliteWorker& worker){return worker.stageTrainer(id);},
        [this,guard=QPointer<QObject>(context),completed](const QString& error){
            staged_=error.isEmpty();
            if(guard)completed(error);
        });
    staged_=true;
}
void LocalStateStore::verifyPin(const QString& id, SecretPin pin,bool family,QObject* context,std::function<void(QString)> done) {
    accessWrite([id,pin,family](SqliteWorker& w){return w.authenticate(id,pin,family);},
        [guard=QPointer<QObject>(context),done](const QString& e){if(guard)done(e);});
}
void LocalStateStore::removeCurrentTrainer(SecretPin familyCode,QObject* context,std::function<void(QString)> done) {
    if(!ready_ || accessRequired_ || staged_ || pending_ || !profile_) {done("Finish the current operation first.");return;}
    write([familyCode](SqliteWorker& w){return w.removeCurrentTrainer(familyCode);},
        [this,guard=QPointer<QObject>(context),done](const QString& e){
            if(!e.isEmpty())staged_=false;
            // Success deliberately keeps all old repositories frozen until the
            // composition is destroyed. No late callback can recreate rows.
            if(guard)done(e);
        });
    staged_=true;
}
void LocalStateStore::changePin(SecretPin old,SecretPin next,bool family,QObject* context,std::function<void(QString)> done) {
    if(!ready_ || accessRequired_){done("Open your Trainer first.");return;}
    const auto id=ownerId_;const bool protectedPin=next && next->size();
    accessWrite([id,old,next,family](SqliteWorker& w){return w.changePin(id,old,next,family);},
        [this,id,family,protectedPin,guard=QPointer<QObject>(context),done](const QString& e){
            if(e.isEmpty()){if(family)familyProtected_=protectedPin;else if(protectedPin)protected_.insert(id);else protected_.remove(id);emit trainersChanged();}
            if(guard)done(e);
        });
}
void LocalStateStore::resetPin(const QString& id,SecretPin pin,QObject* context,std::function<void(QString)> done) {
    accessWrite([id,pin](SqliteWorker& w){return w.resetPin(id,pin);},
        [this,id,guard=QPointer<QObject>(context),done](const QString& e){if(e.isEmpty()){protected_.remove(id);emit trainersChanged();}if(guard)done(e);});
}
void LocalStateStore::unlock(const QString& id,QObject* context,std::function<void(QString)> done) {
    if(!accessRequired_ || pending_){done("Finish the current operation first.");return;}
    accessWrite([id](SqliteWorker& w){return w.stageTrainer(id);},
        [this,id,guard=QPointer<QObject>(context),done](const QString& e){
            if(e.isEmpty()){grant_=id;chooseOnOpen_=false;QTimer::singleShot(0,this,[this]{open();});}
            if(guard)done(e);
        });
}
void LocalStateStore::createProtectedTrainer(const TrainerProfile& p,SecretPin pin,QObject* context,std::function<void(ProfileWriteResult)> done) {
    if(p.id.isEmpty() || p.name.trimmed().isEmpty() || p.name.size()>96 || !p.createdAt.isValid()){done({false,"Choose a valid Trainer name."});return;}
    const bool first=profiles_.isEmpty(), hasPin=pin && pin->size();
    accessWrite([p,pin,first](SqliteWorker& w){return first?w.profile(p,pin):w.createTrainer(p,pin);},
        [this,p,first,hasPin,guard=QPointer<QObject>(context),done](const QString& e){
            if(e.isEmpty()){
                if(first){if(accountOwner_==ownerId_)accountOwner_=p.id;ownerId_=p.id;if(ready_)profile_=p;}
                profiles_.append(p);if(hasPin)protected_.insert(p.id);emit trainersChanged();
            }
            if(guard)done({e.isEmpty(),e});
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
HomeSnapshot LocalStateStore::home() const {
    if (history_.recent.isEmpty()) return {{}, {}, {}, "Your next journey starts in Worlds."};
    const auto& last = history_.recent.front();
    return {last.adventureId, {}, {}, "Choose an Adventure with Y. Press A on Home to play."};
}
std::optional<qint64> LocalStateStore::recordedSeconds(const QString& id) const {
    if (!history_.totals.contains(id)) return {};
    return history_.totals.value(id);
}
void LocalStateStore::saveSessionAsync(const PlaySession& value, QObject* context, std::function<void(QString)> completed) {
    saveSessionMediaAsync(value, {}, {}, context, std::move(completed));
}
void LocalStateStore::saveSessionMediaAsync(const PlaySession& value, const std::optional<ExitMediaSource>& source,
                                           const std::optional<ExitCapture>& capture, QObject* context, std::function<void(QString)> completed) {
    auto snapshot = std::make_shared<PlayHistorySnapshot>();
    auto media = std::make_shared<QList<ExitMedia>>();
    auto warning = std::make_shared<QString>();
    write([value, source, capture, snapshot, media, warning](SqliteWorker& worker) { return worker.playSession(value, source, capture, *snapshot, *media, *warning); },
          [this, snapshot, media, warning, guard = QPointer<QObject>(context), completed](const QString& error) {
        if (error.isEmpty()) { history_ = *snapshot; exitMedia_ = *media; emit libraryChanged(); }
        else emit userWriteFailed();
        if (guard) completed(error.isEmpty() ? *warning : error);
    });
}
std::optional<ExitMedia> LocalStateStore::exitMedia(const QString& adventureId) const {
    const auto record = registration(adventureId);
    if (!profile_ || !record) return {};
    for (const auto& media : exitMedia_) if (media.trainerId == profile_->id && media.domain == "pokemon"
        && media.adventureId == adventureId && media.registrationRevision == record->revision) return media;
    return {};
}
QImage LocalStateStore::exitImage(const QString& sessionId) const {
    for (const auto& media : exitMedia_) if (media.sessionId == sessionId) {
        const auto current = exitMedia(media.adventureId);
        if (current && current->sessionId == sessionId) return QImage::fromData(current->jpeg, "JPEG");
    }
    return {};
}
void LocalStateStore::saveRecordAsync(const QString& id, const PokedexProgress& record, QObject* context, std::function<void(PokedexWriteResult)> completed) {
    auto result = std::make_shared<PokedexWriteResult>();
    auto snapshot = std::make_shared<PokedexJournalSnapshot>();
    write([id, record, result, snapshot](SqliteWorker& worker) { *result=worker.journal(id,record,*snapshot);return result->error; },
          [this, result, snapshot, guard=QPointer<QObject>(context), completed](const QString& error) {
        if(error.isEmpty()) journal_=snapshot->records;
        else emit userWriteFailed();
        if(guard) completed({error.isEmpty(),error,result->revision});
    });
}
void LocalStateStore::saveArchiveAsync(const HallOfFameEntry& entry, QObject* context, std::function<void(ArchiveWriteResult)> completed) {
    auto result = std::make_shared<ArchiveWriteResult>();
    auto snapshot = std::make_shared<ArchiveResult>();
    write([entry, result, snapshot](SqliteWorker& worker) { *result = worker.memory(entry, *snapshot); return result->error; },
          [this, result, snapshot, guard = QPointer<QObject>(context), completed](const QString& error) {
        if (error.isEmpty()) archive_ = snapshot->entries;
        else emit userWriteFailed();
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
