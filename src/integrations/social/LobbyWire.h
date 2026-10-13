#pragma once
#include <QJsonObject>
#include <QMap>
#include <QStringList>

namespace trainer {
// Bounded metadata frames for provider messages. No emulator frames or files.
class LobbyWire {
public:
    static QStringList encode(const QJsonObject& packet,qint64 now);
    static QJsonObject frame(const QString& content);
    QJsonObject receive(const QString& authenticatedPeer,const QJsonObject& frame,qint64 now);
    void clear() {pending_.clear();}
private:
    struct Pending {int count=0;qint64 expires=0;QMap<int,QByteArray> parts;};
    QMap<QString,Pending> pending_;
};
}
