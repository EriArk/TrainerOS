// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QLocalServer>
#include <QLocalSocket>
#include <QFile>
#include <QtEndian>
#include <functional>

// Opaque native Flycast datagrams, framed only across private local IPC.
// Native Naomi payload (4 + 0x4000 bytes) plus two bounded seat IDs.
// No arbitrary IP destination crosses this channel.
class CabinetChannel final : public QObject {
public:
    static constexpr int MaxPacket=0x4006;
    static constexpr int QueueBytes=1024*1024;
    using Sender=std::function<bool(const QByteArray&)>;
    CabinetChannel(Sender send,std::function<void()> fail,QObject* parent=nullptr)
        :QObject(parent),send_(std::move(send)),fail_(std::move(fail)),server_(this) {}
    bool start(const QString& path) {
        server_.setSocketOptions(QLocalServer::UserAccessOption);
        connect(&server_,&QLocalServer::newConnection,this,[this]{
            while(server_.hasPendingConnections()) {
                auto* incoming=server_.nextPendingConnection();
                if(accepted_){incoming->abort();incoming->deleteLater();continue;}
                accepted_=true;socket_=incoming;socket_->setReadBufferSize(QueueBytes);
                connect(socket_,&QLocalSocket::readyRead,this,[this]{read();});
                connect(socket_,&QLocalSocket::disconnected,this,[this]{failed();});
            }
        });
        return server_.listen(path)&&QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner);
    }
    void receive(const QByteArray& bytes) {
        if(bytes.isEmpty()||bytes.size()>MaxPacket){failed();return;}
        if(failed_||!socket_)return; // Native discovery retries before local attach.
        if(socket_->bytesToWrite()+bytes.size()+2>QueueBytes){failed();return;}
        QByteArray packet(2,0);qToBigEndian<quint16>(bytes.size(),packet.data());packet+=bytes;
        if(socket_->write(packet)!=packet.size())failed();
    }
private:
    void failed(){if(failed_)return;failed_=true;server_.close();if(socket_)socket_->abort();fail_();}
    void read() {
        if(failed_)return;
        while(socket_->bytesAvailable()>0) {
            bytes_+=socket_->read(65536);
            while(bytes_.size()>=2) {
                const auto size=qFromBigEndian<quint16>(bytes_.constData());
                if(size<1||size>MaxPacket){failed();return;}
                if(bytes_.size()<size+2)break;
                const auto packet=bytes_.mid(2,size);bytes_.remove(0,size+2);
                if(!send_(packet)){failed();return;}
            }
        }
    }
    Sender send_;std::function<void()> fail_;QLocalServer server_;
    QLocalSocket* socket_=nullptr;QByteArray bytes_;bool accepted_=false,failed_=false;
};
