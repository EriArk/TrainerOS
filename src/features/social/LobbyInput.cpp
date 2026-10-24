#include "LobbyInput.h"
#include <QJsonArray>
#include <QUuid>
#include <chrono>
#include <cstring>
#ifdef Q_OS_LINUX
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace trainer {
LobbyInput::LobbyInput(LobbyCoordinator& lobby,QObject* parent):QObject(parent),lobby_(lobby) {
    connect(&lobby_,&LobbyCoordinator::changed,this,&LobbyInput::reconcile);
    timer_.setInterval(50);timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_,&QTimer::timeout,this,&LobbyInput::heartbeat);
}
LobbyInput::~LobbyInput() {for(const auto& id:grants_.keys())revoke(id);}
bool LobbyInput::matches(const Grant& grant) const {
    const auto s=lobby_.state();
    if(!lobby_.hosting()||lobby_.recoveryUncertain()||s["recovering"].toBool()
       ||!s["connection"].toString().isEmpty()||s["id"]!=grant.lobby||s["ticket"]!=grant.ticket
       ||s["generation"].toInteger()!=grant.generation||s["phase"]!="playing"||s["game"].toObject()!=grant.game
       ||grant.game["mode"]!="remoteplay.v1"||grant.game["seats"].toInt()<grant.port)return false;
    bool owner=false,guest=false;
    for(const auto& v:s["members"].toArray()) {
        const auto m=v.toObject();const auto seat=m["seat"].toInt();
        if(m["id"]==s["owner"])owner=m["connected"].toBool()&&!m["spectator"].toBool()&&seat==1;
        if(m["id"]==grant.peer)guest=m["connected"].toBool()&&!m["spectator"].toBool()
            &&seat==grant.port&&m["admission"].toString()==grant.admission;
        else if(seat==grant.port)return false;
    }
    return owner&&guest&&grant.peer!=s["owner"].toString();
}
bool LobbyInput::authorize(QString peer,QString connection,QString ticket,int port) {
    if(grants_.contains(connection)||peer.isEmpty()||QUuid(connection).isNull()||ticket.isEmpty()||port<2||port>4)return false;
    for(const auto& grant:grants_)if(grant.peer==peer||grant.port==port)return false;
    const auto s=lobby_.state();Grant grant;grant.peer=peer;grant.ticket=ticket;grant.port=port;
    grant.lobby=s["id"].toString();grant.generation=s["generation"].toInteger();grant.game=s["game"].toObject();
    for(const auto& v:s["members"].toArray())if(v.toObject()["id"]==peer)grant.admission=v.toObject()["admission"].toString();
    if(!matches(grant))return false;
#ifdef Q_OS_LINUX
    int pair[2];if(socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0,pair))return false;
    grant.writer=pair[0];grant.reader=pair[1];
#endif
    grants_.insert(connection,grant);timer_.start();heartbeat();return grants_.contains(connection);
}
int LobbyInput::takeDescriptor(const QString& connection) {
    auto it=grants_.find(connection);if(it==grants_.end()||!matches(*it))return -1;
    const int fd=it->reader;it->reader=-1;return fd;
}
void LobbyInput::release(const QString& connection) {revoke(connection);}
void LobbyInput::reconcile() {for(const auto& id:grants_.keys())if(!matches(grants_[id]))revoke(id);}
void LobbyInput::heartbeat() {
    reconcile();
#ifdef Q_OS_LINUX
    const uint64_t now=std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    char packet[9];packet[0]='1';std::memcpy(packet+1,&now,sizeof(now));
    for(const auto& id:grants_.keys()) {
        const auto sent=send(grants_[id].writer,packet,sizeof(packet),MSG_DONTWAIT|MSG_NOSIGNAL);
        if(sent<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)revoke(id);
    }
#endif
}
void LobbyInput::revoke(const QString& id) {
    auto it=grants_.find(id);if(it==grants_.end())return;
#ifdef Q_OS_LINUX
    if(it->writer>=0)close(it->writer);
    if(it->reader>=0)close(it->reader);
#endif
    grants_.erase(it);if(grants_.isEmpty())timer_.stop();emit revoked(id);
}
}
