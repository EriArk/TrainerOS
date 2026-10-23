#include "integrations/adventure/retroarch/NetplayRelaySelector.h"
#include "core/model/NetplayRelay.h"
#include "core/model/GamePlayers.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>

namespace trainer::retroarch {
class NetplayRelayTests : public QObject {
    Q_OBJECT
    static QJsonObject scores(int europe,int america,int south=500,int asia=500) {
        return {{"madrid",europe},{"nyc",america},{"saopaulo",south},{"singapore",asia}};
    }
private slots:
    void allPlayersChooseRegionInsteadOfHostOnly() {
        QMap<QString,QJsonObject> reports{{"host",scores(20,90)},{"guest",scores(220,80)}};
        QCOMPARE(NetplayRelaySelector::choose({"host","guest"},reports,"auto"),"nyc");
        reports["third"]=scores(40,300);
        QCOMPARE(NetplayRelaySelector::choose({"host","guest","third"},reports,"auto"),"madrid");
        QCOMPARE(NetplayRelaySelector::choose({"host","guest"},reports,"madrid"),"madrid");
        QVERIFY(NetplayRelaySelector::choose({"host","missing"},reports,"auto").isEmpty());
    }
    void unreachableManualRegionDoesNotSilentlySwitch() {
        QMap<QString,QJsonObject> reports{{"host",scores(10,80)},{"guest",scores(-1,90)}};
        QVERIFY(NetplayRelaySelector::choose({"host","guest"},reports,"madrid").isEmpty());
        QCOMPARE(NetplayRelaySelector::choose({"host","guest"},reports,"auto"),"nyc");
        QVERIFY(NetplayRelaySelector::choose({"host"},reports,"custom-host").isEmpty());
    }
    void reportsAreBoundedRosterScopedAndCancellationSafe() {
        NetplayRelaySelector selector;selector.self_=selector.owner_="host";selector.players_={"host","guest"};selector.preference_="auto";
        selector.pending_={"madrid"};selector.reports_["host"]=scores(20,90);
        selector.receive("outsider",scores(5,5));QCOMPARE(selector.reports_.size(),1);
        auto malformed=scores(5,5);malformed["madrid"]=1e100;selector.receive("guest",malformed);QCOMPARE(selector.reports_.size(),1);
        selector.receive("guest",scores(30,70));QCOMPARE(selector.reports_.size(),2);
        selector.receive("guest",scores(1,1));QCOMPARE(selector.reports_["guest"]["madrid"].toInt(),30);
        const auto generation=selector.generation_;selector.stop();
        selector.result("madrid","127.0.0.1|1234",10,generation);QVERIFY(selector.local_.isEmpty());
        selector.receive("guest",scores(1,1));QVERIFY(selector.reports_.isEmpty());
    }
    void actualDirectoryAndTcpSamplesCompleteWithoutGameSession() {
        QTcpServer directory,relay;QVERIFY(directory.listen(QHostAddress::LocalHost));QVERIFY(relay.listen(QHostAddress::LocalHost));
        int connections=0;
        connect(&relay,&QTcpServer::newConnection,&relay,[&]{while(auto* socket=relay.nextPendingConnection()){++connections;socket->disconnectFromHost();socket->deleteLater();}});
        connect(&directory,&QTcpServer::newConnection,&directory,[&]{
            while(auto* socket=directory.nextPendingConnection())connect(socket,&QTcpSocket::readyRead,socket,[&,socket]{
                socket->readAll();const QByteArray body="status=OK\ntunnel_addr=127.0.0.1\ntunnel_port="+QByteArray::number(relay.serverPort())+"\n";
                socket->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);
                socket->disconnectFromHost();connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            });
        });
        NetplayRelaySelector selector,guest;selector.directory_="http://127.0.0.1:"+QString::number(directory.serverPort())+"/?name=";guest.directory_=selector.directory_;
        QSignalSpy selected(&selector,&NetplayRelaySelector::selected),failed(&selector,&NetplayRelaySelector::failed);
        QSignalSpy report(&guest,&NetplayRelaySelector::report);
        selector.start("host","host",{"host","guest"},"auto");
        guest.start("guest","host",{"host","guest"},"auto");
        QTRY_VERIFY_WITH_TIMEOUT(!report.isEmpty()&&selector.pending_.isEmpty(),5000);
        QCOMPARE(selected.size(),0); // The host cannot select using its measurements alone.
        selector.receive("outsider",report.first()[1].toJsonObject());QCOMPARE(selected.size(),0);
        selector.receive("guest",report.first()[1].toJsonObject());
        QCOMPARE(selected.size(),1);QCOMPARE(failed.size(),0);QCOMPARE(connections,24);
        QCOMPARE(selected.first()[1].toString(),"127.0.0.1|"+QString::number(relay.serverPort()));
        selector.stop();guest.stop();QVERIFY(!guest.repeat_.isActive());QVERIFY(!selector.deadline_.isActive());
    }
    void binaryDifferencesDoNotGrantDifferentGameOrMode() {
        QJsonObject arm{{"id","runtime.retroarch.psx.pcsx_rearmed.v1"},{"content","same-rom"},{"firmware","same-bios"},
            {"settings","pcsx-v2"},{"runtime","arm-binary"},{"core","arm-core"},{"players",2},{"controllerModes",QJsonArray{"separate","pass-p1"}}};
        auto x86=arm;x86["runtime"]="x86-binary";x86["core"]="x86-core";x86.remove("controllerModes");
        QVERIFY(supportsMultiplayerSession(x86,arm));
        for(const auto& key:{"content","firmware","settings","id"}) {auto other=x86;other[key]="different";QVERIFY(!supportsMultiplayerSession(other,arm));}
        auto handoff=multiplayerModeProfile(arm,"pass-p1");QVERIFY(!supportsMultiplayerSession(x86,handoff));
        auto standalone=arm;standalone["id"]="runtime.dolphin.gc.v1";auto other=standalone;other["runtime"]="x86";
        QVERIFY(supportsMultiplayerSession(other,standalone));other["settings"]="incompatible";QVERIFY(!supportsMultiplayerSession(other,standalone));
    }
};
}
QTEST_GUILESS_MAIN(trainer::retroarch::NetplayRelayTests)
#include "NetplayRelayTests.moc"
