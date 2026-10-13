// Explicit, non-installed Linux integration probe. Uses the ordinary account,
// coordinator, provider and recovery journal; never exports account credentials.
#include "features/social/SocialController.h"
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>
#include <unistd.h>

int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);app.setApplicationName("TrainerOS");app.setOrganizationName("TrainerOS");
    QCommandLineParser parser;parser.addHelpOption();parser.addOption({"owner","Existing local test Trainer ID (shell must be stopped).","id"});parser.process(app);
    if(parser.value("owner").isEmpty())return 2;
    trainer::SocialController social;QByteArray last,input;
    const auto report=[&]{
        const auto account=social.account();auto room=social.lobbyCoordinator()->state();
        auto game=room.value("game").toObject();game.remove("runtime");if(!game.isEmpty())room["game"]=game;
        const auto output=QJsonDocument(QJsonObject{{"state",account["state"].toString()},{"self",account["userId"].toString()},{"name",account["name"].toString()},
            {"room",room},{"invitation",social.lobbyCoordinator()->invitation()},
            {"contacts",QJsonObject::fromVariantMap(account["temporaryContacts"].toMap())},
            {"recent",QJsonArray::fromVariantList(social.recentPeople())},{"chatStatus",social.lobbyChatStatus()},
            {"notice",social.toastText()},{"status",account["status"].toString()}}).toJson(QJsonDocument::Compact);
        if(output!=last) {last=output;QTextStream(stdout)<<output<<Qt::endl;}
    };
    QObject::connect(&social,&trainer::SocialController::changed,&app,report);
    QObject::connect(&social,&trainer::SocialController::presentationChanged,&app,report);
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
            else if(op=="name")social.reviewCommand("profile-update",{{"field","global_name"},{"value",command["value"].toString()}});
            else if(op=="quit")app.quit(); // Deliberately preserves membership, like normal shell exit.
            last.clear();report();
        }
    });
    social.setOwner(parser.value("owner"));return app.exec();
}
