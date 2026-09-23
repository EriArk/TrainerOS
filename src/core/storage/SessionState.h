#pragma once
#include "LocalStateStore.h"
#include "core/navigation/ShellController.h"
#include <QTimer>
#include "features/trainer/TrainerAccessController.h"

namespace trainer {
// Application lifecycle coordinator. QML sees status/actions, never database paths.
class SessionState final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool entryGate READ entryGate NOTIFY changed)
    Q_PROPERTY(trainer::TrainerAccessController* access READ access CONSTANT)
    Q_PROPERTY(bool persistent READ persistent CONSTANT)
    Q_PROPERTY(bool blocked READ blocked NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QStringList choices READ choices NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
public:
    SessionState(ShellController&, LocalStateStore*, QObject* parent = nullptr);
    TrainerAccessController* access() {return &access_;}
    bool entryGate() const {return entryGate_;}
    QString restartGrant() const {return nextTrainer_;}
    bool persistent() const { return store_ != nullptr; }
    bool blocked() const { return entryGate_ || access_.active() || creating_ || switching_ || closing_ || (store_ && (!restored_ || !error_.isEmpty())); }
    QString title() const;
    QString message() const;
    QStringList choices() const;
    int focusIndex() const { return focus_; }
    void start();
    void setTrainerSwitchGuard(std::function<bool()> guard) { switchGuard_=std::move(guard); }
    void setTrainerRemovalPreparation(std::function<QString()> prepare) { prepareRemoval_=std::move(prepare); }
    void requestTrainers();
    void requestTrainerSwitch(const QString&);
    void createTrainer(const TrainerProfile&);
    void requestTrainerRemoval();
    void dispatch(Action);
    void setAdventureActive(bool active) { adventureActive_ = active; }
    void setServiceActive(bool active) { serviceActive_=active; finishExit(); }
    void cancelPendingExit() { if(switching_)return; closing_=false; nextTrainer_.clear(); emit changed(); }
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void requestExit();
signals:
    void changed();
    void exitReady();
    void trainerRestartReady();
private:
    void stateChanged();
    void flush();
    void finishExit();
    ShellController& shell_;
    LocalStateStore* store_;
    QTimer debounce_;
    QJsonObject desired_, committed_;
    QString error_;
    bool restored_ = false, closing_ = false, writing_ = false, paused_ = false;
    bool adventureActive_ = false;
    bool serviceActive_ = false;
    int focus_ = 0;
    bool creating_ = false, switching_ = false;
    QString nextTrainer_;
    std::function<bool()> switchGuard_;
    std::function<QString()> prepareRemoval_;
    bool canChangeTrainer() const;
    TrainerAccessController access_;
    bool entryGate_=false;
    QString verifiedTrainer_;
};
}
