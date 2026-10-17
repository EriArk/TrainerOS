#pragma once
#include "LobbyRoomCoordinator.h"
#include <QList>
#include <QJsonArray>

namespace trainer {
// Stable runtime endpoint. Memberships live independently; viewing a room never
// retargets the emulator/input connection. Only explicit preparation activates it.
class LobbyCoordinator final : public QObject {
    Q_OBJECT
public:
    explicit LobbyCoordinator(QObject* parent=nullptr);
    QList<LobbyRoomCoordinator*> rooms() const {return rooms_;}
    LobbyRoomCoordinator* room(const QString& id) const;
    LobbyRoomCoordinator* activeRoom() const {return active_;}
    LobbyRoomCoordinator* createRoom(QString access="private");
    bool activateRoom(const QString& id);
    void configure(QString self,QString name,bool online);
    void setStorage(QString file);
    bool recoveryUncertain() const { return active_->recoveryUncertain(); }
    void reconnect() { active_->reconnect(); }
    bool remove(const QString& peer) { return active_->remove(peer); }
    qint64 nextPublicationRevision() { return active_->nextPublicationRevision(); }
    QString publicationTopic() const { return active_->publicationTopic(); }
    void setPublicationTopic(QString topic) { active_->setPublicationTopic(topic); }
    bool create(QString access="private");
    bool invite(const QString& peer) { return active_->invite(peer); }
    bool join(const QString& host,const QString& lobby,bool request=false,bool watch=false);
    void answer(bool accept);
    bool approve(const QString& peer) { return active_->approve(peer); }
    void leave() { active_->leave(); }
    bool selectGame(const LobbySession::Game& game) { return active_->selectGame(game); }
    bool seat(int seat) { return active_->seat(seat); }
    bool spectate(bool value) { return active_->spectate(value); }
    bool requestPlay() { return active_->requestPlay(); }
    bool answerPlay(const QString& peer,const QString& answer) { return active_->answerPlay(peer,answer); }
    bool prepared(quint64 generation,bool value) { return active_->prepared(generation,value); }
    bool ready(bool value) { return active_->ready(value); }
    bool start() { return active_->start(); }
    bool setAccess(QString access) { return active_->setAccess(access); }
    bool setDetails(QString title,qint64 startsAt) { return active_->setDetails(title,startsAt); }
    bool setConversation(QString channel) { return active_->setConversation(channel); }
    void receive(QString peer,QString name,const QJsonObject& packet);
    void deliveryFailed(const QString& peer);
    QJsonObject state() const { return active_->state(); }
    QJsonObject invitation() const;
    QJsonArray requests() const { return active_->requests(); }
    QStringList invitedPeers(qint64 now) const { return active_->invitedPeers(now); }
    bool hosting() const { return active_->hosting(); }
    bool acknowledgeLaunch(const QString& ticket,bool success) { return active_->acknowledgeLaunch(ticket,success); }
    bool finishGame(const QString& ticket) { return active_->finishGame(ticket); }
    bool sendRuntimeSignal(const QString& peer,const QString& ticket,const QString& connection,const QJsonObject& payload) { return active_->sendRuntimeSignal(peer,ticket,connection,payload); }
    bool setLiveJoining(bool enabled) { return active_->setLiveJoining(enabled); }
    bool requestLive(QString operation) { return active_->requestLive(operation); }
    void cancelLive(QString operation) { active_->cancelLive(operation); }
    bool admitLive(QString peer,QString operation,int seat) { return active_->admitLive(peer,operation,seat); }
    void rejectLive(QString peer,QString operation) { active_->rejectLive(peer,operation); }
    bool releaseRemotePlayer(QString peer,QString ticket,QString admission) { return active_->releaseRemotePlayer(peer,ticket,admission); }
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
    void roomsChanged();
private:
    LobbyRoomCoordinator* appendRoom();
    void saveIndex();
    QList<LobbyRoomCoordinator*> rooms_;
    QSet<QString> departedRooms_;
    LobbyRoomCoordinator* active_=nullptr;
    QString self_,name_,file_;
    bool online_=false,loading_=false,indexError_=false;
};
}
