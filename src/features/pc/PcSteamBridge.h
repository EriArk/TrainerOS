#pragma once
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QJsonObject>
#include <QLockFile>
#include <functional>
namespace trainer {
// Steam waits on this connection; the existing Adventure lifecycle owns launch.
class PcSteamBridge final : public QObject {
public:
    explicit PcSteamBridge(QObject* parent=nullptr);
    void configure(const QString& state,const QString& owner);
    void finished(bool failed);
    bool active() const {return active_;}
    QJsonObject environment() const {return environment_;}
    std::function<QString(const QString&)> launch;
private:
    QLocalServer server_;
    std::unique_ptr<QLockFile> lock_;
    QPointer<QLocalSocket> client_;
    QString owner_;
    QJsonObject environment_;
    bool active_=false;
};
}
