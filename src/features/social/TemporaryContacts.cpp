#include "TemporaryContacts.h"
#include <QDateTime>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QPointer>
#include <QUuid>

namespace trainer {
namespace {
bool id(const QString& s) {if(s.isEmpty()||s.size()>20||s[0]=='0')return false;for(auto c:s)if(c<'0'||c>'9')return false;return true;}
bool stamped(const QJsonObject& r,int type) {
    return r.value("type")==type&&!r.value("since").toString().isEmpty()&&QDateTime::fromString(r.value("since").toString(),Qt::ISODateWithMs).isValid();
}
bool temporary(QString phase) {return phase!="kept"&&phase!="exempt"&&phase!="blocked"&&phase!="error"&&phase!="uncertain"&&phase!="gone"&&phase!="done";}
}
void TemporaryContacts::configure(QString file,QString self) {
    if(file_==file&&self_==self)return;
    ++epoch_;++flight_;busy_.clear();records_={};members_.clear();roomMembers_.clear();roomHosts_.clear();held_.clear();next_.clear();room_.clear();host_.clear();online_=false;
    file_=file;self_=self;storageError_.clear();now_=QDateTime::currentSecsSinceEpoch();nextCreate_=now_;
    if(file.isEmpty()||!id(self))return;
    QFile input(file);
    if(input.exists()) {
        if(QFileInfo(file).isSymLink()||input.size()>512*1024||!input.open(QIODevice::ReadOnly)) {storageError_="Temporary contact history is unavailable.";return;}
        const auto data=QJsonDocument::fromJson(input.readAll()).object();
        if(data.value("v")!=1||data.value("self")!=self||!data.value("contacts").isObject()||data.value("contacts").toObject().size()>256) {storageError_="Temporary contact history needs recovery.";return;}
        const auto contacts=data.value("contacts").toObject();
        for(auto it=contacts.begin();it!=contacts.end();++it) {
            auto r=it.value().toObject();
            if(!id(it.key())||it.key()==self||QUuid(r.value("lease").toString()).isNull()||QUuid(r.value("room").toString()).isNull()) {storageError_="Temporary contact history needs recovery.";records_={};return;}
            // Lost mutation replies cannot confer ownership of an unrelated record.
            if(r.value("phase")=="creating"||r.value("phase")=="accepting") {r["phase"]="uncertain";r["error"]="Check this friendship after the interrupted request.";}
            if(r.value("phase")=="deleting")r["phase"]="cleanup";
            records_[it.key()]=r;next_[it.key()]=now_+15;
        }
    }
    emit changed();
}
bool TemporaryContacts::write(const QJsonObject& records) {
    if(!storageError_.isEmpty()||file_.isEmpty()||!id(self_))return false;
    const auto dir=QFileInfo(file_).absolutePath();
    if(QFileInfo(file_).isSymLink()||QFileInfo(dir).isSymLink()||!QDir().mkpath(dir))return false;
    QFile::setPermissions(dir,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    QSaveFile file(file_);if(!file.open(QIODevice::WriteOnly))return false;
    file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    const auto bytes=QJsonDocument(QJsonObject{{"v",1},{"self",self_},{"contacts",records}}).toJson(QJsonDocument::Compact);
    return file.write(bytes)==bytes.size()&&file.commit();
}
bool TemporaryContacts::put(const QString& peer,QJsonObject record) {
    if((record.value("phase")=="kept"||record.value("phase")=="gone"||record.value("phase")=="done")&&!record.contains("ended"))record["ended"]=now_;
    auto next=records_;if(record.isEmpty())next.remove(peer);else next[peer]=record;
    if(next==records_)return true;
    if(!write(next)) {storageError_="Couldn't save temporary contact history. Automatic changes stopped.";emit changed();return false;}
    records_=next;emit changed();return true;
}
void TemporaryContacts::context(QJsonObject lobby,bool online) {
    recovering_=false;online_=online;members_.clear();room_.clear();host_.clear();roomMembers_.clear();roomHosts_.clear();
    const auto rooms=lobby.contains("rooms")?lobby["rooms"].toArray():QJsonArray{lobby};
    for(const auto& value:rooms) {
        const auto r=value.toObject();const auto room=r["id"].toString(),host=r["owner"].toString();
        recovering_=recovering_||r["recovering"].toBool();
        if(!id(self_)||QUuid(room).isNull()||r["phase"]=="joining")continue;
        QSet<QString> members;for(const auto& v:r["members"].toArray())if(id(v.toObject()["id"].toString()))members.insert(v.toObject()["id"].toString());
        if(!members.contains(self_)||!members.contains(host))continue;
        roomMembers_[room]=members;roomHosts_[room]=host;members_.unite(members);
        if(room_.isEmpty()){room_=room;host_=host;}
    }
}
bool TemporaryContacts::active(const QString& peer) const {
    return members_.contains(peer)&&members_.contains(self_);
}

bool TemporaryContacts::managed(const QString& peer) const {
    const auto phase=records_.value(peer).toObject()["phase"].toString();
    return !phase.isEmpty()&&phase!="exempt"&&phase!="kept"&&phase!="done"&&phase!="gone";
}
QVariantMap TemporaryContacts::state() const {
    QVariantMap peers;
    for(auto it=records_.begin();it!=records_.end();++it) {
        const auto r=it.value().toObject();const auto phase=r.value("phase").toString();
        if(phase=="exempt"||phase=="kept"||phase=="gone"||phase=="done")continue;
        const bool ending=phase=="released"||phase=="cleanup"||phase=="deleting";
        const auto status=!r.value("error").toString().isEmpty()?r.value("error").toString():phase=="keep"?QString("Keeping friendship…"):
            ending?QString("Finishing temporary contact…"):phase=="friend"||phase=="accepted"?QString("Temporary lobby friend"):QString("Connecting lobby contact…");
        peers[it.key()]=QVariantMap{{"temporary",temporary(phase)},{"canKeep",phase=="friend"||phase=="accepted"},
            {"pendingKeep",phase=="keep"},{"status",status}};
    }
    return {{"peers",peers},{"error",storageError_}};
}
void TemporaryContacts::send(const QString& peer,QString operation) {
    const auto r=records_.value(peer).toObject();if(r.isEmpty()||!online_)return;
    next_[peer]=now_+15;
    emit outgoing(peer,{{"ns","org.traineros.lobby"},{"v",1},{"kind","contact"},{"lobby",r.value("room")},
        {"expires",now_+60},{"data",QJsonObject{{"op",operation},{"lease",r.value("lease")}}}});
}
void TemporaryContacts::fail(const QString& peer,QString text) {
    auto r=records_.value(peer).toObject();r["phase"]="error";r["error"]=text;if(put(peer,r))send(peer,"blocked");
}
bool TemporaryContacts::keep(const QString& peer) {
    auto r=records_.value(peer).toObject();if(r.isEmpty())return false;
    const auto phase=r.value("phase").toString();
    if(phase=="deleting"||phase=="released"||phase=="cleanup")return false;
    if(phase=="kept"||phase=="exempt")return true;
    r["phase"]=r.value("host").toBool()?"kept":"keep";r.remove("error");
    if(!put(peer,r))return false;
    send(peer,r.value("host").toBool()?"kept":"keep");return true;
}
bool TemporaryContacts::manual(const QString& peer) {
    if(!managed(peer))return true;
    return keep(peer);
}
void TemporaryContacts::release(const QString& peer) {
    auto r=records_.value(peer).toObject();
    if(r.value("phase")=="keep"||r.value("phase")=="kept") {send(peer,r.value("host").toBool()?"kept":"keep");return;}
    if(recovering_||busy_==peer||held_.contains(peer)) {r["releaseRequested"]=true;put(peer,r);return;}
    r["phase"]="released";r.remove("releaseRequested");if(put(peer,r))send(peer,"remove-ok");
}
void TemporaryContacts::receive(QString peer,const QJsonObject& packet) {
    if(!online_||!id(peer)||peer==self_||packet.value("ns")!="org.traineros.lobby"||packet.value("v")!=1||packet.value("kind")!="contact"
       ||packet.value("expires").toInteger()<=now_||packet.value("expires").toInteger()>now_+90||!storageError_.isEmpty())return;
    const auto data=packet.value("data").toObject();const auto lease=data.value("lease").toString(),room=packet.value("lobby").toString(),op=data.value("op").toString();
    if(QUuid(lease).isNull()||QUuid(room).isNull())return;
    auto r=records_.value(peer).toObject();
    const bool replace=r.value("phase")=="done"||(!active(peer)&&busy_!=peer&&!r.contains("type")&&r.value("phase")!="uncertain"&&r.value("phase")!="creating"&&r.value("phase")!="accepting");
    if((r.isEmpty()||replace)&&op=="offer"&&roomHosts_.value(room)==peer&&roomMembers_.value(room).contains(self_)&&roomMembers_.value(room).contains(peer)&&records_.size()<256) {
        put(peer,{{"lease",lease},{"room",room},{"host",false},{"phase","baseline"}});return;
    }
    if(r.isEmpty()||r.value("lease")!=lease||r.value("room")!=room)return;
    const bool host=r.value("host").toBool();const auto phase=r.value("phase").toString();
    if(op=="keep"&&host) {if(phase!="deleting") {r["phase"]="kept";r.remove("error");if(put(peer,r))send(peer,"kept");}return;}
    if(op=="kept"&&!host) {r["phase"]="kept";r.remove("error");if(put(peer,r))send(peer,"kept-ok");return;}
    if(op=="kept-ok"&&host&&phase=="kept") {r["acknowledged"]=true;put(peer,r);return;}
    if(op=="release"&&!host) {release(peer);return;}
    if(op=="remove-ok"&&host&&phase=="release") {r["phase"]="cleanup";put(peer,r);next_[peer]=now_;return;}
    if(op=="gone"&&!host&&(phase=="released"||phase=="done")) {r["phase"]="done";if(put(peer,r))send(peer,"gone-ok");return;}
    if(op=="gone-ok"&&host&&phase=="gone") {put(peer,{});return;}
    if(op=="blocked"&&(phase=="offer"||phase=="baseline"||phase=="agree")) {r["phase"]="blocked";r["error"]="Friendship permissions need attention.";put(peer,r);return;}
    if(op=="agree"&&host&&phase=="offer"&&active(peer)) {r["phase"]="create";put(peer,r);next_[peer]=now_;return;}
    if(op=="requested"&&!host&&phase=="agree"&&active(peer)) {r["phase"]="accept";put(peer,r);next_[peer]=now_;return;}
    if(op=="accepted"&&host&&(phase=="wait"||phase=="friend")&&active(peer)) {
        if(phase=="friend")send(peer,"complete");else {r["phase"]="confirm";put(peer,r);next_[peer]=now_;}return;
    }
    if(op=="complete"&&!host&&phase=="accepted") {r["phase"]="friend";put(peer,r);return;}
    if(op=="offer"&&!host&&phase=="agree")send(peer,"agree");
}
void TemporaryContacts::request(const QString& peer,QString phase,QByteArray method,QString path,QJsonObject body,Completion completion) {
    if(!request_)return;
    busy_=peer;deadline_=now_+45;const auto epoch=epoch_,flight=++flight_;
    const auto lease=records_.value(peer).toObject().value("lease").toString();
    QPointer<TemporaryContacts> guard(this);
    request_(method,path,body,[guard,epoch,flight,peer,phase,lease,completion](Reply reply) {
        if(!guard||guard->epoch_!=epoch||guard->flight_!=flight)return;
        guard->busy_.clear();const auto current=guard->records_.value(peer).toObject();
        if(current.value("lease")!=lease||current.value("phase")!=phase)return;
        completion(reply);
    });
}
void TemporaryContacts::read(const QString& peer,QString phase) {
    request(peer,phase,"GET","/v1/users/@me/relationships",{},[this,peer,phase](Reply reply) {
        if(reply.status!=200||!reply.body.isArray()) {next_[peer]=now_+qMax(15,reply.retrySeconds);return;}
        QJsonObject relation;for(const auto& v:reply.body.array())if(v.toObject()["id"]==peer)relation=v.toObject();
        emit observed(peer,relation);
        auto r=records_.value(peer).toObject();if(r.value("phase")!=phase||r.value("releaseRequested").toBool())return;
        const int type=relation.value("type").toInt();
        if(phase=="baseline") {
            if(type!=0) {
                r["phase"]=type==1?"exempt":"blocked";
                if(type!=1)r["error"]="An existing request or block needs your decision.";
                if(put(peer,r)&&!r.value("host").toBool())send(peer,"blocked");
                return;
            }
            r["phase"]=r.value("host").toBool()?"offer":"agree";if(put(peer,r))send(peer,r.value("host").toBool()?"offer":"agree");return;
        }
        if(phase=="create") {
            if(!active(peer))return;
            if(type!=0) {fail(peer,"Friendship changed; automatic request stopped.");return;}
            if(now_<nextCreate_) {next_[peer]=nextCreate_;return;}
            nextCreate_=now_+7;mutate(peer,"creating","POST",{},4);return;
        }
        if(phase=="accept") {
            if(!active(peer))return;
            if(type==0) {next_[peer]=now_+5;return;}
            if(!stamped(relation,3)) {fail(peer,"Friendship changed; automatic acceptance stopped.");return;}
            mutate(peer,"accepting","PUT",{{"type",1}},1);return;
        }
        if(phase=="confirm") {
            if(!stamped(relation,1)) {fail(peer,"Couldn't verify the temporary friendship.");return;}
            r["type"]=1;r["since"]=relation.value("since");r["phase"]="friend";if(put(peer,r))send(peer,"complete");return;
        }
        if(phase=="cleanup") {
            if(held_.contains(peer)) {next_[peer]=now_+15;return;}
            if(type==0) {r["phase"]="gone";if(put(peer,r))send(peer,"gone");return;}
            if((r.value("type")!=1&&r.value("type")!=4)||!stamped(relation,r.value("type").toInt())||relation.value("since")!=r.value("since")) {
                r["phase"]="uncertain";r["error"]="Friendship changed; it was kept. Review it in the profile.";put(peer,r);send(peer,"kept");return;
            }
            mutate(peer,"deleting","DELETE",{},0);
        }
    });
}
void TemporaryContacts::mutate(const QString& peer,QString phase,QByteArray method,QJsonObject body,int expectedType) {
    auto r=records_.value(peer).toObject();r["phase"]=phase;if(!put(peer,r))return;
    request(peer,phase,method,"/v1/users/@me/relationships/"+peer,body,[this,peer,phase,expectedType](Reply reply) {
        auto r=records_.value(peer).toObject();
        if(phase=="deleting") {
            if((reply.status>=200&&reply.status<300)||reply.status==404) {r["phase"]="gone";if(put(peer,r)){emit observed(peer,{});send(peer,"gone");}return;}
            r["phase"]="cleanup";put(peer,r);next_[peer]=now_+qMax(15,reply.retrySeconds);return;
        }
        const auto relation=reply.body.object();
        if(reply.status==429) {r["phase"]=phase=="creating"?"create":"accept";put(peer,r);next_[peer]=now_+qMax(15,reply.retrySeconds);return;}
        if(reply.status<200||reply.status>=300||relation.value("id")!=peer||!stamped(relation,expectedType)) {
            r["phase"]=(reply.status==0||reply.status>=500||(reply.status>=200&&reply.status<300))?"uncertain":"error";
            r["error"]="Couldn't confirm temporary friendship. Review permissions or open the profile.";if(put(peer,r))send(peer,"blocked");return;
        }
        r["type"]=expectedType;r["since"]=relation.value("since");r["phase"]=phase=="creating"?"wait":"accepted";
        if(put(peer,r)) {emit observed(peer,relation);send(peer,phase=="creating"?"requested":"accepted");}
    });
}
void TemporaryContacts::tick(qint64 now) {
    now_=now;if(!online_||recovering_||!storageError_.isEmpty()||file_.isEmpty()||!request_)return;
    if(!busy_.isEmpty()) {
        if(now_<deadline_)return;
        const auto peer=busy_;busy_.clear();++flight_;auto r=records_.value(peer).toObject();const auto phase=r.value("phase").toString();
        if(phase=="creating"||phase=="accepting") {r["phase"]="uncertain";r["error"]="Check this friendship after the interrupted request.";put(peer,r);}
        if(phase=="deleting") {r["phase"]="cleanup";put(peer,r);}
        next_[peer]=now_+15;
    }
    for(auto rooms=roomMembers_.cbegin();rooms!=roomMembers_.cend();++rooms)if(roomHosts_.value(rooms.key())==self_)
    for(const auto& peer:rooms.value())if(peer!=self_&&records_.size()<256) {
        const auto& room_=rooms.key();
        const auto r=records_.value(peer).toObject();const auto phase=r.value("phase").toString();
        const bool replace=r.value("room")!=room_&&(phase=="gone"||phase=="done"||((phase=="release"||phase=="blocked"||phase=="error")&&!r.contains("type")));
        if(r.isEmpty()||replace)put(peer,{{"room",room_},{"lease",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"host",true},{"phase","baseline"}});
    }
    const auto peers=records_.keys();
    for(const auto& peer:peers) {
        if(now_<next_.value(peer))continue;
        auto r=records_.value(peer).toObject();auto phase=r.value("phase").toString();const bool host=r.value("host").toBool();
        if((phase=="done"||phase=="gone"||phase=="kept")&&!active(peer)&&r.value("ended").toInteger()>0&&now_-r.value("ended").toInteger()>86400) {put(peer,{});continue;}
        if(phase=="done")continue;
        if(phase=="gone") {send(peer,"gone");continue;}
        if(held_.contains(peer)&&!active(peer))continue;
        if(!host&&r.value("releaseRequested").toBool()) {release(peer);continue;}
        if(phase=="kept") {
            if(!active(peer)&&host&&r.value("acknowledged").toBool()) {put(peer,{});continue;}
            if(host&&!r.value("acknowledged").toBool())send(peer,"kept");
            continue;
        }
        if(phase=="keep") {send(peer,"keep");continue;}
        if(phase=="exempt") {if(!active(peer))put(peer,{});continue;}
        if(phase=="blocked"||phase=="error"||phase=="uncertain")continue;
        if(host&&!active(peer)&&phase!="release"&&phase!="cleanup") {r["phase"]="release";if(!put(peer,r))return;phase="release";}
        if(!host&&!active(peer)&&phase!="released")continue; // Requester remains cleanup authority.
        if(phase=="release") {send(peer,"release");continue;}
        if(phase=="released") {send(peer,"remove-ok");continue;}
        if(phase=="offer"||phase=="agree") {send(peer,phase);continue;}
        if(phase=="wait") {send(peer,"requested");continue;}
        if(phase=="accepted") {send(peer,"accepted");continue;}
        if(phase=="baseline"||phase=="create"||phase=="accept"||phase=="confirm"||phase=="cleanup") {read(peer,phase);return;}
    }
}
}
