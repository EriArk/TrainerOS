#include "LobbySession.h"
#include <QJsonArray>
#include <QUuid>

namespace trainer {
LobbySession::LobbySession(QObject* parent):QObject(parent) {}
bool LobbySession::host(const QString& actor) const {
    return phase_!="closed"&&actor==owner_&&members_.contains(actor)&&members_.value(actor).connected;
}
bool LobbySession::editable() const {return phase_=="choosing"||phase_=="gathering";}
int LobbySession::freeSeat() const {
    for(int seat=1;seat<=game_.seats;++seat) {
        bool used=false;for(const auto& m:members_)if(m.seat==seat) {used=true;break;}
        if(!used)return seat;
    }
    return 0;
}
void LobbySession::publish() {++revision_;emit changed();}
void LobbySession::resetReady() {for(auto& m:members_)m.ready=false;}
void LobbySession::invalidateLaunch() {
    if(phase_=="launching") {
        phase_="gathering";ticket_.clear();++generation_;
        for(auto& m:members_)m.prepared=false;
    }
    resetReady();
}
bool LobbySession::create(QString owner,QString name,QString access) {
    if(phase_!="closed"||owner.isEmpty()||owner.size()>128||name.trimmed().isEmpty()||name.size()>100
       ||(access!="private"&&access!="public"&&access!="approval"))return false;
    id_=QUuid::createUuid().toString(QUuid::WithoutBraces);owner_=owner;access_=access;
    members_.clear();invites_.clear();game_={};ticket_.clear();conversation_.clear();revision_=0;++generation_;
    members_.insert(owner,{name.trimmed()});phase_="choosing";publish();return true;
}
bool LobbySession::setAccess(const QString& actor,QString access) {
    if(!host(actor)||(access!="private"&&access!="public"&&access!="approval"))return false;
    if(access_==access)return true;
    access_=access;publish();return true;
}
bool LobbySession::restore(const QJsonObject& s) {
    if(phase_!="closed")return false;
    id_=s["id"].toString();owner_=s["owner"].toString();access_=s["access"].toString();
    conversation_=s["conversation"].toString();ticket_.clear();invites_.clear();members_.clear();
    const auto g=s["game"].toObject();
    game_={g["id"].toString(),g["title"].toString(),g["platform"].toString(),g["mode"].toString(),g["seats"].toInt(),g["runtime"].toObject()};
    revision_=quint64(s["revision"].toInteger());generation_=quint64(s["generation"].toInteger())+1;
    for(const auto& value:s["members"].toArray()) {
        const auto m=value.toObject();members_.insert(m["id"].toString(),{m["name"].toString(),m["seat"].toInt(),false,false,false,m["spectator"].toBool()});
    }
    phase_=game_.id.isEmpty()?"choosing":"gathering";publish();return true;
}
bool LobbySession::setConversation(const QString& actor,QString channel) {
    if(!host(actor)||channel.isEmpty()||channel.size()>20)return false;
    for(const auto c:channel)if(c<'0'||c>'9')return false;
    if(conversation_==channel)return true;
    // A conversation belongs to the room, never to a game generation.
    if(!conversation_.isEmpty())return false;
    conversation_=channel;publish();return true;
}
bool LobbySession::invite(const QString& actor,QString peer,qint64 expiresAt) {
    if(!host(actor)||peer.isEmpty()||peer.size()>128||members_.contains(peer)||expiresAt<=0||invites_.size()>=64)return false;
    invites_.insert(peer,expiresAt);return true;
}
bool LobbySession::join(QString peer,QString name,qint64 now) {
    if(!host(owner_)||peer.isEmpty()||peer.size()>128||name.trimmed().isEmpty()||name.size()>100||now<=0)return false;
    if(members_.contains(peer))return connected(peer,true);
    if(!editable())return false;
    const bool invited=invites_.contains(peer)&&invites_.value(peer)>now;
    if(members_.size()>=32||(access_!="public"&&!invited))return false;
    invites_.remove(peer);members_.insert(peer,{name.trimmed(),freeSeat()});resetReady();publish();return true;
}
bool LobbySession::approve(const QString& actor,const QString& peer,QString name) {
    if(!host(actor)||!editable()||peer.isEmpty()||peer.size()>128||name.trimmed().isEmpty()||name.size()>100||members_.size()>=32)return false;
    if(members_.contains(peer))return false;
    invites_.remove(peer);members_.insert(peer,{name.trimmed(),freeSeat()});resetReady();publish();return true;
}
bool LobbySession::leave(const QString& peer) {
    if(phase_=="closed"||!members_.contains(peer))return false;
    if(peer==owner_) {
        phase_="closed";ticket_.clear();members_.clear();invites_.clear();publish();return true;
    }
    members_.remove(peer);invalidateLaunch();publish();return true;
}
bool LobbySession::remove(const QString& actor,const QString& peer) {
    return host(actor)&&peer!=owner_&&leave(peer);
}
bool LobbySession::close(const QString& actor) {return host(actor)&&leave(actor);}
bool LobbySession::selectGame(const QString& actor,const Game& game) {
    if(!host(actor)||!editable()||game.id.isEmpty()||game.id.size()>128||game.title.trimmed().isEmpty()
       ||game.title.size()>160||game.platform.isEmpty()||game.platform.size()>32||game.mode.isEmpty()
       ||game.mode.size()>64||game.seats<1||game.seats>16)return false;
    game_=game;++generation_;ticket_.clear();phase_="gathering";
    // Seat assignment is per game. The existing runtime families use the
    // organizer's console as P1; other players may change their free port.
    for(auto& m:members_) {m.seat=0;m.prepared=false;m.ready=false;}
    int seat=1;
    if(!members_[owner_].spectator)members_[owner_].seat=seat++;
    for(auto it=members_.begin();it!=members_.end();++it)
        if(it.key()!=owner_&&!it->spectator&&seat<=game_.seats)it->seat=seat++;
    publish();return true;
}
bool LobbySession::claimSeat(const QString& peer,int seat,quint64 generation) {
    if(!editable()||phase_!="gathering"||!host(owner_)||generation!=generation_||!members_.contains(peer)
       ||!members_.value(peer).connected||seat<0||seat>game_.seats)return false;
    for(auto it=members_.cbegin();it!=members_.cend();++it)
        if(seat&&it.key()!=peer&&it.value().seat==seat)return false;
    auto& m=members_[peer];if(m.seat==seat)return true;
    m.seat=seat;m.spectator=false;m.prepared=false;resetReady();publish();return true;
}
bool LobbySession::spectate(const QString& peer,bool value,quint64 generation) {
    if(!editable()||!host(owner_)||generation!=generation_||!members_.contains(peer)
       ||!members_.value(peer).connected)return false;
    auto& m=members_[peer];if(m.spectator==value)return true;
    m.spectator=value;m.seat=0;m.prepared=false;resetReady();publish();return true;
}
bool LobbySession::prepared(const QString& peer,quint64 generation,bool value) {
    if(phase_!="gathering"||!host(owner_)||generation!=generation_||!members_.contains(peer)
       ||!members_.value(peer).connected||!members_.value(peer).seat)return false;
    auto& m=members_[peer];if(m.prepared==value)return true;
    m.prepared=value;m.ready=false;publish();return true;
}
bool LobbySession::ready(const QString& peer,quint64 generation,bool value) {
    if(phase_!="gathering"||!host(owner_)||generation!=generation_||!members_.contains(peer))return false;
    auto& m=members_[peer];if(!m.connected||!m.seat||!m.prepared)return false;
    if(m.ready==value)return true;
    m.ready=value;publish();tryLaunch();return true;
}
void LobbySession::tryLaunch() {
    if(phase_!="gathering"||!host(owner_))return;
    int players=0;
    for(const auto& m:members_)if(!m.spectator) {
        ++players;if(!m.seat||!m.connected||!m.prepared||!m.ready)return;
    }
    if(!players)return;
    ticket_=QUuid::createUuid().toString(QUuid::WithoutBraces);phase_="launching";
    const auto ticket=ticket_;publish();
    if(phase_=="launching"&&ticket_==ticket)emit launchRequested(ticket,state());
}
bool LobbySession::connected(const QString& peer,bool value) {
    if(phase_=="closed"||!members_.contains(peer))return false;
    auto& m=members_[peer];if(m.connected==value)return true;
    m.connected=value;m.prepared=false;invalidateLaunch();publish();return true;
}
bool LobbySession::acknowledgeLaunch(const QString& ticket,bool success) {
    if(phase_!="launching"||ticket.isEmpty()||ticket!=ticket_)return false;
    // A launch intent is not evidence that the runtime has started.
    phase_=success?"playing":"gathering";
    if(!success) {ticket_.clear();for(auto& m:members_) {m.prepared=false;m.ready=false;}}
    publish();return true;
}
bool LobbySession::finishGame(const QString& ticket) {
    if(phase_!="playing"||ticket.isEmpty()||ticket!=ticket_)return false;
    phase_="gathering";ticket_.clear();++generation_;
    for(auto& m:members_) {m.prepared=false;m.ready=false;}
    publish();return true;
}
QJsonObject LobbySession::state() const {
    QJsonArray members;
    for(auto it=members_.cbegin();it!=members_.cend();++it) {
        const auto& m=it.value();members.append(QJsonObject{{"id",it.key()},{"name",m.name},{"seat",m.seat},
            {"prepared",m.prepared},{"ready",m.ready},{"connected",m.connected},{"spectator",m.spectator}});
    }
    return {{"id",id_},{"owner",owner_},{"access",access_},{"phase",phase_},{"revision",qint64(revision_)},
        {"generation",qint64(generation_)},{"members",members},{"ticket",ticket_},{"conversation",conversation_},
        {"game",QJsonObject{{"id",game_.id},{"title",game_.title},{"platform",game_.platform},{"mode",game_.mode},{"seats",game_.seats},{"runtime",game_.runtime}}}};
}
}
