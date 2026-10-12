#include "features/adventure/HotseatHandoff.h"
#include <QtTest>
#include <QUuid>

namespace trainer {
class HotseatHandoffTests final : public QObject {
    Q_OBJECT
    struct Pair {
        HotseatHandoff host,guest;
        QList<QPair<int,bool>> commands;
        QString session=QUuid::createUuid().toString(QUuid::WithoutBraces);
        quint64 sequence=0;
        Pair() {
            QObject::connect(&host,&HotseatHandoff::outgoing,&guest,[this](int slot,QJsonObject p){if(slot==2)guest.receive(1,p);});
            QObject::connect(&guest,&HotseatHandoff::outgoing,&host,[this](int slot,QJsonObject p){if(slot==1)host.receive(2,p);});
            host.requestNativeRole=[this](bool playing){commands.append({1,playing});return true;};
            guest.requestNativeRole=[this](bool playing){commands.append({2,playing});return true;};
            host.begin(session,1);guest.begin(session,2);
        }
        void observe(int owner,bool initial=false){guest.observed(++sequence,owner,initial);}
        void ready(){observe(0,true);}
    };
private slots:
    void nativeHostAcquisitionCanOvertakeItsProviderPhase_data() {
        QTest::addColumn<bool>("idleFirst");
        QTest::newRow("native-then-acquiring-then-idle")<<false;
        QTest::newRow("native-then-idle-then-delayed-acquiring")<<true;
    }
    void nativeHostAcquisitionCanOvertakeItsProviderPhase() {
        QFETCH(bool,idleFirst);
        Pair p;p.ready();p.host.pass();p.observe(-1);p.observe(1);
        QVERIFY(p.guest.canPass());
        disconnect(&p.host,&HotseatHandoff::outgoing,&p.guest,nullptr);
        QList<QJsonObject> delayed;
        connect(&p.host,&HotseatHandoff::outgoing,&p.guest,[&](int,QJsonObject packet){
            if(packet["action"]=="phase"&&packet["phase"].toInt()>=HotseatHandoff::Acquiring)delayed.append(packet);
            else p.guest.receive(1,packet);
        });
        QSignalSpy errors(&p.guest,&HotseatHandoff::failed);
        p.guest.pass();p.observe(-1);p.observe(0);
        QCOMPARE(delayed.size(),2);QVERIFY(p.host.canPass());QVERIFY(!p.guest.ready());
        // Native MODE and the provider use different connections. The new host
        // owner can be observed before the provider's Acquiring/Idle messages.
        p.guest.receive(1,delayed[idleFirst?1:0]);
        p.guest.receive(1,delayed[idleFirst?0:1]);
        QVERIFY(errors.isEmpty());QVERIFY(p.guest.ready());QCOMPARE(p.guest.owner(),1);
        QCOMPARE(p.commands.size(),4); // one release and acquire per turn only
        p.guest.receive(1,delayed[0]);QCOMPARE(p.commands.size(),4);
    }
    void threePassesWaitForEachNativeAcknowledgement() {
        Pair p;QSignalSpy errors(&p.host,&HotseatHandoff::failed);
        QVERIFY(!p.host.ready());p.host.pass();QVERIFY(p.commands.isEmpty());
        p.ready();QVERIFY(p.host.canPass());QVERIFY(!p.guest.canPass());
        for(int turn=0;turn<3;++turn) {
            const int old=turn%2==0?1:2,next=3-old;
            auto& holder=old==1?p.host:p.guest;
            auto& spectator=old==1?p.guest:p.host;
            const auto count=p.commands.size();spectator.pass();QCOMPARE(p.commands.size(),count);
            holder.pass();QCOMPARE(p.commands.size(),count+1);
            QCOMPARE(p.commands.last(),qMakePair(old,false));
            QVERIFY(!p.host.ready());QVERIFY(!p.guest.ready());
            holder.pass();spectator.pass();QCOMPARE(p.commands.size(),count+1);
            p.observe(-1);QCOMPARE(p.commands.size(),count+2);
            QCOMPARE(p.commands.last(),qMakePair(next,true));
            QVERIFY(!p.host.ready());QCOMPARE(p.host.owner(),old);
            p.observe(next-1);
            QVERIFY(p.host.ready());QVERIFY(p.guest.ready());
            QCOMPARE(p.host.owner(),next);QCOMPARE(p.guest.owner(),next);
        }
        QVERIFY(errors.isEmpty());QCOMPARE(p.host.epoch(),quint64(3));
    }
    void staleClicksAndDuplicatePacketsNeverToggleAgain() {
        Pair p;QList<QJsonObject> phases,proofs;
        connect(&p.host,&HotseatHandoff::outgoing,&p.host,[&](int,QJsonObject v){if(v["action"]=="phase")phases.append(v);});
        connect(&p.guest,&HotseatHandoff::outgoing,&p.guest,[&](int,QJsonObject v){if(v["action"]=="observed")proofs.append(v);});
        p.ready();p.host.pass();p.observe(-1);p.observe(1);
        const auto count=p.commands.size();
        for(const auto& phase:phases)p.guest.receive(1,phase);
        for(const auto& proof:proofs)p.host.receive(2,proof);
        p.host.receive(2,{{"channel","hotseat-v1"},{"session",p.session},{"epoch",0},{"action","pass"}});
        QCOMPARE(p.commands.size(),count);QVERIFY(p.guest.canPass());
        auto wrong=phases.last();wrong["session"]=QUuid::createUuid().toString(QUuid::WithoutBraces);
        p.guest.receive(1,wrong);p.guest.receive(3,phases.first());
        QVERIFY(p.guest.canPass());QCOMPARE(p.commands.size(),count);
    }
    void unexpectedNativeChangeAndMissingAcknowledgementEndCoordination() {
        {
            Pair p;p.ready();QSignalSpy errors(&p.host,&HotseatHandoff::failed);
            p.observe(-1);QCOMPARE(errors.size(),1);QVERIFY(!p.host.active());QVERIFY(!p.guest.active());
            QVERIFY(p.commands.isEmpty());
        }
        {
            Pair p;p.ready();p.host.pass();QSignalSpy errors(&p.host,&HotseatHandoff::failed);
            p.host.timer_.setInterval(1);p.host.timer_.start();QTRY_COMPARE(errors.size(),1);
            QCOMPARE(p.commands.size(),1);QVERIFY(!p.guest.active());
            p.observe(-1);QCOMPARE(p.commands.size(),1); // late ack cannot acquire
        }
        {
            Pair p;p.ready();p.host.requestNativeRole=[](bool){return false;};
            QSignalSpy errors(&p.host,&HotseatHandoff::failed);p.host.pass();
            QCOMPARE(errors.size(),1);QVERIFY(!p.guest.active());
        }
    }
    void acquisitionCannotReplaceReleaseAndNewSessionRejectsOldProof() {
        Pair p;p.ready();p.host.pass();p.observe(1);
        QVERIFY(!p.host.active());QVERIFY(!p.guest.active());QCOMPARE(p.commands.size(),1);
        const auto old=p.session;
        p.session=QUuid::createUuid().toString(QUuid::WithoutBraces);
        p.host.begin(p.session,1);p.guest.begin(p.session,2);
        p.host.receive(2,{{"channel","hotseat-v1"},{"session",old},{"epoch",1},
            {"action","observed"},{"sequence",3},{"nativeOwner",2},{"initial",false}});
        QVERIFY(p.host.active());QVERIFY(!p.host.ready());
        p.sequence=0;p.ready();QVERIFY(p.host.canPass());
    }
};
}
QTEST_GUILESS_MAIN(trainer::HotseatHandoffTests)
#include "HotseatHandoffTests.moc"
