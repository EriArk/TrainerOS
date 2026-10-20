#pragma once
#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <functional>

namespace trainer {
class PowerControl final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool keepAwake READ keepAwake NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    using QObject::QObject;
    ~PowerControl() override;
    void start();
    bool available() const {return available_;}
    bool keepAwake() const {return keepAwake_;}
    bool busy() const {return busy_;}
    QString error() const {return error_;}
    std::function<void(std::function<void(QString)>)> prepareSleep;
    Q_INVOKABLE void toggleKeepAwake();
    Q_INVOKABLE void sleep();
signals:
    void changed();
    void menuRequested();
    void woke();
private:
    void send(const QJsonObject&);
    void receive();
    QProcess process_;
    QByteArray buffer_;
    bool available_=false,keepAwake_=false,busy_=false;
    QString error_;
};
}
