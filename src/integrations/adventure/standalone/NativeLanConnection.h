// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

namespace trainer {
// Local launch material owned by Lobby, never a peer-supplied destination.
// One edge per accepted guest; all consoles share a generation-bound LAN ID.
struct NativeLanConnection {
    QString mode,room;
    int seat=0,players=0;
    QMap<int,QString> sockets;
    bool valid() const {
        if((mode!="ps2"&&mode!="bba"&&mode!="xbox")||!QRegularExpression("^[a-f0-9]{12}$").match(room).hasMatch()
           ||players<2||players>4||seat<1||seat>players||sockets.size()!=(seat==1?players-1:1))return false;
        QSet<QString> paths;
        for(auto it=sockets.cbegin();it!=sockets.cend();++it) {
            const auto path=it.value();
            if(it.key()==seat||it.key()<1||it.key()>players||(seat!=1&&it.key()!=1)
               ||!QFileInfo(path).isAbsolute()||QDir::cleanPath(path)!=path||path.toUtf8().size()>100
               ||!path.endsWith("/"+(mode=="xbox"?QString("bba"):mode)+".sock")||path.contains(QChar(0))||paths.contains(path))return false;
            paths.insert(path);
        }
        return true;
    }
    QString mac(int member=0) const {
        return (mode=="xbox"?QString("02:50:f2:"):QString("00:09:bf:"))+room.left(2)+":"+room.mid(2,2)+":"+QString::number(member?member:seat,16).rightJustified(2,'0');
    }
    QByteArray encode() const {
        QJsonObject peers;for(auto it=sockets.cbegin();it!=sockets.cend();++it)peers[QString::number(it.key())]=it.value();
        return QJsonDocument(QJsonObject{{"protocol",2},{"mode",mode},{"room",room},{"seat",seat},{"players",players},{"sockets",peers}}).toJson(QJsonDocument::Compact);
    }
    static NativeLanConnection read(const QString& path) {
        QFile file(path);QFileInfo info(file);
        if(!info.isAbsolute()||info.isSymLink()||!file.open(QIODevice::ReadOnly)||file.size()>4096
           ||(info.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)))return {};
        const auto o=QJsonDocument::fromJson(file.readAll()).object();
        if(o.size()!=6||o["protocol"]!=2)return {};
        NativeLanConnection c;c.mode=o["mode"].toString();c.room=o["room"].toString();c.seat=o["seat"].toInt();c.players=o["players"].toInt();
        const auto peers=o["sockets"].toObject();
        for(auto it=peers.begin();it!=peers.end();++it){const int seat=it.key().toInt();if(QString::number(seat)!=it.key())return {};c.sockets[seat]=it.value().toString();}
        return c.valid()?c:NativeLanConnection{};
    }
};
}
