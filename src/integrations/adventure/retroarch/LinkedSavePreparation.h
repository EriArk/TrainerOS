#pragma once
#include <QObject>
#include <QProcess>
#include <QJsonObject>

namespace trainer::retroarch {
// Private process pipes only. No SRAM, admission code or certificate enters logs.
class LinkedSavePreparation final : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~LinkedSavePreparation() override { stop(); }
    void start(const QString& python,const QString& helper,const QJsonObject& config);
    void stop();
signals:
    void ready(QJsonObject endpoint);
    void completed(QByteArray peerSram);
    void failed();
private:
    QProcess* process_=nullptr;
};
}
