#include "integrations/practice/PracticeSession.h"
#include <QtTest>
#include <QStandardPaths>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QFile>
using namespace trainer;
namespace {
GameProgress fixture() {
    GameProgress p;p.availability=ProgressAvailability::Available;p.contextRevision="context";p.saveRevision="hash";
    p.contentRevision="a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af";
    p.party=PartySnapshot{};
    PokemonRecord mon;mon.kind=PokemonSlotKind::Known;mon.number=257;mon.formId="257";mon.level=50;mon.hp=0;
    mon.stats={155,140,90,130,90,100};mon.moves={{"Flamethrower",0,15},{},{"Return",1,28},{}};
    mon.battle=PokemonBattleTraits{};mon.battle->ivs={31,31,31,31,31,31};mon.battle->gender="M";
    mon.battle->abilityId=66;mon.battle->moveIds={53,0,216,0};mon.battle->ppUps={0,0,2,0};
    p.party->party={mon,mon};return p;
}
PracticeSource source(const GameProgress& p) {return {"owner","adventure",p.contextRevision,p.contentRevision,p.saveRevision};}
}
class PracticeSessionTests:public QObject {
    Q_OBJECT
    const QString worker=QStringLiteral(TRAINER_SOURCE_DIR "/src/integrations/practice/emerald-worker.cjs");
    const QString engine=QStringLiteral(TRAINER_SOURCE_DIR "/tools/research/emerald-practice/node_modules/pokemon-showdown");
private slots:
    void invalidSourceDoesNotStart() {
        PracticeSession session;const auto p=fixture();auto s=source(p);s.trainerId.clear();
        QVERIFY(!session.begin("missing",worker,engine,s,p,0,1,{1,2,3,4}));QVERIFY(!session.active());
        s=source(p);s.saveRevision="other";QVERIFY(!session.begin("missing",worker,engine,s,p,0,1,{1,2,3,4}));
    }
    void missingRuntimeIsReaped() {
        PracticeSession session;QSignalSpy stopped(&session,&PracticeSession::stopped);const auto p=fixture();
        QVERIFY(session.begin("/missing/trainer-node",worker,engine,source(p),p,0,1,{1,2,3,4}));
        QTRY_COMPARE(stopped.size(),1);QVERIFY(!session.active());QVERIFY(session.state().isEmpty());
    }
    void unresponsiveAndMalformedWorkersAreReaped() {
        const auto node=QStandardPaths::findExecutable("node");if(node.isEmpty())QSKIP("Node unavailable");
        QTemporaryDir dir;QVERIFY(dir.isValid());const auto p=fixture();
        for(const auto script:QList<QByteArray>{
            "process.stdin.resume(); setInterval(()=>{},1000);",
            "process.stdout.write('not-json\\n'); process.stdin.resume();",
            "process.stdout.write('x'.repeat(70000)); process.stdin.resume();",
            "process.exit(3);"}) {
            const auto file=dir.filePath("fault.cjs");QFile f(file);QVERIFY(f.open(QIODevice::WriteOnly));f.write(script);f.close();
            PracticeSession session;QSignalSpy stopped(&session,&PracticeSession::stopped);
            QVERIFY(session.begin(node,file,engine,source(p),p,0,1,{1,2,3,4}));
            QTRY_COMPARE_WITH_TIMEOUT(stopped.size(),1,6500);QVERIFY(!session.active());QVERIFY(session.state().isEmpty());
            QVERIFY(!stopped[0][0].toString().isEmpty());
        }
    }
    void actualWorkerCompletesAndDoesNotReuseStaleInput() {
        const auto node=QStandardPaths::findExecutable("node");
        if(node.isEmpty() || !QFileInfo::exists(engine+"/package.json"))QSKIP("Install the pinned research dependency for worker integration proof.");
        PracticeSession session;const auto p=fixture();const auto s=source(p);
        QVERIFY(session.begin(node,worker,engine,s,p,0,1,{1,2,3,4}));
        QTRY_VERIFY_WITH_TIMEOUT(!session.state().isEmpty(),5000);
        QCOMPARE(session.state()["sides"].toArray()[0].toObject()["hp"].toInt(),155);
        QVERIFY(!session.choose(2,1));QVERIFY(session.choose(1,1));QVERIFY(!session.choose(1,1));
        QTRY_COMPARE(session.state()["turn"].toInt(),2);
        while(!session.state()["ended"].toBool()) {
            const int turn=session.state()["turn"].toInt();
            const auto sides=session.state()["sides"].toArray();
            const auto slot=[&](int i){return sides[i].toObject()["moves"].toArray()[0].toObject()["slot"].toInt();};
            QVERIFY(session.choose(slot(0),slot(1)));
            QTRY_VERIFY(session.state()["ended"].toBool() || session.state()["turn"].toInt()>turn);
        }
        QTRY_VERIFY(!session.active());QVERIFY(!session.state()["winner"].toString().isEmpty());
        auto switched=s;switched.trainerId="other";session.updateSource(switched,true);
        QVERIFY(session.state().isEmpty()); // Completed results also belong to the frozen owner.
        QCOMPARE(*p.party->party[0].hp,0);QCOMPARE(p.party->party[0].moves[0].pp,0);
        for(int field=0;field<6;++field) {
            QVERIFY(session.begin(node,worker,engine,s,p,0,1,{1,2,3,4}));
            QTRY_VERIFY(!session.state().isEmpty());auto changed=s;
            if(field==0)changed.trainerId="other";if(field==1)changed.adventureId="other";
            if(field==2)changed.contextRevision="other";if(field==3)changed.contentRevision="other";
            if(field==4)changed.saveRevision="other";
            session.updateSource(changed,field!=5);QVERIFY(session.state().isEmpty());
            QTRY_VERIFY(!session.active());QVERIFY(!session.choose(1,1));
        }
        // Cancelling during startup also reaps the child; no ready event can revive it.
        QVERIFY(session.begin(node,worker,engine,s,p,0,1,{1,2,3,4}));session.cancel();
        QTRY_VERIFY(!session.active());QVERIFY(session.state().isEmpty());
    }
};
QTEST_GUILESS_MAIN(PracticeSessionTests)
#include "PracticeSessionTests.moc"
