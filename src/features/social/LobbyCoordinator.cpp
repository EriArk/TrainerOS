#include "LobbyCoordinator.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QUuid>

namespace trainer {
LobbyCoordinator::LobbyCoordinator(QObject* parent):QObject(parent) {
    active_=appendRoom();active_->allowRuntime(true);
}
LobbyRoomCoordinator* LobbyCoordinator::appendRoom() {
    auto* r=new LobbyRoomCoordinator(this);rooms_.append(r);r->allowRuntime(false);
    connect(r,&LobbyRoomCoordinator::changed,this,[this,r]{
        if(loading_||!active_)return;
        if(r==active_)emit changed();
        if(!loading_&&!file_.isEmpty()&&!r->state()["id"].toString().isEmpty())
            r->setStorage(file_+".rooms/"+r->state()["id"].toString()+".json");
        saveIndex();emit roomsChanged();
    });
    connect(r,&LobbyRoomCoordinator::outgoing,this,&LobbyCoordinator::outgoing);
    connect(r,&LobbyRoomCoordinator::notice,this,&LobbyCoordinator::notice);
    connect(r,&LobbyRoomCoordinator::membershipEnded,this,&LobbyCoordinator::membershipEnded);
    connect(r,&LobbyRoomCoordinator::providerMembershipEnded,this,[this](QString id){departedRooms_.insert(id);});
    connect(r,&LobbyRoomCoordinator::launchRequested,this,[this,r](QString t,QJsonObject s){if(r==active_)emit launchRequested(t,s);});
    connect(r,&LobbyRoomCoordinator::liveJoinRequested,this,[this,r](QString p,QString op,int seat){if(r==active_)emit liveJoinRequested(p,op,seat);else r->rejectLive(p,op);});
    connect(r,&LobbyRoomCoordinator::liveLaunchRequested,this,[this,r](QString op,QString t,QJsonObject s){if(r==active_)emit liveLaunchRequested(op,t,s);});
    connect(r,&LobbyRoomCoordinator::liveJoinFailed,this,[this,r](QString op){if(r==active_)emit liveJoinFailed(op);});
    connect(r,&LobbyRoomCoordinator::liveReleased,this,[this,r](QString p,QString op){if(r==active_)emit liveReleased(p,op);});
    connect(r,&LobbyRoomCoordinator::runtimeSignal,this,[this,r](QString p,QString t,QString c,QJsonObject v){if(r==active_)emit runtimeSignal(p,t,c,v);});
    r->configure(self_,name_,online_);return r;
}
LobbyRoomCoordinator* LobbyCoordinator::room(const QString& id) const {
    if(id.isEmpty())return nullptr;
    for(auto* r:rooms_)if(r->state()["id"]==id||r->invitation()["lobby"]==id)return r;
    return nullptr;
}
void LobbyCoordinator::configure(QString self,QString name,bool online) {
    if(self_==self&&name_==name&&online_==online)return;
    if(self_!=self) {
        loading_=true;file_.clear();indexError_=false;
        for(auto* r:rooms_){r->setStorage({});r->configure({}, {},false);r->disconnect(this);r->deleteLater();}
        rooms_.clear();departedRooms_.clear();active_=nullptr;self_=self;name_=name;online_=online;
        active_=appendRoom();active_->allowRuntime(true);loading_=false;
    } else {name_=name;online_=online;for(auto* r:rooms_)r->configure(self,name,online);}
    emit changed();emit roomsChanged();
}
LobbyRoomCoordinator* LobbyCoordinator::createRoom(QString access) {
    if(!online_)return nullptr;
    LobbyRoomCoordinator* r=nullptr;
    for(auto* candidate:rooms_)if(candidate->state().isEmpty()&&candidate->invitation().isEmpty()){r=candidate;break;}
    if(!r){if(rooms_.size()>=150)return nullptr;r=appendRoom();}
    return r->create(access)?r:nullptr;
}
bool LobbyCoordinator::create(QString access) {
    auto* r=createRoom(access);return r&&activateRoom(r->state()["id"].toString());
}
bool LobbyCoordinator::activateRoom(const QString& id) {
    auto* r=room(id);if(!r||r->state().isEmpty())return false;if(r==active_)return true;
    const auto s=active_->state();
    if(s["phase"]=="playing"||s["phase"]=="launching")return false;
    for(const auto& v:s["members"].toArray())if(v.toObject()["id"]==self_&&v.toObject()["ready"].toBool())return false;
    active_->allowRuntime(false);active_=r;r->allowRuntime(true);saveIndex();emit changed();emit roomsChanged();return true;
}
bool LobbyCoordinator::join(const QString& host,const QString& id,bool request) {
    auto* r=room(id);if(!r){if(rooms_.size()>=150)return false;r=active_->state().isEmpty()?active_:appendRoom();}
    const bool ok=r->join(host,id,request);if(ok&&active_->state().isEmpty())activateRoom(id);return ok;
}
QJsonObject LobbyCoordinator::invitation() const {
    for(auto* r:rooms_)if(!r->invitation().isEmpty())return r->invitation();return {};
}
void LobbyCoordinator::answer(bool accept) {
    for(auto* r:rooms_)if(!r->invitation().isEmpty()){r->answer(accept);if(accept&&active_->state().isEmpty())activateRoom(r->state()["id"].toString());emit changed();return;}
}
void LobbyCoordinator::receive(QString peer,QString name,const QJsonObject& packet) {
    const auto id=packet["lobby"].toString();auto* r=room(id);
    if(!r&&departedRooms_.contains(id)&&packet["kind"]!="invite")return;
    if(!r&&packet["kind"]=="invite"&&!QUuid(id).isNull()&&rooms_.size()<150)r=active_->state().isEmpty()&&active_->invitation().isEmpty()?active_:appendRoom();
    if(r){r->receive(peer,name,packet);if(!r->invitation().isEmpty())emit changed();}
    else active_->receive(peer,name,packet);
}
void LobbyCoordinator::deliveryFailed(const QString& peer){for(auto* r:rooms_)r->deliveryFailed(peer);}
void LobbyCoordinator::setStorage(QString file) {
    if(file_==file)return;file_=file;
    if(file.isEmpty()){for(auto* r:rooms_)r->setStorage({});return;}
    loading_=true;indexError_=false;
    // Retain the legacy single-room checkpoint as a migration source.
    QFile index(file+".index");
    if(!index.exists())active_->setStorage(file);
    else if(!QFileInfo(index).isSymLink()&&index.size()<65536&&index.open(QIODevice::ReadOnly)) {
        const auto data=QJsonDocument::fromJson(index.readAll()).object();index.close();
        if(data["self"]==self_&&data["v"]==1&&data["rooms"].isArray()&&data["rooms"].toArray().size()<=150) {
            for(const auto& v:data["rooms"].toArray()) {
                const auto id=v.toString();if(QUuid(id).isNull()){indexError_=true;continue;}
                if(room(id)||rooms_.size()>=151)continue;
                auto* r=appendRoom();r->setStorage(file+".rooms/"+id+".json");
                if(r->state()["id"]!=id){r->setStorage({});r->forgetMembership();indexError_=true;}
            }
            if(auto* r=room(data["active"].toString())){active_->allowRuntime(false);active_=r;active_->allowRuntime(true);}
        } else indexError_=true;
    } else indexError_=true;
    for(auto* r:rooms_)if(!r->state()["id"].toString().isEmpty())r->setStorage(file+".rooms/"+r->state()["id"].toString()+".json");
    loading_=false;if(indexError_)emit notice("Lobby recovery is unavailable. Existing history was kept.");
    saveIndex();emit changed();emit roomsChanged();
}
void LobbyCoordinator::saveIndex() {
    if(loading_||indexError_||!active_||file_.isEmpty()||self_.isEmpty())return;
    QJsonArray ids;for(auto* r:rooms_)if(!r->state()["id"].toString().isEmpty()&&r->state()["phase"]!="joining")ids.append(r->state()["id"]);
    QSaveFile f(file_+".index");if(QFileInfo(f.fileName()).isSymLink()||!QDir().mkpath(QFileInfo(file_).absolutePath())||!f.open(QIODevice::WriteOnly))return;
    f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    f.write(QJsonDocument(QJsonObject{{"v",1},{"self",self_},{"active",active_->state()["id"]},{"rooms",ids}}).toJson());
    if(!f.commit())emit notice("Couldn't save the lobby list. Keep TrainerOS open and retry.");
}
}
