#pragma once
#include <QObject>
#include <QThread>
#include <QMap>
#include <QString>
#include <functional>

namespace trainer {
struct DeviceSnapshot {
    int volume = -1;
    bool muted = false;
    int brightness = -1;
    QString network = "Unavailable";
    qint64 internalFree = -1, internalTotal = -1, libraryFree = -1, libraryTotal = -1;
};
struct BacklightValue { QString directory; int current = -1, maximum = -1; };
BacklightValue readBacklight(const QString& root);
QString writeBacklight(const QString& root, int percent);
DeviceSnapshot parseVolume(const QByteArray& output);
struct DeviceBackend {
    std::function<DeviceSnapshot()> read;
    std::function<QString(const QString&, int)> write;
};
DeviceBackend systemDeviceBackend(const QString& dataDirectory, const QString& libraryDirectory);

class DeviceService final : public QObject {
    Q_OBJECT
public:
    explicit DeviceService(DeviceBackend backend, QObject* parent = nullptr);
    ~DeviceService() override;
    DeviceSnapshot snapshot() const { return snapshot_; }
    bool busy() const { return busy_; }
    QString error() const { return error_; }
    void refresh();
    void setValue(const QString& control, int value);
    void adjust(const QString& control, int delta);
    void toggleMute();
    void hardwareVolume(int delta);
signals:
    void changed();
private:
    void execute(const QString& control, int value);
    DeviceBackend backend_;
    DeviceSnapshot snapshot_;
    QString error_;
    QThread thread_;
    QObject* worker_;
    bool busy_ = false;
    QMap<QString, int> pending_;
    QString activeControl_;
    int activeValue_ = 0;
    int desired(const QString& control) const;
};
}
