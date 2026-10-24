// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QTimer>
#include <QtEndian>
#include <functional>

// ECLX v1 envelopes, unchanged, over the existing authenticated datagram channel.
// ARMSX2 owns Ethernet, DHCP, SipHash, nonce/replay checks and reassembly.
// No destination is accepted from the remote peer. Two emulator instances only.
class Ps2Sockets final : public QObject {
public:
    static constexpr quint16 HostPort = 19072;
    static constexpr int HeaderSize = 44, MaxPacket = 1044;
    using Sender = std::function<bool(const QByteArray&)>;
    Ps2Sockets(bool host, Sender send, QObject* parent = nullptr)
        : QObject(parent), host_(host), send_(std::move(send)), socket_(this) {}
    bool start(quint16 guestPort=0) {
        if(socket_.state() != QAbstractSocket::UnconnectedState) return false;
        if(!socket_.bind(QHostAddress::LocalHost, host_?0:guestPort, QUdpSocket::DontShareAddress)) return false;
        connect(&socket_, &QUdpSocket::readyRead, this, [this]{ drain(); });
        return true;
    }
    quint16 port() const { return host_ ? HostPort : socket_.localPort(); }
    // Bounds only: authentication remains the upstream emulator's responsibility.
    static bool envelope(const QByteArray& bytes) {
        if(bytes.size() < HeaderSize || bytes.size() > MaxPacket || bytes.left(4) != "ECLX"
           || bytes[4] != 1 || bytes[5] < 1 || bytes[5] > 3) return false;
        auto word = [&](int at){ return qFromBigEndian<quint16>(bytes.constData()+at); };
        const auto payload = word(24), index = word(20), count = word(22);
        if(word(6) != HeaderSize || word(26) != 0 || payload != bytes.size()-HeaderSize
           || payload > 1000 || count < 1 || count > 2 || index >= count) return false;
        if(bytes[5] != 3) return payload == 0 && index == 0 && count == 1;
        return payload > 0 && index*1000+payload <= 1514;
    }
    void receive(const QByteArray& bytes) {
        if(!envelope(bytes)) return;
        const quint16 target = host_ ? HostPort : peerPort_;
        // A guest cannot receive anything until its own emulator sends HELLO.
        // Old datagrams never choose a destination in a subsequent session.
        if(target) socket_.writeDatagram(bytes, QHostAddress::LocalHost, target);
    }
private:
    void drain() {
        int budget = 64;
        while(socket_.hasPendingDatagrams() && budget--) {
            const auto packet = socket_.receiveDatagram(MaxPacket+1);
            if(packet.senderAddress() != QHostAddress::LocalHost || !envelope(packet.data())) continue;
            const auto port = packet.senderPort();
            if(host_ ? port != HostPort : (peerPort_ && peerPort_ != port)) continue;
            if(!host_ && !peerPort_) {
                if(packet.data()[5] != 1) continue; // native HELLO, not arbitrary UDP
                peerPort_ = port;
            }
            send_(packet.data()); // congestion drops preserve native UDP semantics
        }
        if(socket_.hasPendingDatagrams()) QTimer::singleShot(0,this,[this]{ drain(); });
    }
    bool host_;
    Sender send_;
    QUdpSocket socket_;
    quint16 peerPort_ = 0;
};
