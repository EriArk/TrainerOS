#pragma once
#include "LocalStateStore.h"
#include "core/navigation/ShellController.h"
#include <QTimer>

namespace trainer {
// Application lifecycle coordinator. QML sees status/actions, never database paths.
class SessionState final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool persistent READ persistent CONSTANT)
    Q_PROPERTY(bool blocked READ blocked NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QStringList choices READ choices NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
public:
    SessionState(ShellController&, LocalStateStore*, QObject* parent = nullptr);
    bool persistent() const { return store_ != nullptr; }
    bool blocked() const { return closing_ || (store_ && (!restored_ || !error_.isEmpty())); }
    QString title() const;
    QString message() const;
    QStringList choices() const;
    int focusIndex() const { return focus_; }
    void start();
    void dispatch(Action);
    void setAdventureActive(bool active) { adventureActive_ = active; }
    void setServiceActive(bool active) { serviceActive_=active; finishExit(); }
    void cancelPendingExit() { closing_=false; emit changed(); }
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void requestExit();
signals:
    void changed();
    void exitReady();
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
};
}
