#pragma once
#include <QObject>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

namespace trainer {
// Bounded JSON-lines transport; credentials only travel over the child's stdin.
class NetworkService : public QObject {
    Q_OBJECT
public:
    explicit NetworkService(QObject* parent=nullptr);
    ~NetworkService() override;
    virtual void request(const QJsonObject& request);
    virtual void respond(const QJsonObject& response);
    virtual void cancel();
signals:
    void event(const QJsonObject& value);
    void finished(const QString& error);
private:
    QProcess process_;
    QTimer timeout_;
    QByteArray buffer_;
    bool result_ = false;
    QString error_;
};
}
