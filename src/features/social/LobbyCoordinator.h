#pragma once
#include "LobbySession.h"
#include <QTimer>

namespace trainer {
// Provider-authenticated metadata coordination, independent of any emulator.
// The runtime coordinator alone supplies preparation and acknowledges launches.
class LobbyCoordinator final : public QObject {
    Q_OBJECT
public:
    explicit LobbyCoordinator(QObject* parent=nullptr);
    void configure(QString self,QString name,bool online);
    bool create(QString access="private");
    bool invite(const QString& peer);
    bool join(const QString& host,const QString& lobby);
    void answer(bool accept);
    bool approve(const QString& peer);
    void leave();
    bool selectGame(const LobbySession::Game& game);
    bool seat(int seat);
    bool spectate(bool value);
    bool prepared(quint64 generation,bool value);
    bool ready(bool value);
    bool setAccess(QString access);
    void receive(QString peer,QString name,const QJsonObject& packet);
    void deliveryFailed(const QString& peer);
    QJsonObject state() const;
    QJsonObject invitation() const {return invitation_;}
    QJsonArray requests() const;
    bool hosting() const;
    bool acknowledgeLaunch(const QString& ticket,bool success);
    bool finishGame(const QString& ticket);
signals:
    void changed();
    void outgoing(QString peer,QJsonObject packet);
    void launchRequested(QString ticket,QJsonObject party);
    void notice(QString message);
private:
    void send(const QString& peer,QString kind,QJsonObject data={});
    void broadcast();
    void tick();
    void clearRemote();
    static bool validState(const QJsonObject& state,const QString& host);
    LobbySession session_{this};
    QTimer heartbeat_{this};
    QString self_,name_,host_,joining_,launchTicket_;
    QString boot_;
    quint64 serial_=0;
    QMap<QString,QString> peerBoots_;
    QMap<QString,quint64> peerSerials_;
    QMap<QString,qint64> retiredBoots_;
    bool online_=false;
    QJsonObject remote_,invitation_;
    QMap<QString,QString> requests_;
    QMap<QString,qint64> seen_;
    qint64 joinedAt_=0,hostSeen_=0;
    quint64 preparedGeneration_=0,readyGeneration_=0;
};
}
