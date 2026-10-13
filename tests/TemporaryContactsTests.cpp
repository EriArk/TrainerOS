#include <QtTest>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QUuid>
#include "features/social/TemporaryContacts.h"

namespace trainer {
class TemporaryContactsTests final : public QObject {
    Q_OBJECT
    struct Pair {
        QTemporaryDir dir;
        TemporaryContacts host,guest;
        QJsonObject h,g,room;
        qint64 now=QDateTime::currentSecsSinceEpoch();
        int posts=0,accepts=0,deletes=0,gets=0;
        int postStatus=200,deleteStatus=204;
        bool dropReply=false,dropGone=false,dropKeep=false;
        TemporaryContacts::Completion delayed;
        bool delayGet=false;
        Pair() {
            room={{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"owner","1"},{"phase","choosing"},
                {"members",QJsonArray{QJsonObject{{"id","1"},{"connected",true}},QJsonObject{{"id","2"},{"connected",true}}}}};
            host.configure(dir.path()+"/host.json","1");guest.configure(dir.path()+"/guest.json","2");
            host.setRequest([this](auto m,auto p,auto b,auto done){request(true,m,p,b,done);});
            guest.setRequest([this](auto m,auto p,auto b,auto done){request(false,m,p,b,done);});
            QObject::connect(&host,&TemporaryContacts::outgoing,&guest,[this](QString peer,QJsonObject p){
                QCOMPARE(peer,"2");if(dropGone&&p["data"].toObject()["op"]=="gone")return;guest.receive("1",p);
            });
            QObject::connect(&guest,&TemporaryContacts::outgoing,&host,[this](QString peer,QJsonObject p){
                QCOMPARE(peer,"1");if(dropKeep&&p["data"].toObject()["op"]=="keep")return;host.receive("2",p);
            });
            host.context(room,true);guest.context(room,true);
        }
        QJsonObject relation(QString peer,int type) {
            return {{"id",peer},{"type",type},{"since",QDateTime::fromSecsSinceEpoch(now,Qt::UTC).toString(Qt::ISODateWithMs)}};
        }
        void request(bool owner,QByteArray method,QString path,QJsonObject body,TemporaryContacts::Completion done) {
            QVERIFY(path.startsWith("/v1/users/@me/relationships"));
            auto& local=owner?h:g;
            if(method=="GET") {
                ++gets;QJsonArray rows;if(!local.isEmpty())rows.append(local);
                if(delayGet){delayGet=false;delayed=[done,rows](auto){done({200,QJsonDocument(rows)});};return;}
                done({200,QJsonDocument(rows)});return;
            }
            if(method=="POST") {
                QVERIFY(owner);++posts;
                if(postStatus!=200) {done({postStatus,{},30});return;}
                h=relation("2",4);g=relation("1",3);
                if(dropReply)return;
                done({200,QJsonDocument(h)});return;
            }
            if(method=="PUT") {
                QVERIFY(!owner);QCOMPARE(body["type"].toInt(),1);++accepts;
                h=relation("2",1);g=relation("1",1);done({200,QJsonDocument(g)});return;
            }
            QCOMPARE(method,QByteArray("DELETE"));QVERIFY(owner);++deletes;
            if(deleteStatus==204) {h={};g={};}
            done({deleteStatus,{},30});
        }
        void run(int seconds=80) {for(int i=0;i<seconds;++i){++now;host.tick(now);guest.tick(now);}}
        void leave() {host.context({},true);guest.context({},true);}
        QString phase(bool owner) const {return (owner?host.records_["2"]:guest.records_["1"]).toObject()["phase"].toString();}
    };
private slots:
    void recoveryDefersCleanupUntilMembershipIsResolved() {
        Pair p;p.run();QCOMPARE(p.phase(true),"friend");
        auto pending=p.room;pending["recovering"]=true;p.guest.context(pending,true);
        p.host.context({},true);p.run(40);QCOMPARE(p.deletes,0);
        QVERIFY(p.guest.records_["1"].toObject()["releaseRequested"].toBool());
        p.guest.context({},true);p.run(40);QCOMPARE(p.deletes,1);
    }
    void automaticRoundTripAndCleanupRequireBothMembers() {
        Pair p;p.run();QCOMPARE(p.posts,1);QCOMPARE(p.accepts,1);QCOMPARE(p.deletes,0);
        QCOMPARE(p.phase(true),"friend");QCOMPARE(p.phase(false),"friend");
        QVERIFY(p.host.state()["peers"].toMap()["2"].toMap()["canKeep"].toBool());
        // A game ends/changes, while membership and the contact stay intact.
        p.room["phase"]="gathering";p.host.context(p.room,true);p.guest.context(p.room,true);p.run(20);QCOMPARE(p.deletes,0);
        p.leave();p.run();QCOMPARE(p.deletes,1);QVERIFY(p.h.isEmpty());QVERIFY(p.g.isEmpty());
        QVERIFY(p.host.records_.isEmpty());QCOMPARE(p.phase(false),"done");
    }
    void neitherInvitationNorUnrelatedIncomingRequestAuthorizesAutomation() {
        Pair p;p.host.context({},true);p.guest.context({},true);p.g=p.relation("8",3);p.run();QCOMPARE(p.posts,0);QCOMPARE(p.accepts,0);
        QJsonObject offer{{"ns","org.traineros.lobby"},{"v",1},{"kind","contact"},{"lobby",p.room["id"]},{"expires",p.now+60},
            {"data",QJsonObject{{"op","offer"},{"lease",QUuid::createUuid().toString(QUuid::WithoutBraces)}}}};
        p.guest.receive("1",offer);p.run();QVERIFY(p.guest.records_.isEmpty());QCOMPARE(p.accepts,0);
    }
    void preexistingFriendsRequestsAndBlocksAreNeverOwned() {
        for(int type:{1,2,3,4}) {
            Pair p;p.h=p.relation("2",type);p.g=p.relation("1",type==3?4:type==4?3:type);p.run();p.leave();p.run();
            QCOMPARE(p.posts,0);QCOMPARE(p.accepts,0);QCOMPARE(p.deletes,0);QCOMPARE(p.h["type"].toInt(),type);
        }
    }
    void eitherSideCanKeepFriendshipAndGuestWaitsForAcknowledgement() {
        for(bool owner:{true,false}) {
            Pair p;p.run();p.dropKeep=!owner;
            QVERIFY(owner?p.host.keep("2"):p.guest.keep("1"));
            if(!owner) {QCOMPARE(p.phase(false),"keep");p.run(20);QCOMPARE(p.phase(true),"friend");p.dropKeep=false;p.run(20);}
            QCOMPARE(p.phase(true),"kept");QCOMPARE(p.phase(false),"kept");
            p.leave();p.run();QCOMPARE(p.deletes,0);QCOMPARE(p.h["type"].toInt(),1);
        }
    }
    void crashReloadRecoversOwnedFriendAndDoesNotNeedNewRequests() {
        Pair p;p.run();p.leave();
        p.host.configure({},{});p.host.configure(p.dir.path()+"/host.json","1");p.host.context({},true);
        p.guest.configure({},{});p.guest.configure(p.dir.path()+"/guest.json","2");p.guest.context({},true);
        p.run();QCOMPARE(p.posts,1);QCOMPARE(p.accepts,1);QCOMPARE(p.deletes,1);
    }
    void droppedCreateReplyIsNotRetriedOrTreatedAsOwnership() {
        Pair p;p.dropReply=true;p.run(100);QCOMPARE(p.posts,1);QCOMPARE(p.phase(true),"uncertain");
        p.leave();p.run();QCOMPARE(p.deletes,0);QCOMPARE(p.accepts,0);
        p.host.configure({},{});p.host.configure(p.dir.path()+"/host.json","1");p.host.context({},true);p.run();QCOMPARE(p.deletes,0);
    }
    void changedRelationshipAndBlockSurviveCleanup() {
        for(int type:{1,2}) {
            Pair p;p.run();p.h=p.relation("2",type);p.leave();p.run();QCOMPARE(p.deletes,0);
            QCOMPARE(p.h["type"].toInt(),type);QCOMPARE(p.phase(true),"uncertain");
        }
    }
    void cleanupWaitsForOfflineGuestAndRetriesLostFinalMessage() {
        Pair p;p.run();p.host.context({},true);p.guest.context({},false);p.run(40);QCOMPARE(p.deletes,0);
        p.guest.context({},true);p.dropGone=true;p.run(40);QCOMPARE(p.deletes,1);QCOMPARE(p.phase(false),"released");
        p.dropGone=false;p.run(40);QCOMPARE(p.phase(false),"done");QVERIFY(p.host.records_.isEmpty());QCOMPARE(p.deletes,1);
    }
    void rateLimitsBackOffAndDeleteFailuresRecheckIdentity() {
        Pair p;p.postStatus=429;p.run(20);QCOMPARE(p.posts,1);p.postStatus=200;p.run();QCOMPARE(p.posts,2);
        p.deleteStatus=503;p.leave();p.run(20);QCOMPARE(p.deletes,1);
        p.h=p.relation("2",2);p.deleteStatus=204;p.run(40);QCOMPARE(p.deletes,1);QCOMPARE(p.h["type"].toInt(),2);
    }
    void delayedReadCannotDeleteAfterKeepOrAccountChange() {
        Pair p;p.run();p.leave();p.delayGet=true;p.run(18);QVERIFY(bool(p.delayed));
        // Cleanup has already been released, so the UI must not claim Keep.
        QVERIFY(!p.host.keep("2"));p.host.configure(p.dir.path()+"/other.json","9");p.host.context({},true);
        p.delayed({});p.run();QCOMPARE(p.deletes,0);
    }
    void unwriteableOrCorruptJournalNeverStartsFriendRequests() {
        Pair p;p.host.configure(p.dir.path(),"1");p.host.context(p.room,true);p.run();QCOMPARE(p.posts,0);
        QVERIFY(!p.host.state()["error"].toString().isEmpty());
    }
    void independentActiveConnectionDefersCleanupWithoutEndingIt() {
        Pair p;p.run();p.guest.setHeld({"1"});p.leave();p.run();QCOMPARE(p.deletes,0);
        p.guest.setHeld({});p.run();QCOMPARE(p.deletes,1);
    }
    void newLobbyCanRetryAnUnfinishedOfferWithoutAdoptingOldPackets() {
        Pair p;p.guest.context({},true);p.run(5);QCOMPARE(p.posts,0);p.leave();p.run(20);
        const auto old=p.host.records_.value("2").toObject();
        p.room["id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
        p.host.context(p.room,true);p.guest.context(p.room,true);p.run();QCOMPARE(p.posts,1);QCOMPARE(p.accepts,1);
        auto forged=QJsonObject{{"ns","org.traineros.lobby"},{"v",1},{"kind","contact"},{"lobby",old["room"]},{"expires",p.now+60},
            {"data",QJsonObject{{"op","remove-ok"},{"lease",old["lease"]}}}};
        p.host.receive("2",forged);p.run();QCOMPARE(p.deletes,0);QCOMPARE(p.phase(true),"friend");
    }
};
}
QTEST_GUILESS_MAIN(trainer::TemporaryContactsTests)
#include "TemporaryContactsTests.moc"
