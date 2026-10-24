#include "integrations/adventure/retroarch/RetroArchNetplayClient.h"
#include "integrations/adventure/retroarch/RetroArchHotseatObserver.h"
#include <QtEndian>
#include <QtTest>

using trainer::retroarch::NetplayClient;
using trainer::retroarch::HotseatObserver;
namespace {
QByteArray number(quint32 value) {
    QByteArray result(4, '\0'); qToBigEndian(value, result.data()); return result;
}
QByteArray packet(quint32 command, const QByteArray& payload) {
    return number(command) + number(quint32(payload.size())) + payload;
}
QByteArray nativeHeader() {
    return QByteArray::fromHex("52414e500000000000000001a1b2c3d40000000700000000");
}
QByteArray nativeSync() {
    QByteArray payload(184, '\0');
    payload.replace(0, 4, number(100)); payload.replace(4, 4, number(1));
    payload.replace(8, 4, number(1)); payload.replace(88, 4, number(1));
    payload.replace(152, 32, QByteArray("private nickname").leftJustified(32, '\0'));
    return payload;
}
QByteArray nativeMode(int client, bool playing, quint32 frame) {
    return packet(0x26, number(frame) + number(quint32(client) | (client == 1 ? 1U << 31 : 0)
        | (playing ? 1U << 30 : 0)) + number(playing ? 1 : 0) + QByteArray(16, '\0') + QByteArray(32, 'n'));
}
}
class NetplayClientTests : public QObject {
    Q_OBJECT
private slots:
    void stockOwnershipPacketsSurviveFragmentationAndLargeStatePackets() {
        const auto wire = nativeHeader() + packet(0x20, QByteArray(32, 'n')) +
            packet(0x23, nativeSync() + QByteArray(128 * 1024, 's')) +
            packet(0x42, QByteArray(1024 * 1024, 'z')) + nativeMode(0, false, 101) +
            nativeMode(1, true, 107) + nativeMode(1, false, 180) + nativeMode(0, true, 183);
        for (int size : {1, 3, 24, 65536, int(wire.size())}) {
            HotseatObserver observer;
            for (qsizetype offset = 0; offset < wire.size(); offset += size)
                QVERIFY(observer.feed(wire.mid(offset, size)));
            const auto events = observer.takeEvents(); QCOMPARE(events.size(), 5);
            QCOMPARE(events[0].owner, 0); QVERIFY(events[0].initial);
            QCOMPARE(events[1].owner, -1); QCOMPARE(events[2].owner, 1);
            QCOMPARE(events[3].owner, -1); QCOMPARE(events[4].owner, 0);
            QCOMPARE(events[4].frame, quint32(183)); QCOMPARE(events[4].sequence, quint64(5));
            QVERIFY(observer.takeEvents().isEmpty()); QVERIFY(!observer.failed());
        }
    }
    void uncertainOrUnexpectedOwnershipFailsClosed() {
        const QList<QByteArray> invalid{
            nativeMode(1, true, 110), // P1 has not been released
            nativeMode(1, false, 110), // spectator cannot release host's P1
            nativeMode(2, true, 110), // unsupported third native client
            packet(0x27, number(4)), // native port request refused
            packet(0x26, QByteArray(59, '\0')),
            number(0x42) + number(64 * 1024 * 1024 + 1),
            packet(0x23, nativeSync()) // a second handshake is not a new session
        };
        for (const auto& bytes : invalid) {
            HotseatObserver observer;
            QVERIFY(observer.feed(nativeHeader() + packet(0x23, nativeSync())));
            QVERIFY(!observer.feed(bytes)); QVERIFY(observer.failed());
            QVERIFY(observer.takeEvents().isEmpty());
            QVERIFY(!observer.feed(nativeMode(0, false, 111)));
            observer.reset(); QVERIFY(observer.feed(nativeHeader() + packet(0x23, nativeSync())));
            QCOMPARE(observer.takeEvents().size(), 1);
        }
    }
    void initialLayoutMustBeExclusiveP1WithOneGuest() {
        for (int variant = 0; variant < 6; ++variant) {
            auto sync = nativeSync();
            if (variant == 0) sync.replace(4, 4, number(2));
            if (variant == 1) sync.replace(8, 4, number(5));
            if (variant == 2) sync[72] = 1;
            if (variant == 3) sync.replace(88, 4, number(3));
            if (variant == 4) sync.replace(92, 4, number(2));
            if (variant == 5) sync.resize(183);
            HotseatObserver observer;
            QVERIFY(!observer.feed(nativeHeader() + packet(0x23, sync)));
            QVERIFY(observer.takeEvents().isEmpty());
        }
        for (quint32 version : {4U, 8U}) {
            auto header = nativeHeader(); header.replace(16, 4, number(version));
            HotseatObserver observer; QVERIFY(!observer.feed(header));
        }
    }
    void modeNeedsCompleteNativeAcknowledgement() {
        HotseatObserver observer;
        QVERIFY(observer.feed(nativeHeader() + packet(0x23, nativeSync())));
        observer.takeEvents();
        const auto release = nativeMode(0, false, 104);
        QVERIFY(observer.feed(release.first(release.size() - 1)));
        QVERIFY(observer.takeEvents().isEmpty());
        QVERIFY(observer.feed(release.last(1)));
        QCOMPARE(observer.takeEvents().first().owner, -1);
        const auto acquire = nativeMode(1, true, 111);
        QVERIFY(observer.feed(acquire)); QCOMPARE(observer.takeEvents().first().owner, 1);
        QVERIFY(!observer.feed(acquire)); // duplicate is not another accepted handoff
    }
    void nearbyObserverForwardsStockProtocolWithoutInventingPasswordOrInputPackets() {
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        NetplayClient bridge; QSignalSpy errors(&bridge, &NetplayClient::failed);
        QSignalSpy ownership(&bridge, &NetplayClient::hotseatOwnership);
        const auto port = bridge.startHotseat("127.0.0.1", server.serverPort(), {}, {}, false);
        QVERIFY(port);
        QTcpSocket emulator; emulator.connectToHost(QHostAddress::LocalHost, port);
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QTcpSocket> host(server.nextPendingConnection());
        auto header = nativeHeader(); header.replace(12, 4, number(0));
        emulator.write(header);
        QTRY_COMPARE(host->bytesAvailable(), 24); QCOMPARE(host->readAll(), header);
        const auto sync = packet(0x23, nativeSync());
        const auto nick = packet(0x20, QByteArray(32, 'n'));
        host->write(header + nick + sync);
        QTRY_COMPARE(emulator.bytesAvailable(), header.size() + nick.size() + sync.size());
        QCOMPARE(emulator.readAll(), header + nick + sync);
        QTRY_COMPARE(ownership.size(), 1);
        QCOMPARE(ownership[0][2].toInt(), 0); QVERIFY(ownership[0][3].toBool());
        emulator.write(nick);
        QTRY_COMPARE(host->bytesAvailable(), nick.size()); QCOMPARE(host->readAll(), nick);
        const auto release = nativeMode(0, false, 101), acquire = nativeMode(1, true, 110);
        host->write(release + acquire);
        QTRY_COMPARE(ownership.size(), 3);
        QCOMPARE(ownership[1][2].toInt(), -1); QCOMPARE(ownership[2][2].toInt(), 1);
        QTRY_COMPARE(emulator.bytesAvailable(), release.size() + acquire.size());
        QCOMPARE(emulator.readAll(), release + acquire);
        QVERIFY(errors.isEmpty()); bridge.stop();
    }
    void hotseatStillRequiresOnlineAdmissionAndRejectsUnexpectedNearbyPassword() {
        NetplayClient bridge;
        QCOMPARE(bridge.startHotseat("127.0.0.1", 55435, {}, "secret", true), 0);
        QCOMPARE(bridge.startHotseat("127.0.0.1", 55435, "AQIDBAUGBwgJCgsM", {}, true), 0);
        QCOMPARE(bridge.startHotseat("127.0.0.1", 55435, {}, "secret", false), 0);
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        QSignalSpy errors(&bridge, &NetplayClient::failed);
        QTcpSocket emulator;
        const auto port = bridge.startHotseat("127.0.0.1", server.serverPort(), {}, {}, false);
        QVERIFY(port); emulator.connectToHost(QHostAddress::LocalHost, port);
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QTcpSocket> host(server.nextPendingConnection()); host->write(nativeHeader());
        QTRY_COMPARE(errors.size(), 1);
        QTRY_COMPARE(emulator.state(), QAbstractSocket::UnconnectedState);
    }
    void ownershipCallbackCannotLeakBufferedEventsIntoANewSession() {
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        NetplayClient bridge;QSignalSpy ownership(&bridge,&NetplayClient::hotseatOwnership);
        quint16 replacement=0;
        connect(&bridge,&NetplayClient::hotseatOwnership,&bridge,[&](quint64,quint32,int,bool initial){
            if(initial)replacement=bridge.startHotseat("127.0.0.1",server.serverPort(),{},{},false);
        });
        QTcpSocket emulator;
        emulator.connectToHost(QHostAddress::LocalHost,bridge.startHotseat("127.0.0.1",server.serverPort(),{},{},false));
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QTcpSocket> host(server.nextPendingConnection());
        auto header=nativeHeader();header.replace(12,4,number(0));
        host->write(header+packet(0x23,nativeSync())+nativeMode(0,false,101)+nativeMode(1,true,107));
        QTRY_VERIFY(replacement);QCOMPARE(ownership.size(),1);
        QTRY_COMPARE(emulator.state(),QAbstractSocket::UnconnectedState);
    }
    void fragmentedRelayHandshakeAuthenticatesWithoutPasswordDialog() {
        QTcpServer relay; QVERIFY(relay.listen(QHostAddress::LocalHost));
        NetplayClient bridge; QSignalSpy errors(&bridge, &NetplayClient::failed);
        const auto port = bridge.start("127.0.0.1", relay.serverPort(), "AQIDBAUGBwgJCgsM", "invite-secret");
        QVERIFY(port);
        QTcpSocket emulator; emulator.connectToHost(QHostAddress::LocalHost, port);
        QTRY_VERIFY(relay.hasPendingConnections());
        QScopedPointer<QTcpSocket> host(relay.nextPendingConnection());
        QTRY_COMPARE(host->bytesAvailable(), 16);
        QCOMPARE(host->readAll(), QByteArray("RATS") + QByteArray::fromHex("0102030405060708090a0b0c"));
        const auto clientHeader = QByteArray::fromHex("52414e500000000000000001000000070000000500000000");
        emulator.write(clientHeader.first(3)); emulator.flush(); QTest::qWait(10);
        QCOMPARE(host->bytesAvailable(), 0);
        emulator.write(clientHeader.mid(3));
        QTRY_COMPARE(host->bytesAvailable(), 24); QCOMPARE(host->readAll(), clientHeader);
        const auto hostHeader = QByteArray::fromHex("52414e500000000000000001a1b2c3d40000000700000000");
        host->write(hostHeader.first(13)); host->flush(); QTest::qWait(10);
        QCOMPARE(emulator.bytesAvailable(), 0);
        const auto nick = QByteArray::fromHex("0000002000000020") + QByteArray("Trainer").leftJustified(32, '\0');
        host->write(hostHeader.mid(13) + nick);
        QTRY_COMPARE(emulator.bytesAvailable(), 64);
        auto expectedHeader = hostHeader; expectedHeader.replace(12, 4, QByteArray(4, '\0'));
        QCOMPARE(emulator.readAll(), expectedHeader + nick);
        emulator.write(nick.first(7)); emulator.flush(); QTest::qWait(10);
        QCOMPARE(host->bytesAvailable(), 0);
        emulator.write(nick.mid(7));
        QTRY_COMPARE(host->bytesAvailable(), 112);
        QCOMPARE(host->readAll(), nick + QByteArray::fromHex("0000002100000040")
            + "9852f36785a4c6a5e01716285162bab44d7d2a4db1b55fe2704aba23d2660256");
        const QByteArray sync(128 * 1024, 's'); host->write(sync);
        QTRY_COMPARE(emulator.bytesAvailable(), sync.size()); QCOMPARE(emulator.readAll(), sync);
        emulator.write("game-input"); QTRY_COMPARE(host->bytesAvailable(), 10);
        QCOMPARE(host->readAll(), QByteArray("game-input")); QVERIFY(errors.isEmpty());
        host->disconnectFromHost(); QTRY_COMPARE(errors.size(), 1);
        QCOMPARE(errors.first().first().toString(),QString("The multiplayer connection ended."));
        QTRY_COMPARE(emulator.state(), QAbstractSocket::UnconnectedState);
        QVERIFY(bridge.start("127.0.0.1", relay.serverPort(), {}, "new-invitation"));
        bridge.stop(); QCOMPARE(errors.size(), 1); // intentional cleanup is silent
    }
    void missingPasswordChallengeFailsClosed() {
        QTcpServer host; QVERIFY(host.listen(QHostAddress::LocalHost));
        NetplayClient bridge; QSignalSpy errors(&bridge, &NetplayClient::failed);
        QTcpSocket emulator;
        emulator.connectToHost(QHostAddress::LocalHost, bridge.start("127.0.0.1", host.serverPort(), {}, "secret"));
        QTRY_VERIFY(host.hasPendingConnections());
        QScopedPointer<QTcpSocket> peer(host.nextPendingConnection());
        peer->write(QByteArray::fromHex("52414e500000000000000001000000000000000700000000"));
        QTRY_COMPARE(errors.size(), 1);
        QTRY_COMPARE(emulator.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(emulator.bytesAvailable(), 0);
    }
    void slowReceiverRetainsLargeStates_data() {
        QTest::addColumn<bool>("fromHost");
        QTest::newRow("host-state-to-local-emulator") << true;
        QTest::newRow("local-emulator-to-host") << false;
    }
    void slowReceiverRetainsLargeStates() {
        QFETCH(bool, fromHost);
        QTcpServer server; QVERIFY(server.listen(QHostAddress::LocalHost));
        NetplayClient bridge; QSignalSpy errors(&bridge, &NetplayClient::failed);
        QTcpSocket emulator;
        emulator.connectToHost(QHostAddress::LocalHost,
            bridge.start("127.0.0.1", server.serverPort(), {}, "secret"));
        QTRY_VERIFY(server.hasPendingConnections());
        QScopedPointer<QTcpSocket> host(server.nextPendingConnection());
        const auto header = nativeHeader();
        const auto nick = packet(0x20, QByteArray(32, 'n'));
        emulator.write(header + nick); host->write(header);
        QTRY_COMPARE(host->bytesAvailable(), 24 + 40 + 72);
        host->readAll();
        QTRY_COMPARE(emulator.bytesAvailable(), 24); emulator.readAll();
        auto* sender = fromHost ? host.data() : &emulator;
        auto* receiver = fromHost ? &emulator : host.data();
        receiver->setReadBufferSize(1); // deliberately stop draining the stream
        const auto payload = packet(0x42, QByteArray(32 * 1024 * 1024, 's'));
        QCOMPARE(sender->write(payload), payload.size());
        QTest::qWait(350);
        QVERIFY2(errors.isEmpty(), "A slow reader must not disconnect a valid state transfer");
        QVERIFY(sender->bytesToWrite() > 0); // bounded bridge propagates pressure
        receiver->setReadBufferSize(256 * 1024);
        QByteArray received;
        QTRY_VERIFY_WITH_TIMEOUT(([&]{
            received += receiver->readAll();
            return received.size() == payload.size() || !errors.isEmpty();
        }()), 15000);
        QVERIFY(errors.isEmpty()); QCOMPARE(received, payload);
        sender->write("after-state");
        QTRY_COMPARE(receiver->bytesAvailable(), 11);
        QCOMPARE(receiver->readAll(), QByteArray("after-state"));
        bridge.stop(); QVERIFY(errors.isEmpty());
    }
    void invalidInvitationDoesNotOpenListener() {
        NetplayClient bridge;
        QCOMPARE(bridge.start("host;command", 55435, {}, "secret"), 0);
        QCOMPARE(bridge.start("127.0.0.1", 55435, "bad-room", "secret"), 0);
        QCOMPARE(bridge.start("127.0.0.1", 55435, {}, {}), 0);
    }
};
QTEST_GUILESS_MAIN(NetplayClientTests)
#include "NetplayClientTests.moc"
