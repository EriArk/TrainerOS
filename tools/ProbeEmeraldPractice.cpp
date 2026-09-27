#include "integrations/practice/PracticeSession.h"
#include "integrations/progress/EmeraldPractice.h"
#include "integrations/progress/Gen3Progress.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTextStream>

// Opt-in read-only device proof, never a normal application launch path.
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()!=6){QTextStream(stderr)<<"Usage: probe ROM SAVE NODE WORKER ENGINE_ROOT\n";return 2;}
    QFile rom(args[1]),save(args[2]);
    if(!rom.open(QIODevice::ReadOnly) || !save.open(QIODevice::ReadOnly) || save.size()!=131072)return 3;
    QCryptographicHash hash(QCryptographicHash::Sha256);if(!hash.addData(&rom))return 3;
    const auto content=QString::fromLatin1(hash.result().toHex());
    if(trainer::gen3Edition(content)!=trainer::Gen3Edition::Emerald)return 4;
    const auto bytes=save.readAll();save.close();
    auto progress=trainer::readGen3Progress(bytes,trainer::Gen3Edition::Emerald);
    progress.contentRevision=content;progress.contextRevision="private-read-only-probe";
    progress.saveRevision=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
    if(!progress.party || progress.availability!=trainer::ProgressAvailability::Available)return 5;
    trainer::PracticeSource source{"probe-owner","probe-adventure",progress.contextRevision,content,progress.saveRevision};
    QJsonArray reports;
    for(int partner=1;partner<6;++partner) {
        if(progress.party->party[partner].kind!=trainer::PokemonSlotKind::Known)continue;
        trainer::PracticeSession session;QEventLoop loop;QTimer deadline;deadline.setSingleShot(true);
        QJsonArray events;int lastTurn=0;QString failure;QJsonObject lastState;
        QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{failure="Proof timed out";session.cancel();});
        QObject::connect(&session,&trainer::PracticeSession::stopped,&loop,[&](const QString& reason){if(!reason.isEmpty())failure=reason;loop.quit();});
        QObject::connect(&session,&trainer::PracticeSession::changed,&loop,[&]{
            const auto state=session.state();if(state.isEmpty() || !session.active())return;
            if(state!=lastState){lastState=state;lastTurn=state["turn"].toInt();for(const auto& e:state["events"].toArray())events.append(e);}
            if(state["ended"].toBool())return;
            const auto sides=state["sides"].toArray();
            const auto first=[&](int side){return sides[side].toObject()["moves"].toArray()[0].toObject()["slot"].toInt();};
            session.choose(first(0),first(1));
        });
        if(!session.begin(args[3],args[4],args[5],source,progress,0,partner,{1,2,3,4}))return 6;
        deadline.start(20000);loop.exec();
        if(!failure.isEmpty() || !session.state()["ended"].toBool()){QTextStream(stderr)<<failure<<'\n';return 7;}
        reports.append(QJsonObject{{"partner",partner},{"turn",lastTurn},{"winner",session.state()["winner"]},
            {"replay",QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(events).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex())},
            {"input",trainer::emeraldPracticePair(progress,0,partner).input}});
    }
    if(reports.isEmpty())return 8;
    // Actual ARM process must also be reaped when a source changes at its first request.
    trainer::PracticeSession cancelled;QEventLoop loop;bool invalidated=false;QString reason;
    QObject::connect(&cancelled,&trainer::PracticeSession::changed,&loop,[&]{
        if(!cancelled.state().isEmpty() && !invalidated){invalidated=true;auto stale=source;stale.saveRevision="changed";cancelled.updateSource(stale,true);}
    });
    QObject::connect(&cancelled,&trainer::PracticeSession::stopped,&loop,[&](const QString& text){reason=text;loop.quit();});
    if(!cancelled.begin(args[3],args[4],args[5],source,progress,0,1,{1,2,3,4}))return 9;
    loop.exec();if(!invalidated || cancelled.active() || !cancelled.state().isEmpty() || reason!="The selected Party changed.")return 10;
    if(!save.open(QIODevice::ReadOnly) || save.readAll()!=bytes)return 11;
    QTextStream(stdout)<<QJsonDocument(QJsonObject{{"ok",true},{"sourceUnchanged",true},{"sourceChangeReaped",true},{"battles",reports}}).toJson();
    return 0;
}
