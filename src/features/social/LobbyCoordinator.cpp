#include "LobbyCoordinator.h"
#include <QDateTime>
#include <QJsonArray>
#include <QSet>
#include <QUuid>

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
    connect(&session_,&LobbySession::changed,this,[this]{broadcast();emit changed();});
    connect(&session_,&LobbySession::launchRequested,this,&LobbyCoordinator::launchRequested);
}
bool LobbyCoordinator::hosting() const {return session_.state()["phase"]!="closed"&&session_.state()["owner"]==self_;}
QJsonObject LobbyCoordinator::state() const {
    if(hosting())return session_.state();
    if(remote_.isEmpty()&&!host_.isEmpty())return {{"id",joining_},{"owner",host_},{"phase","joining"}};
    return remote_;
}
QJsonArray LobbyCoordinator::requests() const {
    QJsonArray list;for(auto it=requests_.cbegin();it!=requests_.cend();++it)list.append(QJsonObject{{"id",it.key()},{"name",it.value()}});return list;
}
void LobbyCoordinator::clearRemote() {
    peerBoots_.remove(host_);peerSerials_.remove(host_);
    remote_={};host_.clear();joining_.clear();launchTicket_.clear();preparedGeneration_=readyGeneration_=0;joinedAt_=hostSeen_=0;
}
void LobbyCoordinator::configure(QString self,QString name,bool online) {
    name=name.left(100);online=online&&identifier(self);
    if(self_==self&&name_==name&&online_==online)return;
    if(self_!=self) {
        online_=false;leave();clearRemote();invitation_={};requests_.clear();seen_.clear();self_=self;
        peerBoots_.clear();peerSerials_.clear();retiredBoots_.clear();serial_=0;boot_=QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    name_=name;const bool changed=online_!=online;online_=online;
    if(hosting()&&changed)session_.connected(self_,online_);
    if(!online_) {preparedGeneration_=readyGeneration_=0;}
    emit this->changed();
}
bool LobbyCoordinator::create(QString access) {
    if(!online_||hosting()||!host_.isEmpty())return false;
    clearRemote();return session_.create(self_,name_,access);
}
void LobbyCoordinator::send(const QString& peer,QString kind,QJsonObject data) {
    if(!online_||!identifier(peer)||peer==self_)return;
    const auto lobby=hosting()?session_.id():!joining_.isEmpty()?joining_:remote_["id"].toString();
    emit outgoing(peer,{{"ns","org.traineros.lobby"},{"v",1},{"kind",kind},{"lobby",lobby},
        {"boot",boot_},{"serial",qint64(++serial_)},
        {"expires",now()+90},{"generation",state()["generation"]},{"data",data}});
}
void LobbyCoordinator::broadcast() {
    if(!hosting()||!online_)return;
    const auto snapshot=session_.state();
    for(const auto& v:snapshot["members"].toArray()) {
        const auto peer=v.toObject()["id"].toString();if(peer!=self_)send(peer,"state",snapshot);
    }
}
bool LobbyCoordinator::invite(const QString& peer) {
    if(!hosting()||!identifier(peer)||!session_.invite(self_,peer,now()+90))return false;
    send(peer,"invite",{{"name",name_},{"game",session_.state()["game"]}});return true;
}
bool LobbyCoordinator::join(const QString& host,const QString& lobby) {
    if(!online_||hosting()||!host_.isEmpty()||!identifier(host)||host==self_||QUuid(lobby).isNull())return false;
    host_=host;joining_=lobby;joinedAt_=hostSeen_=now();send(host_,"join",{{"name",name_}});emit changed();return true;
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
        session_.leave(self_);
    } else if(!host_.isEmpty())send(host_,"leave");
    clearRemote();requests_.clear();seen_.clear();emit changed();
}
bool LobbyCoordinator::selectGame(const LobbySession::Game& game) {return hosting()&&session_.selectGame(self_,game);}
bool LobbyCoordinator::setAccess(QString access) {return hosting()&&session_.setAccess(self_,access);}
bool LobbyCoordinator::setConversation(QString channel) {return hosting()&&session_.setConversation(self_,channel);}
bool LobbyCoordinator::seat(int seat) {
    if(hosting())return session_.claimSeat(self_,seat,session_.generation());
    if(!online_||remote_.isEmpty())return false;
    preparedGeneration_=readyGeneration_=0;send(host_,"seat",{{"seat",seat}});return true;
}
bool LobbyCoordinator::spectate(bool value) {
    if(hosting())return session_.spectate(self_,value,session_.generation());
    if(!online_||remote_.isEmpty())return false;
    preparedGeneration_=readyGeneration_=0;send(host_,"spectate",{{"value",value}});return true;
}
bool LobbyCoordinator::prepared(quint64 generation,bool value) {
    if(hosting())return session_.prepared(self_,generation,value);
    if(!online_||remote_.isEmpty()||remote_["generation"].toInteger()!=qint64(generation))return false;
    preparedGeneration_=value?generation:0;readyGeneration_=0;send(host_,"prepared",{{"value",value}});return true;
}
bool LobbyCoordinator::ready(bool value) {
    if(hosting())return session_.ready(self_,session_.generation(),value);
    const auto generation=quint64(remote_["generation"].toInteger());
    if(!online_||remote_.isEmpty()||!preparedGeneration_||preparedGeneration_!=generation)return false;
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
    if(!online_||ticket.isEmpty()||ticket!=launchTicket_||remote_["ticket"]!=ticket||remote_["phase"]!="launching")return false;
    send(host_,"launch-result",{{"ticket",ticket},{"success",success}});return true;
}
bool LobbyCoordinator::finishGame(const QString& ticket) {
    if(hosting())return session_.finishGame(ticket);
    if(!online_||ticket.isEmpty()||ticket!=launchTicket_||remote_["ticket"]!=ticket||remote_["phase"]!="playing")return false;
    send(host_,"game-finished",{{"ticket",ticket}});return true;
}
bool LobbyCoordinator::validState(const QJsonObject& s,const QString& host) {
    if(!s["conversation"].toString().isEmpty()&&!identifier(s["conversation"].toString()))return false;
    if(s["owner"]!=host||s["revision"].toInteger()<1||s["generation"].toInteger()<1)return false;
    if(!QStringList{"choosing","gathering","launching","playing"}.contains(s["phase"].toString()))return false;
    const auto members=s["members"].toArray();if(members.isEmpty()||members.size()>32)return false;
    QSet<QString> identities;QSet<int> seats;const auto capacity=s["game"].toObject()["seats"].toInt(-1);
    if(capacity<0||capacity>16)return false;
    for(const auto& value:members) {
        const auto m=value.toObject();const auto id=m["id"].toString();const auto seat=m["seat"].toInt(-1);
        if(!identifier(id)||identities.contains(id)||m["name"].toString().size()>100||seat<0||seat>capacity||(seat&&seats.contains(seat)))return false;
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
        if(!hosting()&&host_.isEmpty()&&invitation_.isEmpty()) {
            invitation_={{"host",peer},{"name",name.left(100)},{"lobby",id},{"expires",p["expires"]},{"game",data["game"]}};emit changed();
        }
        return;
    }
    if(hosting()) {
        if(id!=session_.id())return;
        if(kind=="join") {
            const auto previous=peerBoots_.value(peer);
            const auto previousSerial=peerSerials_.value(peer);
            // Bind before join emits state, so a synchronous prepared reply is
            // authenticated against this incarnation as well.
            peerBoots_[peer]=boot;peerSerials_[peer]=serial;
            if(session_.join(peer,name.left(100),now())) {
                if(!previous.isEmpty()&&previous!=boot) {
                    retiredBoots_[peer+"/"+previous]=now()+180;session_.connected(peer,false);session_.connected(peer,true);
                }
                seen_[peer]=now();send(peer,"state",session_.state());
            }
            else if(session_.state()["access"]=="approval"&&requests_.size()<32) {
                if(!previous.isEmpty()&&previous!=boot)retiredBoots_[peer+"/"+previous]=now()+180;
                peerBoots_[peer]=boot;peerSerials_[peer]=serial;requests_[peer]=name.left(100);seen_[peer]=now();emit changed();
            }
            else {peerBoots_[peer]=previous;peerSerials_[peer]=previousSerial;send(peer,"rejected");}
            return;
        }
        bool member=false;for(const auto& v:session_.state()["members"].toArray())if(v.toObject()["id"]==peer)member=true;
        if(!member)return;
        if(peerBoots_.value(peer)!=boot)return;
        peerSerials_[peer]=serial;
        seen_[peer]=now();
        if(kind=="leave") {session_.leave(peer);seen_.remove(peer);return;}
        if(kind=="ping") {session_.connected(peer,true);send(peer,"state",session_.state());return;}
        if(kind=="launch-result"&&data["success"].isBool()) {recordLaunchResult(peer,data["ticket"].toString(),data["success"].toBool());return;}
        if(kind=="game-finished") {
            // A seated player may end their part of the current game, not a
            // spectator or a delayed report from an earlier launch.
            for(const auto& m:session_.state()["members"].toArray())if(m.toObject()["id"]==peer&&m.toObject()["seat"].toInt()>0)
                session_.finishGame(data["ticket"].toString());
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
    if(peerBoots_.contains(peer)&&peerBoots_[peer]!=boot)return;
    if(kind=="problem") {peerSerials_[peer]=serial;emit notice(data["message"].toString().left(160));return;}
    if(kind=="closed"||kind=="rejected") {clearRemote();emit notice(kind=="closed"?"The lobby was closed.":"The lobby couldn't accept your request.");emit changed();return;}
    if(kind!="state"||data["id"]!=id||!validState(data,peer))return;
    peerBoots_[peer]=boot;peerSerials_[peer]=serial;
    hostSeen_=now();
    if(data["revision"].toInteger()<=remote_["revision"].toInteger())return;
    QJsonObject mine;for(const auto& v:data["members"].toArray())if(v.toObject()["id"]==self_)mine=v.toObject();
    if(mine.isEmpty()) {clearRemote();emit notice("You left the lobby.");emit changed();return;}
    if(data["generation"]!=remote_["generation"])preparedGeneration_=readyGeneration_=0;
    remote_=data;joining_.clear();joinedAt_=0;
    const auto generation=quint64(data["generation"].toInteger());const auto ticket=data["ticket"].toString();
    if(data["phase"]=="launching"&&mine["seat"].toInt()>0&&preparedGeneration_==generation&&readyGeneration_==generation
       &&!ticket.isEmpty()&&ticket!=launchTicket_) {launchTicket_=ticket;emit launchRequested(ticket,data);}
    if(!mine["prepared"].toBool())preparedGeneration_=readyGeneration_=0;
    if(!mine["ready"].toBool())readyGeneration_=0;
    emit changed();
}
void LobbyCoordinator::deliveryFailed(const QString& peer) {
    if(hosting()) {if(!session_.connected(peer,false))emit notice("Couldn't deliver the lobby invitation. Try again.");}
    else if(peer==host_) {preparedGeneration_=readyGeneration_=0;emit notice("Lobby connection interrupted. Reconnecting…");emit changed();}
}
void LobbyCoordinator::tick() {
    for(auto it=retiredBoots_.begin();it!=retiredBoots_.end();)if(it.value()<=now())it=retiredBoots_.erase(it);else ++it;
    if(!invitation_.isEmpty()&&invitation_["expires"].toInteger()<=now()) {invitation_={};emit changed();}
    if(!online_)return;
    if(hosting()) {
        for(auto it=requests_.begin();it!=requests_.end();) {
            if(now()-seen_.value(it.key())>90) {it=requests_.erase(it);emit changed();}else ++it;
        }
        for(auto it=seen_.cbegin();it!=seen_.cend();++it)if(now()-it.value()>45)session_.connected(it.key(),false);
    } else if(!host_.isEmpty()) {
        if(joinedAt_&&now()-joinedAt_>90) {clearRemote();emit notice("The join request expired.");emit changed();return;}
        if(hostSeen_&&now()-hostSeen_>45)deliveryFailed(host_);
        send(host_,joining_.isEmpty()?"ping":"join");
    }
}
}
