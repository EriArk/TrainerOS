#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>
#include <functional>

namespace trainer {
struct BatterySnapshot {
    int percent = -1;
    QString state = "Unknown";
    bool operator==(const BatterySnapshot&) const = default;
};
// Read-only Linux boundary; an explicit root also permits content-free fixtures.
BatterySnapshot readBatteryStatus(const QString& powerSupplyRoot);
BatterySnapshot systemBatteryStatus();

class PowerStatus final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int percent READ percent NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(bool charging READ charging NOTIFY changed)
public:
    explicit PowerStatus(std::function<BatterySnapshot()> reader = systemBatteryStatus, QObject* parent = nullptr);
    ~PowerStatus() override;
    int percent() const { return snapshot_.percent; }
    bool available() const { return percent() >= 0; }
    QString state() const { return snapshot_.state; }
    bool charging() const { return available() && state() == "Charging"; }
    bool busy() const { return busy_; }
    void start();
    void refresh();
signals:
    void changed();
    void refreshed();
private:
    std::function<BatterySnapshot()> reader_;
    BatterySnapshot snapshot_;
    QThread thread_;
    QObject* worker_;
    QTimer timer_;
    bool busy_ = false;
};
}
