// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCoreApplication>
#include <QLocalSocket>
#include <QNetworkInterface>
#include <QNetworkDatagram>
#include <QUdpSocket>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#include <QMap>

// Runs in a private network namespace. The native protocol stays entirely in
// Flycast; this process forwards only between its two fixed loopback ports.
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()<4)return 2;
    for(const auto& interface:QNetworkInterface::allInterfaces())
        if(!(interface.flags()&QNetworkInterface::IsLoopBack))return 2;
    QFile config(args[1]);QFileInfo info(config);
    if(!info.isAbsolute()||info.isSymLink()||!config.open(QIODevice::ReadOnly)||config.size()>4096
       ||(info.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)))return 2;
    QMap<QByteArray,QByteArray> values;
    for(const auto& line:config.readAll().split('\n')) {
        if(line.isEmpty())continue;const auto at=line.indexOf('=');
        if(at<1||line.contains('\r')||values.contains(line.left(at)))return 2;
        values[line.left(at)]=line.mid(at+1);
    }
    if(values.size()!=3||values["mode"]!="cabinet"||(values["host"]!="true"&&values["host"]!="false"))return 2;
    const bool host=values["host"]=="true";const quint16 local=host?37391:37392,peer=host?37392:37391;
    const auto path=QString::fromUtf8(values["socket"]);
    if(path!=info.absolutePath()+"/cabinet.sock")return 2;
    constexpr int QueueBytes=1024*1024;
    QLocalSocket channel;channel.setReadBufferSize(QueueBytes);channel.connectToServer(path);
    if(!channel.waitForConnected(3000))return 3;
    QUdpSocket socket;if(!socket.bind(QHostAddress::LocalHost,peer,QUdpSocket::DontShareAddress))return 3;
    bool network=true;QByteArray buffered;
    const auto revoke=[&]{if(!network)return;network=false;buffered.clear();socket.close();channel.abort();};
    QObject::connect(&socket,&QUdpSocket::readyRead,&app,[&]{
        while(network&&socket.hasPendingDatagrams()) {
            const auto data=socket.receiveDatagram(0x4005);
            if(data.senderAddress()!=QHostAddress(QHostAddress::LocalHost)||data.senderPort()!=local
               ||data.data().isEmpty()||data.data().size()>0x4004)continue;
            if(channel.bytesToWrite()+data.data().size()+2>QueueBytes){revoke();return;}
            QByteArray packet(2,0);qToBigEndian<quint16>(data.data().size(),packet.data());packet+=data.data();
            if(channel.write(packet)!=packet.size()){revoke();return;}
        }
    });
    QObject::connect(&channel,&QLocalSocket::readyRead,&app,[&]{
        if(!network)return;
        while(channel.bytesAvailable()>0) {
            buffered+=channel.read(65536);
            while(buffered.size()>=2) {
                const auto size=qFromBigEndian<quint16>(buffered.constData());
                if(size<1||size>0x4004){revoke();return;}
                if(buffered.size()<size+2)break;
                const auto data=buffered.mid(2,size);buffered.remove(0,size+2);
                if(socket.writeDatagram(data,QHostAddress::LocalHost,local)!=size){revoke();return;}
            }
        }
    });
    QObject::connect(&channel,&QLocalSocket::disconnected,&app,revoke);
    QProcess game;game.setProcessChannelMode(QProcess::ForwardedChannels);
    game.setInputChannelMode(QProcess::ForwardedInputChannel);
    QObject::connect(&game,&QProcess::finished,&app,[&](int code,QProcess::ExitStatus status){app.exit(status==QProcess::NormalExit?code:1);});
    game.start(args[2],args.mid(3));if(!game.waitForStarted(5000))return 4;
    return app.exec();
}
