// SPDX-License-Identifier: GPL-3.0-or-later
#include "../remote-play-stream/transport/ps2_sockets.hpp"
#include <QCoreApplication>
#include <QLocalSocket>
#include <QNetworkInterface>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <csignal>

// Runs inside bwrap --unshare-net with only loopback. The game keeps its own
// files/GPU/input, but can reach only this session's accepted datagram channel.
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()<4)return 2;
    for(const auto& interface:QNetworkInterface::allInterfaces())
        if(!(interface.flags()&QNetworkInterface::IsLoopBack))return 2;
    QFile config(args[1]);if(!config.open(QIODevice::ReadOnly)||config.size()>4096)return 2;
    QMap<QByteArray,QByteArray> values;
    for(const auto& line:config.readAll().split('\n')) {
        if(line.isEmpty())continue;const auto at=line.indexOf('=');
        if(at<1||values.contains(line.left(at)))return 2;
        values[line.left(at)]=line.mid(at+1);
    }
    if(values.size()!=7||values["mode"]!="ps2"||values["address"]!="127.0.0.1"
       ||(values["host"]!="true"&&values["host"]!="false"))return 2;
    const bool host=values["host"]=="true";
    if(values["port"]!=(host?"19072":"19073")||values["peer"]!=(host?"1":"2"))return 2;
    const auto path=QString::fromUtf8(values["socket"]);
    if(path!=QFileInfo(args[1]).absolutePath()+"/ps2.sock")return 2;
    QLocalSocket channel;channel.connectToServer(path);
    if(!channel.waitForConnected(3000))return 3;
    bool network=true;QByteArray buffered;
    Ps2Sockets sockets(host,[&](const QByteArray& data){
        if(!network||channel.bytesToWrite()+data.size()+2>65536)return false;
        QByteArray packet(2,0);qToBigEndian<quint16>(data.size(),packet.data());packet+=data;
        return channel.write(packet)==packet.size();
    });
    if(!sockets.start(host?0:19073))return 3;
    QObject::connect(&channel,&QLocalSocket::readyRead,&app,[&]{
        buffered+=channel.readAll();
        if(buffered.size()>65536){channel.abort();return;}
        while(buffered.size()>=2) {
            const auto size=qFromBigEndian<quint16>(buffered.constData());
            if(size<44||size>1044){channel.abort();return;}
            if(buffered.size()<size+2)return;
            const auto data=buffered.mid(2,size);buffered.remove(0,size+2);sockets.receive(data);
        }
    });
    // Connection loss revokes network access; it never terminates a live game
    // or launches/rejoins it. Home/Exit still belongs to the owning process tree.
    QObject::connect(&channel,&QLocalSocket::disconnected,&app,[&]{network=false;buffered.clear();});
    QProcess game;game.setProcessChannelMode(QProcess::ForwardedChannels);
    QObject::connect(&game,&QProcess::finished,&app,[&](int code,QProcess::ExitStatus status){app.exit(status==QProcess::NormalExit?code:1);});
    game.start(args[2],args.mid(3));if(!game.waitForStarted(5000))return 4;
    return app.exec();
}
