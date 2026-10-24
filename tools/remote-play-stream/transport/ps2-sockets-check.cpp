// SPDX-License-Identifier: GPL-3.0-or-later
#include "ps2_sockets.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <stdexcept>

static void require(bool ok) { if(!ok) throw std::runtime_error("PS2 socket boundary check failed"); }
static void spin(std::function<bool()> done) {
    QElapsedTimer timer; timer.start();
    while(!done() && timer.elapsed()<1500) {QCoreApplication::processEvents(); QThread::msleep(1);}
    require(done());
}
static QByteArray frame(int type, int size=0, int index=0, int count=1) {
    QByteArray data(44+size,'\0');data.replace(0,4,"ECLX");data[4]=1;data[5]=char(type);
    qToBigEndian<quint16>(44,data.data()+6);qToBigEndian<quint16>(index,data.data()+20);
    qToBigEndian<quint16>(count,data.data()+22);qToBigEndian<quint16>(size,data.data()+24);
    for(int i=44;i<data.size();++i)data[i]=char(i%251);
    return data;
}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        const auto hello=frame(1),ack=frame(2),first=frame(3,1000,0,2),last=frame(3,514,1,2);
        require(Ps2Sockets::envelope(hello)&&Ps2Sockets::envelope(ack));
        require(Ps2Sockets::envelope(first)&&Ps2Sockets::envelope(last));
        require(!Ps2Sockets::envelope(frame(3,515,1,2))&&!Ps2Sockets::envelope(frame(3,1,2,2)));
        require(!Ps2Sockets::envelope(frame(3,1,0,3))&&!Ps2Sockets::envelope(frame(1,1)));
        for(int i=0;i<44;++i)require(!Ps2Sockets::envelope(hello.left(i)));
        auto wrong=hello;wrong[4]=2;require(!Ps2Sockets::envelope(wrong));
        wrong=hello;wrong[0]='X';require(!Ps2Sockets::envelope(wrong));
        wrong=hello;wrong[26]=1;require(!Ps2Sockets::envelope(wrong));
        wrong=hello;wrong[7]=43;require(!Ps2Sockets::envelope(wrong));
        QUdpSocket nativeHost,nativeGuest,other;
        require(nativeHost.bind(QHostAddress::LocalHost,Ps2Sockets::HostPort,QUdpSocket::DontShareAddress));
        require(nativeGuest.bind(QHostAddress::LocalHost,0,QUdpSocket::DontShareAddress));
        require(other.bind(QHostAddress::LocalHost,0,QUdpSocket::DontShareAddress));
        int outgoing=0;Ps2Sockets* hostPtr=nullptr;Ps2Sockets* guestPtr=nullptr;
        Ps2Sockets host(true,[&](const QByteArray& b){++outgoing;guestPtr->receive(b);return true;});
        Ps2Sockets guest(false,[&](const QByteArray& b){++outgoing;hostPtr->receive(b);return true;});
        hostPtr=&host;guestPtr=&guest;require(host.start()&&guest.start());
        guest.receive(ack);require(!nativeGuest.hasPendingDatagrams());
        nativeGuest.writeDatagram(hello,QHostAddress::LocalHost,guest.port());
        spin([&]{return nativeHost.hasPendingDatagrams();});
        const auto received=nativeHost.receiveDatagram();require(received.data()==hello);
        nativeHost.writeDatagram(ack,received.senderAddress(),received.senderPort());
        spin([&]{return nativeGuest.hasPendingDatagrams();});require(nativeGuest.receiveDatagram().data()==ack);
        for(const auto& bytes:{first,last}) {
            nativeGuest.writeDatagram(bytes,QHostAddress::LocalHost,guest.port());
            spin([&]{return nativeHost.hasPendingDatagrams();});require(nativeHost.receiveDatagram().data()==bytes);
            nativeHost.writeDatagram(bytes,received.senderAddress(),received.senderPort());
            spin([&]{return nativeGuest.hasPendingDatagrams();});require(nativeGuest.receiveDatagram().data()==bytes);
        }
        const int before=outgoing;
        other.writeDatagram(hello,QHostAddress::LocalHost,guest.port());
        other.writeDatagram(hello,received.senderAddress(),received.senderPort());
        nativeGuest.writeDatagram(QByteArray(2048,'x'),QHostAddress::LocalHost,guest.port());
        nativeGuest.writeDatagram(wrong,QHostAddress::LocalHost,guest.port());
        QElapsedTimer timer;timer.start();while(timer.elapsed()<100){QCoreApplication::processEvents();QThread::msleep(1);}
        require(outgoing==before&&!nativeHost.hasPendingDatagrams());
        return 0;
    } catch(const std::exception& error) {qCritical("%s",error.what());return 1;}
}
