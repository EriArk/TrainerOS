// SPDX-License-Identifier: GPL-3.0-or-later
// BUILD_TESTING only: exercise the real lobby grant and inherited supervisor.
#include "features/social/LobbyInput.h"
#include "core/input/RemoteGamePad.h"
#include "integrations/adventure/retroarch/RetroArchRemotePlay.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QProcess>
#include <QTextStream>
#include <QUuid>
#include <fcntl.h>
#include <unistd.h>

using namespace trainer;
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()==2&&args[1]=="identify") {
        SDL_SetMainReady();SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS,"1");
        if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER))return 8;
        int remote=0,local=0;
        for(int i=0;i<SDL_NumJoysticks();++i) {
            if(isRemoteGamePad(i))++remote;else ++local;
        }
        QTextStream(stdout)<<"REMOTE_IDENTIFIED="<<remote<<" LOCAL_RETAINED="<<local<<Qt::endl;
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);return remote==1&&local>=1?0:8;
    }
    if(args.size()==5&&args[1]=="ports") {
        ProcessCommand cmd;cmd.arguments={"--config",args[2],"game"};
        RetroArchInstallation install;install.configFile=args[2];install.runtimeFile=args[4];
        std::atomic_bool cancel=false;
        const auto error=retroarch::prepareRemotePlay(cmd,install,{args[3]},cancel);
        if(!error.isEmpty()) {qCritical("%s",qPrintable(error));return 1;}
        QFile src(cmd.arguments[cmd.arguments.indexOf("--appendconfig")+1]),dest(args[2]);
        if(!src.open(QIODevice::ReadOnly)||!dest.open(QIODevice::WriteOnly))return 2;
        const auto bytes=src.readAll();if(dest.write(bytes)!=bytes.size())return 2;dest.close();
        cmd.settled({});return 0;
    }
    if(args.size()!=6||args[1]!="run")return 2;
    LobbyCoordinator host,guest;host.configure("1","Host",true);guest.configure("2","Guest",true);
    QObject::connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString peer,QJsonObject packet){if(peer=="2")guest.receive("1","Host",packet);});
    QObject::connect(&guest,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject packet){host.receive("2","Guest",packet);});
    if(!host.create("public")||!host.selectGame({"probe","Input probe","nes","remoteplay.v1",2})
       ||!guest.join("1",host.state()["id"].toString()))return 3;
    const auto generation=quint64(host.state()["generation"].toInteger());
    if(!host.prepared(generation,true)||!guest.prepared(generation,true)||!host.ready(true)||!guest.ready(true))return 3;
    const auto ticket=host.state()["ticket"].toString();
    // The caller owns an already running host game; this probe launches only
    // capture. It never promises to start another emulator for either member.
    if(!host.acknowledgeLaunch(ticket,true)||!guest.acknowledgeLaunch(ticket,true)||host.state()["phase"]!="playing")return 3;
    LobbyInput input(host);const auto connection=QUuid::createUuid().toString();
    if(!input.authorize("2",connection,ticket,2))return 4;
    const int fd=input.takeDescriptor(connection);if(fd<3)return 4;
    QProcess supervisor;
    auto env=QProcessEnvironment::systemEnvironment();env.insert("TRAINEROS_LOBBY_INPUT_FD",QString::number(fd));
    supervisor.setProcessEnvironment(env);supervisor.setProcessChannelMode(QProcess::ForwardedChannels);
    supervisor.setChildProcessModifier([fd]{fcntl(fd,F_SETFD,0);});
    supervisor.start("/usr/bin/python3",{args[2],args[3],args[4],args[5]});
    if(!supervisor.waitForStarted(5000)){close(fd);return 5;}close(fd);
    QTimer::singleShot(15000,&app,[&]{
        const bool removed=host.remove("2");
        QTextStream(stdout)<<"LOBBY_REMOVED="<<removed<<" INPUT_ACTIVE="<<input.active()<<Qt::endl;
    });
    QTimer::singleShot(25000,&app,[&]{supervisor.terminate();app.exit(6);});
    QObject::connect(&supervisor,&QProcess::finished,&app,[&](int code,QProcess::ExitStatus){
        QTextStream(stdout)<<"SUPERVISOR_EXIT="<<code<<" LOBBY_PHASE="<<host.state()["phase"].toString()<<Qt::endl;
        app.exit(!input.active()&&host.state()["phase"]=="playing"?0:7);
    });
    const auto result=app.exec();
    if(supervisor.state()!=QProcess::NotRunning){supervisor.terminate();if(!supervisor.waitForFinished(3000)){supervisor.kill();supervisor.waitForFinished();}}
    return result;
}
