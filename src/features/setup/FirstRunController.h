#pragma once
#include "core/input/Action.h"
#include "platform/storage/LibraryStorage.h"
#include "features/settings/ClockController.h"
#include <QObject>
#include <QJsonObject>
#include <QVariantList>
#include <QThread>
#include <functional>

namespace trainer {
class FirstRunController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(trainer::ClockController* clock READ clock CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool connections READ connections NOTIFY changed)
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString description READ description NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int step READ step NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(int checkedControls READ checkedControls NOTIFY changed)
public:
    ClockController* clock() { return &clock_; }
    explicit FirstRunController(QObject* parent = nullptr);
    ~FirstRunController() override;
    void configure(const QString& directory, const QString& root);
    static QString libraryRoot(const QString& directory, const QString& fallback);
    void begin(bool hasTrainers);
    void trainerOpened();
    void saveTrainerDraft(const QJsonObject& draft);
    QJsonObject trainerDraft() const { return state_["trainerDraft"].toObject(); }
    bool active() const { return active_; }
    bool busy() const { return busy_ || clock_.busy(); }
    bool connections() const { return connections_; }
    QString stage() const { return state_["stage"].toString("welcome"); }
    QString title() const;
    QString description() const;
    QString error() const { return error_; }
    int focusIndex() const { return focus_; }
    int step() const;
    int checkedControls() const { return checked_; }
    QVariantList rows() const;
    void dispatch(Action action);
    void closeConnections();
    void backFromTrainer();
    Q_INVOKABLE void activate(int index);
    std::function<QString(const QString&)> useLibraryRoot;
    // Injected by tests; production uses the mounted-volume service.
    std::function<QList<LibraryLocation>(const QString&)> locations = libraryLocations;
    std::function<QString(const LibraryLocation&)> prepare = prepareLibraryLocation;
signals:
    void changed();
    void trainerRequested();
    void protectTrainerRequested();
    void finished();
private:
    ClockController clock_;
    bool persist(const QJsonObject& next);
    bool move(const QString& next);
    void loadLocations();
    QString directory_, root_, error_;
    QJsonObject state_;
    QList<LibraryLocation> locations_;
    bool active_ = false, busy_ = false, connections_ = false, configured_ = false;
    bool loadFailed_ = false;
    bool hasTrainers_ = false;
    int focus_ = 0, checked_ = 0;
    QThread* worker_ = nullptr;
};
}
