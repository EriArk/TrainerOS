// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/integrations/adventure/standalone/CabinetConnection.h"
#include <QCoreApplication>
#include <QLocalSocket>
#include <QNetworkInterface>
#include <QNetworkDatagram>
#include <QUdpSocket>
#include <QProcess>
#include <QtEndian>

// Each namespace presents identical seat addresses. The host forwards between
// authenticated guest channels; Flycast's advertised ring ports stay unchanged.
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();if(args.size()<4)return 2;
    for(const auto& interface:QNetworkInterface::allInterfaces())
        if(!(interface.flags()&QNetworkInterface::IsLoopBack))return 2;
    const auto config=trainer::flycast::CabinetConnection::read(args[1]);if(!config.valid())return 2;
    constexpr int QueueBytes=1024*1024,MaxPacket=0x4006;
    const quint16 local=37390+config.seat;
    QMap<int,QLocalSocket*> channels;QMap<int,QUdpSocket*> sockets;QMap<int,QByteArray> buffers;
    bool network=true;
    const auto revoke=[&]{if(!network)return;network=false;buffers.clear();for(auto* s:sockets)s->close();for(auto* c:channels)c->abort();};
    const auto send=[&](int peer,const QByteArray& data){
        auto* c=channels.value(peer);if(!network||!c||c->bytesToWrite()+data.size()+2>QueueBytes){revoke();return;}
        QByteArray frame(2,0);qToBigEndian<quint16>(data.size(),frame.data());frame+=data;
        if(c->write(frame)!=frame.size())revoke();
    };
    for(int peer=1;peer<=config.players;++peer) {
        if(peer==config.seat)continue;
        auto* socket=new QUdpSocket(&app);sockets[peer]=socket;
        if(!socket->bind(QHostAddress::LocalHost,37390+peer,QUdpSocket::DontShareAddress))return 3;
        QObject::connect(socket,&QUdpSocket::readyRead,&app,[&,socket,peer]{
            while(network&&socket->hasPendingDatagrams()) {
                const auto d=socket->receiveDatagram(0x4005);
                if(d.senderAddress()!=QHostAddress(QHostAddress::LocalHost)||d.senderPort()!=local||d.data().isEmpty()||d.data().size()>0x4004)continue;
                QByteArray packet;packet+=char(config.seat);packet+=char(peer);packet+=d.data();
                send(config.seat==1?peer:1,packet);
            }
        });
    }
    for(auto it=config.sockets.cbegin();it!=config.sockets.cend();++it) {
        const int peer=it.key();auto* channel=new QLocalSocket(&app);channels[peer]=channel;
        channel->setReadBufferSize(QueueBytes);channel->connectToServer(it.value());if(!channel->waitForConnected(3000))return 3;
        QObject::connect(channel,&QLocalSocket::disconnected,&app,revoke);
        QObject::connect(channel,&QLocalSocket::readyRead,&app,[&,channel,peer]{
            if(!network)return;
            while(network&&channel->bytesAvailable()>0) {
                auto& b=buffers[peer];b+=channel->read(65536);
                while(b.size()>=2) {
                    const auto size=qFromBigEndian<quint16>(b.constData());
                    if(size<3||size>MaxPacket){revoke();return;}
                    if(b.size()<size+2)break;
                    const auto packet=b.mid(2,size);b.remove(0,size+2);
                    const int source=quint8(packet[0]),target=quint8(packet[1]);
                    // Guests cannot impersonate another seat or reflect into
                    // themselves. A guest accepts only packets addressed to it.
                    if(source<1||source>config.players||target<1||target>config.players||source==target
                       ||(config.seat==1?source!=peer:target!=config.seat)){revoke();return;}
                    if(target==config.seat) {
                        auto* socket=sockets.value(source);const auto data=packet.mid(2);
                        if(!socket||socket->writeDatagram(data,QHostAddress::LocalHost,local)!=data.size()){revoke();return;}
                    } else {
                        send(target,packet);if(!network)return;
                    }
                }
            }
        });
    }
    QProcess game;game.setProcessChannelMode(QProcess::ForwardedChannels);game.setInputChannelMode(QProcess::ForwardedInputChannel);
    QObject::connect(&game,&QProcess::finished,&app,[&](int code,QProcess::ExitStatus status){app.exit(status==QProcess::NormalExit?code:1);});
    game.start(args[2],args.mid(3));if(!game.waitForStarted(5000))return 4;
    return app.exec();
}
