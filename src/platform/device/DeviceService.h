#pragma once
#include <QObject>
#include <QThread>
#include <QMap>
#include <QString>
#include <QVariantList>
#include <functional>
#include <QProcess>
#include <QTimer>

namespace trainer {
struct DeviceSnapshot {
    int volume = -1;
    bool muted = false;
    QVariantList audioOutputs;
    QString audioOutput;
    QVariantList audioStreams;
    QVariantList audioInputs;
    QString audioInput;
    int brightness = -1;
    QString network = "Unavailable";
    int wifi = -1, bluetooth = -1, airplane = -1;
    qint64 internalFree = -1, internalTotal = -1, libraryFree = -1, libraryTotal = -1;
};
struct BacklightValue { QString directory; int current = -1, maximum = -1; };
BacklightValue readBacklight(const QString& root);
QString writeBacklight(const QString& root, int percent);
DeviceSnapshot parseVolume(const QByteArray& output);
QVariantList parseAudioOutputs(const QByteArray& output);
QVariantList parseAudioStreams(const QByteArray& output);
QVariantList parseAudioInputs(const QByteArray& output);
struct DeviceBackend {
    std::function<DeviceSnapshot()> read;
    std::function<QString(const QString&, int)> write;
    std::function<QString(const QString&)> selectOutput;
    std::function<QString(const QString&, int, bool)> setStream;
    std::function<QString(const QString&, int, const QString&)> setInput;
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
    void toggleRadio(const QString& control);
    void hardwareVolume(int delta);
    void selectOutput(const QString& id);
    void setStream(const QString& id, int value, bool mute=false);
    void monitorAudio(bool enabled);
    void setInput(const QString& id,int value,const QString& operation);
signals:
    void changed();
private:
    void execute(const QString& control, int value, const QString& output = {});
    DeviceBackend backend_;
    DeviceSnapshot snapshot_;
    QString error_;
    QThread thread_;
    QObject* worker_;
    bool busy_ = false;
    QMap<QString, int> pending_;
    QString activeControl_;
    struct AudioChange {QString control;int value;QString target;};
    QMap<QString,AudioChange> pendingAudio_;
    int activeValue_ = 0;
    QProcess audioEvents_;
    QTimer audioDebounce_;
    int desired(const QString& control) const;
};
}
