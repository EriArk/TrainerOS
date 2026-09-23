#pragma once
#include "core/repository/TrainerRepository.h"
#include "core/repository/PokedexRepository.h"
#include "core/repository/LibraryRepository.h"
#include "core/repository/PreferencesRepository.h"
#include "core/repository/PlayHistoryRepository.h"
#include "SqlitePlayHistory.h"
#include "TrainerPin.h"
#include "SqliteHallOfFame.h"
#include "SqlitePokedexJournal.h"
#include <QJsonObject>
#include <QSet>
#include <QThread>

namespace trainer {
class SqliteWorker;
// The UI thread owns committed projections; the worker exclusively owns SQLite.
// No sample records, inferred game progress or achievement unlocks enter this store.
class LocalStateStore final : public QObject, public TrainerRepository, public PokedexProgressRepository,
                              public LibraryRepository, public PreferencesRepository, public PlayHistoryRepository, public HallOfFameRepository {
    Q_OBJECT
public:
    explicit LocalStateStore(QString directory, QObject* parent = nullptr, QString scope = "user-library-v1");
    ~LocalStateStore() override;
    void open();
    // A grant only survives an in-process, authenticated Trainer switch.
    void enforceAccess(const QString& grant = {}) { enforceAccess_=true; grant_=grant; }
    bool accessRequired() const { return accessRequired_; }
    bool pinProtected(const QString& id) const { return protected_.contains(id); }
    bool familyProtected() const { return familyProtected_; }
    void verifyPin(const QString&, SecretPin, bool family, QObject*, std::function<void(QString)>);
    void changePin(SecretPin previous, SecretPin next, bool family, QObject*, std::function<void(QString)>);
    void resetPin(const QString&, SecretPin familyPin, QObject*, std::function<void(QString)>);
    void unlock(const QString&, QObject*, std::function<void(QString)>);
    void createProtectedTrainer(const TrainerProfile&, SecretPin, QObject*, std::function<void(ProfileWriteResult)>);

    bool ready() const { return ready_; }
    bool opening() const { return opening_; }
    int pending() const { return pending_; }
    QList<TrainerProfile> trainers() const { return profiles_; }
    QString accountDirectory() const;
    void createTrainerAsync(const TrainerProfile&, QObject*, std::function<void(ProfileWriteResult)>);
    // Final drained operation before destroying this store and rebuilding the session.
    void stageTrainerAsync(const QString&, QObject*, std::function<void(QString)>);
    QString ownerId() const { return ownerId_; }
    QString error() const { return error_; }
    std::optional<TrainerProfile> load() const override { return profile_; }
    PokedexProgress progress(const QString& id) const override { auto record=journal_.value(id);record.favorite=favorites_.contains(id);return record; }
    bool recordsEditable() const override { return true; }
    void saveRecordAsync(const QString&, const PokedexProgress&, QObject*, std::function<void(PokedexWriteResult)>) override;
    QJsonObject navigation() const { return navigation_; }
    void saveAsync(const TrainerProfile&, QObject*, std::function<void(ProfileWriteResult)>) override;
    void setFavoriteAsync(const QString&, bool, QObject*, std::function<void(QString)>) override;
    void saveNavigation(const QJsonObject&, QObject*, std::function<void(QString)>);
    bool editable() const override { return true; }
    QList<World> worlds() const override { return worlds_; }
    QList<Adventure> adventures() const override;
    QList<ResumePoint> resumePoints() const override { return {}; }
    HomeSnapshot home() const override;
    QList<PlaySession> recentSessions() const override { return history_.recent; }
    std::optional<qint64> recordedSeconds(const QString&) const override;
    void saveSessionAsync(const PlaySession&, QObject*, std::function<void(QString)>) override;
    void saveSessionMediaAsync(const PlaySession&, const std::optional<ExitMediaSource>&,
                              const std::optional<ExitCapture>&, QObject*, std::function<void(QString)>) override;
    std::optional<ExitMedia> exitMedia(const QString&) const override;
    QImage exitImage(const QString& sessionId) const;
    std::optional<AdventureRegistration> registration(const QString&) const override;
    void saveAdventureAsync(const AdventureRegistration&, QObject*, std::function<void(LibraryWriteResult)>) override;
    ShellPreferences preferences() const override { return preferences_; }
    ArchiveResult loadArchive() const override { return {ready_, archive_, ready_ ? QString() : error_}; }
    bool archiveEditable() const override { return true; }
    void saveArchiveAsync(const HallOfFameEntry&, QObject*, std::function<void(ArchiveWriteResult)>) override;
    void savePreferences(const ShellPreferences&, QObject*, std::function<void(QString)>) override;
signals:
    void opened(bool success);
    void accessNeeded();
    void trainersChanged();
    void pendingChanged();
    void userWriteFailed();
    void libraryChanged();
    void preferencesChanged();
private:
    void accessWrite(std::function<QString(SqliteWorker&)>, std::function<void(QString)>);
    void write(std::function<QString(SqliteWorker&)>, std::function<void(QString)>);
    QThread thread_;
    SqliteWorker* worker_;
    QString directory_, scope_, error_, ownerId_, accountOwner_;
    QList<TrainerProfile> profiles_;
    bool enforceAccess_=false, accessRequired_=false, familyProtected_=false;
    QString grant_;
    QSet<QString> protected_;
    bool staged_ = false;
    bool ready_ = false, opening_ = false;
    int pending_ = 0;
    std::optional<TrainerProfile> profile_;
    QSet<QString> favorites_;
    QJsonObject navigation_;
    QList<World> worlds_;
    QList<AdventureRegistration> registrations_;
    ShellPreferences preferences_;
    PlayHistorySnapshot history_;
    QList<ExitMedia> exitMedia_;
    QList<HallOfFameEntry> archive_;
    QHash<QString,PokedexProgress> journal_;
};
}
