#include "integrations/adventure/retroarch/RetroArchNetplayClient.h"
#include <QtTest>

using trainer::retroarch::NetplayClient;
class NetplayClientTests : public QObject {
    Q_OBJECT
private slots:
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
    void invalidInvitationDoesNotOpenListener() {
        NetplayClient bridge;
        QCOMPARE(bridge.start("host;command", 55435, {}, "secret"), 0);
        QCOMPARE(bridge.start("127.0.0.1", 55435, "bad-room", "secret"), 0);
        QCOMPARE(bridge.start("127.0.0.1", 55435, {}, {}), 0);
    }
};
QTEST_GUILESS_MAIN(NetplayClientTests)
#include "NetplayClientTests.moc"
