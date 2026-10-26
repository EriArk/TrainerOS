// SPDX-License-Identifier: GPL-3.0-or-later
#include "../remote-play-stream/transport/ps2_sockets.hpp"
#include "../remote-play-stream/transport/bba_channel.hpp"
#include "../../src/integrations/adventure/standalone/NativeLanConnection.h"
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QProcess>
#include <QUdpSocket>
#include <memory>
#include <vector>

// Private Unix edges only. PS2's existing native host forwards ECLX Ethernet;
// Dolphin's TAP backend needs a small Ethernet hub, without a host TAP or NIC.
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();if(args.size()<4)return 2;
    const auto config=trainer::NativeLanConnection::read(args[1]);if(!config.valid())return 2;
    const bool ps2=config.mode=="ps2",xbox=config.mode=="xbox",host=config.seat==1;
    if(ps2)for(const auto& interface:QNetworkInterface::allInterfaces())
        if(!(interface.flags()&QNetworkInterface::IsLoopBack))return 2;
    struct Edge {int seat;QLocalSocket channel;QByteArray buffered;std::unique_ptr<Ps2Sockets> native;};
    std::vector<std::unique_ptr<Edge>> edges;
    bool network=true;
    auto revoke=[&]{if(!network)return;network=false;for(auto& e:edges){e->channel.abort();e->buffered.clear();e->native.reset();}};
    auto send=[&](Edge& edge,const QByteArray& data) {
        if(!network||edge.channel.state()!=QLocalSocket::ConnectedState||edge.channel.bytesToWrite()+data.size()+2>65536)return false;
        QByteArray packet(2,0);
        if(ps2)qToBigEndian<quint16>(data.size(),packet.data());else qToLittleEndian<quint16>(data.size(),packet.data());
        packet+=data;return edge.channel.write(packet)==packet.size();
    };
    auto localFrame=[&](const QByteArray& data){
        if(!network)return false;
        if(data.mid(6,6)!=QByteArray::fromHex(config.mac().toLatin1().replace(":",""))){revoke();return false;}
        for(auto& e:edges)send(*e,data);return true;
    };
    BbaChannel local(localFrame,revoke);
    QUdpSocket udp;quint16 nativePort=0;
    if(xbox) {
        if(!udp.bind(QHostAddress::LocalHost,0))return 3;
        QObject::connect(&udp,&QUdpSocket::readyRead,&app,[&]{
            while(udp.hasPendingDatagrams()) {
                QByteArray data(1519,0);QHostAddress address;quint16 port=0;
                const auto size=udp.readDatagram(data.data(),data.size(),&address,&port);data.resize(qMax<qint64>(size,0));
                if(address!=QHostAddress::LocalHost||!BbaChannel::frame(data)||(nativePort&&nativePort!=port))continue;
                if(data.mid(6,6)!=QByteArray::fromHex(config.mac().toLatin1().replace(":","")))continue;
                nativePort=port;localFrame(data);
            }
        });
    }
    for(auto it=config.sockets.cbegin();it!=config.sockets.cend();++it) {
        auto owned=std::make_unique<Edge>();auto* e=owned.get();e->seat=it.key();edges.push_back(std::move(owned));
        e->channel.setReadBufferSize(65536);e->channel.connectToServer(it.value());if(!e->channel.waitForConnected(3000))return 3;
        if(ps2){e->native=std::make_unique<Ps2Sockets>(host,[&,e](const QByteArray& data){return send(*e,data);});if(!e->native->start(host?0:19073))return 3;}
        QObject::connect(&e->channel,&QLocalSocket::readyRead,&app,[&,e]{
            if(!network)return;e->buffered+=e->channel.readAll();if(e->buffered.size()>65536){revoke();return;}
            while(network&&e->buffered.size()>=2) {
                const auto size=ps2?qFromBigEndian<quint16>(e->buffered.constData()):qFromLittleEndian<quint16>(e->buffered.constData());
                if(size<(ps2?44:14)||size>(ps2?1044:1518)){revoke();return;}
                if(e->buffered.size()<size+2)return;
                const auto data=e->buffered.mid(2,size);e->buffered.remove(0,size+2);
                if(ps2){
                    const auto source=data.size()>=12?qFromBigEndian<quint32>(data.constData()+8):0;
                    if(!Ps2Sockets::envelope(data)||source<1||source>quint32(config.players)||source==quint32(config.seat)
                       ||(host&&source!=quint32(e->seat))){revoke();return;}
                    e->native->receive(data);
                }
                else {
                    const auto source=data.mid(6,6);bool accepted=false;
                    for(int seat=1;seat<=config.players;++seat)
                        if(seat!=config.seat&&(!host||seat==e->seat)&&source==QByteArray::fromHex(config.mac(seat).toLatin1().replace(":","")))accepted=true;
                    if(!accepted){revoke();return;}
                    if(xbox){if(nativePort)udp.writeDatagram(data,QHostAddress::LocalHost,nativePort);}
                    else local.receive(data);
                    if(host)for(auto& other:edges)if(other.get()!=e)send(*other,data);
                }
            }
        });
        QObject::connect(&e->channel,&QLocalSocket::disconnected,&app,revoke);
    }
    if(!ps2&&!xbox&&!local.start(QFileInfo(args[1]).absolutePath()+"/bba.sock"))return 3;
    // Revocation never closes or relaunches the game. Home owns safe process exit.
    QProcess game;game.setProcessChannelMode(QProcess::ForwardedChannels);game.setInputChannelMode(QProcess::ForwardedInputChannel);
    QObject::connect(&game,&QProcess::finished,&app,[&](int code,QProcess::ExitStatus status){app.exit(status==QProcess::NormalExit?code:1);});
    auto arguments=args.mid(3);
    if(xbox)for(auto& argument:arguments)argument.replace("TRAINEROS_XBOX_PORT",QString::number(udp.localPort()));
    game.start(args[2],arguments);if(!game.waitForStarted(5000))return 4;
    return app.exec();
}
