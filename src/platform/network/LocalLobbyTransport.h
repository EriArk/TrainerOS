#pragma once
#include <QObject>
#include <QSslSocket>
#include <QSslConfiguration>
#include <QTcpServer>
#include <QUdpSocket>
#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantList>
#include <functional>

namespace trainer {
// Discovery is a hint, never an authenticated identity or admission. TLS proves
// possession of the advertised certificate; LobbyRoomCoordinator admits players.
class LocalLobbyListener final : public QTcpServer {
public:
    using QTcpServer::QTcpServer;
    std::function<void(qintptr)> accept;
protected:
    void incomingConnection(qintptr descriptor) override { accept(descriptor); }
};
class LocalLobbyTransport final : public QObject {
    Q_OBJECT
public:
    explicit LocalLobbyTransport(QObject* parent=nullptr,quint16 discoveryPort=47849,bool loopbackOnly=false);
    ~LocalLobbyTransport() override;
    bool configure(QString directory,QString name);
    bool open();
    void close(bool graceful=false);
    bool running() const {return timer_.isActive();}
    QString identity() const {return identity_;}
    QString name() const {return name_;}
    void setName(QString name) {name_=name.trimmed().left(48);if(name_.isEmpty())name_="Local player";}
    QString error() const {return error_;}
    void setRooms(QJsonArray rooms);
    QVariantList rooms() const;
    bool send(QString peer,QJsonObject packet);
    QString addressOf(QString peer) const;
    QString localAddressFor(QString peer) const;
    static bool validIdentity(const QString& id);
signals:
    void changed();
    void received(QString peer,QString name,QJsonObject packet);
    void disconnected(QString peer);
private:
    friend class LocalLobbyTests;
    friend class SocialTests;
    friend class LobbySmokeScenario;
    friend class RuntimeMultiplayerTests;
    struct Link {QString expected,peer,name;QByteArray buffer;QList<QJsonObject> pending;bool hello=false;};
    void refresh();
    void discover();
    void attach(QSslSocket* socket,QString expected={});
    void read(QSslSocket* socket);
    void drop(QSslSocket* socket);
    bool write(QSslSocket* socket,const QJsonObject& packet);
    bool local(const QHostAddress& address) const;
    QHash<QHostAddress,QHostAddress> interfaces() const;
    QSslSocket* connection(QString peer) const;
    QSslConfiguration tls_;
    QString identity_,name_,error_;
    QMap<QString,QVariantMap> discovered_;
    QHash<QHostAddress,LocalLobbyListener*> listeners_;
    QMap<QSslSocket*,Link> links_;
    QJsonArray rooms_;
    QUdpSocket discovery_;
    QTimer timer_;
    quint16 discoveryPort_;
    bool loopbackOnly_;
};
}
