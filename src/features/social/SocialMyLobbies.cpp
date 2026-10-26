#include "SocialController.h"
#include <QDateTime>

namespace trainer {
LobbyRoomCoordinator& SocialController::viewedLobby() const {
    if(auto* r=lobby_.room(viewedLobbyId_))return *r;
    if(!lobby_.activeRoom()->state().isEmpty())return *lobby_.activeRoom();
    for(auto* r:lobby_.rooms())if(!r->state().isEmpty())return *r;
    return *lobby_.activeRoom();
}
QVariantList SocialController::myLobbies() const {
    QVariantList result;
    for(auto* r:lobby_.rooms()) {
        const auto s=r->state();if(s.isEmpty()||s["phase"]=="joining")continue;
        auto row=s.toVariantMap();QVariantList people;QString host;
        for(const auto& v:s["members"].toArray()) {
            auto person=lobbyPerson(v.toObject().toVariantMap());person["host"]=person["id"].toString()==s["owner"].toString();
            if(person["host"].toBool())host=person["name"].toString();people.append(person);
        }
        row["people"]=people;row["hostName"]=host;row["title"]=s["title"].toString().isEmpty()?host+"'s lobby":s["title"].toString();row["selected"]=s["id"]==viewedLobby().state()["id"];
        row["gameTitle"]=s["game"].toObject()["title"].toString();
        row["modeLabel"]=s["game"].toObject()["mode"]=="remoteplay.v1"?"Remote Play":"Netplay";
        for(const auto& g:lobbyGames_)if(g.toMap()["mode"]==s["game"].toObject()["mode"].toVariant())row["modeLabel"]=g.toMap().value("modeLabel",row["modeLabel"]);
        int occupied=0;for(const auto& m:s["members"].toArray())if(m.toObject()["seat"].toInt()>0)++occupied;
        row["occupied"]=occupied;
        row["active"]=r==lobby_.activeRoom();result.append(row);
    }
    return result;
}
QVariantMap SocialController::homeLobby() const {
    const auto rows=myLobbies();if(rows.isEmpty())return {};
    auto result=rows.first().toMap();for(const auto& v:rows)if(v.toMap()["selected"].toBool())result=v.toMap();
    result["count"]=rows.size();return result;
}
void SocialController::showMyLobbies(){closeMenu();myLobbiesOpen_=true;myLobbiesIndex_=0;myLobbyFocus_=myLobbies().value(0).toMap()["id"].toString();emit changed();}
void SocialController::closeMyLobbies(){myLobbiesOpen_=false;emit changed();}
void SocialController::chooseMyLobby(QString id) {
    auto* r=lobby_.room(id);if(!r||r->state().isEmpty())return;
    viewedLobbyId_=id;myLobbiesOpen_=false;lobbyRoomFocus_="game";openLobby();syncLobbyChat();emit changed();
}
void SocialController::requestLobbyReady() {
    auto& room=viewedLobby();const auto id=room.state()["id"].toString();
    if(room.readyPending())return;
    if(&room!=lobby_.activeRoom()) {
        if(gameActive_||lobby_.state()["phase"]=="launching"||lobby_.state()["phase"]=="playing"){toastTitle_="Lobby";toastText_="Finish your current game before getting ready here.";toastTimer_.start();emit presentationChanged();return;}
        if(!lobby_.activateRoom(id)) {
            menuMode_="lobby";menuTitle_="Prepare this room?";menuDetail_="Your other room will see you as Preparing.";
            menu_={"Prepare here","Back"};menuCommands_={"lobby:ready-switch:"+id,"cancel"};menuFocus_=0;emit changed();return;
        }
    }
    for(const auto& v:room.state()["members"].toArray())if(v.toObject()["id"]==viewedLobby().self()) {
        const auto m=v.toObject();if(m["ready"].toBool()){room.ready(false);return;}
        if(m["prepared"].toBool()){room.ready(true);return;}
    }
    lobbyAutoPreparation_.clear();reconcileLobbyPreparation();
}
void SocialController::leaveViewedLobby(bool close) {
    auto& room=viewedLobby();const auto s=room.state();const auto id=s["id"].toString(),channel=s["conversation"].toString();
    if(id.isEmpty()||(!room.local()&&(lobbyLeaving_.contains(id)||snapshot_["mutationBusy"].toBool()||lobbyChatPending_.contains(id))))return;
    if(&room==lobby_.activeRoom()&&(s["phase"]=="launching"||s["phase"]=="playing")) {
        toastTitle_="Lobby";toastText_="Finish the current game before leaving its lobby.";toastTimer_.start();emit presentationChanged();return;
    }
    QString successor;
    for(const auto& v:s["members"].toArray()) {
        const auto m=v.toObject();
        if(m["id"]!=viewedLobby().self()&&!m["spectator"].toBool()) {successor=m["id"].toString();break;}
    }
    if(room.local()){room.leave();emit changed();return;}
    if(channel.isEmpty()) {if(room.hosting()&&successor.isEmpty())room.leave();else room.forgetMembership();emit changed();return;}
    if(room.hosting()&&(close||successor.isEmpty())) {
        lobbyCloseOnLeave_.insert(id);lobbyLeaving_.insert(id);leavingLobby_=id;
        emit commandRequested("lobby-chat-close",{{"lobby",id},{"channel",channel},{"close",true}});emit changed();return;
    }
    if(room.hosting()&&successor.isEmpty())lobbyCloseOnLeave_.insert(id);
    lobbyLeaving_.insert(id);leavingLobby_=id;
    emit commandRequested("leave-group",{{"channel",channel},{"successor",room.hosting()?successor:QString()}});emit changed();
}
void SocialController::syncMyLobbies() {
    if(snapshot_["state"]!="connected")return;
    const auto chats=snapshot_["chats"].toList();
    for(auto* r:lobby_.rooms()) {
        if(r->local())continue;
        const auto s=r->state();const auto id=s["id"].toString(),channel=s["conversation"].toString();
        lobbyReminders_.observe(s);recentTogether_.observe(s,QDateTime::currentSecsSinceEpoch());
        if(id.isEmpty()||s["phase"]=="joining"||lobbyLeaving_.contains(id))continue;
        QString provider;
        for(const auto& v:chats) {
            const auto chat=v.toMap();if(chat["id"]!=channel||chat["kind"]!="groups")continue;
            auto members=QJsonArray::fromVariantList(chat["members"].toList());
            members.append(QJsonObject{{"id",r->self()},{"name",snapshot_["name"].toString()}});
            QStringList ids;for(const auto& m:members)ids.append(m.toObject()["id"].toString());ids.sort();provider=chat["owner"].toString()+":"+ids.join(',');
            r->reconcileProvider(channel,chat["owner"].toString(),members);break;
        }
        QStringList peers;for(const auto& v:r->state()["members"].toArray())if(v.toObject()["id"]!=r->self())peers.append(v.toObject()["id"].toString());
        peers.sort();const auto key=channel+"/"+peers.join(',')+"/"+provider;
        if((!r->hosting()&&channel.isEmpty())||lobbyChatPending_.contains(id)||lobbyChatKeys_.value(id)==key)continue;
        lobbyChatKeys_[id]=key;lobbyChatPending_.insert(id);
        emit commandRequested("lobby-chat",{{"lobby",id},{"channel",channel},{"host",r->hosting()},{"peers",r->hosting()?peers:QStringList{}}});
    }
}
}
