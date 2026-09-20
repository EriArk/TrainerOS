#pragma once
#include "core/input/Action.h"
#include "platform/device/DeviceService.h"
#include <QVariantList>
#include <QTimer>

namespace trainer {
class DeviceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantList status READ status NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    using QObject::QObject;
    void configure(DeviceService* service, bool powerAvailable);
    void begin();
    void setMonitoring(bool enabled);
    void adjustQuick(int index, Action action);
    Q_INVOKABLE void setQuickLevel(int index, int value);
    void requestPower(bool restart);
    QVariantList rows() const;
    QVariantList status() const;
    int focusIndex() const { return focus_; }
    bool busy() const { return service_ && service_->busy(); }
    QString error() const { return service_ ? service_->error() : QString(); }
    void dispatch(Action);
    void activate(int index);
signals:
    void changed();
    void closeRequested();
    void powerRequested(const QString& mode);
    void messageRequested(const QString& message);
private:
    DeviceService* service_ = nullptr;
    bool powerAvailable_ = false;
    int focus_ = 0;
    QTimer monitor_;
};
}
