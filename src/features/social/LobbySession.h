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
    // A validated account-local checkpoint restores membership, never a launch.
    bool restore(const QJsonObject& state);
    bool setAccess(const QString& actor,QString access);
    bool setDetails(const QString& actor,QString title,qint64 startsAt);
    bool setConversation(const QString& actor,QString channel);
    bool invite(const QString& actor,QString peer,qint64 expiresAt);
    bool join(QString peer,QString name,qint64 now,bool watch=false);
    bool approve(const QString& actor,const QString& peer,QString name,bool watch=false);
    bool leave(const QString& peer);
    bool remove(const QString& actor,const QString& peer);
    bool close(const QString& actor);
    bool selectGame(const QString& actor,const Game& game);
    bool proposeVote(const QString& actor,const QJsonArray& choices);
    bool vote(const QString& actor,QString id,int choice);
    bool confirmVote(const QString& actor,QString id,int choice);
    bool spectate(const QString& peer,bool value,quint64 generation);
    bool claimSeat(const QString& peer,int seat,quint64 generation);
    bool prepared(const QString& peer,quint64 generation,bool value);
    bool ready(const QString& peer,quint64 generation,bool value);
    bool canStart(const QString& actor) const;
    bool start(const QString& actor);
    bool connected(const QString& peer,bool value);
    // Only the runtime coordinator acknowledges/finishes the pending launch.
    bool acknowledgeLaunch(const QString& ticket,bool success);
    bool finishGame(const QString& ticket);
    bool finishPlayer(const QString& peer,const QString& ticket);
    bool setLiveJoining(const QString& actor,bool enabled);
    int liveSeat(const QString& peer) const;
    bool admitLive(const QString& peer,const QString& token,int seat);
    bool releaseLive(const QString& peer,const QString& ticket,const QString& token);
    QJsonObject state() const;
    QString id() const {return id_;}
    QStringList invitedPeers(qint64 now=0) const {QStringList peers;for(auto it=invites_.cbegin();it!=invites_.cend();++it)if(it.value()>now)peers.append(it.key());return peers;}
    quint64 generation() const {return generation_;}
signals:
    void changed();
    void launchRequested(QString ticket,QJsonObject party);
private:
    struct Member {QString name;int seat=0;bool prepared=false,ready=false,connected=true,spectator=false;QString admission;};
    bool host(const QString& actor) const;
    bool editable() const;
    int freeSeat() const;
    void resetReady();
    void invalidateLaunch();
    void publish();
    QString id_,owner_,access_,phase_="closed",ticket_,conversation_;
    Game game_;
    QMap<QString,Member> members_;
    QMap<QString,qint64> invites_;
    quint64 revision_=0,generation_=0;
    bool liveJoining_=false;
    QString title_;
    qint64 startsAt_=0;
    QJsonObject ballot_;
};
}
