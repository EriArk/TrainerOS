#include "integrations/practice/PracticeSession.h"
#include "adapters/pokemon/center/PracticeController.h"
#include "integrations/progress/EmeraldPractice.h"
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
    void playbackKeepsOrderedExactHealthAndRosterIdentity() {
        const QJsonArray teams{QJsonArray{QJsonObject{{"name","First"}},QJsonObject{{"name","Bench"}}},QJsonArray{QJsonObject{{"name","Second"}}}};
        const QJsonObject side{{"hp",100},{"maxHp",100},{"member",0},{"team",QJsonArray{QJsonObject{{"member",0},{"hp",100},{"maxHp",100},{"active",true}},QJsonObject{{"member",1},{"hp",80},{"maxHp",80},{"active",false}}}}};
        const QJsonObject before{{"turn",1},{"sides",QJsonArray{side,side}}};
        auto finalSide=side;finalSide["hp"]=40;
        QJsonObject after{{"turn",2},{"sides",QJsonArray{finalSide,finalSide}},
            {"events",QJsonArray{"|move|p2a: Second|Flamethrower|p1a: First|[traineros-type] Fire",
            "|split|p1","|-damage|p1a: First|60/100|[traineros-member] 0","|-damage|p1a: First|60/100",
            "|move|p1a: First|Return|p2a: Second|[traineros-type] Normal",
            "|split|p2","|-damage|p2a: Second|40/100|[traineros-member] 0","|-damage|p2a: Second|40/100"}}};
        BattlePlayback playback;playback.load(before,after,teams);
        QVERIFY(playback.active());QCOMPARE(playback.turn(),1);
        QCOMPARE(playback.event()["actor"].toInt(),1);QCOMPARE(playback.event()["element"],"Fire");
        QCOMPARE(playback.sides()[0].toObject()["hp"].toInt(),100);
        QVERIFY(playback.advance());QCOMPARE(playback.event()["effect"],"hit");
        QCOMPARE(playback.sides()[0].toObject()["hp"].toInt(),60);
        QCOMPARE(playback.sides()[1].toObject()["hp"].toInt(),100);
        QVERIFY(playback.advance());QCOMPARE(playback.event()["kind"],"move"); // No public HP duplicate.
        QVERIFY(playback.advance());QCOMPARE(playback.sides()[1].toObject()["hp"].toInt(),40);
        QVERIFY(!playback.advance());QCOMPARE(playback.sides(),after["sides"].toArray());
        after["events"]=QJsonArray{"|split|p1","|switch|p1a: SameSpecies|Blaziken, L50|80/80|[traineros-member] 1","|switch|p1a: SameSpecies|Blaziken, L50|100/100|[traineros-member] 1"};
        playback.load(before,after,teams);QCOMPARE(playback.event()["text"],"Go, Bench!");
        QCOMPARE(playback.sides()[0].toObject()["member"].toInt(),1);
        QCOMPARE(playback.sides()[0].toObject()["hp"].toInt(),80);
        after["events"]=QJsonArray{"|-heal|p1: Bench|80/80|[traineros-member] 1"};
        playback.load(before,after,teams);QCOMPARE(playback.sides()[0].toObject()["hp"].toInt(),100);
        QVERIFY(playback.event()["effect"].toString().isEmpty()); // Bench healing never animates active HP.
    }
    void controllerSelectionAndAsyncCancellation() {
        const auto node=QStandardPaths::findExecutable("node");
        if(node.isEmpty() || !QFileInfo::exists(engine+"/package.json"))QSKIP("Pinned practice runtime unavailable");
        PracticeController c;const auto p=fixture();auto s=source(p);
        const QVariantList actors{QVariantMap{{"index",0},{"name","First"}},QVariantMap{{"index",1},{"name","Second"}}};
        c.configureRuntime(node,worker,engine);
        std::function<void(bool)> reply;
        c.configureVerification([&](const PracticeSource&,const GameProgress&,QObject*,auto done){reply=std::move(done);});
        c.setObservation(s,p,actors);c.enter();QVERIFY(c.ready());QCOMPARE(c.candidates().size(),2);
        c.dispatch(Action::Confirm);QCOMPARE(c.stage(),"second");
        c.activate(0);QCOMPARE(c.stage(),"second"); // A participant cannot fight itself.
        c.dispatch(Action::Right);
        c.dispatch(Action::Confirm);QCOMPARE(c.stage(),"ready");
        c.dispatch(Action::Back);QCOMPARE(c.stage(),"second");
        c.activate(1);c.dispatch(Action::Confirm);QCOMPARE(c.stage(),"starting");QVERIFY(reply);
        c.leave();reply(true);QVERIFY(!c.running());QVERIFY(!c.isOpen()); // Late source checks cannot restart a closed screen.
        c.enter();c.activate(0);c.activate(1);c.activate(0);reply(false);
        QCOMPARE(c.stage(),"error");QVERIFY(!c.running());
        c.activate(0);c.activate(0);s.trainerId="another-owner";c.setObservation(s,p,actors);
        QCOMPARE(c.stage(),"error");QVERIFY(!c.running());
    }
    void controllerActualBattleAndSourceChange() {
        const auto node=QStandardPaths::findExecutable("node");
        if(node.isEmpty() || !QFileInfo::exists(engine+"/package.json"))QSKIP("Pinned practice runtime unavailable");
        PracticeController c;const auto p=fixture();const auto s=source(p);
        c.configureRuntime(node,worker,engine);int checks=0;bool valid=true;
        c.configureVerification([&](const PracticeSource&,const GameProgress&,QObject*,auto done){++checks;done(valid);});
        c.setObservation(s,p,{QVariantMap{{"index",0},{"name","First"}},QVariantMap{{"index",1},{"name","Second"}}});
        c.enter();c.activate(0);c.activate(1);c.activate(0);
        QTRY_VERIFY(c.stage()=="moves" || c.stage()=="events");
        while(c.stage()=="events")c.dispatch(Action::Confirm);
        QCOMPARE(c.stage(),"moves");QCOMPARE(c.fighters()[0].toMap()["battleHp"].toInt(),155);
        c.activate(0);QCOMPARE(c.side(),1);c.dispatch(Action::Back);QCOMPARE(c.side(),0);
        c.activate(0);c.activate(0);QTRY_COMPARE(c.stage(),"events");
        QVERIFY(checks>=2);while(c.stage()=="events")c.dispatch(Action::Confirm);
        QVERIFY(c.fighters()[0].toMap()["battleHp"].toInt()<155);
        valid=false;c.activate(0);c.activate(0);QCOMPARE(c.stage(),"error");
        QTRY_VERIFY(!c.running());QCOMPARE(*p.party->party[0].hp,0);QCOMPARE(p.party->party[0].moves[0].pp,0);
    }
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
    void fullTeamsSwitchAndReplaceFaintedPartners() {
        const auto node=QStandardPaths::findExecutable("node");
        if(node.isEmpty() || !QFileInfo::exists(engine+"/package.json"))QSKIP("Install the pinned research dependency for team-battle proof.");
        const auto mon=emeraldPracticeMember(fixture(),0);QVERIFY(!mon.isEmpty());
        PracticeSession session;QJsonArray team{mon,mon,mon,mon,mon,mon};
        QVERIFY(session.beginLink(node,worker,engine,{{"protocol",1},{"teams",QJsonArray{team,team}}},{1,2,3,4}));
        QTRY_VERIFY(!session.state().isEmpty());
        QCOMPARE(session.state()["sides"].toArray()[0].toObject()["total"].toInt(),6);
        QVERIFY(session.choose(11,12));QTRY_COMPARE(session.state()["turn"].toInt(),2);
        QCOMPARE(session.state()["sides"].toArray()[0].toObject()["member"].toInt(),1);
        QCOMPARE(session.state()["sides"].toArray()[1].toObject()["member"].toInt(),2);
        int replacements=0,requests=0;
        while(!session.state()["ended"].toBool() && requests++<100) {
            const auto sides=session.state()["sides"].toArray();const int request=session.state()["request"].toInt();
            int selected[2];for(int i=0;i<2;++i){const auto side=sides[i].toObject();if(side["forceSwitch"].toBool())++replacements;
                const auto choices=side["moves"].toArray();QVERIFY(!choices.isEmpty());selected[i]=choices[0].toObject()["slot"].toInt();}
            QVERIFY(session.choose(selected[0],selected[1]));
            QTRY_VERIFY(session.state()["request"].toInt()>request);
        }
        QVERIFY(replacements>=5);QVERIFY(session.state()["ended"].toBool());QTRY_VERIFY(!session.active());
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
