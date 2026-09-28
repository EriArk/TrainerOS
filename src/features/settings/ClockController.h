#pragma once
#include "platform/device/ClockService.h"
#include "core/input/Action.h"
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <functional>

namespace trainer {
class ClockController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QString time READ time NOTIFY changed)
    Q_PROPERTY(QString date READ date NOTIFY changed)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
public:
    explicit ClockController(QObject* parent = nullptr);
    ~ClockController() override;
    void begin(bool onboarding = false);
    void leave();
    bool busy() const { return busy_; }
    QString mode() const { return mode_; }
    QString time() const;
    QString date() const;
    QString subtitle() const;
    QString error() const { return error_; }
    int focusIndex() const { return focus_; }
    QVariantList rows() const;
    void dispatch(Action);
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void adjust(int direction);
    std::function<ClockSnapshot()> read = readClock;
    std::function<QString(const QString&, const QString&)> write = changeClock;
signals:
    void changed();
    void backRequested();
    void continueRequested();
private:
    void refresh(const QString& operation = {}, const QString& value = {});
    void openZones();
    QStringList cities() const;
    QDateTime now() const;
    ClockSnapshot snapshot_;
    QThread* worker_ = nullptr;
    QTimer timer_;
    QString mode_ = "main", error_;
    QStringList regions_;
    QDateTime draft_;
    int focus_ = 0, region_ = 0, ticks_ = 0;
    bool busy_ = false, onboarding_ = false;
};
}
