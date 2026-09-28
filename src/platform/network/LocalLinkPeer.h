#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QJsonObject>
#include <QVariantList>
namespace trainer {
// Opt-in LAN transport. Discovery contains device names only, never saves or
// credentials. Application acceptance gates all Party/transaction messages.
class LocalLinkPeer final:public QObject {
    Q_OBJECT
public:
    explicit LocalLinkPeer(QObject* parent=nullptr);
    ~LocalLinkPeer() override;
    void configure(const QString& id,const QString& name);
    bool open();void close();void disconnectPeer();
    QVariantList peers() const;
    void connectPeer(int index);
    void connectId(const QString& id,const QString& interface={});
    bool outgoing() const{return outgoing_;}
    void setVisible(bool visible){advertising_=visible;}
    void send(const QJsonObject&);
    bool connected() const{return socket_ && socket_->state()==QAbstractSocket::ConnectedState;}
    QString id() const{return id_;}
    QString name() const{return name_;}
signals:
    void changed();void connectedToPeer();void disconnectedFromPeer();
    void received(const QJsonObject&);void error(const QString&);
private:
    void attach(QTcpSocket*);void announce();void readDiscovery();void receive();
    bool localAddress(const QHostAddress&) const;
    QTcpServer server_;QUdpSocket discovery_;QTimer timer_;
    QTcpSocket* socket_=nullptr;QByteArray buffer_;QString id_,name_;
    QMap<QString,QVariantMap> peers_;
    bool outgoing_=false;
    bool advertising_=true;
};
}
