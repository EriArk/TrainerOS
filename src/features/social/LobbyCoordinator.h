#pragma once
#include "LobbySession.h"
#include <QTimer>
#include <QSet>

namespace trainer {
// Provider-authenticated metadata coordination, independent of any emulator.
// The runtime coordinator alone supplies preparation and acknowledges launches.
class LobbyCoordinator final : public QObject {
    Q_OBJECT
public:
    explicit LobbyCoordinator(QObject* parent=nullptr);
    void configure(QString self,QString name,bool online);
    void setStorage(QString file);
    bool recoveryUncertain() const {return restoring_||!storageError_.isEmpty();}
    void reconnect();
    bool remove(const QString& peer);
    qint64 nextPublicationRevision();
    QString publicationTopic() const {return publicationTopic_;}
    void setPublicationTopic(QString topic) {publicationTopic_=std::move(topic);save();}
    bool create(QString access="private");
    bool invite(const QString& peer);
    bool join(const QString& host,const QString& lobby,bool request=false);
    void answer(bool accept);
    bool approve(const QString& peer);
    void leave();
    bool selectGame(const LobbySession::Game& game);
    bool seat(int seat);
    bool spectate(bool value);
    bool requestPlay();
    bool answerPlay(const QString& peer,const QString& answer);
    bool prepared(quint64 generation,bool value);
    bool ready(bool value);
    bool setAccess(QString access);
    bool setDetails(QString title,qint64 startsAt);
    bool setConversation(QString channel);
    void receive(QString peer,QString name,const QJsonObject& packet);
    void deliveryFailed(const QString& peer);
    QJsonObject state() const;
    QJsonObject invitation() const {return invitation_;}
    QJsonArray requests() const;
    QStringList invitedPeers(qint64 now) const {return hosting()?session_.invitedPeers(now):QStringList{};}
    bool hosting() const;
    bool acknowledgeLaunch(const QString& ticket,bool success);
    bool finishGame(const QString& ticket);
    // Private runtime signaling only; never included in snapshots/checkpoints.
    // Provider authenticates peer; the runtime additionally binds connection ID.
    bool sendRuntimeSignal(const QString& peer,const QString& ticket,const QString& connection,const QJsonObject& payload);
    bool setLiveJoining(bool enabled);
    bool requestLive(QString operation);
    void cancelLive(QString operation);
    bool admitLive(QString peer,QString operation,int seat);
    void rejectLive(QString peer,QString operation);
    bool releaseRemotePlayer(QString peer,QString ticket,QString admission);
signals:
    void changed();
    void outgoing(QString peer,QJsonObject packet);
    void launchRequested(QString ticket,QJsonObject party);
    void liveJoinRequested(QString peer,QString operation,int seat);
    void liveLaunchRequested(QString operation,QString ticket,QJsonObject party);
    void liveJoinFailed(QString operation);
    void liveReleased(QString peer,QString operation);
    void runtimeSignal(QString peer,QString ticket,QString connection,QJsonObject payload);
    void notice(QString message);
    void membershipEnded(QString room);
private:
    friend class LobbyTests;
    void sendInvitation(const QString& peer);
    void send(const QString& peer,QString kind,QJsonObject data={},QString reply={},QString lobby={});
    void save();
    void broadcast();
    void tick();
    void clearRemote();
    bool recordLaunchResult(const QString& peer,const QString& ticket,bool success);
    QString resultTicket_;
    QString liveOperation_;
    QSet<QString> launchResults_;
    static bool validState(const QJsonObject& state,const QString& host);
    bool runtimePeer(const QString& peer,const QString& ticket) const;
    void receiveRuntimeSignal(const QString& peer,const QJsonObject& packet);
    LobbySession session_{this};
    QTimer heartbeat_{this};
    QString self_,name_,host_,joining_,launchTicket_,finishedTicket_;
    QString boot_;
    QString file_,probe_,connection_,storageError_;
    bool restoring_=false,loading_=false;
    qint64 publicationRevision_=0;
    QString publicationTopic_="other";
    quint64 serial_=0;
    QMap<QString,QString> peerBoots_;
    QMap<QString,quint64> peerSerials_;
    QMap<QString,qint64> retiredBoots_;
    bool online_=false;
    bool joiningRequest_=false;
    QJsonObject remote_,invitation_;
    QMap<QString,QString> requests_;
    QMap<QString,QString> playRequests_;
    quint64 playRequestGeneration_=0;
    QMap<QString,qint64> seen_;
    QMap<QString,QJsonObject> pendingInvites_;
    QMap<QString,qint64> receivedInvites_;
    qint64 joinedAt_=0,hostSeen_=0;
    quint64 preparedGeneration_=0,readyGeneration_=0;
};
}
