#pragma once
#include "core/repository/LibraryRepository.h"
#include "platform/process/ProcessService.h"
#include <QJsonArray>
#include <QUrl>
#include <QWebSocket>

namespace trainer::rmg {
inline constexpr auto sourceRevision="4777ed3f1fb7f1e4a0e70438a27dc4a5e9fb1927";
inline constexpr auto serverRevision="bf0f28746971fcb6341fac55c0d2d19f646a6e09";
QString bundleRoot();
QJsonObject bundle(const std::atomic_bool& cancel);
QJsonObject netplayIdentity(const AdventureRegistration&,const std::atomic_bool&,int players=2);
ProcessCommand command(const AdventureRegistration&,const QJsonObject& expected,
                       const QString& address,int port,int controller,const QStringList& inputPrefix={});

// RMG's existing version-17 lobby only. Emulator synchronization remains in
// upstream Mupen64Plus and rmg-netplay-server, including their native sockets.
class Lobby final : public QObject {
    Q_OBJECT
public:
    explicit Lobby(QObject* parent=nullptr);
    ~Lobby() override;
    struct Request {
        bool host=false;
        QUrl server;
        QString localServer; // host Nearby only; validated installed bundle
        QJsonObject identity;
        QString token;
        QJsonArray seats;
        int slot=1,port=0;
        bool privateLan=false;
        QJsonObject connection;
    };
    void start(const Request&);
    void stop();
signals:
    void roomReady(int port);
    void launchReady(QString address,int port,int controller);
    void failed(QString message);
private:
    void send(QString type,QJsonObject room={});
    void receive(const QString&);
    void fail();
    void open();
    Request request_;
    QWebSocket* socket_=nullptr;
    QProcess* server_=nullptr;
    QTimer deadline_{this},retry_{this};
    QStringList expected_,roster_;
    QString name_;
    bool active_=false,admitted_=false,begin_=false,running_=false;
    int controller_=0;
};
ProcessCommand privateCommand(const AdventureRegistration&,const Lobby::Request&,const QStringList& inputPrefix={});
QJsonObject privateEvent(const QByteArray&);
}
