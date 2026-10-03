#include <QtTest>
#include "features/adventure/GameParty.h"

namespace trainer {
class GamePartyTests : public QObject {
    Q_OBJECT
    static QJsonObject game(int players=4) {
        return {{"id","runtime.test"},{"label","Test cartridge"},{"content","exact-digest"},{"players",players}};
    }
    struct Room {
        GameParty a,b,c,d,e;
        QMap<QString,GameParty*> people{{"a",&a},{"b",&b},{"c",&c},{"d",&d},{"e",&e}};
        Room(int players=4) {
            for(auto it=people.cbegin();it!=people.cend();++it) {
                const auto id=it.key();auto* node=it.value();node->configure(id,{game(players)},id=="a"?game(players):QJsonObject{},true);
                QObject::connect(node,&GameParty::outgoing,node,[this,id](QString target,QJsonObject p){
                    if(people.contains(target))people[target]->receive(id,id,p);
                });
            }
        }
    };
private slots:
    void backgroundFailureIsQuietButRequestedInviteReportsFailure() {
        GameParty a;a.configure("Trainer",{game()},game(),true);
        QSignalSpy notices(&a,&GameParty::notice),packets(&a,&GameParty::outgoing);
        a.query("online:friend");a.deliveryFailed("online:friend");
        QCOMPARE(notices.count(),0);a.query("online:friend");QCOMPARE(packets.count(),1);
        a.invite("online:other");a.deliveryFailed("online:other");
        QCOMPARE(notices.count(),1);QCOMPARE(a.state()["members"].toArray().size(),1);
    }
    void fourMembersWaitForExplicitStartAndReceiveSameEndpoint() {
        Room r;QSignalSpy host(&r.a,&GameParty::startRequested),b(&r.b,&GameParty::startRequested),c(&r.c,&GameParty::startRequested),d(&r.d,&GameParty::startRequested);
        for(const auto& id:{"b","c","d"}){r.a.invite(id);QVERIFY(!r.people[id]->pending().isEmpty());r.people[id]->answer(true);}
        QCOMPARE(r.a.state()["free"].toInt(),0);QCOMPARE(host.count(),0);QCOMPARE(b.count(),0);
        QCOMPARE(r.b.state()["members"].toArray().size(),4);
        r.a.invite("e");QVERIFY(r.e.pending().isEmpty());
        r.a.start();QCOMPARE(host.count(),1);QCOMPARE(b.count(),0);
        r.a.ready({{"address","relay.test"},{"kind","ready"},{"port",55435}});
        for(auto* spy:{&b,&c,&d}){QCOMPARE(spy->count(),1);QCOMPARE(spy->first()[1].toJsonObject()["address"].toString(),QString("relay.test"));}
        QCOMPARE(b.first()[1].toJsonObject()["slot"].toInt(),2);
        QCOMPARE(c.first()[1].toJsonObject()["slot"].toInt(),3);
        QCOMPARE(d.first()[1].toJsonObject()["slot"].toInt(),4);
        r.a.ready({{"address","relay.test"}});QCOMPARE(b.count(),1);
    }
    void capacityCancellationAndDeclineReleaseOnlyReservedSlot() {
        Room r(2);r.a.invite("b");r.a.invite("c");QVERIFY(r.c.pending().isEmpty());
        r.a.cancelInvite("b");QVERIFY(r.b.pending().isEmpty());QCOMPARE(r.a.state()["free"].toInt(),1);
        r.a.invite("c");r.c.answer(false);QCOMPARE(r.a.state()["free"].toInt(),1);
        r.a.invite("b");r.b.answer(true);r.a.cancelInvite("b");QCOMPARE(r.a.state()["free"].toInt(),0);
        r.b.leave();QCOMPARE(r.a.state()["free"].toInt(),1);
    }
    void reverseRequestNeedsHostAcceptanceAndMatchingEdition() {
        Room r;r.b.query("a");QVERIFY(r.b.offer("a")["joinable"].toBool());
        r.b.requestJoin("a");QVERIFY(r.a.pending()["joining"].toBool());QVERIFY(!r.b.active());
        r.a.answer(true);QVERIFY(r.b.active());QCOMPARE(r.b.state()["members"].toArray().size(),2);
        r.c.configure("c",{QJsonObject{{"id","runtime.test"},{"content","other-edition"}}},{},true);
        r.c.query("a");r.c.requestJoin("a");QVERIFY(r.a.pending().isEmpty());QVERIFY(!r.c.active());
    }
    void lateAdmissionCannotSubstituteGameAndReplayCannotReprompt() {
        Room r;QJsonObject invite;
        connect(&r.a,&GameParty::outgoing,&r.a,[&](QString,QJsonObject p){if(p["kind"]=="invite")invite=p;});
        r.a.invite("b");r.b.answer(false);r.b.receive("a","a",invite);QVERIFY(r.b.pending().isEmpty());
        r.b.query("a");r.b.requestJoin("a");
        auto admitted=invite;admitted["kind"]="admitted";admitted["request"]=r.b.joiningId_;
        admitted["game"]=QJsonObject{{"id","runtime.other"}};r.b.receive("a","a",admitted);QVERIFY(!r.b.active());
        r.b.leave();r.a.answer(true);QVERIFY(!r.b.active());
    }
    void expiredRequestsBootAndTransportLossDoNotLaunchOrFreeLiveSeats() {
        Room r;r.a.invite("b");r.b.answer(true);r.a.start();r.a.ready({{"address","relay.test"}});
        r.a.disconnected("b");QVERIFY(r.a.running());QCOMPARE(r.a.state()["members"].toArray().size(),2);
        r.b.disconnected("a");QVERIFY(r.b.running());
        QSignalSpy changes(&r.c,&GameParty::changed);r.c.tick();QCOMPARE(changes.count(),0);
        r.a.invite("c");r.a.members_["c"].expires=1;r.a.tick();QVERIFY(r.c.pending().isEmpty());
        r.a.leave();QVERIFY(!r.a.active());
    }
    void envelopesAreBoundedAndSeparateFromSavedLink() {
        Room r;QJsonObject sent;connect(&r.a,&GameParty::outgoing,&r.a,[&](QString,QJsonObject p){sent=p;});
        r.a.query("b");QCOMPARE(GameParty::decode(GameParty::encode(sent)),sent);
        sent["ns"]="org.traineros.link";QVERIFY(GameParty::decode(GameParty::encode(sent)).isEmpty());
        QVERIFY(GameParty::decode(QString(2001,'a')).isEmpty());
    }
};
}
QTEST_GUILESS_MAIN(trainer::GamePartyTests)
#include "GamePartyTests.moc"
