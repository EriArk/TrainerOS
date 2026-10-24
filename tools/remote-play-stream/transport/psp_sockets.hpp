// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QtEndian>
#include <functional>

// Fixed PSP services over the already authenticated reliable RTC channel.
// No peer-supplied destination addresses, game protocol or packet rewriting.
class PspSockets final:public QObject {
public:
    using Sender=std::function<bool(const QByteArray&)>;
    PspSockets(bool host,Sender send,std::function<void()> fail,QObject* parent)
        :QObject(parent),host_(host),send_(std::move(send)),fail_(std::move(fail)) {}
    bool start() {
        if(host_)return true;
        for(int i=0;i<2;++i) {
            auto* server=new QTcpServer(this);
            if(!server->listen(QHostAddress("127.0.0.2"),27312+i))return false;
            connect(server,&QTcpServer::newConnection,this,[this,server,i]{
                while(server->hasPendingConnections()) {
                    auto* socket=server->nextPendingConnection();
                    if(socket->peerAddress()!=QHostAddress::LocalHost||sockets_.size()>=32||next_>=100000) {
                        socket->abort();socket->deleteLater();continue;
                    }
                    const quint32 id=++next_;attach(id,socket);
                    frame(id,'O',QByteArray(1,char(i)));
                }
            });
        }
        return true;
    }
    void receive(const QByteArray& packet) {
        if(packet.size()<5||packet.size()>16389){fail_();return;}
        const quint32 id=qFromBigEndian<quint32>(packet.constData());const char op=packet[4];
        const auto data=packet.mid(5);
        if(!id||id>100000){fail_();return;}
        if(op=='O') {
            if(!host_||id!=next_+1||data.size()!=1||data[0]<0||data[0]>1||sockets_.size()>=32){fail_();return;}
            next_=id;auto* socket=new QTcpSocket(this);attach(id,socket);
            // PPSSPP's coordinator distinguishes the host and guest by IP.
            if(!socket->bind(QHostAddress("127.0.0.2"),0)){finish(id,true);return;}
            socket->connectToHost(QHostAddress::LocalHost,27312+int(data[0]));
            QTimer::singleShot(5000,socket,[this,id,socket]{
                if(sockets_.value(id)==socket&&socket->state()!=QAbstractSocket::ConnectedState)finish(id,true);
            });
            return;
        }
        if(id>next_||(op!='D'&&op!='C'&&op!='A')||(op=='C'&&!data.isEmpty())||(op=='D'&&data.isEmpty())||(op=='A'&&data.size()!=4)){fail_();return;}
        auto* socket=sockets_.value(id);
        // Opposite directions may cross an ordinary socket close. IDs never
        // repeat, so these late bytes cannot enter a new native connection.
        if(!socket)return;
        if(op=='A') {
            const auto count=qFromBigEndian<quint32>(data.constData());
            if(!count||count>outstanding_.value(id)){fail_();return;}
            outstanding_[id]-=count;read(id,socket);return;
        }
        if(op=='C'){finish(id,false);return;}
        if(socket->bytesToWrite()+data.size()>65536){fail_();return;}
        if(socket->write(data)!=data.size())finish(id,true);
    }
private:
    void frame(quint32 id,char op,const QByteArray& data={}) {
        QByteArray packet(5,0);qToBigEndian(id,packet.data());packet[4]=op;packet+=data;send_(packet);
    }
    void read(quint32 id,QTcpSocket* socket) {
        if(sockets_.value(id)!=socket)return;
        while(socket->bytesAvailable()&&outstanding_.value(id)<65536) {
            auto data=socket->read(qMin<qint64>(16384,65536-outstanding_.value(id)));
            if(data.isEmpty())break;
            outstanding_[id]+=data.size();frame(id,'D',data);
        }
        if(closed_.contains(id)&&!socket->bytesAvailable())finish(id,true);
    }
    void attach(quint32 id,QTcpSocket* socket) {
        sockets_[id]=socket;socket->setReadBufferSize(65536);
        socket->setSocketOption(QAbstractSocket::LowDelayOption,1);
        connect(socket,&QTcpSocket::connected,this,[socket]{socket->setSocketOption(QAbstractSocket::LowDelayOption,1);});
        connect(socket,&QTcpSocket::readyRead,this,[this,id,socket]{read(id,socket);});
        connect(socket,&QTcpSocket::bytesWritten,this,[this,id](qint64 size){
            QByteArray count(4,0);qToBigEndian(quint32(size),count.data());frame(id,'A',count);
        });
        connect(socket,&QTcpSocket::disconnected,this,[this,id,socket]{closed_.insert(id);read(id,socket);});
        connect(socket,&QTcpSocket::errorOccurred,this,[this,id](auto error){
            if(error!=QAbstractSocket::RemoteHostClosedError)finish(id,true);
        });
    }
    void finish(quint32 id,bool notify) {
        auto* socket=sockets_.take(id);if(!socket)return;
        outstanding_.remove(id);closed_.remove(id);
        socket->disconnect(this);
        if(notify)frame(id,'C');
        // Flush already accepted bytes before closing the local TCP peer.
        connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        socket->disconnectFromHost();
        if(socket->state()==QAbstractSocket::UnconnectedState)socket->deleteLater();
        else QTimer::singleShot(5000,socket,[socket]{socket->abort();socket->deleteLater();});
    }
    bool host_;Sender send_;std::function<void()> fail_;quint32 next_=0;
    QHash<quint32,QTcpSocket*> sockets_;
    QHash<quint32,quint32> outstanding_;QSet<quint32> closed_;
};
