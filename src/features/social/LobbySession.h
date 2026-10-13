#pragma once
#include <QObject>
#include <QJsonObject>
#include <QMap>

namespace trainer {
// Host-authoritative lobby state. The caller authenticates actor IDs and serializes
// commands on this object's thread. Public listings never authorize these commands.
// No transport credentials, local library paths or save data belong in this model.
class LobbySession final : public QObject {
    Q_OBJECT
public:
    struct Game {
        QString id, title, platform, mode;
        int seats=0; // Supplied by a supported runtime profile, not catalogue counts.
        QJsonObject runtime; // Private compatibility descriptor, after membership consent.
    };
    explicit LobbySession(QObject* parent=nullptr);
    bool create(QString owner,QString name,QString access="private");
    bool setAccess(const QString& actor,QString access);
    bool setConversation(const QString& actor,QString channel);
    bool invite(const QString& actor,QString peer,qint64 expiresAt);
    bool join(QString peer,QString name,qint64 now);
    bool approve(const QString& actor,const QString& peer,QString name);
    bool leave(const QString& peer);
    bool remove(const QString& actor,const QString& peer);
    bool close(const QString& actor);
    bool selectGame(const QString& actor,const Game& game);
    bool spectate(const QString& peer,bool value,quint64 generation);
    bool claimSeat(const QString& peer,int seat,quint64 generation);
    bool prepared(const QString& peer,quint64 generation,bool value);
    bool ready(const QString& peer,quint64 generation,bool value);
    bool connected(const QString& peer,bool value);
    // Only the runtime coordinator acknowledges/finishes the pending launch.
    bool acknowledgeLaunch(const QString& ticket,bool success);
    bool finishGame(const QString& ticket);
    QJsonObject state() const;
    QString id() const {return id_;}
    QStringList invitedPeers() const {return invites_.keys();}
    quint64 generation() const {return generation_;}
signals:
    void changed();
    void launchRequested(QString ticket,QJsonObject party);
private:
    struct Member {QString name;int seat=0;bool prepared=false,ready=false,connected=true,spectator=false;};
    bool host(const QString& actor) const;
    bool editable() const;
    int freeSeat() const;
    void resetReady();
    void invalidateLaunch();
    void publish();
    void tryLaunch();
    QString id_,owner_,access_,phase_="closed",ticket_,conversation_;
    Game game_;
    QMap<QString,Member> members_;
    QMap<QString,qint64> invites_;
    quint64 revision_=0,generation_=0;
};
}
