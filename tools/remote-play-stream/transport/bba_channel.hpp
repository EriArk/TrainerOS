// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QLocalServer>
#include <QLocalSocket>
#include <QFile>
#include <QtEndian>
#include <functional>

// Dolphin's existing TAP server protocol: uint16 little-endian length, Ethernet
// frame. No host TAP/NIC, IP routing or emulator protocol implementation.
class BbaChannel final : public QObject {
public:
    using Sender=std::function<bool(const QByteArray&)>;
    BbaChannel(Sender send,std::function<void()> fail,QObject* parent=nullptr)
        :QObject(parent),send_(std::move(send)),fail_(std::move(fail)),server_(this) {}
    bool start(const QString& path) {
        server_.setSocketOptions(QLocalServer::UserAccessOption);
        connect(&server_,&QLocalServer::newConnection,this,[this]{
            while(server_.hasPendingConnections()) {
                auto* incoming=server_.nextPendingConnection();
                if(accepted_){incoming->abort();incoming->deleteLater();continue;}
                accepted_=true;socket_=incoming;socket_->setReadBufferSize(65536);
                connect(socket_,&QLocalSocket::readyRead,this,[this]{read();});
                connect(socket_,&QLocalSocket::disconnected,this,[this]{failed();});
            }
        });
        if(!server_.listen(path))return false;
#ifndef Q_OS_WIN
        if(!QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner))return false;
#endif
        return true;
    }
    static bool frame(const QByteArray& bytes) {return bytes.size()>=14&&bytes.size()<=1518;}
    void receive(const QByteArray& bytes) {
        if(!frame(bytes)){failed();return;}
        // Packets before the game opens its adapter and excess datagrams are
        // dropped, never queued into a later match or allowed to grow memory.
        if(failed_||!socket_||socket_->state()!=QLocalSocket::ConnectedState
           ||socket_->bytesToWrite()+bytes.size()+2>65536)return;
        QByteArray packet(2,0);qToLittleEndian<quint16>(bytes.size(),packet.data());packet+=bytes;
        if(socket_->write(packet)!=packet.size())failed();
    }
private:
    void failed(){if(failed_)return;failed_=true;server_.close();if(socket_)socket_->abort();fail_();}
    void read() {
        if(failed_)return;
        bytes_+=socket_->readAll();if(bytes_.size()>65536){failed();return;}
        while(bytes_.size()>=2) {
            const auto size=qFromLittleEndian<quint16>(bytes_.constData());
            if(size<14||size>1518){failed();return;}
            if(bytes_.size()<size+2)return;
            const auto packet=bytes_.mid(2,size);bytes_.remove(0,size+2);
            send_(packet); // Datagram congestion drops a frame, as on a LAN.
        }
    }
    Sender send_;std::function<void()> fail_;QLocalServer server_;
    QLocalSocket* socket_=nullptr;QByteArray bytes_;bool accepted_=false,failed_=false;
};
