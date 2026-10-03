#include "RetroArchNetplayClient.h"
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QtEndian>
#include <utility>

namespace trainer::retroarch {
namespace {
constexpr qint64 BufferLimit = 8 * 1024 * 1024;
quint32 word(const QByteArray& bytes, int offset) {
    return qFromBigEndian<quint32>(bytes.constData() + offset);
}
QByteArray word(quint32 value) {
    QByteArray bytes(4, '\0');
    qToBigEndian(value, bytes.data());
    return bytes;
}
}
NetplayClient::NetplayClient(QObject* parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this]{ fail("The multiplayer connection timed out."); });
    connect(&listener_, &QTcpServer::newConnection, this, [this]{
        while (listener_.hasPendingConnections()) {
            auto* socket = listener_.nextPendingConnection();
            if (local_ || !socket->peerAddress().isLoopback()) {
                socket->abort(); socket->deleteLater(); continue;
            }
            local_ = socket;
            socket->setReadBufferSize(BufferLimit);
            socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
            connect(socket, &QTcpSocket::readyRead, this, &NetplayClient::pumpLocal);
            connect(socket, &QTcpSocket::disconnected, this, [this]{
                if (running_) fail("The game disconnected from multiplayer.");
            });
            remote_.connectToHost(address_, port_);
        }
    });
    remote_.setReadBufferSize(BufferLimit);
    connect(&remote_, &QTcpSocket::connected, this, [this]{
        remote_.setSocketOption(QAbstractSocket::LowDelayOption, 1);
        if (!session_.isEmpty() && !write(remote_, QByteArray("RATS") + session_)) return;
        pumpLocal();
    });
    connect(&remote_, &QTcpSocket::readyRead, this, &NetplayClient::pumpRemote);
    connect(&remote_, &QTcpSocket::errorOccurred, this, [this]{
        if (running_) fail("Couldn't connect to your friend's game.");
    });
    connect(&remote_, &QTcpSocket::disconnected, this, [this]{
        if (running_) fail("The multiplayer connection ended.");
    });
}
quint16 NetplayClient::start(QString address, quint16 port, QString session, QString password) {
    stop();
    if (!port || address.size() > 253 || !QRegularExpression("^[A-Za-z0-9.-]+$").match(address).hasMatch()
        || !QRegularExpression("^[A-Za-z0-9_-]{1,32}$").match(password).hasMatch()
        || (!session.isEmpty() && !QRegularExpression("^[A-Za-z0-9+/]{16}$").match(session).hasMatch())) return 0;
    address_ = std::move(address); port_ = port;
    session_ = QByteArray::fromBase64(session.toLatin1()); password_ = password.toUtf8();
    if (!listener_.listen(QHostAddress::LocalHost, 0)) { stop(); return 0; }
    running_ = true; timeout_.start(20000);
    return listener_.serverPort();
}
void NetplayClient::stop() {
    running_ = false; timeout_.stop(); listener_.close(); remote_.abort();
    if (local_) {
        local_->disconnect(this); local_->abort(); local_->deleteLater(); local_ = nullptr;
    }
    password_.fill('\0'); password_.clear(); session_.clear();
    localBuffer_.clear(); remoteBuffer_.clear(); salt_ = 0;
    localHeader_ = remoteHeader_ = authenticated_ = false;
}
void NetplayClient::fail(QString message) {
    if (!running_) return;
    stop(); emit failed(std::move(message));
}
bool NetplayClient::write(QTcpSocket& socket, const QByteArray& bytes) {
    if (socket.bytesToWrite() + bytes.size() > BufferLimit || socket.write(bytes) != bytes.size()) {
        fail("The multiplayer connection stopped responding."); return false;
    }
    return true;
}
void NetplayClient::pumpLocal() {
    if (!running_ || !local_ || remote_.state() != QAbstractSocket::ConnectedState) return;
    localBuffer_ += local_->readAll();
    if (localBuffer_.size() > BufferLimit) { fail("The multiplayer connection stopped responding."); return; }
    // RetroArch 1.22.2 protocol: 24-byte header, then NICK(0x20, 32 bytes).
    if (!localHeader_) {
        if (localBuffer_.size() < 24) return;
        if (word(localBuffer_, 0) != 0x52414e50) { fail("This emulator uses an unsupported multiplayer protocol."); return; }
        if (!write(remote_, localBuffer_.first(24))) return;
        localBuffer_.remove(0, 24); localHeader_ = true;
    }
    if (!authenticated_) {
        if (!remoteHeader_ || localBuffer_.size() < 40) return;
        if (word(localBuffer_, 0) != 0x20 || word(localBuffer_, 4) != 32) {
            fail("This emulator uses an unsupported multiplayer protocol."); return;
        }
        if (!write(remote_, localBuffer_.first(40))) return;
        localBuffer_.remove(0, 40);
        // Upstream hashes eight uppercase hexadecimal salt digits followed by
        // the password, sending the 64 lowercase ASCII SHA-256 digits.
        const auto salted = QByteArray::number(salt_, 16).rightJustified(8, '0').toUpper() + password_;
        const auto digest = QCryptographicHash::hash(salted, QCryptographicHash::Sha256).toHex();
        if (!write(remote_, word(0x21) + word(64) + digest)) return;
        password_.fill('\0'); password_.clear(); authenticated_ = true; timeout_.stop();
    }
    if (!localBuffer_.isEmpty()) {
        const auto bytes = std::exchange(localBuffer_, {}); write(remote_, bytes);
    }
}
void NetplayClient::pumpRemote() {
    if (!running_ || !local_) return;
    remoteBuffer_ += remote_.readAll();
    if (remoteBuffer_.size() > BufferLimit) { fail("The multiplayer connection stopped responding."); return; }
    if (!remoteHeader_) {
        if (remoteBuffer_.size() < 24) return;
        const auto protocol = word(remoteBuffer_, 16);
        if (word(remoteBuffer_, 0) != 0x52414e50 || protocol < 5 || protocol > 7) {
            fail("Your friend's emulator rejected the multiplayer connection."); return;
        }
        salt_ = word(remoteBuffer_, 12);
        if (!salt_) { fail("Your friend's game did not request the invitation password."); return; }
        // Only the local client sees zero salt, suppressing its password UI.
        // The remote host receives the real PASSWORD command and verifies it.
        remoteBuffer_.replace(12, 4, QByteArray(4, '\0')); remoteHeader_ = true;
    }
    const auto bytes = std::exchange(remoteBuffer_, {});
    if (write(*local_, bytes)) pumpLocal();
}
}
