#include "SocialController.h"
#include <QJsonDocument>
#include <QDateTime>
#include <algorithm>

namespace trainer {
QVariantList SocialController::lobbyRoomMessages() const {
    QSet<QString> spectators;for(const auto& v:lobbySpectators())spectators.insert(v.toMap()["id"].toString());
    QVariantList result;const auto all=messages();
    for(int i=0;i<all.size();++i){auto message=all[i].toMap();if(spectators.contains(message["author"].toString())!=lobbySpectatorChat_)continue;
        message["sourceIndex"]=i;result.append(message);}
    return result;
}
void SocialController::toggleLobbySpectatorChat() {
    if(!lobbyChatVisible())return;
    lobbySpectatorChat_=!lobbySpectatorChat_;lobbyChatFocused_=true;reading_=true;
    const auto rows=lobbyRoomMessages();if(!rows.isEmpty())messageFocus_=rows.last().toMap()["sourceIndex"].toInt();
    emit changed();
}
QVariantMap SocialController::lobbyPerson(QVariantMap member) const {
    const auto id=member["id"].toString(),self=snapshot_["userId"].toString();
    QString avatar;bool blocked=false;
    if(id==self)avatar=snapshot_["profile"].toMap()["avatar"].toString();
    for(const auto& value:snapshot_["friends"].toList()) {
        const auto person=value.toMap();if(person["id"]!=id)continue;
        blocked=person["type"].toInt()==2;if(avatar.isEmpty())avatar=person["avatar"].toString();
    }
    for(const auto& value:snapshot_["chats"].toList())for(const auto& v:value.toMap()["members"].toList()) {
        const auto person=v.toMap();if(person["id"]==id&&avatar.isEmpty())avatar=person["avatar"].toString();
    }
    if(avatar.isEmpty())avatar=directConversation(id)["avatar"].toString();
    const auto voice=snapshot_["voice"].toMap();
    member["avatar"]=blocked?QString():avatar;member["self"]=id==self;
    member["host"]=id==viewedLobby().state()["owner"].toString();member["key"]="person:"+id;
    member["speaking"]=!blocked&&member["connected"].toBool()&&voice["state"]=="connected"
        &&voice["channel"].toString()==lobbyChatChannel_&&!lobbyChatChannel_.isEmpty()&&voice["speakers"].toStringList().contains(id);
    member["status"]=!member["connected"].toBool()?"Reconnecting":member["returnSeat"].toInt()>0?"Can return to P"+QString::number(member["returnSeat"].toInt()):member["spectator"].toBool()?"Spectator":member["ready"].toBool()?"Ready":"Preparing";
    member["technical"]=member["prepared"].toBool()?"verified":QString();
    if(id==self&& &viewedLobby()==lobby_.activeRoom()&&!member["spectator"].toBool())
        member["technical"]=lobbyRuntime_.busy()?"checking":!lobbyRuntime_.problem().isEmpty()?"attention":member["technical"];
    return member;
}
QVariantList SocialController::lobbyPlayers() const {
    QVariantList result,unseated;const auto s=viewedLobby().state();const int seats=s["game"].toObject()["seats"].toInt();
    for(int seat=1;seat<=seats;++seat)result.append(QVariantMap{{"key","seat:"+QString::number(seat)},{"seat",seat},{"empty",true},{"name","Invite"}});
    for(const auto& v:s["members"].toArray()) {
        auto member=lobbyPerson(v.toObject().toVariantMap());const int returning=member["returnSeat"].toInt();
        if(member["spectator"].toBool()&&!returning)continue;
        const int seat=returning?returning:member["seat"].toInt();
        if(returning)member["seat"]=returning; // Presentation only: the native port is still disconnected.
        if(seat>0&&seat<=result.size())result[seat-1]=member;else unseated.append(member);
    }
    std::stable_sort(unseated.begin(),unseated.end(),[](const QVariant& a,const QVariant& b){return a.toMap()["host"].toBool()&&!b.toMap()["host"].toBool();});
    result+=unseated;
    for(const auto& id:viewedLobby().invitedPeers(QDateTime::currentSecsSinceEpoch())) {
        QVariantMap person{{"id",id},{"name","Invited friend"},{"connected",false}};
        for(const auto& v:snapshot_["friends"].toList())if(v.toMap()["id"]==id)person["name"]=v.toMap()["name"];
        person=lobbyPerson(person);person["key"]="invited:"+id;person["invited"]=true;person["status"]="Invited";result.append(person);
    }
    if(!seats&&viewedLobby().hosting())result.append(QVariantMap{{"key","invite"},{"empty",true},{"name","Invite friends"}});
    return result;
}
QVariantList SocialController::lobbySpectators() const {
    QVariantList result;for(const auto& v:viewedLobby().state()["members"].toArray())if(v.toObject()["spectator"].toBool()&&!v.toObject()["returnSeat"].toInt())result.append(lobbyPerson(v.toObject().toVariantMap()));return result;
}
QVariantMap SocialController::lobbyRoomGame() const {
    auto game=viewedLobby().state()["game"].toObject().toVariantMap();
    game["canStart"]=&viewedLobby()==lobby_.activeRoom()&&viewedLobby().canStart();
    for(const auto& v:lobbyGames_) {const auto local=v.toMap();if(local["id"]==game["id"]&&local["mode"]==game["mode"]) {
        game["cover"]=local["cover"];game["modeLabel"]=local.value("modeLabel",QString("Emulator multiplayer"));break;
    }}
    if(!game.contains("modeLabel"))game["modeLabel"]=game["mode"]=="remoteplay.v1"?"Remote Play":"Emulator multiplayer";
    QString status=&viewedLobby()==lobby_.activeRoom()?lobbyRuntime_.status():QString("Preparing");
    if(status.isEmpty()&&!game["id"].toString().isEmpty()) {
        const auto state=viewedLobby().state();
        status=state["phase"]=="playing"?"Playing together":gameActive_?"Save your current game, then continue setup":
            lobbyRuntime_.available()?"Checking this game…":"Waiting for the players";
        for(const auto& v:state["members"].toArray()) {const auto m=v.toObject();if(m["id"]!=snapshot_["userId"].toString()||m["spectator"].toBool())continue;
            if(!m["connected"].toBool())status="Reconnecting…";
            else if(!m["seat"].toInt())status="Choose a free controller from your avatar";
            else if(!gameActive_&&!lobbyRuntime_.available()&&state["phase"]=="gathering")status="Game setup is unavailable on this device";
        }
    }
    game["status"]=status;
    game["canImport"]=&viewedLobby()==lobby_.activeRoom()&&lobbyRuntime_.problem()=="local-content"&&lobbyRuntime_.available();
    return game;
}
QVariantList SocialController::lobbyRoomActions() const {
    QVariantList result;const auto add=[&](QString key,QString label,bool enabled=true){result.append(QVariantMap{{"key","action:"+key},{"command",key},{"label",label},{"enabled",enabled},{"busy",key=="ready"&&viewedLobby().readyPending()}});};
    const auto state=viewedLobby().state();if(state.isEmpty())return result;
    const auto voice=snapshot_["voice"].toMap();
    if(!voice["channel"].toString().isEmpty()) {
        add("microphone",voice["muted"].toBool()?"Mic off":"Mic on");add("audio",voice["deaf"].toBool()?"Sound off":"Sound on");
    } else add("call","Join voice",!lobbyChatChannel_.isEmpty()&&voice["available"].toBool());
    for(const auto& v:lobbyActions()) {const auto action=v.toMap();const auto id=action["id"].toString();
        if(id=="vote"||id=="ready"||id=="join-live"||id=="request-play"||id=="spectate")add(id,action["label"].toString(),action["enabled"].toBool());
        if(id=="prepare"&& &viewedLobby()==lobby_.activeRoom())add("prepare",gameActive_?"Continue setup":lobbyRuntime_.busy()?"Preparing…":!lobbyRuntime_.available()?"Unavailable":lobbyAutoPreparation_.isEmpty()?"Preparing…":"Retry setup",
            action["enabled"].toBool()&&(gameActive_||!lobbyAutoPreparation_.isEmpty()));
    }
    return result;
}
QString SocialController::lobbyRoomFocus() const {
    if(lobbyRoomFocus_=="start"&&lobbyRoomGame()["canStart"].toBool())return "start";
    if(lobbyRoomFocus_=="game")return lobbyRoomFocus_;
    for(const auto& rows:{lobbyPlayers(),lobbyRoomActions()})for(const auto& v:rows)if(v.toMap()["key"]==lobbyRoomFocus_)return lobbyRoomFocus_;
    return "game";
}
void SocialController::expandLobbyChat() {
    if(!lobbyChatVisible())return;
    lobbyChatExpanded_=!lobbyChatExpanded_;lobbyChatFocused_=true;reading_=true;emit changed();
}
void SocialController::activateLobbyRoom(QString key) {
    if(!lobbyDetailFocused_||viewedLobby().state().isEmpty())return;
    bool found=key=="game"||(key=="start"&&lobbyRoomGame()["canStart"].toBool());for(const auto& rows:{lobbyPlayers(),lobbyRoomActions()})for(const auto& v:rows)
        if(v.toMap()["key"]==key)found=true;
    if(!found)return;
    lobbyRoomFocus_=key;lobbyChatFocused_=false;emit changed();
    if(key=="start"){viewedLobby().start();return;}
    if(key=="game"){if(lobbyRoomGame()["canImport"].toBool())emit lobbyImportRequested();else if(viewedLobby().hosting())lobbyAction("game");return;}
    if(key=="invite"){if(viewedLobby().hosting())lobbyAction("invite");return;}
    if(key.startsWith("invited:")){profile(key.mid(8));return;}
    if(key=="action:microphone"||key=="action:audio") {
        if(!snapshot_["voice"].toMap()["channel"].toString().isEmpty())emit commandRequested(key=="action:microphone"?"voice-mute":"voice-output",{});return;
    }
    if(key.startsWith("person:")||key.startsWith("seat:")) {
        menuMode_="lobby";menuFocus_=0;menu_.clear();menuCommands_.clear();menuDetail_.clear();
        const auto add=[&](QString label,QString command){menu_.append(label);menuCommands_.append(command);};
        const auto id=key.mid(7);const bool self=id==snapshot_["userId"].toString();
        menuTitle_="Player";
        if(key.startsWith("seat:")) {
            menuTitle_="P"+key.mid(5);if(viewedLobby().hosting())add("Invite friend","lobby:invite");
            if(viewedLobby().state()["phase"]=="gathering")add("Take this controller","lobby:"+key);
        } else {
            for(const auto& v:viewedLobby().state()["members"].toArray())if(v.toObject()["id"]==id)menuTitle_=v.toObject()["name"].toString();
            add("View profile","profile:"+id);
            if(self&& &viewedLobby()==lobby_.activeRoom()) {
                menuDetail_=lobbyRuntime_.status();
                if(lobbyRuntime_.problem()=="local-content")add("Import your game","lobby:import");
                else if(lobbyRuntime_.available()&&!lobbyRuntime_.busy()&&!lobbyRuntime_.status().isEmpty())
                    add("Retry setup","lobby:prepare");
            }
            if(self)for(const auto& v:lobbyActions()) {const auto action=v.toMap();if(action["enabled"].toBool()&&(action["id"]=="seat"||action["id"]=="spectate"))add(action["label"].toString(),"lobby:"+action["id"].toString());}
            else if(viewedLobby().hosting())add("Remove from lobby","lobby:remove:"+id);
        }
        add("Back","cancel");emit changed();return;
    }
    for(const auto& v:lobbyRoomActions())if(v.toMap()["key"]==key&&v.toMap()["enabled"].toBool()){lobbyAction(v.toMap()["command"].toString());return;}
}
void SocialController::dispatchLobbyRoom(Action action) {
    if(action==Action::ToggleFavorite){toggleLobbySpectatorChat();return;}
    if(lobbyChatFocused_) {
        if(action==Action::Back||action==Action::Left) {if(lobbyChatExpanded_)lobbyChatExpanded_=false;else {lobbyChatFocused_=false;reading_=false;}}
        else if(action==Action::ChangeView)expandLobbyChat();
        else if(action==Action::Secondary&&!lobbySpectatorChat_)compose();
        else if(action==Action::ToggleContinue)call();
        else if(action==Action::ContextMenu||action==Action::LocalAction)lobbyChatTools();
        else if(action==Action::Confirm&&!lobbySpectatorChat_){if(lobbyRoomMessages().isEmpty())compose();else lobbyChatTools();}
        else if(action==Action::Up||action==Action::Down){
            const auto rows=lobbyRoomMessages();int index=-1;for(int i=0;i<rows.size();++i)if(rows[i].toMap()["sourceIndex"].toInt()==messageFocus_)index=i;
            if(!rows.isEmpty()){index=qBound(0,index+(action==Action::Down?1:-1),int(rows.size())-1);retainMessagePosition(rows[index].toMap()["sourceIndex"].toInt(),false);}
            if(index<=0&&action==Action::Up)earlierMessages();
        }
        emit changed();return;
    }
    if(action==Action::Back){lobbyAction("browse");return;}
    if(action==Action::ChangeView){expandLobbyChat();return;}
    if(action==Action::Secondary&&lobbyChatVisible()){focusLobbyChat();if(!lobbySpectatorChat_)compose();return;}
    if(action==Action::ToggleContinue&&lobbyChatVisible()){focusLobbyChat();call();return;}
    if(action==Action::ContextMenu||action==Action::LocalAction){if(lobbyRoomFocus().startsWith("person:"))activateLobbyRoom(lobbyRoomFocus());else lobbyAction("room-menu");return;}
    if(action==Action::Confirm){activateLobbyRoom(lobbyRoomFocus());return;}
    QList<QStringList> groups;groups.append(lobbyRoomGame()["canStart"].toBool()?QStringList{"game","start"}:QStringList{"game"});
    for(const auto& rows:{lobbyPlayers(),lobbyRoomActions()}) {QStringList keys;for(const auto& v:rows)keys.append(v.toMap()["key"].toString());if(!keys.isEmpty())groups.append(keys);}
    int group=0,index=0;for(int i=0;i<groups.size();++i)if(groups[i].contains(lobbyRoomFocus())){group=i;index=groups[i].indexOf(lobbyRoomFocus());break;}
    if(action==Action::Up||action==Action::Down){group=qBound(0,group+(action==Action::Down?1:-1),int(groups.size())-1);lobbyRoomFocus_=groups[group][qMin(index,int(groups[group].size())-1)];}
    else if(action==Action::Right){if(index+1<groups[group].size())lobbyRoomFocus_=groups[group][index+1];else focusLobbyChat();}
    else if(action==Action::Left&&index>0)lobbyRoomFocus_=groups[group][index-1];
    emit changed();
}
void SocialController::reconcileLobbyPreparation() {
    // One automatic attempt per actual runtime context. Never close, interrupt or
    // repeatedly prompt over an active game; explicit Continue setup uses the
    // existing protected save/exit consent. Failures require deliberate retry.
    if(!readySwitch_.isEmpty()) {
        auto* target=lobby_.room(readySwitch_);
        if(!target||gameActive_||readySwitchExpires_<QDateTime::currentSecsSinceEpoch()||quint64(target->state()["generation"].toInteger())!=readySwitchGeneration_)readySwitch_.clear();
        else if(lobby_.activateRoom(readySwitch_)) {
            readySwitch_.clear();lobbyAutoPreparation_.clear();
        } else return;
    }
    if(gameActive_||snapshot_["state"]!="connected"||!lobbyRuntime_.available()) {lobbyAutoPreparation_.clear();return;}
    if(lobbyRuntime_.busy())return;
    const auto s=lobby_.state();QJsonArray people;
    for(const auto& v:s["members"].toArray()){const auto m=v.toObject();if(m["id"]==snapshot_["userId"].toString()&&m["prepared"].toBool()) {
            return;
        }
        people.append(QJsonObject{{"id",m["id"]},{"seat",m["seat"]},{"connected",m["connected"]},{"spectator",m["spectator"]}});}
    const auto key=QString::fromUtf8(QJsonDocument(QJsonObject{{"room",s["id"]},{"owner",s["owner"]},{"game",s["game"]},{"relay",s["relay"]},{"generation",s["generation"]},{"members",people}}).toJson(QJsonDocument::Compact));
    if(key==lobbyAutoPreparation_)return;
    lobbyAutoPreparation_=key;lobbyRuntime_.prepare();
}
}
