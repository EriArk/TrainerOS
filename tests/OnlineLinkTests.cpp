#include <QtTest>
#include "integrations/social/OnlineLink.h"
#include <QDateTime>
namespace trainer {
class OnlineLinkTests : public QObject {
    Q_OBJECT
    const QString a="11111111-1111-4111-8111-111111111111",b="22222222-2222-4222-8222-222222222222";
    QJsonObject trade{{"id","org.traineros.emerald.trade"},{"family","systemActivity"},{"version",1},{"build","exact"},{"schema","individual-1"}};
    struct Packet {OnlineLink* target;QString author;QJsonObject data;};
    QList<Packet> packets;
    void pair(OnlineLink& first,OnlineLink& second) {
        first.bind("100",a);second.bind("200",b);first.setAvailable(true);second.setAvailable(true);
        first.setCapabilities({trade});second.setCapabilities({trade});
        connect(&first,&OnlineLink::outgoing,this,[&,this](QString,QString content){QVERIFY(content.size()<=2000);packets.append({&second,"100",OnlineLink::decode(content)});});
        connect(&second,&OnlineLink::outgoing,this,[&,this](QString,QString content){packets.append({&first,"200",OnlineLink::decode(content)});});
    }
    void deliver(){int n=0;while(!packets.isEmpty()&&n++<100){const auto p=packets.takeFirst();p.target->receive("300",p.author,"Friend",p.data);}}
private slots:
    void init(){packets.clear();}
    void simultaneousChecksAndProtectedSurface() {
        OnlineLink first,second;pair(first,second);
        first.probe("300","200");second.probe("300","100");deliver();
        QCOMPARE(first.state()["stage"].toString(),"available");
        first.invite(trade["id"].toString());deliver();QVERIFY(second.state()["incoming"].toBool());
        second.setAvailable(false);deliver();
        QVERIFY(!first.state()["open"].toBool());QVERIFY(!second.state()["open"].toBool());
    }
    void incompleteDeliveryExpiresWithoutExecutingOrReplaying() {
        OnlineLink first,second;pair(first,second);first.probe("300","200");deliver();
        first.invite(trade["id"].toString());deliver();second.answer(true);deliver();
        QSignalSpy received(&second,&OnlineLink::frameReceived),ended(&second,&OnlineLink::ended);
        first.sendFrame({{"type","offer"},{"data",QString(3000,'x')}});
        const auto partial=packets.takeFirst();packets.clear();second.receive("300","100","Friend",partial.data);
        QVERIFY(received.isEmpty());second.deadline_=QDateTime::currentSecsSinceEpoch()-1;second.tick();
        QCOMPARE(ended.size(),1);second.receive("300","100","Friend",partial.data);QVERIFY(received.isEmpty());
    }
    void independentFamiliesAndConsent() {
        OnlineLink first,second;pair(first,second);QSignalSpy started(&first,&OnlineLink::established),incoming(&second,&OnlineLink::established);
        first.probe("300","200");deliver();QCOMPARE(first.state()["stage"].toString(),"available");QCOMPARE(first.state()["actions"].toList().size(),1);
        QVERIFY(started.isEmpty());first.invite(trade["id"].toString());deliver();QVERIFY(second.state()["incoming"].toBool());QVERIFY(incoming.isEmpty());
        second.answer(true);deliver();QCOMPARE(started.size(),1);QCOMPARE(incoming.size(),1);
        QCOMPARE(started[0][0].toString(),incoming[0][1].toString());
        QCOMPARE(first.state()["stage"].toString(),"connected");
        // An unavailable runtime route must not remove the supported system action.
        QCOMPARE(trade["family"].toString(),"systemActivity");
    }
    void staleOwnerEndpointAudienceAndReplayNeverStart() {
        OnlineLink first,second;pair(first,second);QSignalSpy started(&second,&OnlineLink::established);
        first.probe("300","200");deliver();first.invite(trade["id"].toString());const auto original=packets.takeFirst();
        for(const auto& key:QStringList{"to","target","request","endpoint","boot"}){auto bad=original.data;bad[key]="invalid";second.receive("300","100","Friend",bad);QVERIFY(!second.state()["open"].toBool());}
        auto expired=original.data;expired["expires"]=QDateTime::currentSecsSinceEpoch()-1;second.receive("300","100","Friend",expired);QVERIFY(!second.state()["open"].toBool());
        second.receive("300","999","Impostor",original.data);QVERIFY(!second.state()["open"].toBool());
        second.receive("301","100","Friend",original.data);QVERIFY(!second.state()["open"].toBool());
        second.receive("300","100","Friend",original.data);second.answer(false);deliver();QVERIFY(started.isEmpty());
        second.receive("300","100","Friend",original.data);QVERIFY(!second.state()["open"].toBool());
        second.bind("200",b);second.receive("300","100","Friend",original.data);QVERIFY(!second.state()["open"].toBool());
    }
    void reorderedFragmentsAndDuplicatesDeliverOnce() {
        OnlineLink first,second;pair(first,second);first.probe("300","200");deliver();first.invite(trade["id"].toString());deliver();second.answer(true);deliver();
        QSignalSpy received(&second,&OnlineLink::frameReceived);
        QJsonObject payload{{"type","offer"},{"data",QString(3000,'x')}};first.sendFrame(payload);QVERIFY(packets.size()>1);
        const auto duplicate=packets.first();std::reverse(packets.begin(),packets.end());deliver();
        QCOMPARE(received.size(),1);QCOMPARE(received[0][0].toJsonObject(),payload);
        second.receive("300","100","Friend",duplicate.data);QCOMPARE(received.size(),1);
        first.sendFrame({{"type","receipt"}});deliver();QCOMPARE(received.size(),2);
        first.sendFrame({{"data",QString(13000,'x')}});QCOMPARE(first.state()["stage"].toString(),"idle");
    }
    void mismatchedBuildAndReadOnlyRemoveActions() {
        OnlineLink first,second;pair(first,second);auto other=trade;other["build"]="different";second.setCapabilities({other});
        first.probe("300","200");deliver();QVERIFY(first.state()["actions"].toList().isEmpty());first.invite(trade["id"].toString());QVERIFY(packets.isEmpty());
        first.setCapabilities({});QVERIFY(first.state()["actions"].toList().isEmpty());
    }
    void multipleDevicesSelectOnlyRespondingEndpoint() {
        OnlineLink first,second,third;pair(first,second);third.bind("200","33333333-3333-4333-8333-333333333333");third.setAvailable(true);third.setCapabilities({trade});
        first.probe("300","200");const auto probe=packets.first();third.receive("300","100","Friend",probe.data);deliver();
        first.invite(trade["id"].toString());const auto invitation=packets.first();third.receive("300","100","Friend",invitation.data);QVERIFY(!third.state()["open"].toBool());deliver();QVERIFY(second.state()["open"].toBool());
    }
};
}
using trainer::OnlineLinkTests;
QTEST_GUILESS_MAIN(OnlineLinkTests)
#include "OnlineLinkTests.moc"
