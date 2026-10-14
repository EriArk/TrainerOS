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
LobbyInput::~LobbyInput() {revoke();}
bool LobbyInput::matches() const {
    const auto s=lobby_.state();
    if(!active()||!lobby_.hosting()||lobby_.recoveryUncertain()||s["recovering"].toBool()
       ||!s["connection"].toString().isEmpty()||s["id"]!=lobbyId_||s["ticket"]!=ticket_
       ||s["generation"].toInteger()!=generation_||s["phase"]!="playing"||s["game"].toObject()!=game_
       ||game_["mode"]!="remoteplay.v1"||game_["seats"].toInt()<port_)return false;
    bool owner=false,guest=false;
    for(const auto& v:s["members"].toArray()) {
        const auto m=v.toObject();const auto seat=m["seat"].toInt();
        if(m["id"]==s["owner"])owner=m["connected"].toBool()&&!m["spectator"].toBool()&&seat==1;
        if(m["id"]==peer_)guest=m["connected"].toBool()&&!m["spectator"].toBool()
            &&seat==port_&&m["admission"].toString()==admission_;
        else if(seat==port_)return false;
    }
    return owner&&guest&&peer_!=s["owner"].toString();
}
bool LobbyInput::authorize(QString peer,QString connection,QString ticket,int reservedPort) {
    // One guest in the current minimal transport. Do not silently replace a
    // running connection or pretend its fixed UDP endpoints support more guests.
    if(active()||peer.isEmpty()||QUuid(connection).isNull()||ticket.isEmpty()||reservedPort!=2)return false;
    const auto s=lobby_.state();peer_=peer;connection_=connection;ticket_=ticket;port_=reservedPort;
    lobbyId_=s["id"].toString();generation_=s["generation"].toInteger();game_=s["game"].toObject();
    for(const auto& v:s["members"].toArray())if(v.toObject()["id"]==peer_)admission_=v.toObject()["admission"].toString();
    if(!matches()) {revoke();return false;}
#ifdef Q_OS_LINUX
    int pair[2];
    if(socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0,pair)) {revoke();return false;}
    writer_=pair[0];reader_=pair[1];
#endif
    timer_.start();heartbeat();return active();
}
int LobbyInput::takeDescriptor(const QString& connection) {
    if(connection!=connection_||!matches())return -1;
    const auto fd=reader_;reader_=-1;return fd;
}
void LobbyInput::release(const QString& connection) {if(connection==connection_)revoke();}
void LobbyInput::reconcile() {if(active()&&!matches())revoke();}
void LobbyInput::heartbeat() {
    reconcile();
#ifdef Q_OS_LINUX
    if(writer_<0)return;
    const uint64_t now=std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    char packet[9];packet[0]='1';std::memcpy(packet+1,&now,sizeof(now));
    const auto sent=send(writer_,packet,sizeof(packet),MSG_DONTWAIT|MSG_NOSIGNAL);
    if(sent<0&&errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR)revoke();
#endif
}
void LobbyInput::revoke() {
    timer_.stop();const auto old=connection_;connection_.clear();peer_.clear();ticket_.clear();admission_.clear();
    game_={};lobbyId_.clear();generation_=0;port_=0;
#ifdef Q_OS_LINUX
    if(writer_>=0)close(writer_);
    if(reader_>=0)close(reader_);
#endif
    writer_=reader_=-1;
    if(!old.isEmpty())emit revoked(old);
}
}
