#pragma once
#include "core/repository/TrainerRepository.h"
#include "core/repository/PokedexRepository.h"
#include "core/repository/LibraryRepository.h"
#include "core/repository/PreferencesRepository.h"
#include "core/repository/PlayHistoryRepository.h"
#include "SqlitePlayHistory.h"
#include <QJsonObject>
#include <QSet>
#include <QThread>

namespace trainer {
class SqliteWorker;
// The UI thread owns committed projections; the worker exclusively owns SQLite.
// No sample library, Seen/Caught values or achievement unlocks enter this store.
class LocalStateStore final : public QObject, public TrainerRepository, public PokedexProgressRepository,
                              public LibraryRepository, public PreferencesRepository, public PlayHistoryRepository {
    Q_OBJECT
public:
    explicit LocalStateStore(QString directory, QObject* parent = nullptr, QString scope = "user-library-v1");
    ~LocalStateStore() override;
    void open();
    bool ready() const { return ready_; }
    bool opening() const { return opening_; }
    int pending() const { return pending_; }
    QString error() const { return error_; }
    std::optional<TrainerProfile> load() const override { return profile_; }
    PokedexProgress progress(const QString& id) const override { return {{}, {}, favorites_.contains(id)}; }
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
    std::optional<AdventureRegistration> registration(const QString&) const override;
    void saveAdventureAsync(const AdventureRegistration&, QObject*, std::function<void(LibraryWriteResult)>) override;
    ShellPreferences preferences() const override { return preferences_; }
    void savePreferences(const ShellPreferences&, QObject*, std::function<void(QString)>) override;
signals:
    void opened(bool success);
    void pendingChanged();
    void userWriteFailed();
    void libraryChanged();
    void preferencesChanged();
private:
    void write(std::function<QString(SqliteWorker&)>, std::function<void(QString)>);
    QThread thread_;
    SqliteWorker* worker_;
    QString directory_, scope_, error_;
    bool ready_ = false, opening_ = false;
    int pending_ = 0;
    std::optional<TrainerProfile> profile_;
    QSet<QString> favorites_;
    QJsonObject navigation_;
    QList<World> worlds_;
    QList<AdventureRegistration> registrations_;
    ShellPreferences preferences_;
    PlayHistorySnapshot history_;
};
}
