#include "LobbyRoomCoordinator.h"
#include "core/model/LobbyLimits.h"
#include "core/model/NetplayRelay.h"
#include <QDateTime>
#include <QJsonArray>
#include <QSet>
#include <QUuid>
#include <QJsonDocument>

namespace trainer {
namespace {
qint64 now() {return QDateTime::currentSecsSinceEpoch();}
bool identifier(const QString& id) {
    if(id.isEmpty()||id.size()>20)return false;
    for(const auto c:id)if(c<'0'||c>'9')return false;
    return true;
}
}
LobbyRoomCoordinator::LobbyRoomCoordinator(QObject* parent):QObject(parent) {
    readyDeadline_.setSingleShot(true);readyDeadline_.setInterval(45000);
    connect(&readyDeadline_,&QTimer::timeout,this,[this]{
        readyGeneration_=0;
        send(host_,"ready",{{"value",false}});
        prepared(quint64(remote_["generation"].toInteger()),false);
        emit notice("Ready confirmation timed out. Please try again.");emit changed();
    });
    boot_=QUuid::createUuid().toString(QUuid::WithoutBraces);
    heartbeat_.setInterval(15000);connect(&heartbeat_,&QTimer::timeout,this,&LobbyRoomCoordinator::tick);heartbeat_.start();
    connect(this,&LobbyRoomCoordinator::changed,this,&LobbyRoomCoordinator::save);
    connect(&session_,&LobbySession::changed,this,[this]{
        if(loading_)return;
        if(playRequestGeneration_!=session_.generation()||session_.state()["phase"]=="closed") {
            playRequests_.clear();playRequestGeneration_=session_.generation();
        }
        QSet<QString> members;for(const auto& m:session_.state()["members"].toArray())members.insert(m.toObject()["id"].toString());
        for(auto it=playRequests_.begin();it!=playRequests_.end();)if(!members.contains(it.key()))it=playRequests_.erase(it);else ++it;
        broadcast();emit changed();
    });
    connect(&session_,&LobbySession::launchRequested,this,&LobbyRoomCoordinator::launchRequested);
}
bool LobbyRoomCoordinator::hosting() const {return session_.ownedBy(self_);}
bool LobbyRoomCoordinator::runtimePeer(const QString& peer,const QString& ticket) const {
    const auto s=state();
    if(!online_||recoveryUncertain()||s["recovering"].toBool()||!s["connection"].toString().isEmpty()
       ||ticket.isEmpty()||s["ticket"]!=ticket||(s["phase"]!="launching"&&s["phase"]!="playing")
       ||peer==self_||(s["owner"]!=self_&&s["owner"]!=peer))return false;
    bool mine=false,theirs=false;
    for(const auto& v:s["members"].toArray()) {
        const auto m=v.toObject();const bool player=m["connected"].toBool()&&!m["spectator"].toBool()&&m["seat"].toInt()>0;
        if(m["id"]==self_)mine=player;
        if(m["id"]==peer)theirs=player;
    }
    return mine&&theirs;
}
bool LobbyRoomCoordinator::sendRuntimeSignal(const QString& peer,const QString& ticket,const QString& connection,const QJsonObject& payload) {
    if(!runtimePeer(peer,ticket)||QUuid(connection).isNull()||payload.isEmpty()
       ||QJsonDocument(payload).toJson(QJsonDocument::Compact).size()>20000)return false;
    send(peer,"runtime-signal",{{"ticket",ticket},{"connection",connection},{"payload",payload}});return true;
}
void LobbyRoomCoordinator::receiveRuntimeSignal(const QString& peer,const QJsonObject& packet) {
    const auto data=packet["data"].toObject();const auto payload=data["payload"].toObject();
    const auto ticket=data["ticket"].toString(),connection=data["connection"].toString();
    if(data.size()!=3||packet["generation"]!=state()["generation"]||!runtimePeer(peer,ticket)
       ||QUuid(connection).isNull()||payload.isEmpty()||QJsonDocument(payload).toJson(QJsonDocument::Compact).size()>20000)return;
    emit runtimeSignal(peer,ticket,connection,payload);
}
QJsonObject LobbyRoomCoordinator::state() const {
    if(hosting()) {auto s=session_.state();s["connection"]=online_?storageError_:QString("Reconnecting to communication…");return s;}
    if(remote_.isEmpty()&&!host_.isEmpty())return {{"id",joining_},{"owner",host_},{"phase","joining"}};
    if(remote_.isEmpty())return {};
    auto s=remote_;s["recovering"]=restoring_;s["connection"]=storageError_.isEmpty()?connection_:storageError_;return s;
}
QJsonArray LobbyRoomCoordinator::requests() const {
    QJsonArray list;for(auto it=requests_.cbegin();it!=requests_.cend();++it)list.append(QJsonObject{{"id",it.key()},{"name",it.value()}});
    for(auto it=playRequests_.cbegin();it!=playRequests_.cend();++it)list.append(QJsonObject{{"id",it.key()},{"name",it.value()},{"play",true}});
    return list;
}
void LobbyRoomCoordinator::clearRemote() {
    readyDeadline_.stop();
    preparationAwaitingAck_=false;
    pendingLaunchResult_={};
    liveOperation_.clear();
    peerBoots_.remove(host_);peerSerials_.remove(host_);
    remote_={};host_.clear();joining_.clear();launchTicket_.clear();finishedTicket_.clear();preparedGeneration_=readyGeneration_=0;joinedAt_=hostSeen_=0;
    restoring_=false;probe_.clear();connection_.clear();
}
void LobbyRoomCoordinator::configure(QString self,QString name,bool online) {
    name=name.left(100);online=online&&identifier(self);
    if(self_==self&&name_==name&&online_==online)return;
    if(self_!=self) {
        online_=false;leave();file_.clear();storageError_.clear();clearRemote();invitation_={};requests_.clear();seen_.clear();self_=self;
        peerBoots_.clear();peerSerials_.clear();retiredBoots_.clear();serial_=0;boot_=QUuid::createUuid().toString(QUuid::WithoutBraces);
        publicationRevision_=0;publicationTopic_="other";
        pendingInvites_.clear();receivedInvites_.clear();
    }
    name_=name;const bool changed=online_!=online;online_=online;
    if(hosting()&&changed)session_.connected(self_,online_);
    if(!online_) {readyDeadline_.stop();preparationAwaitingAck_=false;preparedGeneration_=readyGeneration_=0;if(!host_.isEmpty())connection_="Reconnecting to communication…";}
    emit this->changed();
    if(changed&&online_)tick();
}
bool LobbyRoomCoordinator::create(QString access) {
    if(!online_||hosting()||!host_.isEmpty())return false;
    clearRemote();return session_.create(self_,name_,access);
}
void LobbyRoomCoordinator::send(const QString& peer,QString kind,QJsonObject data,QString reply,QString targetLobby) {
    if(!online_||!identifier(peer)||peer==self_)return;
    const auto lobby=!targetLobby.isEmpty()?targetLobby:hosting()?session_.id():!joining_.isEmpty()?joining_:remote_.value("id").toString();
    emit outgoing(peer,{{"ns","org.traineros.lobby"},{"v",1},{"kind",kind},{"lobby",lobby},
        {"boot",boot_},{"serial",qint64(++serial_)},
        {"expires",now()+90},{"generation",state()["generation"]},{"reply",reply},{"data",data}});
}
void LobbyRoomCoordinator::broadcast() {
    if(!hosting()||!online_||loading_)return;
    const auto snapshot=session_.state();
    for(const auto& v:snapshot["members"].toArray()) {
        const auto peer=v.toObject()["id"].toString();if(peer!=self_)send(peer,"state",snapshot);
    }
}
bool LobbyRoomCoordinator::invite(const QString& peer) {
    if(!hosting()||!identifier(peer)||!session_.invite(self_,peer,now()+300))return false;
    pendingInvites_[peer]={{"until",now()+300},{"offer",QUuid::createUuid().toString(QUuid::WithoutBraces)}};
    sendInvitation(peer);return true;
}
void LobbyRoomCoordinator::sendInvitation(const QString& peer) {
    const auto pending=pendingInvites_.value(peer);if(pending.isEmpty()||pending["until"].toInteger()<=now())return;
    auto game=session_.state()["game"].toObject();game.remove("runtime");
    const auto state=session_.state();
    send(peer,"invite",{{"name",name_},{"game",game},{"title",state["title"]},{"startsAt",state["startsAt"]},{"answerUntil",pending["until"]},{"offer",pending["offer"]}});
}
bool LobbyRoomCoordinator::setDetails(QString title,qint64 startsAt) {
    return hosting()&&session_.setDetails(self_,std::move(title),startsAt);
}
bool LobbyRoomCoordinator::join(const QString& host,const QString& lobby,bool request,bool watch) {
    if(!online_||hosting()||!host_.isEmpty()||!identifier(host)||host==self_||QUuid(lobby).isNull())return false;
    host_=host;joining_=lobby;joiningRequest_=request;joiningWatch_=watch;joinedAt_=hostSeen_=now();reconnect();emit changed();return true;
}
void LobbyRoomCoordinator::answer(bool accept) {
    const auto offer=invitation_;invitation_={};emit changed();
    if(accept&&offer["expires"].toInteger()>now())join(offer["host"].toString(),offer["lobby"].toString());
}
bool LobbyRoomCoordinator::approve(const QString& peer) {
    if(!hosting()||!requests_.contains(peer))return false;
    const auto name=requests_.take(peer);seen_[peer]=now();return session_.approve(self_,peer,name,watchRequests_.remove(peer));
}
void LobbyRoomCoordinator::leave() {
    const auto departing=state()["id"].toString();
    if(!departing.isEmpty())emit membershipEnded(departing);
    if(hosting()) {
        const auto snapshot=session_.state();
        for(const auto& value:snapshot["members"].toArray())if(value.toObject()["id"]!=self_)send(value.toObject()["id"].toString(),"closed");
        for(const auto& peer:session_.invitedPeers())send(peer,"closed");
        for(const auto& peer:requests_.keys())send(peer,"closed");
        session_.leave(self_);
    } else if(!host_.isEmpty())send(host_,"leave");
    clearRemote();requests_.clear();seen_.clear();pendingInvites_.clear();emit changed();
}
bool LobbyRoomCoordinator::selectGame(const LobbySession::Game& game) {return hosting()&&session_.selectGame(self_,game);}
bool LobbyRoomCoordinator::setAccess(QString access) {return hosting()&&session_.setAccess(self_,access);}
bool LobbyRoomCoordinator::setRelay(QString relay) {return hosting()&&session_.setRelay(self_,relay);}
bool LobbyRoomCoordinator::setConversation(QString channel) {return hosting()&&session_.setConversation(self_,channel);}
bool LobbyRoomCoordinator::seat(int seat) {
    if(hosting())return session_.claimSeat(self_,seat,session_.generation());
    if(!online_||remote_.isEmpty()||restoring_||!connection_.isEmpty())return false;
    readyDeadline_.stop();preparationAwaitingAck_=false;
    preparedGeneration_=readyGeneration_=0;send(host_,"seat",{{"seat",seat}});return true;
}
bool LobbyRoomCoordinator::spectate(bool value) {
    if(hosting())return session_.spectate(self_,value,session_.generation());
    if(!online_||remote_.isEmpty()||restoring_||!connection_.isEmpty())return false;
    readyDeadline_.stop();preparationAwaitingAck_=false;
    preparedGeneration_=readyGeneration_=0;send(host_,"spectate",{{"value",value}});return true;
}
bool LobbyRoomCoordinator::requestPlay() {
    if(hosting()||!online_||restoring_||!connection_.isEmpty())return false;
    for(const auto& v:remote_.value("members").toArray())if(v.toObject()["id"]==self_&&v.toObject()["spectator"].toBool()) {
        send(host_,"play-request");emit notice("Asked the host to play. You can keep watching.");return true;
    }
    return false;
}
bool LobbyRoomCoordinator::answerPlay(const QString& peer,const QString& answer) {
    if(!hosting()||!playRequests_.contains(peer)||!QStringList{"accept","later","decline"}.contains(answer))return false;
    playRequests_.remove(peer);
    send(peer,"play-response",{{"answer",answer}});emit changed();return true;
}
bool LobbyRoomCoordinator::locallyPrepared() const {
    if(!hosting())return preparedGeneration_&&preparedGeneration_==quint64(remote_["generation"].toInteger());
    for(const auto& v:session_.state()["members"].toArray())if(v.toObject()["id"]==self_)return v.toObject()["prepared"].toBool();
    return false;
}
bool LobbyRoomCoordinator::prepared(quint64 generation,bool value) {
    if(value&&!runtimeAllowed_)return false;
    if(hosting())return session_.prepared(self_,generation,value);
    if(!online_||remote_.isEmpty()||restoring_||!connection_.isEmpty()||remote_.value("generation").toInteger()!=qint64(generation))return false;
    readyDeadline_.stop();
    preparedGeneration_=value?generation:0;preparationAwaitingAck_=true;
    preparationRequestGeneration_=generation;preparationTarget_=value;preparationDeadline_.setRemainingTime(45000);
    readyGeneration_=0;send(host_,"prepared",{{"value",value}});return true;
}
bool LobbyRoomCoordinator::ready(bool value) {
    if(value&&!runtimeAllowed_)return false;
    if(hosting())return session_.ready(self_,session_.generation(),value);
    const auto generation=quint64(remote_.value("generation").toInteger());
    if(!online_||remote_.isEmpty()||restoring_||!connection_.isEmpty()||!preparedGeneration_||preparedGeneration_!=generation)return false;
    if(readyPending()&&readyTarget_==value)return false;
    readyTarget_=value;readyRequestGeneration_=generation;readyDeadline_.start();
    readyGeneration_=value?generation:0;emit changed();
    send(host_,"ready",{{"value",value}});return true;
}
bool LobbyRoomCoordinator::recordLaunchResult(const QString& peer,const QString& ticket,bool success) {
    const auto s=session_.state();if(s["phase"]!="launching"||ticket.isEmpty()||s["ticket"]!=ticket)return false;
    bool player=false;for(const auto& m:s["members"].toArray())if(m.toObject()["id"]==peer&&m.toObject()["seat"].toInt()>0&&!m.toObject()["spectator"].toBool())player=true;
    if(!player)return false;
    if(!success)return session_.acknowledgeLaunch(ticket,false);
    if(resultTicket_!=ticket) {resultTicket_=ticket;launchResults_.clear();}
    launchResults_.insert(peer);
    for(const auto& m:s["members"].toArray())if(!m.toObject()["spectator"].toBool()&&!launchResults_.contains(m.toObject()["id"].toString()))return true;
    return session_.acknowledgeLaunch(ticket,true);
}
bool LobbyRoomCoordinator::acknowledgeLaunch(const QString& ticket,bool success) {
    if(hosting())return recordLaunchResult(self_,ticket,success);
    if(!liveOperation_.isEmpty()&&remote_.value("phase")=="playing"&&remote_.value("ticket")==ticket) {
        if(!success)send(host_,"live-ended",{{"ticket",ticket},{"operation",liveOperation_}});
        return true;
    }
    if(ticket.isEmpty()||ticket!=launchTicket_||remote_.value("ticket")!=ticket||remote_.value("phase")!="launching")return false;
    // Keep the local result until the host acknowledges it through room state.
    // A late success cannot undo an already reported cancellation/failure.
    if(pendingLaunchResult_.value("ticket")==ticket) {
        const bool previous=pendingLaunchResult_.value("success").toBool();
        if(previous==success)return true;
        if(!previous&&success)return false;
    }
    pendingLaunchResult_={{"ticket",ticket},{"success",success}};
    launchResultGeneration_=remote_["generation"].toInteger();launchResultDeadline_.setRemainingTime(180000);
    retryLaunchResult();return true;
}
void LobbyRoomCoordinator::retryLaunchResult() {
    if(pendingLaunchResult_.isEmpty())return;
    if(launchResultDeadline_.hasExpired()||remote_["phase"]!="launching"||
       remote_["ticket"]!=pendingLaunchResult_["ticket"]||remote_["generation"].toInteger()!=launchResultGeneration_) {
        pendingLaunchResult_={};return;
    }
    if(online_&&!restoring_&&connection_.isEmpty())send(host_,"launch-result",pendingLaunchResult_);
}
bool LobbyRoomCoordinator::finishGame(const QString& ticket) {
    if(hosting())return session_.finishGame(ticket);
    if(ticket.isEmpty()||ticket!=launchTicket_||remote_.value("ticket")!=ticket||remote_.value("phase")!="playing")return false;
    // Preserve the actual child completion through a provider reconnect. The
    // ticket prevents this retry from ending a later game in the same room.
    finishedTicket_=ticket;
    if(!liveOperation_.isEmpty()) {
        send(host_,"live-ended",{{"ticket",ticket},{"operation",liveOperation_}});return true;
    }
    send(host_,"game-finished",{{"ticket",ticket}});return true;
}
bool LobbyRoomCoordinator::setLiveJoining(bool enabled) {return hosting()&&session_.setLiveJoining(self_,enabled);}
bool LobbyRoomCoordinator::canJoinLive() const {
    if(hosting()||!runtimeAllowed_||!online_||restoring_||!connection_.isEmpty()||remote_["phase"]!="playing")return false;
    int reserved=0;QJsonObject mine;
    for(const auto& v:remote_["members"].toArray()) {
        const auto m=v.toObject();if(m["seat"].toInt()||m["returnSeat"].toInt())++reserved;
        if(m["id"]==self_)mine=m;
    }
    if(!mine["connected"].toBool()||!mine["spectator"].toBool())return false;
    if(mine["returnSeat"].toInt()>0)return remote_["rejoin"].toBool()&&mine["returnUntil"].toInteger()>now();
    return remote_["liveJoining"].toBool()&&reserved<remote_["game"].toObject()["seats"].toInt();
}
bool LobbyRoomCoordinator::requestLive(QString operation) {
    if(!canJoinLive()||QUuid(operation).isNull())return false;
    liveOperation_=operation;launchTicket_.clear();finishedTicket_.clear();
    send(host_,"live-request",{{"ticket",remote_.value("ticket")},{"operation",operation}});return true;
}
bool LobbyRoomCoordinator::releaseRemotePlayer(QString peer,QString ticket,QString admission) {
    if(!hosting()||peer==session_.state()["owner"].toString()||session_.state()["game"].toObject()["mode"]!="remoteplay.v1")return false;
    // An ended connection may release only its own admission, never a later P2.
    return admission.isEmpty()?session_.finishPlayer(peer,ticket):session_.releaseLive(peer,ticket,admission);
}
void LobbyRoomCoordinator::cancelLive(QString operation) {
    if(hosting()||operation.isEmpty()||liveOperation_!=operation)return;
    finishedTicket_=remote_.value("ticket").toString();
    send(host_,"live-ended",{{"ticket",finishedTicket_},{"operation",operation}});
}
bool LobbyRoomCoordinator::admitLive(QString peer,QString operation,int seat) {
    return hosting()&&session_.admitLive(peer,operation,seat);
}
void LobbyRoomCoordinator::rejectLive(QString peer,QString operation) {
    send(peer,"live-rejected",{{"operation",operation}});
}
bool LobbyRoomCoordinator::proposeVote(const QJsonArray& choices){return hosting()&&session_.proposeVote(self_,choices);}
bool LobbyRoomCoordinator::vote(QString id,int choice){if(hosting())return session_.vote(self_,id,choice);if(!online_||restoring_||remote_.value("vote").toObject()["id"]!=id)return false;send(host_,"vote",{{"id",id},{"choice",choice}});return true;}
bool LobbyRoomCoordinator::confirmVote(QString id,int choice){return hosting()&&session_.confirmVote(self_,id,choice);}
bool LobbyRoomCoordinator::validState(const QJsonObject& s,const QString& host) {
    if(!netplayRelay::valid(s["relay"].toString("auto")))return false;
    if(s["title"].toString().size()>80||s["startsAt"].toInteger()<0||s["startsAt"].toInteger()>4102444800LL)return false;
    if(!identifier(host)||!QStringList{"private","public","approval"}.contains(s["access"].toString()))return false;
    if(!s["conversation"].toString().isEmpty()&&!identifier(s["conversation"].toString()))return false;
    if(s["owner"]!=host||s["revision"].toInteger()<1||s["generation"].toInteger()<1)return false;
    if(s["revision"].toInteger()>9007199254740990LL||s["generation"].toInteger()>9007199254740990LL)return false;
    if(!QStringList{"choosing","gathering","launching","playing"}.contains(s["phase"].toString()))return false;
    const auto members=s["members"].toArray();if(members.isEmpty()||members.size()>lobbyLimits::MaxMembers)return false;
    QSet<QString> identities;QSet<int> seats;const auto capacity=s["game"].toObject()["seats"].toInt(-1);
    if(capacity<0||capacity>16)return false;
    const auto game=s["game"].toObject();
    if(s["relay"]=="psp-host"&&!netplayRelay::compatible(game["runtime"].toObject(),"psp-host"))return false;
    if(game["id"].toString().size()>128||game["title"].toString().size()>160||game["platform"].toString().size()>32||game["mode"].toString().size()>64)return false;
    if(!game["id"].toString().isEmpty()&&(capacity<1||game["title"].toString().trimmed().isEmpty()||game["mode"].toString().isEmpty()||game["platform"].toString().isEmpty()))return false;
    for(const auto& value:members) {
        const auto m=value.toObject();const auto id=m["id"].toString();const auto seat=m["seat"].toInt(-1);
        if(!identifier(id)||identities.contains(id)||m["name"].toString().trimmed().isEmpty()||m["name"].toString().size()>100||seat<0||seat>capacity||(seat&&seats.contains(seat)))return false;
        identities.insert(id);if(seat)seats.insert(seat);
        const auto returning=m["returnSeat"].toInt();
        if(returning<0||returning>capacity||(returning&&(!s["rejoin"].toBool()||s["phase"]!="playing"||seat||!m["spectator"].toBool()||id==host||returning==1||seats.contains(returning)||m["returnUntil"].toInteger()<=0)))return false;
        if(returning)seats.insert(returning);
    }
    const auto ballot=s["vote"].toObject();
    if(!ballot.isEmpty()) {
        if(QUuid(ballot["id"].toString()).isNull())return false;
        const auto choices=ballot["choices"].toArray();if(choices.size()<2||choices.size()>4)return false;
        QSet<QString> options;
        for(const auto& v:choices){const auto g=v.toObject();const auto id=g["id"].toString(),mode=g["mode"].toString();
            if(id.isEmpty()||id.size()>128||mode.isEmpty()||mode.size()>64||g["title"].toString().trimmed().isEmpty()||g["title"].toString().size()>160||g["platform"].toString().isEmpty()||g["platform"].toString().size()>32||g["seats"].toInt()<1||g["seats"].toInt()>16||options.contains(id+"/"+mode))return false;
            options.insert(id+"/"+mode);
        }
        const auto votes=ballot["votes"].toObject();for(auto it=votes.begin();it!=votes.end();++it)if(!identities.contains(it.key())||!it.value().isDouble()||it.value().toInt(-1)<0||it.value().toInt()>=choices.size())return false;
    }
    return identities.contains(host);
}
void LobbyRoomCoordinator::receive(QString peer,QString name,const QJsonObject& p) {
    if(!online_||!identifier(peer)||peer==self_||p["ns"]!="org.traineros.lobby"||p["v"]!=1
       ||p["expires"].toInteger()<=now()||p["expires"].toInteger()>now()+90)return;
    const auto id=p["lobby"].toString(),kind=p["kind"].toString();const auto data=p["data"].toObject();
    const auto boot=p["boot"].toString();const auto serial=quint64(p["serial"].toInteger());
    if(QUuid(id).isNull()||QUuid(boot).isNull()||!serial||serial>9007199254740991ULL)return;
    if(retiredBoots_.value(peer+"/"+boot)>now())return;
    if(peerBoots_.value(peer)==boot&&serial<=peerSerials_.value(peer))return;
    if(kind=="invite") {
        const auto offer=data["offer"].toString();const auto key=peer+"/"+id+"/"+offer;
        const auto answerUntil=data["answerUntil"].toInteger(p["expires"].toInteger());
        if(answerUntil<=now()||answerUntil>now()+300||(!offer.isEmpty()&&QUuid(offer).isNull()))return;
        if(receivedInvites_.contains(key)) {send(peer,"invite-received",{{"offer",offer}},{},id);return;}
        if(!hosting()&&host_.isEmpty()&&invitation_.isEmpty()) {
            peerBoots_[peer]=boot;peerSerials_[peer]=serial;
            if(receivedInvites_.size()>=128)return;
            receivedInvites_[key]=answerUntil;
            invitation_={{"host",peer},{"boot",boot},{"name",name.left(100)},{"lobby",id},{"expires",answerUntil},{"game",data["game"]},
                {"title",data["title"].toString().left(80)},{"startsAt",qBound(0LL,data["startsAt"].toInteger(),4102444800LL)}};emit changed();
            send(peer,"invite-received",{{"offer",offer}},{},id);
        }
        return;
    }
    if(kind=="closed"&&invitation_["host"]==peer&&invitation_["lobby"]==id&&invitation_["boot"]==boot) {
        peerSerials_[peer]=serial;invitation_={};emit changed();return;
    }
    const auto probe=data["probe"].toString();
    const bool probing=(kind=="join"||kind=="ping")&&!QUuid(probe).isNull();
    if(probing&&(!hosting()||id!=session_.id())) {send(peer,"closed",{},probe,id);return;}
    if(hosting()) {
        if(id!=session_.id())return;
        if(kind=="invite-received") {
            if(pendingInvites_.contains(peer)&&pendingInvites_[peer]["offer"]==data["offer"])pendingInvites_.remove(peer);
            return;
        }
        if(kind=="join") {
            if(data["restore"].toBool()) {
                bool known=false;for(const auto& v:session_.state()["members"].toArray())if(v.toObject()["id"]==peer)known=true;
                if(!known) {send(peer,"removed",{},probe);return;}
            }
            const auto previous=peerBoots_.value(peer);
            const auto previousSerial=peerSerials_.value(peer);
            // Bind before join emits state, so a synchronous prepared reply is
            // authenticated against this incarnation as well.
            peerBoots_[peer]=boot;peerSerials_[peer]=serial;
            if(session_.join(peer,name.left(100),now(),data["watch"].toBool())) {
                if(!previous.isEmpty()&&previous!=boot) {
                    retiredBoots_[peer+"/"+previous]=now()+180;
                    // A new shell cannot still own the previous game's runtime.
                    const auto s=session_.state();QString admission;
                    for(const auto& m:s["members"].toArray())if(m.toObject()["id"]==peer)admission=m.toObject()["admission"].toString();
                    const auto ticket=s["ticket"].toString();
                    const bool released=admission.isEmpty()?session_.finishPlayer(peer,ticket):session_.releaseLive(peer,ticket,admission);
                    if(released)emit liveReleased(peer,admission.isEmpty()?ticket:admission);
                    session_.connected(peer,false);session_.connected(peer,true);
                }
                pendingInvites_.remove(peer);seen_[peer]=now();save();send(peer,"state",session_.state(),probe);
            }
            else if((session_.state()["access"]=="approval"||data["request"].toBool())&&requests_.size()<32
                    &&(session_.state()["phase"]=="choosing"||session_.state()["phase"]=="gathering"||session_.state()["phase"]=="playing")) {
                if(!previous.isEmpty()&&previous!=boot)retiredBoots_[peer+"/"+previous]=now()+180;
                peerBoots_[peer]=boot;peerSerials_[peer]=serial;requests_[peer]=name.left(100);if(data["watch"].toBool())watchRequests_.insert(peer);else watchRequests_.remove(peer);seen_[peer]=now();emit changed();
            }
            else {peerBoots_[peer]=previous;peerSerials_[peer]=previousSerial;send(peer,"rejected",{},probe);}
            return;
        }
        bool member=false;for(const auto& v:session_.state()["members"].toArray())if(v.toObject()["id"]==peer)member=true;
        if(!member) {if(probing)send(peer,"removed",{},probe);return;}
        if(peerBoots_.value(peer)!=boot)return;
        peerSerials_[peer]=serial;
        seen_[peer]=now();
        if(kind=="runtime-signal") {receiveRuntimeSignal(peer,p);return;}
        if(kind=="leave") {playRequests_.remove(peer);session_.leave(peer);seen_.remove(peer);return;}
        if(kind=="play-request"&&p["generation"]==session_.state()["generation"]) {
            for(const auto& v:session_.state()["members"].toArray())if(v.toObject()["id"]==peer&&v.toObject()["spectator"].toBool()) {
                playRequests_[peer]=name.left(100);emit changed();
            }
            return;
        }
        if(kind=="vote"&&p["generation"]==session_.state()["generation"]){session_.vote(peer,data["id"].toString(),data["choice"].toInt(-1));return;}
        if(kind=="ping") {session_.connected(peer,true);send(peer,"state",session_.state(),probe);return;}
        if(kind=="live-request"&&p["generation"]==session_.state()["generation"]&&data["ticket"]==session_.state()["ticket"]&&!QUuid(data["operation"].toString()).isNull()) {
            session_.expireRejoins(now());
            const auto operation=data["operation"].toString();const int seat=session_.liveSeat(peer);
            for(const auto& m:session_.state()["members"].toArray())if(m.toObject()["id"]==peer&&m.toObject()["admission"]==operation) {
                send(peer,"state",session_.state());return;
            }
            if(seat)emit liveJoinRequested(peer,operation,seat);
            else rejectLive(peer,operation);
            return;
        }
        if(kind=="live-ended") {
            if(session_.releaseLive(peer,data["ticket"].toString(),data["operation"].toString()))emit liveReleased(peer,data["operation"].toString());
            send(peer,"state",session_.state());
            return;
        }
        if(kind=="launch-result"&&data["success"].isBool()) {recordLaunchResult(peer,data["ticket"].toString(),data["success"].toBool());return;}
        if(kind=="game-finished") {
            // A seated player may end their part of the current game, not a
            // spectator or a delayed report from an earlier launch.
            for(const auto& m:session_.state()["members"].toArray())if(m.toObject()["id"]==peer&&m.toObject()["seat"].toInt()>0&&m.toObject()["admission"].toString().isEmpty())
                if(session_.finishPlayer(peer,data["ticket"].toString()))emit liveReleased(peer,data["ticket"].toString());
            return;
        }
        const auto generation=quint64(p["generation"].toInteger());
        if(kind=="seat") {
            if(!session_.claimSeat(peer,data["seat"].toInt(-1),generation))send(peer,"problem",{{"message","That controller is already taken or unavailable."}});
        }
        else if(kind=="spectate"&&data["value"].isBool())session_.spectate(peer,data["value"].toBool(),generation);
        else if(kind=="prepared"&&data["value"].isBool())session_.prepared(peer,generation,data["value"].toBool());
        else if(kind=="ready"&&data["value"].isBool())session_.ready(peer,generation,data["value"].toBool());
        return;
    }
    if(peer!=host_||id!=(joining_.isEmpty()?remote_.value("id").toString():joining_))return;
    const bool confirmed=!probe_.isEmpty()&&p["reply"]==probe_;
    const bool newBoot=peerBoots_.contains(peer)&&peerBoots_[peer]!=boot;
    if((newBoot||restoring_)&&!confirmed)return;
    if(kind=="runtime-signal") {
        if(peerBoots_.value(peer)!=boot)return;
        peerSerials_[peer]=serial;receiveRuntimeSignal(peer,p);return;
    }
    if(kind=="live-rejected"&&data["operation"]==liveOperation_) {
        peerSerials_[peer]=serial;const auto operation=liveOperation_;liveOperation_.clear();emit liveJoinFailed(operation);return;
    }
    if(kind=="play-response"&&p["generation"]==remote_.value("generation")) {
        peerSerials_[peer]=serial;const auto answer=data["answer"].toString();
        if(answer=="accept")emit notice("The host agreed. Choose your controller after this game; the current game continues.");
        else if(answer=="later")emit notice("The host asked to play later. You can keep watching.");
        else if(answer=="decline")emit notice("The host declined. You can keep watching.");
        return;
    }
    if(kind=="problem") {peerSerials_[peer]=serial;emit notice(data["message"].toString().left(160));return;}
    if(kind=="closed"||kind=="rejected"||kind=="removed") {emit membershipEnded(id);clearRemote();emit notice(kind=="closed"?"The lobby was closed.":kind=="removed"?"You are no longer in this lobby.":"The lobby couldn't accept your request.");emit changed();return;}
    if(kind!="state"||data["id"]!=id||!validState(data,peer))return;
    if(newBoot) {retiredBoots_[peer+"/"+peerBoots_[peer]]=now()+180;preparedGeneration_=readyGeneration_=0;launchTicket_.clear();finishedTicket_.clear();pendingLaunchResult_={};}
    peerBoots_[peer]=boot;peerSerials_[peer]=serial;
    hostSeen_=now();
    if(!newBoot&&!restoring_&&data["revision"].toInteger()<remote_.value("revision").toInteger())return;
    const bool recovered=newBoot||restoring_||!connection_.isEmpty();restoring_=false;connection_.clear();if(confirmed)probe_.clear();
    if(!recovered&&data["revision"]==remote_.value("revision"))return;
    QJsonObject mine;for(const auto& v:data["members"].toArray())if(v.toObject()["id"]==self_)mine=v.toObject();
    if(mine.isEmpty()) {emit membershipEnded(id);clearRemote();emit notice("You left the lobby.");emit changed();return;}
    if(data["generation"]!=remote_.value("generation")) {preparedGeneration_=readyGeneration_=0;preparationAwaitingAck_=false;}
    remote_=data;joining_.clear();joinedAt_=0;
    if(data["ticket"]!=finishedTicket_||data["phase"]!="playing")finishedTicket_.clear();
    if(data["phase"]!="launching"||data["ticket"]!=pendingLaunchResult_["ticket"]||
       data["generation"].toInteger()!=launchResultGeneration_)pendingLaunchResult_={};
    const auto generation=quint64(data["generation"].toInteger());const auto ticket=data["ticket"].toString();
    if(data["phase"]=="playing"&&!liveOperation_.isEmpty()&&mine["admission"]==liveOperation_&&launchTicket_!=ticket) {
        launchTicket_=ticket;emit liveLaunchRequested(liveOperation_,ticket,data);
    }
    if(!finishedTicket_.isEmpty()&&mine["spectator"].toBool()) {
        finishedTicket_.clear();liveOperation_.clear();launchTicket_.clear();
    }
    if(data["phase"]=="launching"&&mine["seat"].toInt()>0&&preparedGeneration_==generation&&readyGeneration_==generation
       &&!ticket.isEmpty()&&ticket!=launchTicket_) {launchTicket_=ticket;emit launchRequested(ticket,data);}
    if(readyPending()&&(newBoot||generation!=readyRequestGeneration_||!mine["prepared"].toBool()
       ||mine["ready"].toBool()==readyTarget_))readyDeadline_.stop();
    // A host snapshot already in transit may precede our preparation request.
    // Keep the local proof until its acknowledgement, without allowing a remote
    // prepared flag to create local proof after reconnect/generation changes.
    if(preparationAwaitingAck_&&(generation!=preparationRequestGeneration_||data["phase"]!="gathering"||mine["prepared"].toBool()==preparationTarget_))preparationAwaitingAck_=false;
    if(!mine["prepared"].toBool()&&!preparationAwaitingAck_)preparedGeneration_=readyGeneration_=0;
    if(!mine["ready"].toBool()&&!readyPending())readyGeneration_=0;
    emit changed();
    // Reconnection must reconcile the host's old Ready with actual local proof.
    // Never reconstruct local preparation from a remote snapshot.
    if(remote_["phase"]=="gathering"&&mine["prepared"].toBool()&&!preparedGeneration_&&!preparationAwaitingAck_)
        prepared(quint64(remote_["generation"].toInteger()),false);
}
void LobbyRoomCoordinator::deliveryFailed(const QString& peer) {
    if(hosting()) {if(!session_.connected(peer,false))emit notice("Couldn't deliver the lobby invitation. Try again.");}
    else if(peer==host_) {readyDeadline_.stop();preparationAwaitingAck_=false;preparedGeneration_=readyGeneration_=0;connection_="Host unavailable. Reconnecting…";emit changed();}
}
void LobbyRoomCoordinator::tick() {
    for(auto it=retiredBoots_.begin();it!=retiredBoots_.end();)if(it.value()<=now())it=retiredBoots_.erase(it);else ++it;
    if(!invitation_.isEmpty()&&invitation_["expires"].toInteger()<=now()) {invitation_={};emit changed();}
    if(!online_)return;
    if(hosting()) {
        session_.expireRejoins(now());
        for(const auto& peer:pendingInvites_.keys()) {
            if(pendingInvites_.value(peer)["until"].toInteger()<=now())pendingInvites_.remove(peer);
            else sendInvitation(peer);
        }
        for(auto it=requests_.begin();it!=requests_.end();) {
            if(now()-seen_.value(it.key())>90) {it=requests_.erase(it);emit changed();}else ++it;
        }
        for(auto it=seen_.cbegin();it!=seen_.cend();++it)if(now()-it.value()>45)session_.connected(it.key(),false);
    } else if(!host_.isEmpty()) {
        if(joinedAt_&&now()-joinedAt_>90) {clearRemote();emit notice("The join request expired.");emit changed();return;}
        if(hostSeen_&&now()-hostSeen_>45)deliveryFailed(host_);
        reconnect();
        if(online_&&!restoring_&&connection_.isEmpty()&&remote_["phase"]=="gathering") {
            if(preparationAwaitingAck_&&preparationRequestGeneration_==quint64(remote_["generation"].toInteger())) {
                if(preparationDeadline_.hasExpired()) {
                    preparationAwaitingAck_=false;preparedGeneration_=readyGeneration_=0;
                    emit notice("Preparation confirmation timed out. Try setup again.");emit changed();
                } else send(host_,"prepared",{{"value",preparationTarget_}});
            }
            if(readyPending()&&readyRequestGeneration_==quint64(remote_["generation"].toInteger()))send(host_,"ready",{{"value",readyTarget_}});
        }
        retryLaunchResult();
        if(!liveOperation_.isEmpty()&&launchTicket_.isEmpty()&&finishedTicket_.isEmpty()&&remote_.value("phase")=="playing")
            send(host_,"live-request",{{"ticket",remote_.value("ticket")},{"operation",liveOperation_}});
        if(!finishedTicket_.isEmpty()) {
            if(!liveOperation_.isEmpty())send(host_,"live-ended",{{"ticket",finishedTicket_},{"operation",liveOperation_}});
            else send(host_,"game-finished",{{"ticket",finishedTicket_}});
        }
    }
    for(auto it=receivedInvites_.begin();it!=receivedInvites_.end();)if(it.value()<=now())it=receivedInvites_.erase(it);else ++it;
}
void LobbyRoomCoordinator::allowRuntime(bool enabled) {
    if(runtimeAllowed_==enabled)return;
    if(!enabled){ready(false);prepared(quint64(state()["generation"].toInteger()),false);}
    runtimeAllowed_=enabled;
}
void LobbyRoomCoordinator::forgetMembership() {
    const auto id=state()["id"].toString();
    if(!id.isEmpty())emit providerMembershipEnded(id);
    loading_=true;session_.leave(self_);clearRemote();pendingInvites_.clear();requests_.clear();playRequests_.clear();seen_.clear();providerMembers_.clear();loading_=false;
    if(!id.isEmpty())emit membershipEnded(id);emit changed();
}
void LobbyRoomCoordinator::reconcileProvider(QString channel,QString owner,QJsonArray people) {
    auto s=state();if(channel.isEmpty()||s["conversation"]!=channel||!identifier(owner))return;
    QSet<QString> ids;for(const auto& v:people)ids.insert(v.toObject()["id"].toString());
    if(!ids.contains(self_)){forgetMembership();return;}
    if(!ids.contains(owner))return;
    const auto previous=providerMembers_;providerMembers_=ids;
    if(s["owner"]==owner) {
        // Provider removal is authoritative; game-seat membership never restores
        // a person who left the underlying group.
        if(hosting())for(const auto& v:s["members"].toArray()){
            const auto id=v.toObject()["id"].toString();if(previous.contains(id)&&!ids.contains(id))session_.remove(self_,id);
        }
        if(previous!=ids)save();return;
    }
    readyDeadline_.stop();
    QJsonArray members;
    for(const auto& v:s["members"].toArray())if(ids.contains(v.toObject()["id"].toString())) {
        auto m=v.toObject();m["ready"]=false;m["prepared"]=false;m["connected"]=false;members.append(m);
    }
    bool ownerKnown=false;for(const auto& v:members)if(v.toObject()["id"]==owner)ownerKnown=true;
    if(!ownerKnown)return;
    s["members"]=members;s["owner"]=owner;s["ticket"]=QString();
    s["phase"]=s["game"].toObject()["id"].toString().isEmpty()?"choosing":"gathering";
    loading_=true;session_.leave(self_);clearRemote();peerBoots_.clear();peerSerials_.clear();
    if(owner==self_) {
        session_.restore(s);session_.connected(self_,online_);
        for(const auto& v:members)seen_[v.toObject()["id"].toString()]=now();
    } else {
        s["generation"]=s["generation"].toInteger()+1;s["revision"]=s["revision"].toInteger()+1;
        remote_=s;host_=owner;restoring_=true;connection_="Updating room organizer...";hostSeen_=now();
    }
    loading_=false;save();emit changed();reconnect();
}

}
