#pragma once
#include <QObject>
#include <QJsonObject>
#include <QTcpServer>
#include <QHash>
#include <QPointer>
#include <memory>

class QLockFile;
class QTcpSocket;
namespace trainer {
class ControlsDashboard;
class SdkNetwork;

// Local, optional app-to-host API. This bridge exposes only the services that
// are actually bound here; it owns neither game execution nor application data.
class PlatformHost final : public QObject {
    Q_OBJECT
public:
    explicit PlatformHost(ControlsDashboard&, QObject* parent = nullptr);
    ~PlatformHost() override;
    static QString defaultEndpoint();
    bool start(const QString& endpointFile, QString* error = nullptr);
    void stop();
    void setContext(const QJsonObject& context);
    void notifyLifecycle(const QString& event);
    void setNetwork(SdkNetwork*);
    void setAvatar(const QJsonObject& descriptor, bool available);
    QJsonObject context() const { return context_; }
private:
    struct Peer {
        QByteArray input;
        QString app;
        bool authenticated = false, subscribed = false;
        bool avatarSubscribed = false;
        QJsonObject widgets;
    };
    ControlsDashboard& dashboard_;
    QTcpServer server_;
    QHash<QTcpSocket*, Peer> peers_;
    std::unique_ptr<QLockFile> lock_;
    QString endpoint_, token_, instance_;
    QJsonObject context_;
    QJsonObject avatar_;
    bool avatarAvailable_ = false;
    QPointer<SdkNetwork> network_;
    quint64 revision_ = 0;
    void accept();
    void read(QTcpSocket*);
    void request(QTcpSocket*, const QJsonObject&);
    void send(QTcpSocket*, const QJsonObject&);
    void release(QTcpSocket*);
    void publishWidgets();
    void activate(const QString&);
    QJsonObject snapshot() const;
};
}
