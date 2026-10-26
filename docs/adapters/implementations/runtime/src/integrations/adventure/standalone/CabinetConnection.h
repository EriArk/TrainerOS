#pragma once
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>

namespace trainer::flycast {
// Local launch contract, never accepted from a network peer. Seat addresses are
// fixed loopback ports in the child's private namespace, not arbitrary targets.
struct CabinetConnection {
    int seat=0,players=0;
    QMap<int,QString> sockets;
    bool valid() const {
        if(players<2||players>4||seat<1||seat>players||sockets.size()!=(seat==1?players-1:1))return false;
        for(auto it=sockets.cbegin();it!=sockets.cend();++it)
            if(it.key()==seat||it.key()<1||it.key()>players||(seat!=1&&it.key()!=1)
               ||!QFileInfo(it.value()).isAbsolute()||!it.value().endsWith("/cabinet.sock"))return false;
        return true;
    }
    QByteArray encode() const {
        QJsonObject peers;for(auto it=sockets.cbegin();it!=sockets.cend();++it)peers[QString::number(it.key())]=it.value();
        return QJsonDocument(QJsonObject{{"protocol",2},{"seat",seat},{"players",players},{"sockets",peers}}).toJson(QJsonDocument::Compact);
    }
    static CabinetConnection read(const QString& path) {
        QFile file(path);QFileInfo info(file);
        if(!info.isAbsolute()||info.isSymLink()||!file.open(QIODevice::ReadOnly)||file.size()>4096
           ||(info.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)))return {};
        const auto o=QJsonDocument::fromJson(file.readAll()).object();
        if(o.size()!=4||o["protocol"]!=2)return {};
        CabinetConnection c;c.seat=o["seat"].toInt();c.players=o["players"].toInt();
        const auto peers=o["sockets"].toObject();
        for(auto it=peers.begin();it!=peers.end();++it){const int seat=it.key().toInt();if(QString::number(seat)!=it.key())return {};c.sockets[seat]=it.value().toString();}
        return c.valid()?c:CabinetConnection{};
    }
};
}
