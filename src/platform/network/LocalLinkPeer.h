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
    explicit LocalLinkPeer(QObject* parent=nullptr, quint16 port=47845, quint16 discoveryPort=47846, QString discoveryKey="trainerosLink");
    ~LocalLinkPeer() override;
    void configure(const QString& id,const QString& name);
    bool open();void close();void disconnectPeer();
    QVariantList peers() const;
    void connectPeer(int index);
    void connectId(const QString& id,const QString& interface={});
    void connectBridge(quint16 port);
    void setBluetoothDiscoveryId(const QString& id){bluetoothId_=id;if(timer_.isActive())announce();}
    bool outgoing() const{return outgoing_;}
    void setVisible(bool visible){advertising_=visible;}
    void send(const QJsonObject&);
    bool connected() const{return socket_ && socket_->state()==QAbstractSocket::ConnectedState;}
    QString id() const{return id_;}
    QString name() const{return name_;}
    QString peerAddress() const{return socket_?socket_->peerAddress().toString():QString();}
signals:
    void changed();void connectedToPeer();void disconnectedFromPeer();
    void received(const QJsonObject&);void error(const QString&);
private:
    const quint16 port_,discoveryPort_;
    const QString discoveryKey_;
    void attach(QTcpSocket*);void announce();void readDiscovery();void receive();
    bool localAddress(const QHostAddress&) const;
    QTcpServer server_;QUdpSocket discovery_;QTimer timer_,connectTimer_;
    QTcpSocket* socket_=nullptr;QByteArray buffer_;QString id_,name_,bluetoothId_;
    QMap<QString,QVariantMap> peers_;
    bool outgoing_=false;
    bool advertising_=true;
};
}
