#include "LobbyCoordinator.h"
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
LobbyCoordinator::LobbyCoordinator(QObject* parent):QObject(parent) {
    boot_=QUuid::createUuid().toString(QUuid::WithoutBraces);
    heartbeat_.setInterval(15000);connect(&heartbeat_,&QTimer::timeout,this,&LobbyCoordinator::tick);heartbeat_.start();
    connect(this,&LobbyCoordinator::changed,this,&LobbyCoordinator::save);
    connect(&session_,&LobbySession::changed,this,[this]{
        if(playRequestGeneration_!=session_.generation()||session_.state()["phase"]=="closed") {
            playRequests_.clear();playRequestGeneration_=session_.generation();
        }
        QSet<QString> members;for(const auto& m:session_.state()["members"].toArray())members.insert(m.toObject()["id"].toString());
        for(auto it=playRequests_.begin();it!=playRequests_.end();)if(!members.contains(it.key()))it=playRequests_.erase(it);else ++it;
        broadcast();emit changed();
    });
    connect(&session_,&LobbySession::launchRequested,this,&LobbyCoordinator::launchRequested);
}
bool LobbyCoordinator::hosting() const {return session_.state()["phase"]!="closed"&&session_.state()["owner"]==self_;}
bool LobbyCoordinator::runtimePeer(const QString& peer,const QString& ticket) const {
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
bool LobbyCoordinator::sendRuntimeSignal(const QString& peer,const QString& ticket,const QString& connection,const QJsonObject& payload) {
    if(!runtimePeer(peer,ticket)||QUuid(connection).isNull()||payload.isEmpty()
       ||QJsonDocument(payload).toJson(QJsonDocument::Compact).size()>20000)return false;
    send(peer,"runtime-signal",{{"ticket",ticket},{"connection",connection},{"payload",payload}});return true;
}
void LobbyCoordinator::receiveRuntimeSignal(const QString& peer,const QJsonObject& packet) {
    const auto data=packet["data"].toObject();const auto payload=data["payload"].toObject();
    const auto ticket=data["ticket"].toString(),connection=data["connection"].toString();
    if(data.size()!=3||packet["generation"]!=state()["generation"]||!runtimePeer(peer,ticket)
       ||QUuid(connection).isNull()||payload.isEmpty()||QJsonDocument(payload).toJson(QJsonDocument::Compact).size()>20000)return;
    emit runtimeSignal(peer,ticket,connection,payload);
}
QJsonObject LobbyCoordinator::state() const {
    if(hosting()) {auto s=session_.state();s["connection"]=online_?storageError_:QString("Reconnecting to communication…");return s;}
    if(remote_.isEmpty()&&!host_.isEmpty())return {{"id",joining_},{"owner",host_},{"phase","joining"}};
    if(remote_.isEmpty())return {};
    auto s=remote_;s["recovering"]=restoring_;s["connection"]=storageError_.isEmpty()?connection_:storageError_;return s;
}
QJsonArray LobbyCoordinator::requests() const {
    QJsonArray list;for(auto it=requests_.cbegin();it!=requests_.cend();++it)list.append(QJsonObject{{"id",it.key()},{"name",it.value()}});
    for(auto it=playRequests_.cbegin();it!=playRequests_.cend();++it)list.append(QJsonObject{{"id",it.key()},{"name",it.value()},{"play",true}});
    return list;
}
void LobbyCoordinator::clearRemote() {
    liveOperation_.clear();
    peerBoots_.remove(host_);peerSerials_.remove(host_);
    remote_={};host_.clear();joining_.clear();launchTicket_.clear();finishedTicket_.clear();preparedGeneration_=readyGeneration_=0;joinedAt_=hostSeen_=0;
    restoring_=false;probe_.clear();connection_.clear();
}
void LobbyCoordinator::configure(QString self,QString name,bool online) {
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
    if(!online_) {preparedGeneration_=readyGeneration_=0;if(!host_.isEmpty())connection_="Reconnecting to communication…";}
    emit this->changed();
    if(changed&&online_)tick();
}
bool LobbyCoordinator::create(QString access) {
    if(!online_||hosting()||!host_.isEmpty())return false;
    clearRemote();return session_.create(self_,name_,access);
}
void LobbyCoordinator::send(const QString& peer,QString kind,QJsonObject data,QString reply,QString targetLobby) {
    if(!online_||!identifier(peer)||peer==self_)return;
    const auto lobby=!targetLobby.isEmpty()?targetLobby:hosting()?session_.id():!joining_.isEmpty()?joining_:remote_["id"].toString();
    emit outgoing(peer,{{"ns","org.traineros.lobby"},{"v",1},{"kind",kind},{"lobby",lobby},
        {"boot",boot_},{"serial",qint64(++serial_)},
        {"expires",now()+90},{"generation",state()["generation"]},{"reply",reply},{"data",data}});
}
void LobbyCoordinator::broadcast() {
    if(!hosting()||!online_||loading_)return;
    const auto snapshot=session_.state();
    for(const auto& v:snapshot["members"].toArray()) {
        const auto peer=v.toObject()["id"].toString();if(peer!=self_)send(peer,"state",snapshot);
    }
}
bool LobbyCoordinator::invite(const QString& peer) {
    if(!hosting()||!identifier(peer)||!session_.invite(self_,peer,now()+300))return false;
    pendingInvites_[peer]={{"until",now()+300},{"offer",QUuid::createUuid().toString(QUuid::WithoutBraces)}};
    sendInvitation(peer);return true;
}
void LobbyCoordinator::sendInvitation(const QString& peer) {
    const auto pending=pendingInvites_.value(peer);if(pending.isEmpty()||pending["until"].toInteger()<=now())return;
    auto game=session_.state()["game"].toObject();game.remove("runtime");
    send(peer,"invite",{{"name",name_},{"game",game},{"answerUntil",pending["until"]},{"offer",pending["offer"]}});
}
bool LobbyCoordinator::join(const QString& host,const QString& lobby,bool request) {
    if(!online_||hosting()||!host_.isEmpty()||!identifier(host)||host==self_||QUuid(lobby).isNull())return false;
    host_=host;joining_=lobby;joiningRequest_=request;joinedAt_=hostSeen_=now();reconnect();emit changed();return true;
}
void LobbyCoordinator::answer(bool accept) {
    const auto offer=invitation_;invitation_={};emit changed();
    if(accept&&offer["expires"].toInteger()>now())join(offer["host"].toString(),offer["lobby"].toString());
}
bool LobbyCoordinator::approve(const QString& peer) {
    if(!hosting()||!requests_.contains(peer))return false;
    const auto name=requests_.take(peer);seen_[peer]=now();return session_.approve(self_,peer,name);
}
void LobbyCoordinator::leave() {
    if(hosting()) {
        const auto snapshot=session_.state();
        for(const auto& value:snapshot["members"].toArray())if(value.toObject()["id"]!=self_)send(value.toObject()["id"].toString(),"closed");
        for(const auto& peer:session_.invitedPeers())send(peer,"closed");
        for(const auto& peer:requests_.keys())send(peer,"closed");
        session_.leave(self_);
    } else if(!host_.isEmpty())send(host_,"leave");
    clearRemote();requests_.clear();seen_.clear();pendingInvites_.clear();emit changed();
}
bool LobbyCoordinator::selectGame(const LobbySession::Game& game) {return hosting()&&session_.selectGame(self_,game);}
bool LobbyCoordinator::setAccess(QString access) {return hosting()&&session_.setAccess(self_,access);}
bool LobbyCoordinator::setConversation(QString channel) {return hosting()&&session_.setConversation(self_,channel);}
bool LobbyCoordinator::seat(int seat) {
    if(hosting())return session_.claimSeat(self_,seat,session_.generation());
    if(!online_||remote_.isEmpty()||restoring_||!connection_.isEmpty())return false;
    preparedGeneration_=readyGeneration_=0;send(host_,"seat",{{"seat",seat}});return true;
}
bool LobbyCoordinator::spectate(bool value) {
    if(hosting())return session_.spectate(self_,value,session_.generation());
    if(!online_||remote_.isEmpty()||restoring_||!connection_.isEmpty())return false;
    preparedGeneration_=readyGeneration_=0;send(host_,"spectate",{{"value",value}});return true;
}
bool LobbyCoordinator::requestPlay() {
    if(hosting()||!online_||restoring_||!connection_.isEmpty())return false;
    for(const auto& v:remote_["members"].toArray())if(v.toObject()["id"]==self_&&v.toObject()["spectator"].toBool()) {
        send(host_,"play-request");emit notice("Asked the host to play. You can keep watching.");return true;
    }
    return false;
}
bool LobbyCoordinator::answerPlay(const QString& peer,const QString& answer) {
    if(!hosting()||!playRequests_.contains(peer)||!QStringList{"accept","later","decline"}.contains(answer))return false;
    playRequests_.remove(peer);
    send(peer,"play-response",{{"answer",answer}});emit changed();return true;
}
bool LobbyCoordinator::prepared(quint64 generation,bool value) {
    if(hosting())return session_.prepared(self_,generation,value);
    if(!online_||remote_.isEmpty()||restoring_||!connection_.isEmpty()||remote_["generation"].toInteger()!=qint64(generation))return false;
    preparedGeneration_=value?generation:0;readyGeneration_=0;send(host_,"prepared",{{"value",value}});return true;
}
bool LobbyCoordinator::ready(bool value) {
    if(hosting())return session_.ready(self_,session_.generation(),value);
    const auto generation=quint64(remote_["generation"].toInteger());
    if(!online_||remote_.isEmpty()||restoring_||!connection_.isEmpty()||!preparedGeneration_||preparedGeneration_!=generation)return false;
    readyGeneration_=value?generation:0;send(host_,"ready",{{"value",value}});return true;
}
bool LobbyCoordinator::recordLaunchResult(const QString& peer,const QString& ticket,bool success) {
    const auto s=session_.state();if(s["phase"]!="launching"||ticket.isEmpty()||s["ticket"]!=ticket)return false;
    bool player=false;for(const auto& m:s["members"].toArray())if(m.toObject()["id"]==peer&&m.toObject()["seat"].toInt()>0&&!m.toObject()["spectator"].toBool())player=true;
    if(!player)return false;
    if(!success)return session_.acknowledgeLaunch(ticket,false);
    if(resultTicket_!=ticket) {resultTicket_=ticket;launchResults_.clear();}
    launchResults_.insert(peer);
    for(const auto& m:s["members"].toArray())if(!m.toObject()["spectator"].toBool()&&!launchResults_.contains(m.toObject()["id"].toString()))return true;
    return session_.acknowledgeLaunch(ticket,true);
}
bool LobbyCoordinator::acknowledgeLaunch(const QString& ticket,bool success) {
    if(hosting())return recordLaunchResult(self_,ticket,success);
    if(!liveOperation_.isEmpty()&&remote_["phase"]=="playing"&&remote_["ticket"]==ticket) {
        if(!success)send(host_,"live-ended",{{"ticket",ticket},{"operation",liveOperation_}});
        return true;
    }
    if(!online_||ticket.isEmpty()||ticket!=launchTicket_||remote_["ticket"]!=ticket||remote_["phase"]!="launching")return false;
    send(host_,"launch-result",{{"ticket",ticket},{"success",success}});return true;
}
bool LobbyCoordinator::finishGame(const QString& ticket) {
    if(hosting())return session_.finishGame(ticket);
    if(ticket.isEmpty()||ticket!=launchTicket_||remote_["ticket"]!=ticket||remote_["phase"]!="playing")return false;
    // Preserve the actual child completion through a provider reconnect. The
    // ticket prevents this retry from ending a later game in the same room.
    finishedTicket_=ticket;
    if(!liveOperation_.isEmpty()) {
        send(host_,"live-ended",{{"ticket",ticket},{"operation",liveOperation_}});return true;
    }
    send(host_,"game-finished",{{"ticket",ticket}});return true;
}
bool LobbyCoordinator::setLiveJoining(bool enabled) {return hosting()&&session_.setLiveJoining(self_,enabled);}
bool LobbyCoordinator::requestLive(QString operation) {
    if(hosting()||!online_||restoring_||!connection_.isEmpty()||remote_["phase"]!="playing"||!remote_["liveJoining"].toBool()||QUuid(operation).isNull())return false;
    liveOperation_=operation;launchTicket_.clear();finishedTicket_.clear();
    send(host_,"live-request",{{"ticket",remote_["ticket"]},{"operation",operation}});return true;
}
bool LobbyCoordinator::releaseRemotePlayer(QString peer,QString ticket,QString admission) {
    if(!hosting()||peer==session_.state()["owner"]||session_.state()["game"].toObject()["mode"]!="remoteplay.v1")return false;
    // An ended connection may release only its own admission, never a later P2.
    return admission.isEmpty()?session_.finishPlayer(peer,ticket):session_.releaseLive(peer,ticket,admission);
}
void LobbyCoordinator::cancelLive(QString operation) {
    if(hosting()||operation.isEmpty()||liveOperation_!=operation)return;
    finishedTicket_=remote_["ticket"].toString();
    send(host_,"live-ended",{{"ticket",finishedTicket_},{"operation",operation}});
}
bool LobbyCoordinator::admitLive(QString peer,QString operation,int seat) {
    return hosting()&&session_.admitLive(peer,operation,seat);
}
void LobbyCoordinator::rejectLive(QString peer,QString operation) {
    send(peer,"live-rejected",{{"operation",operation}});
}
bool LobbyCoordinator::validState(const QJsonObject& s,const QString& host) {
    if(!identifier(host)||!QStringList{"private","public","approval"}.contains(s["access"].toString()))return false;
    if(!s["conversation"].toString().isEmpty()&&!identifier(s["conversation"].toString()))return false;
    if(s["owner"]!=host||s["revision"].toInteger()<1||s["generation"].toInteger()<1)return false;
    if(s["revision"].toInteger()>9007199254740990LL||s["generation"].toInteger()>9007199254740990LL)return false;
    if(!QStringList{"choosing","gathering","launching","playing"}.contains(s["phase"].toString()))return false;
    const auto members=s["members"].toArray();if(members.isEmpty()||members.size()>32)return false;
    QSet<QString> identities;QSet<int> seats;const auto capacity=s["game"].toObject()["seats"].toInt(-1);
    if(capacity<0||capacity>16)return false;
    const auto game=s["game"].toObject();
    if(game["id"].toString().size()>128||game["title"].toString().size()>160||game["platform"].toString().size()>32||game["mode"].toString().size()>64)return false;
    if(!game["id"].toString().isEmpty()&&(capacity<1||game["title"].toString().trimmed().isEmpty()||game["mode"].toString().isEmpty()||game["platform"].toString().isEmpty()))return false;
    for(const auto& value:members) {
        const auto m=value.toObject();const auto id=m["id"].toString();const auto seat=m["seat"].toInt(-1);
        if(!identifier(id)||identities.contains(id)||m["name"].toString().trimmed().isEmpty()||m["name"].toString().size()>100||seat<0||seat>capacity||(seat&&seats.contains(seat)))return false;
        identities.insert(id);if(seat)seats.insert(seat);
    }
    return identities.contains(host);
}
void LobbyCoordinator::receive(QString peer,QString name,const QJsonObject& p) {
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
            invitation_={{"host",peer},{"boot",boot},{"name",name.left(100)},{"lobby",id},{"expires",answerUntil},{"game",data["game"]}};emit changed();
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
            if(session_.join(peer,name.left(100),now())) {
                if(!previous.isEmpty()&&previous!=boot) {
                    retiredBoots_[peer+"/"+previous]=now()+180;session_.connected(peer,false);session_.connected(peer,true);
                }
                pendingInvites_.remove(peer);seen_[peer]=now();save();send(peer,"state",session_.state(),probe);
            }
            else if((session_.state()["access"]=="approval"||data["request"].toBool())&&requests_.size()<32
                    &&(session_.state()["phase"]=="choosing"||session_.state()["phase"]=="gathering"||session_.state()["phase"]=="playing")) {
                if(!previous.isEmpty()&&previous!=boot)retiredBoots_[peer+"/"+previous]=now()+180;
                peerBoots_[peer]=boot;peerSerials_[peer]=serial;requests_[peer]=name.left(100);seen_[peer]=now();emit changed();
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
        if(kind=="ping") {session_.connected(peer,true);send(peer,"state",session_.state(),probe);return;}
        if(kind=="live-request"&&p["generation"]==session_.state()["generation"]&&data["ticket"]==session_.state()["ticket"]&&!QUuid(data["operation"].toString()).isNull()) {
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
                session_.finishPlayer(peer,data["ticket"].toString());
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
    if(peer!=host_||id!=(joining_.isEmpty()?remote_["id"].toString():joining_))return;
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
    if(kind=="play-response"&&p["generation"]==remote_["generation"]) {
        peerSerials_[peer]=serial;const auto answer=data["answer"].toString();
        if(answer=="accept")emit notice("The host agreed. Choose your controller after this game; the current game continues.");
        else if(answer=="later")emit notice("The host asked to play later. You can keep watching.");
        else if(answer=="decline")emit notice("The host declined. You can keep watching.");
        return;
    }
    if(kind=="problem") {peerSerials_[peer]=serial;emit notice(data["message"].toString().left(160));return;}
    if(kind=="closed"||kind=="rejected"||kind=="removed") {clearRemote();emit notice(kind=="closed"?"The lobby was closed.":kind=="removed"?"You are no longer in this lobby.":"The lobby couldn't accept your request.");emit changed();return;}
    if(kind!="state"||data["id"]!=id||!validState(data,peer))return;
    if(newBoot) {retiredBoots_[peer+"/"+peerBoots_[peer]]=now()+180;preparedGeneration_=readyGeneration_=0;launchTicket_.clear();finishedTicket_.clear();}
    peerBoots_[peer]=boot;peerSerials_[peer]=serial;
    hostSeen_=now();
    if(!newBoot&&!restoring_&&data["revision"].toInteger()<remote_["revision"].toInteger())return;
    const bool recovered=newBoot||restoring_||!connection_.isEmpty();restoring_=false;connection_.clear();if(confirmed)probe_.clear();
    if(!recovered&&data["revision"]==remote_["revision"])return;
    QJsonObject mine;for(const auto& v:data["members"].toArray())if(v.toObject()["id"]==self_)mine=v.toObject();
    if(mine.isEmpty()) {clearRemote();emit notice("You left the lobby.");emit changed();return;}
    if(data["generation"]!=remote_["generation"])preparedGeneration_=readyGeneration_=0;
    remote_=data;joining_.clear();joinedAt_=0;
    if(data["ticket"]!=finishedTicket_||data["phase"]!="playing")finishedTicket_.clear();
    const auto generation=quint64(data["generation"].toInteger());const auto ticket=data["ticket"].toString();
    if(data["phase"]=="playing"&&!liveOperation_.isEmpty()&&mine["admission"]==liveOperation_&&launchTicket_!=ticket) {
        launchTicket_=ticket;emit liveLaunchRequested(liveOperation_,ticket,data);
    }
    if(!finishedTicket_.isEmpty()&&mine["spectator"].toBool()) {
        finishedTicket_.clear();liveOperation_.clear();launchTicket_.clear();
    }
    if(data["phase"]=="launching"&&mine["seat"].toInt()>0&&preparedGeneration_==generation&&readyGeneration_==generation
       &&!ticket.isEmpty()&&ticket!=launchTicket_) {launchTicket_=ticket;emit launchRequested(ticket,data);}
    if(!mine["prepared"].toBool())preparedGeneration_=readyGeneration_=0;
    if(!mine["ready"].toBool())readyGeneration_=0;
    emit changed();
}
void LobbyCoordinator::deliveryFailed(const QString& peer) {
    if(hosting()) {if(!session_.connected(peer,false))emit notice("Couldn't deliver the lobby invitation. Try again.");}
    else if(peer==host_) {preparedGeneration_=readyGeneration_=0;connection_="Host unavailable. Reconnecting…";emit changed();}
}
void LobbyCoordinator::tick() {
    for(auto it=retiredBoots_.begin();it!=retiredBoots_.end();)if(it.value()<=now())it=retiredBoots_.erase(it);else ++it;
    if(!invitation_.isEmpty()&&invitation_["expires"].toInteger()<=now()) {invitation_={};emit changed();}
    if(!online_)return;
    if(hosting()) {
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
        if(!liveOperation_.isEmpty()&&launchTicket_.isEmpty()&&finishedTicket_.isEmpty()&&remote_["phase"]=="playing")
            send(host_,"live-request",{{"ticket",remote_["ticket"]},{"operation",liveOperation_}});
        if(!finishedTicket_.isEmpty()) {
            if(!liveOperation_.isEmpty())send(host_,"live-ended",{{"ticket",finishedTicket_},{"operation",liveOperation_}});
            else send(host_,"game-finished",{{"ticket",finishedTicket_}});
        }
    }
    for(auto it=receivedInvites_.begin();it!=receivedInvites_.end();)if(it.value()<=now())it=receivedInvites_.erase(it);else ++it;
}
}
