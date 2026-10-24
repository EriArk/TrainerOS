// SPDX-License-Identifier: GPL-3.0-or-later
#include "../AvatarCards.h"
#include <QtTest>
#include <QTcpServer>
#include <QTemporaryDir>

class AvatarCardsTests:public QObject {
    Q_OBJECT
private slots:
    void peerReplacementWithdrawalAndRevocation(){
        QTemporaryDir dir;QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));
        QFile model(dir.filePath("fixture.glb"));QVERIFY(model.open(QIODevice::WriteOnly));model.write("fixture");model.close();
        QJsonObject descriptor{{"schema",1},{"rig","trainer-humanoid-54-v1"},{"profile","self"},{"glb",model.fileName()},
            {"sha256",QString::fromLatin1(QCryptographicHash::hash("fixture",QCryptographicHash::Sha256).toHex())},
            {"recipe",QJsonObject{{"appearance","local"}}}};
        QFile endpoint(dir.filePath("endpoint.json"));QVERIFY(endpoint.open(QIODevice::WriteOnly));
        endpoint.write(QJsonDocument(QJsonObject{{"protocol",1},{"host","127.0.0.1"},{"port",server.serverPort()},{"token","fixture"}}).toJson());endpoint.close();
        AvatarCards cards("org.example.cards",endpoint.fileName());QList<QJsonObject> sent;
        cards.sendRecord=[&](const QJsonObject& r){sent.append(r);return true;};cards.start();
        QTRY_VERIFY(server.hasPendingConnections());auto* socket=server.nextPendingConnection();QByteArray buffer;QString held;
        auto write=[&](QJsonObject value){socket->write(QJsonDocument(value).toJson(QJsonDocument::Compact)+'\n');};
        auto drain=[&]{buffer+=socket->readAll();while(buffer.contains('\n')){
            auto n=buffer.indexOf('\n');auto p=QJsonDocument::fromJson(buffer.left(n)).object();buffer.remove(0,n+1);
            QJsonObject result;
            if(p["method"]=="hello")result={{"capabilities",QJsonObject{{"avatar.local",1},{"avatar.recipe",1}}},{"context",QJsonObject{{"profile","self"}}}};
            if(p["method"]=="avatar.subscribe")result={{"avatar",descriptor}};
            if(p["method"]=="avatar.resolve"){held=p["id"].toString();continue;}
            write({{"id",p["id"]},{"result",result}});
        }};
        connect(socket,&QTcpSocket::readyRead,&server,drain);drain();QTRY_VERIFY(!cards.local().isEmpty());
        cards.session("friend");QTRY_COMPARE(sent.size(),1);QVERIFY(!sent.last().contains("glb"));
        cards.receive({{"namespace","traineros.avatar.v1"},{"recipe",QJsonObject{{"appearance","old"}}}});QTRY_VERIFY(!held.isEmpty());
        // Withdrawal must fence the in-flight result, including a new session.
        cards.receive({{"namespace","traineros.avatar.v1"},{"recipe",QJsonObject{}}});
        write({{"id",held},{"result",QJsonObject{{"avatar",descriptor}}}});held.clear();QTest::qWait(80);QVERIFY(cards.peer().isEmpty());
        cards.receive({{"namespace","traineros.avatar.v1"},{"recipe",QJsonObject{{"appearance","new"}}}});QTRY_VERIFY(!held.isEmpty());
        write({{"id",held},{"result",QJsonObject{{"avatar",descriptor}}}});held.clear();QTRY_VERIFY(!cards.peer().isEmpty());
        cards.clearPeer();QVERIFY(cards.peer().isEmpty());QVERIFY(!cards.local().isEmpty());
        cards.session("another");QTRY_COMPARE(sent.size(),2);
        write({{"event","context.revoked"}});QTRY_VERIFY(cards.local().isEmpty());QVERIFY(cards.peer().isEmpty());
        QCOMPARE(sent.last()["recipe"].toObject(),QJsonObject{});
    }
};
QTEST_GUILESS_MAIN(AvatarCardsTests)
#include "AvatarCardsTests.moc"
