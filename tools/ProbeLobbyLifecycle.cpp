// Explicit, non-installed Linux integration probe. Uses the ordinary account,
// coordinator, provider and recovery journal; never exports account credentials.
#include "features/social/SocialController.h"
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>
#include <QVideoSink>
#include <QVideoFrame>
#include <unistd.h>

int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);app.setApplicationName("TrainerOS");app.setOrganizationName("TrainerOS");
    QCommandLineParser parser;parser.addHelpOption();parser.addOption({"owner","Existing local test Trainer ID (shell must be stopped).","id"});
    parser.addOption({"frame-output","Save one received frame as private diagnostic evidence.","path"});parser.process(app);
    if(parser.value("owner").isEmpty())return 2;
    trainer::SocialController social;QByteArray last,input;
    QVideoSink video;int frames=0;QVariantMap frameInfo;
    social.stream()->setVideoSink(&video);
    QObject::connect(&video,&QVideoSink::videoFrameChanged,&app,[&](QVideoFrame frame){
        if(!frame.isValid()||!frame.map(QVideoFrame::ReadOnly))return;
        ++frames;const auto* p=frame.bits(0)+(frame.height()/2)*frame.bytesPerLine(0)+(frame.width()/2)*4;
        frameInfo={{"count",frames},{"width",frame.width()},{"height",frame.height()},{"blue",p[0]},{"green",p[1]},{"red",p[2]}};
        if(frames==60&&!parser.value("frame-output").isEmpty())
            QImage(frame.bits(0),frame.width(),frame.height(),frame.bytesPerLine(0),QImage::Format_RGB32).copy().save(parser.value("frame-output"));
        frame.unmap();
    });
    const auto report=[&]{
        const auto account=social.account();auto room=social.lobbyCoordinator()->state();
        auto game=room.value("game").toObject();game.remove("runtime");if(!game.isEmpty())room["game"]=game;
        const auto output=QJsonDocument(QJsonObject{{"state",account["state"].toString()},{"self",account["userId"].toString()},{"name",account["name"].toString()},
            {"room",room},{"invitation",social.lobbyCoordinator()->invitation()},
            {"voice",QJsonObject::fromVariantMap(account["voice"].toMap())},{"audio",QJsonObject::fromVariantMap(account["audio"].toMap())},
            {"frames",QJsonObject::fromVariantMap(frameInfo)},
            {"contacts",QJsonObject::fromVariantMap(account["temporaryContacts"].toMap())},
            {"recent",QJsonArray::fromVariantList(social.recentPeople())},{"chatStatus",social.lobbyChatStatus()},
            {"notice",social.toastText()},{"status",account["status"].toString()}}).toJson(QJsonDocument::Compact);
        if(output!=last) {last=output;QTextStream(stdout)<<output<<Qt::endl;}
    };
    QObject::connect(&social,&trainer::SocialController::changed,&app,report);
    QObject::connect(&social,&trainer::SocialController::presentationChanged,&app,report);
    QTimer frameReport;frameReport.setInterval(1000);QObject::connect(&frameReport,&QTimer::timeout,&app,report);frameReport.start();
    QSocketNotifier commands(STDIN_FILENO,QSocketNotifier::Read);
    QObject::connect(&commands,&QSocketNotifier::activated,&app,[&]{
        char bytes[4096];const auto n=::read(STDIN_FILENO,bytes,sizeof(bytes));if(n<=0) {app.quit();return;}
        input.append(bytes,n);if(input.size()>8192) {app.exit(3);return;}
        while(input.contains('\n')) {
            const auto end=input.indexOf('\n');const auto command=QJsonDocument::fromJson(input.left(end)).object();input.remove(0,end+1);
            const auto op=command["op"].toString();auto* room=social.lobbyCoordinator();
            if(op=="create")room->create();
            else if(op=="invite")room->invite(command["peer"].toString());
            else if(op=="accept")room->answer(true);
            else if(op=="leave")room->leave();
            else if(op=="remove")room->remove(command["peer"].toString());
            else if(op=="reconnect")room->reconnect();
            else if(op=="lobby")social.openLobby();
            else if(op=="game-process")social.setGameProcess(command["pid"].toInteger());
            else if(op=="stream")social.streamAction(command["action"].toString());
            else if(op=="voice") {
                const auto action=command["action"].toString();
                if(QStringList{"voice-join","voice-leave","voice-mute","voice-output","audio-settings"}.contains(action))
                    social.reviewCommand(action,command["args"].toObject().toVariantMap());
            }
            else if(op=="name")social.reviewCommand("profile-update",{{"field","global_name"},{"value",command["value"].toString()}});
            else if(op=="quit")app.quit(); // Deliberately preserves membership, like normal shell exit.
            last.clear();report();
        }
    });
    social.setOwner(parser.value("owner"));return app.exec();
}
