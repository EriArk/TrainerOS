// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "ps2_sockets.hpp"
#include <QLocalServer>
#include <QLocalSocket>
#include <QFile>

// Private pathname socket crosses the emulator's network namespace; no IP
// listener exists in the ordinary host namespace. Framing is local IPC only.
class Ps2Channel final : public QObject {
public:
    using Sender=Ps2Sockets::Sender;
    Ps2Channel(Sender send, std::function<void()> fail, QObject* parent)
        :QObject(parent),send_(std::move(send)),fail_(std::move(fail)),server_(this) {}
    bool start(const QString& path) {
        server_.setSocketOptions(QLocalServer::UserAccessOption);
        connect(&server_,&QLocalServer::newConnection,this,[this]{
            while(server_.hasPendingConnections()) {
                auto* incoming=server_.nextPendingConnection();
                if(socket_||accepted_){incoming->abort();incoming->deleteLater();continue;}
                socket_=incoming;accepted_=true;socket_->setReadBufferSize(65536);
                connect(socket_,&QLocalSocket::readyRead,this,[this]{read();});
                connect(socket_,&QLocalSocket::disconnected,this,[this]{fail_();});
            }
        });
        return server_.listen(path)&&QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner);
    }
    void receive(const QByteArray& bytes) {
        if(!socket_||!Ps2Sockets::envelope(bytes))return;
        if(socket_->bytesToWrite()+bytes.size()+2>65536)return;
        QByteArray packet(2,0);qToBigEndian<quint16>(bytes.size(),packet.data());packet+=bytes;
        if(socket_->write(packet)!=packet.size())fail_();
    }
private:
    void read() {
        bytes_+=socket_->readAll();if(bytes_.size()>65536){fail_();return;}
        while(bytes_.size()>=2) {
            const auto size=qFromBigEndian<quint16>(bytes_.constData());
            if(size<44||size>1044){fail_();return;}
            if(bytes_.size()<size+2)return;
            const auto packet=bytes_.mid(2,size);bytes_.remove(0,size+2);
            if(!Ps2Sockets::envelope(packet)){fail_();return;}
            send_(packet);
        }
    }
    Sender send_;std::function<void()> fail_;QLocalServer server_;
    QLocalSocket* socket_=nullptr;QByteArray bytes_;bool accepted_=false;
};
