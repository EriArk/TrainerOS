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
    struct CompanyRoom {
        GameParty a,b,c,d;
        QMap<QString,GameParty*> people{{"a",&a},{"b",&b},{"c",&c},{"d",&d}};
        CompanyRoom() {
            for(auto it=people.begin();it!=people.end();++it) {
                const auto id=it.key();auto* node=it.value();node->configure(id,{game()},game(),true);
                QObject::connect(node,&GameParty::outgoing,node,[this,id](QString target,QJsonObject p){
                    if(target.count(':')==2) {
                        for(auto it=people.begin();it!=people.end();++it)if(it.key()!=id)it.value()->receive(target+":"+id,id,p);
                    } else if(people.contains(target.section(':',-1)))people[target.section(':',-1)]->receive(target.section(':',0,2)+":"+id,id,p);
                });
            }
        }
    };
private slots:
    void nativeRoomClosesAdmissionAtStartWithoutChangingOtherProfiles() {
        Room r;
        auto native=game();native["lateJoin"]=false;
        for(auto it=r.people.begin();it!=r.people.end();++it)it.value()->configure(it.key(),{native},it.key()=="a"?native:QJsonObject{},true);
        r.a.invite("b");r.b.answer(true);r.a.start();
        QCOMPARE(r.a.state()["free"].toInt(),0);
        r.a.invite("c");QVERIFY(r.c.pending().isEmpty());
        r.c.query("a");QVERIFY(!r.c.offer("a")["joinable"].toBool());
        QSignalSpy launched(&r.b,&GameParty::startRequested);
        r.a.ready({{"kind","dolphin-ready"},{"port",2626}});
        QCOMPARE(launched.size(),1);QCOMPARE(launched.first()[1].toJsonObject()["slot"].toInt(),2);
        r.a.leave();r.a.invite("c");QVERIFY(!r.c.pending().isEmpty());
    }
    void independentPartiesShareCompanyWithoutSharingCapacityOrEndpoint() {
        CompanyRoom r;
        QVERIFY(r.a.openCompany("group","request",{}));QVERIFY(r.c.openCompany("group","request",{}));
        QCOMPARE(r.b.companyOffers().size(),2);
        r.b.requestJoin("online:company:group:a");r.d.requestJoin("online:company:group:c");
        QVERIFY(!r.a.pending().isEmpty());QVERIFY(!r.c.pending().isEmpty());
        r.a.answer(true);r.c.answer(true);
        QCOMPARE(r.a.state()["members"].toArray().size(),2);QCOMPARE(r.c.state()["members"].toArray().size(),2);
        QVERIFY(r.a.state()["party"]!=r.c.state()["party"]);
        QSignalSpy b(&r.b,&GameParty::startRequested),d(&r.d,&GameParty::startRequested);
        r.a.start();r.a.ready({{"password","private"},{"address","host-a"}});
        QCOMPARE(b.count(),1);QCOMPARE(d.count(),0);
        for(const auto& v:r.d.companyOffers()){QVERIFY(!v.toObject().contains("endpoint"));QVERIFY(!v.toObject().contains("password"));}
        r.a.leave();QCOMPARE(r.b.companyOffers().size(),1);QVERIFY(r.c.active());
    }
    void onlySelectedCompanyMembersSkipOrganizerPrompt() {
        CompanyRoom r;r.a.openCompany("group","selected",{"b"});
        r.d.requestJoin("online:company:group:a");QVERIFY(!r.a.pending().isEmpty());QVERIFY(!r.d.active());
        r.b.requestJoin("online:company:group:a");QVERIFY(r.b.active());
        QCOMPARE(r.a.pending()["name"].toString(),QString("d")); // did not accept another pending person
        QCOMPARE(r.a.state()["members"].toArray().size(),2);
        r.a.setCompanyAccess("group","closed",{});
        r.c.requestJoin("online:company:group:a");QVERIFY(!r.c.active());
        r.a.answer(true);QVERIFY(!r.d.active());
    }
    void groupAnnouncementsExpireAndOtherCompanyCannotAdmit() {
        CompanyRoom r;r.a.openCompany("group","selected",{"b"});
        QJsonObject advert=r.b.offer("online:company:group:a");
        advert["company"]="other";r.b.receive("online:company:other:a","a",advert);
        r.b.requestJoin("online:company:other:a");QVERIFY(!r.b.active());QVERIFY(r.a.pending().isEmpty());
        r.b.leave();r.b.peers_["online:company:group:a"].expires=1;r.b.tick();
        for(const auto& v:r.b.companyOffers())QVERIFY(v.toObject()["company"]!="group");
        r.a.reset();QVERIFY(r.a.state()["company"].toString().isEmpty());
    }
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
