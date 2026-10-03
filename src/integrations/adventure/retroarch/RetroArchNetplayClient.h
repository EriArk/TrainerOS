#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

namespace trainer::retroarch {
// Loopback-only adapter for the stock RetroArch password handshake. The accepted
// invitation supplies the password; RetroArch still verifies it on the host.
// After the handshake the game protocol passes through unchanged.
class NetplayClient final : public QObject {
    Q_OBJECT
public:
    explicit NetplayClient(QObject* parent = nullptr);
    ~NetplayClient() override { stop(); }
    quint16 start(QString address, quint16 port, QString session, QString password);
    void stop();
signals:
    void failed(QString message);
private:
    void pumpLocal();
    void pumpRemote();
    void fail(QString message);
    bool write(QTcpSocket&, const QByteArray&);
    QTcpServer listener_{this};
    QTcpSocket remote_{this};
    QTcpSocket* local_ = nullptr;
    QTimer timeout_{this};
    QString address_;
    quint16 port_ = 0;
    QByteArray session_, password_, localBuffer_, remoteBuffer_;
    quint32 salt_ = 0;
    bool running_ = false, localHeader_ = false, remoteHeader_ = false, authenticated_ = false;
};
}
