#include "SocialController.h"
#include "core/model/GamePlayers.h"
#include <QCryptographicHash>

namespace trainer {
void SocialController::configureLocalLobby(QString directory,QString name) {
    if(owner_.isEmpty()||syncingLocalLobby_)return;
    directory+="/local-lobby/"+QString::fromLatin1(QCryptographicHash::hash(owner_.toUtf8(),QCryptographicHash::Sha256).toHex());
    if(localLobbyDirectory_!=directory) {
        syncingLocalLobby_=true;
        localLobby_.close();lobby_.configureLocal({}, {},false);
        localLobbyDirectory_=directory;localLobby_.configure(directory,name);
        lobby_.configureLocal(localLobby_.identity(),localLobby_.name(),false);
        if(!localLobby_.identity().isEmpty())lobby_.setLocalStorage(directory+"/lobby.json");
        syncingLocalLobby_=false;
    } else localLobby_.setName(name);
    syncLocalLobby();
}
void SocialController::setLobbyScope(QString scope) {
    if(scope!="online"&&scope!="local")return;
    lobbyScope_=scope;lobbySelection_.clear();lobbyLastRoom_.clear();lobbyDetailFocused_=false;
    syncLocalLobby();emit changed();
}
QVariantMap SocialController::lobbyDirectory() const {
    if(lobbyScope_!="local")return snapshot_["lobbyDirectory"].toMap();
    return {{"rows",localLobby_.rooms()},{"state",localLobby_.running()?"ready":"unavailable"},
        {"status",localLobby_.error()},{"error",localLobby_.error()}};
}
void SocialController::syncLocalLobby() {
    if(syncingLocalLobby_)return;syncingLocalLobby_=true;
    bool needed=(face_=="lobby"&&lobbyScope_=="local")||(lobbyCreateOpen_&&lobbyDraft_["type"]=="local");
    QJsonArray announcements;
    for(auto* room:lobby_.rooms())if(room->local()&&!room->state().isEmpty()) {
        needed=true;if(!room->hosting())continue;
        const auto s=room->state(),g=s["game"].toObject();int used=0;
        for(const auto& m:s["members"].toArray())if(m.toObject()["seat"].toInt()>0)++used;
        announcements.append(QJsonObject{{"lobby",s["id"]},{"title",s["title"]},{"topic",room->publicationTopic()},
            {"status",s["phase"]},{"members",s["members"].toArray().size()},{"seats",g["seats"]},
            {"available",qMax(0,g["seats"].toInt()-used)},
            {"game",QJsonObject{{"title",g["title"]},{"platform",g["platform"]},{"mode",g["mode"]}}}});
    }
    if(needed&&!owner_.isEmpty()) {localLobby_.open();localLobby_.setRooms(announcements);}
    else if(localLobby_.running())localLobby_.close(true);
    lobby_.configureLocal(localLobby_.identity(),localLobby_.name(),localLobby_.running());
    syncingLocalLobby_=false;
}
QVariantList SocialController::availableLobbyGames() const {
    const bool local=lobbyCreateOpen_?lobbyDraft_["type"]=="local":viewedLobby().local();
    QVariantList result;
    for(const auto& value:lobbyGames_) {
        const auto g=value.toMap();
        const auto profile=QJsonObject::fromVariantMap(g["runtime"].toMap());
        if(local?localLobbyRuntime(profile):(!profile.contains("connections")||multiplayerConnectionAllowed(profile,"online")))result.append(value);
    }
    return result;
}
bool SocialController::localPartyAllowed(QString peer,const QJsonObject& packet) const {
    const auto* room=lobby_.activeRoom();const auto s=room->state();
    if(!room->local()||packet["ns"]!="org.traineros.party"||s["ticket"].toString().isEmpty()
       ||packet["party"]!=s["ticket"]||(s["phase"]!="launching"&&s["phase"]!="playing")
       ||(peer!=s["owner"].toString()&&!room->hosting()))return false;
    for(const auto& value:s["members"].toArray()) {
        const auto m=value.toObject();if(m["id"]==peer)return m["connected"].toBool()&&!m["spectator"].toBool()&&m["seat"].toInt()>0;
    }
    return false;
}
bool SocialController::sendLocalParty(QString peer,QJsonObject packet) {
    return localPartyAllowed(peer,packet)&&localLobby_.send(peer,packet);
}
}
