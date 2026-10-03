#include "GameParty.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QUuid>

namespace trainer {
namespace {
QString token(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
qint64 now(){return QDateTime::currentSecsSinceEpoch();}
QString companyOf(const QString& peer){return peer.startsWith("online:company:")?peer.section(':',2,2):QString();}
QString personOf(const QString& peer){return peer.startsWith("online:")?"online:"+peer.section(':',-1):peer;}
bool uuid(const QString& v){return v.size()==36&&!QUuid(v).isNull();}
int capacity(const QJsonObject& g){return qBound(2,g["players"].toInt(2),4);}
}
GameParty::GameParty(QObject* p):QObject(p),boot_(token()) {
    timer_.setParent(this);timer_.setInterval(1000);connect(&timer_,&QTimer::timeout,this,&GameParty::tick);timer_.start();
}
QString GameParty::encode(const QJsonObject& p){return "TrainerOS game party\n```json\n"+QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact))+"\n```";}
QJsonObject GameParty::decode(const QString& s) {
    if(s.size()>2000||!s.startsWith("TrainerOS game party\n```json\n")||!s.endsWith("\n```"))return {};
    const auto prefix=QStringLiteral("TrainerOS game party\n```json\n");
    const auto p=QJsonDocument::fromJson(s.mid(prefix.size(),s.size()-prefix.size()-4).toUtf8()).object();
    return p["ns"]=="org.traineros.party"&&p["v"]==1?p:QJsonObject{};
}
bool GameParty::supports(const QJsonObject& game) const{return !game.isEmpty()&&games_.contains(game);}
void GameParty::configure(QString name,QJsonArray games,QJsonObject current,bool available) {
    name_=name.left(48);games_=std::move(games);current_=std::move(current);available_=available;
    if(!available_&&active())leave();
}
void GameParty::clear(){party_.clear();host_.clear();hostBoot_.clear();members_.clear();game_={};endpoint_={};roster_={};running_=false;revision_=remoteRevision_=0;joiningPeer_.clear();joiningId_.clear();joiningGame_={};joiningParty_.clear();joiningDeadline_=0;requests_.clear();company_.clear();access_="request";allowedMembers_.clear();advertised_=0;for(auto& p:peers_)p.invite=false;}
void GameParty::reset(){leave();peers_.clear();seen_.clear();companyQueries_.clear();boot_=token();}
void GameParty::packet(const QString& peer,QString kind,QJsonObject p) {
    if(!companyOf(peer).isEmpty())p["company"]=companyOf(peer);
    p["trainer"]=name_;p["ns"]="org.traineros.party";p["v"]=1;p["boot"]=boot_;p["kind"]=kind;p["expires"]=now()+90;
    if(!p.contains("target"))p["target"]=peers_.value(peer).boot;
    if(!p.contains("party"))p["party"]=party_;
    if(encode(p).size()>2000){emit notice("Couldn't send the game invitation.");return;}
    emit outgoing(peer,p);
}
QJsonObject GameParty::state() const {
    QJsonArray people=roster_;
    if(host()) {
        people={QJsonObject{{"name",name_},{"slot",1},{"ready",true}}};
        for(auto it=members_.cbegin();it!=members_.cend();++it)people.append(QJsonObject{{"peer",it.key()},{"name",it->name},{"slot",it->slot},{"ready",it->accepted}});
    }
    return {{"party",party_},{"joining",!joiningId_.isEmpty()},{"game",active()?game_:joiningGame_},{"host",host()},{"running",running_},{"members",people},
        {"company",company_},{"access",access_},{"capacity",capacity(game_)},{"free",active()?capacity(game_)-people.size():capacity(current_)}};
}
QJsonObject GameParty::offer(const QString& peer) const {const auto p=peers_.value(peer);return p.expires>=now()?p.offer:QJsonObject{};}
QJsonObject GameParty::pending() const {
    if(requests_.isEmpty())return {};
    const auto& r=requests_.first();return {{"request",r.id},{"name",r.name},{"game",r.game},{"joining",r.joining}};
}
void GameParty::query(const QString& peer) {
    if(!available_||peer.isEmpty()||(!peers_.contains(peer)&&peers_.size()>=64))return;
    auto& p=peers_[peer];if(p.queried>now()-20)return;
    p.query=token();p.queried=now();packet(peer,"query",{{"request",p.query}});
}
void GameParty::browseCompany(const QString& company) {
    if(!available_||company.isEmpty()||companyQueries_.value(company)>now()-25)return;
    companyQueries_[company]=now();
    packet("online:company:"+company,"company-query",{{"target",""}});
}
void GameParty::setCompanyAccess(const QString& company,QString policy,QStringList allowed) {
    if(!host()||company_!=company)return;
    access_=policy=="selected"||policy=="closed"?policy:"request";
    allowedMembers_=allowed;advertiseCompany();emit changed();
}
bool GameParty::openCompany(const QString& company,QString policy,QStringList allowed) {
    if(company.isEmpty()||active()||!create())return false;
    company_=company;setCompanyAccess(company,std::move(policy),std::move(allowed));return true;
}
void GameParty::advertiseCompany() {
    if(!host()||company_.isEmpty())return;
    advertised_=now();
    // This group announcement deliberately contains no endpoint or admission token.
    packet("online:company:"+company_,"company-offer",{{"target",""},{"game",game_},
        {"joinable",access_!="closed"&&freeSlot()>0},{"free",freeSlot()?state()["free"]:QJsonValue(0)},
        {"capacity",capacity(game_)},{"running",running_},{"access",access_}});
}
QJsonArray GameParty::companyOffers() const {
    QJsonArray rows;
    for(auto it=peers_.cbegin();it!=peers_.cend();++it) {
        if(companyOf(it.key()).isEmpty()||it->expires<now()||it->offer["party"].toString().isEmpty()||it->offer["game"].toObject().isEmpty())continue;
        auto row=it->offer;row["peer"]=it.key().mid(7);row["name"]=it->name;
        row["joinable"]=row["joinable"].toBool()&&supports(row["game"].toObject())&&!active()&&joiningId_.isEmpty();rows.append(row);
    }
    return rows;
}
bool GameParty::sameMember(const QString& peer) const {
    for(auto it=members_.cbegin();it!=members_.cend();++it)if(personOf(it.key())==personOf(peer))return true;
    return false;
}
bool GameParty::create(){
    if(active())return host();
    if(!available_||!supports(current_))return false;
    party_=token();game_=current_;return true;
}
int GameParty::freeSlot() const {
    for(int i=2;i<=capacity(game_);++i){bool occupied=false;for(const auto& m:members_)occupied|=m.slot==i;if(!occupied)return i;}
    return 0;
}
void GameParty::invite(const QString& peer) {
    if(!create()||sameMember(peer)||peer.isEmpty())return;
    const auto route=[](const QString& key){return key.contains(':')?key.section(':',0,0):QString();};
    if(!members_.isEmpty()&&route(members_.firstKey())!=route(peer)){emit notice("Invite players using the same connection as this party.");return;}
    const auto p=peers_.value(peer);const int slot=freeSlot();
    if(p.boot.isEmpty()||p.expires<now()){
        if(!peers_.contains(peer)&&peers_.size()>=64)return;
        peers_[peer].invite=true;query(peer);emit changed();return;
    }
    if(!slot){emit notice("This party is full.");return;}
    const auto request=token();members_[peer]={p.name,p.boot,request,slot,false,now()+60};
    packet(peer,"invite",{{"game",game_},{"request",request}});publish();
}
void GameParty::requestJoin(const QString& peer) {
    if(active()||!joiningId_.isEmpty())return;
    const auto p=peers_.value(peer);const auto g=p.offer["game"].toObject();
    if(p.expires<now()||!p.offer["joinable"].toBool()||!supports(g)){emit notice("This game isn't available to join.");return;}
    joiningPeer_=peer;joiningId_=token();joiningDeadline_=now()+60;
    joiningGame_=g;joiningParty_=p.offer["party"].toString();
    packet(peer,"join",{{"request",joiningId_},{"party",p.offer["party"]},{"game",g}});emit changed();
}
void GameParty::answer(bool accept) {
    if(requests_.isEmpty())return;const auto r=requests_.takeFirst();
    if(!accept||r.expires<now()||!available_||!supports(r.game)||(r.joining&&access_=="closed")) {
        packet(r.peer,"decline",{{"party",r.party},{"request",r.id},{"target",r.boot}});emit changed();return;
    }
    if(r.joining) {
        if((active()&&(!host()||party_!=r.party))||(!active()&&current_!=r.game)||!create()||!freeSlot()) {
            packet(r.peer,"decline",{{"party",r.party},{"request",r.id},{"target",r.boot}});emit changed();return;
        }
        members_[r.peer]={r.name,r.boot,r.id,freeSlot(),true,0};
        packet(r.peer,"admitted",{{"request",r.id},{"game",game_}});publish();
        if(running_&&!endpoint_.isEmpty())packet(r.peer,"launch",{{"endpoint",endpoint_},{"game",game_},{"slot",members_[r.peer].slot}});
    }else {
        if(active()){packet(r.peer,"decline",{{"party",r.party},{"request",r.id}});emit changed();return;}
        company_=companyOf(r.peer);party_=r.party;game_=r.game;host_=r.peer;hostBoot_=r.boot;joiningDeadline_=now()+90;
        packet(r.peer,"accept",{{"request",r.id},{"game",game_}});
    }
    emit changed();
}
void GameParty::publish() {
    if(!host())return;++revision_;
    const auto s=state();const auto members=members_;
    for(auto it=members.cbegin();it!=members.cend();++it)if(it->accepted)
        packet(it.key(),"roster",{{"members",s["members"]},{"revision",qint64(revision_)},{"running",running_}});
    advertiseCompany();emit changed();
}
void GameParty::start() {
    if(!host()||running_||members_.isEmpty())return;
    for(const auto& m:members_)if(!m.accepted){emit notice("Waiting for invited players. Cancel their invitation to start without them.");return;}
    running_=true;publish();emit startRequested(true,{});
}
void GameParty::ready(QJsonObject endpoint) {
    if(!host()||!running_)return;endpoint_=std::move(endpoint);
    const auto members=members_;
    for(auto it=members.cbegin();it!=members.cend();++it)if(it->accepted)
        packet(it.key(),"launch",{{"endpoint",endpoint_},{"game",game_},{"slot",it->slot}});
}
void GameParty::leave() {
    if(host()&&!company_.isEmpty())packet("online:company:"+company_,"company-offer",{{"target",""},{"game",QJsonObject{}},{"joinable",false}});
    const auto members=members_;
    if(host())for(auto it=members.cbegin();it!=members.cend();++it)packet(it.key(),"closed",{{"request",it->request}});
    else if(active())packet(host_,"left");
    if(!joiningPeer_.isEmpty())packet(joiningPeer_,"cancel",{{"request",joiningId_}});
    clear();emit changed();
}
void GameParty::disconnected(const QString& peer) {
    if(peers_.contains(peer)){auto& p=peers_[peer];p.offer={};p.expires=0;p.query.clear();p.invite=false;}
    requests_.removeIf([&](const Request& r){return r.peer==peer;});
    // Signalling loss is not an emulator departure. Never free an occupied
    // running slot or stop gameplay merely because chat reconnects.
    if(!running_) {
        if(host()&&members_.remove(peer))publish();
        else if(host_==peer){clear();emit notice("The organizer disconnected.");}
    }
    if(joiningPeer_==peer){joiningPeer_.clear();joiningId_.clear();joiningDeadline_=0;}
    emit changed();
}
void GameParty::deliveryFailed(const QString& peer) {
    const bool requested=peers_.value(peer).invite||joiningPeer_==peer||(!running_&&members_.contains(peer));
    disconnected(peer);
    if(requested)emit notice("Couldn't reach your friend. Try again when they're connected.");
}
void GameParty::cancelInvite(const QString& peer) {
    auto m=members_.find(peer);if(!host()||m==members_.end()||m->accepted)return;
    const auto request=m->request;members_.erase(m);
    packet(peer,"closed",{{"request",request}});publish();
}
void GameParty::receive(QString peer,QString name,const QJsonObject& p) {
    if(!p["trainer"].toString().trimmed().isEmpty())name=p["trainer"].toString().left(48);
    if(!available_||p["ns"]!="org.traineros.party"||p["v"]!=1||encode(p).size()>2000)return;
    const auto expiry=p["expires"].toInteger();const auto boot=p["boot"].toString(),kind=p["kind"].toString(),id=p["request"].toString();
    if(expiry<now()||expiry>now()+120||!uuid(boot))return;
    const auto company=companyOf(peer);
    if(kind=="company-query") {
        if(!company.isEmpty()&&company==company_&&advertised_<now()-3)advertiseCompany();
        return;
    }
    if(kind=="company-offer") {
        if(company.isEmpty()||p["company"]!=company||(!peers_.contains(peer)&&peers_.size()>=64))return;
        auto& remote=peers_[peer];
        remote.name=name.left(48);remote.boot=boot;remote.offer=p;remote.expires=expiry;
        emit changed();return;
    }
    if(kind=="query") {
        if(!uuid(id)||peers_.size()>=64&&!peers_.contains(peer))return;
        auto& remote=peers_[peer];
        if(!remote.boot.isEmpty()&&remote.boot!=boot&&members_.contains(peer))return;
        remote.name=name.left(48);remote.boot=boot;remote.expires=now()+90;
        const auto g=active()?game_:current_;
        packet(peer,"offer",{{"request",id},{"game",g},{"joinable",supports(g)&&access_!="closed"&&(!active()||host())&&(!active()||freeSlot()>0)},
            {"free",active()?state()["free"]:QJsonValue(capacity(g)-1)},{"running",running_},{"access",access_},{"automatic",access_=="selected"&&company==company_&&allowedMembers_.contains(personOf(peer).mid(7))}});return;
    }
    if(p["target"]!=boot_)return;
    auto remote=peers_.find(peer);
    if(kind=="offer") {
        if(remote==peers_.end()||remote->query!=id||!uuid(id))return;
        remote->boot=boot;remote->name=name.left(48);remote->offer=p;remote->expires=now()+90;remote->query.clear();
        const bool inviting=remote->invite;remote->invite=false;
        if(inviting)invite(peer);emit changed();return;
    }
    // A group announcement is a broadcast: the guest's first private Join
    // establishes its reply identity after the provider checks group membership.
    if(kind=="join"&&!company.isEmpty()&&company==company_&&p["company"]==company&&remote==peers_.end()&&peers_.size()<64) {
        peers_[peer]={name.left(48),boot,{}, {},expiry,0,false};remote=peers_.find(peer);
    }
    if(remote==peers_.end()||remote->boot!=boot)return;
    if(kind=="invite"||kind=="join") {
        if(!uuid(id)||requests_.size()>=8)return;
        const auto seen=peer+boot+id;if(seen_.contains(seen)||seen_.size()>=256)return;seen_[seen]=expiry;
        const auto g=p["game"].toObject();const auto party=p["party"].toString();
        const bool join=kind=="join";
        const auto route=[](const QString& key){return key.contains(':')?key.section(':',0,0):QString();};
        if(join&&!members_.isEmpty()&&route(members_.firstKey())!=route(peer)){packet(peer,"decline",{{"party",party},{"request",id}});return;}
        if(!supports(g)||(join&&(sameMember(peer)||access_=="closed"||(!company.isEmpty()&&company!=company_)))||(!join&&(active()||!joiningId_.isEmpty()||!uuid(party)))||
            (join&&((active()&&(!host()||party!=party_||!freeSlot()))||(!active()&&(!party.isEmpty()||current_!=g))))) {
            packet(peer,"decline",{{"party",party},{"request",id}});return;
        }
        for(const auto& r:requests_)if(r.peer==peer)return;
        requests_.append({peer,name.left(48),boot,id,party,g,join,now()+60});
        if(join&&access_=="selected"&&!company.isEmpty()&&company==company_&&allowedMembers_.contains(personOf(peer).mid(7))) {
            // Answer this member, not an older pending request from another person.
            const auto approved=requests_.takeLast();requests_.prepend(approved);answer(true);
        }else emit changed();return;
    }
    if(kind=="cancel") {requests_.removeIf([&](const Request& r){return r.peer==peer&&r.id==id;});emit changed();return;}
    if(kind=="closed"&&!active()) {requests_.removeIf([&](const Request& r){return r.peer==peer&&r.id==id&&r.party==p["party"];});emit changed();return;}
    if(kind=="admitted"&&peer==joiningPeer_&&id==joiningId_&&joiningDeadline_>=now()&&uuid(p["party"].toString())&&p["game"].toObject()==joiningGame_&&(joiningParty_.isEmpty()||p["party"]==joiningParty_)) {
        company_=company;party_=p["party"].toString();host_=peer;hostBoot_=boot;game_=p["game"].toObject();joiningPeer_.clear();joiningId_.clear();joiningDeadline_=now()+90;emit changed();return;
    }
    if(kind=="decline"&&peer==joiningPeer_&&id==joiningId_){joiningPeer_.clear();joiningId_.clear();joiningDeadline_=0;emit notice("The join request was declined.");emit changed();return;}
    if(p["party"]!=party_||party_.isEmpty())return;
    if(host()) {
        auto m=members_.find(peer);if(m==members_.end()||m->boot!=boot)return;
        if(kind=="accept"&&id==m->request&&!m->accepted&&m->expires>=now()&&p["game"]==game_) {
            m->accepted=true;m->expires=0;publish();
            if(running_&&!endpoint_.isEmpty())packet(peer,"launch",{{"endpoint",endpoint_},{"game",game_},{"slot",m->slot}});
        } else if(kind=="left"||(kind=="decline"&&id==m->request)){members_.erase(m);publish();}
        return;
    }
    if(peer!=host_||boot!=hostBoot_)return;
    if(kind=="closed"){clear();emit notice("The organizer ended the party.");emit changed();return;}
    if(kind=="roster") {
        const auto revision=p["revision"].toInteger();const auto roster=p["members"].toArray();
        if(revision<=qint64(remoteRevision_)||roster.size()>capacity(game_))return;
        remoteRevision_=revision;roster_=roster;joiningDeadline_=0;emit changed();
    } else if(kind=="launch"&&!running_&&p["game"]==game_&&!p["endpoint"].toObject().isEmpty()) {
        const int slot=p["slot"].toInt();if(slot<2||slot>capacity(game_))return;
        running_=true;joiningDeadline_=0;auto endpoint=p["endpoint"].toObject();endpoint["slot"]=slot;
        emit startRequested(false,endpoint);emit changed();
    }
}
void GameParty::tick() {
    const auto t=now();bool changed=false;
    if(host()&&!company_.isEmpty()&&advertised_<t-40)advertiseCompany();
    for(auto it=members_.begin();it!=members_.end();)if(!it->accepted&&it->expires<t){packet(it.key(),"closed",{{"request",it->request}});it=members_.erase(it);changed=true;}else ++it;
    changed|=requests_.removeIf([&](const Request& r){return r.expires<t;})>0;
    for(auto it=seen_.begin();it!=seen_.end();)if(it.value()<t)it=seen_.erase(it);else ++it;
    for(auto& peer:peers_) {
        if(peer.invite&&peer.queried&&peer.queried<t-25){peer.invite=false;changed=true;emit notice("Your friend didn't answer the invitation. Try again when they're online.");}
        if(!peer.offer.isEmpty()&&peer.expires<t){peer.offer={};changed=true;}
    }
    if(joiningDeadline_&&joiningDeadline_<t){leave();emit notice("The invitation expired. Try again.");return;}
    if(changed){if(host())publish();else emit this->changed();}
}
}
