#include "SocialController.h"
#include "core/model/NetplayRelay.h"
#include "integrations/social/LobbyDirectory.h"
#include <QDateTime>
#include <QUuid>

namespace trainer {
void SocialController::syncLobbyReminder() {
    const auto s=viewedLobby().state();
    if(s["id"]==pendingLobbyReminder_["id"]&&s["owner"]==pendingLobbyReminder_["owner"]&&s["phase"]!="joining") {
        if(s["startsAt"]==pendingLobbyReminder_["startsAt"])
            lobbyReminders_.accept(s,QDateTime::currentSecsSinceEpoch());
        pendingLobbyReminder_={};
    }
    lobbyReminders_.observe(s);
}
void SocialController::tickLobbyReminders() {
    if(owner_.isEmpty()||viewedLobby().self().isEmpty()||snapshot_["state"]=="signed-out")return;
    for(const auto& v:lobbyReminders_.due(QDateTime::currentSecsSinceEpoch())) {
        const auto r=v.toObject();const auto time=QDateTime::fromSecsSinceEpoch(r["startsAt"].toInteger()).toString("HH:mm");
        const auto text="Gathering at "+time;
        activityNotifications_.prepend(QVariantMap{{"id","reminder:"+r["id"].toString()+":"+QString::number(r["startsAt"].toInteger())},
            {"activity",true},{"lobbyReminder",true},{"name",r["title"].toString()},{"detail",text},{"room",r["id"].toString()}});
        while(activityNotifications_.size()>32)activityNotifications_.removeLast();saveNotifications();
        if(!snapshot_["doNotDisturb"].toBool()) {
            toastTitle_=r["title"].toString();toastText_=text;toastChannel_="reminder:"+r["id"].toString();toastTimer_.start();
            if(snapshot_.value("notificationSound",true).toBool())media_.chime();
            if(gameActive_)emit backgroundNotification(toastTitle_,toastText_);
            emit presentationChanged();
        }
        emit changed();
    }
}
void SocialController::turnLobbyPage(int delta) {
    const auto page=qBound(0,lobbyPage()+delta,lobbyPageCount()-1);
    const auto index=qMin(int(lobbyRows().size())-1,1+page*6);
    focusLobbyCard(index);lobbyDetailFocused_=false;emit changed();
}
int SocialController::rememberedLobbyRoom() const {
    const auto rows=lobbyRows();
    for(int i=1;i<rows.size();++i)if(rows[i].toMap()["selection"]==lobbyLastRoom_)return i;
    return rows.size()>1?1:0;
}
int SocialController::lobbyPage() const {
    return qMax(0,(lobbyFocus()>0?lobbyFocus():rememberedLobbyRoom())-1)/6;
}
void SocialController::focusLobbyCard(int index) {
    const auto rows=lobbyRows();index=qBound(0,index,int(rows.size())-1);
    lobbySelection_=rows[index].toMap()["selection"].toString();
    if(index>0)lobbyLastRoom_=lobbySelection_;
    lobbyRailFocused_=false;
}
void SocialController::activateLobbyRail(int index) {
    const auto categories=QStringList{QString()}+lobbyDirectory::publicationTopics();
    if(index<0||index>categories.size()+2)return;
    lobbyRailFocus_=index;lobbyRailFocused_=true;
    if(index==0)editLobbySearch();
    else if(index<=categories.size()) {
        lobbyAction("category:"+categories[index-1]);focusLobbyCard(rememberedLobbyRoom());
    } else lobbyAction(index==categories.size()+1?"free":"filters");
    emit changed();
}
void SocialController::dispatchLobbyCatalogue(Action action) {
    const auto categories=QStringList{QString()}+lobbyDirectory::publicationTopics();
    if(action==Action::ContextMenu||action==Action::LocalAction){lobbyAction("filters");return;}
    if(lobbyRailFocused_) {
        if(action==Action::Up||action==Action::Down)
            lobbyRailFocus_=qBound(0,lobbyRailFocus_+(action==Action::Down?1:-1),int(categories.size())+2);
        else if(action==Action::Confirm){activateLobbyRail(lobbyRailFocus_);return;}
    } else if(action==Action::Back) {
        lobbyRailFocused_=true;lobbyRailFocus_=qMax(0,int(categories.indexOf(lobbyTopic_)))+1;
    } else if(action==Action::Confirm){selectLobby(lobbyFocus());return;}
    else if(action==Action::Up||action==Action::Down||action==Action::Left||action==Action::Right) {
        const int focus=lobbyFocus(),count=lobbyRows().size()-1;
        if(focus==0) {if(action==Action::Down)focusLobbyCard(rememberedLobbyRoom());}
        else {
            const int slot=(focus-1)%6,row=slot/2,column=slot%2,page=(focus-1)/6;
            if(action==Action::Up)focusLobbyCard(row==0?0:focus-2);
            else if(action==Action::Down&&row<2&&focus+2<=count)focusLobbyCard(focus+2);
            else if(action==Action::Left||action==Action::Right) {
                const bool right=action==Action::Right;
                if((right&&column==0&&focus<count)||(!right&&column==1))focusLobbyCard(focus+(right?1:-1));
                else if((right&&column==1&&page+1<lobbyPageCount())||(!right&&column==0&&page>0)) {
                    const int start=(page+(right?1:-1))*6+1;
                    focusLobbyCard(qMin(count,start+row*2+(right?0:1)));
                }
            }
        }
    }
    emit changed();
}
bool SocialController::lobbyChatVisible() const {
    return face_=="lobby"&&lobbyDetailFocused_&&lobbyRows().value(lobbyFocus()).toMap()["kind"]=="own"
        &&!lobbyChatChannel_.isEmpty()&&snapshot_["channel"]==lobbyChatChannel_;
}
void SocialController::focusLobbyChat() {
    if(!lobbyChatVisible())return;
    const auto rows=lobbyRoomMessages();bool found=false;for(const auto& v:rows)if(v.toMap()["sourceIndex"].toInt()==messageFocus_)found=true;
    if(!found&&!rows.isEmpty())messageFocus_=rows.last().toMap()["sourceIndex"].toInt();
    lobbyChatFocused_=true;reading_=true;emit changed();
}
void SocialController::openLobbyChat() {
    if(!directConversationLease_.isEmpty())return;
    if(face_!="lobby"||lobbyRows().value(lobbyFocus()).toMap()["kind"]!="own"||lobbyChatChannel_.isEmpty())return;
    selection_.stop();
    if(snapshot_["channel"]!=lobbyChatChannel_)emit commandRequested("open",{{"id",lobbyChatChannel_}});
}
void SocialController::syncLobbyChat() {
    const auto s=viewedLobby().state();const auto id=s["id"].toString();
    if(lobbyChatRoom_!=id){lobbyChatRoom_=id;lobbyPublicationTopic_=viewedLobby().publicationTopic();lobbyChatFocused_=false;lobbyChatExpanded_=false;lobbySpectatorChat_=false;lobbyRoomFocus_="game";}
    lobbyChatChannel_=s["conversation"].toString();lobbyChatStatus_=lobbyChatErrors_.value(id);lobbyChatBusy_=lobbyChatPending_.contains(id);
    openLobbyChat();
}

void SocialController::openLobby() {
    closeMenu();setFace("lobby");lobbySelection_.clear();lobbyActionFocus_=0;lobbyDetailFocused_=true;lobbyChatFocused_=false;openLobbyChat();emit lobbyRequested();emit changed();
}
void SocialController::openLobbyForPeople(QString peer,QString conversation) {
    if(snapshot_["state"]!="connected") {openLobby();return;}
    QVariantMap chat;
    if(!conversation.isEmpty())for(const auto& v:snapshot_["chats"].toList())if(v.toMap()["id"]==conversation) {chat=v.toMap();break;}
    if(!conversation.isEmpty()&&chat.isEmpty())return;
    if(viewedLobby().state().isEmpty()){if(!lobbyCreateOpen_)createAnotherLobby();lobbyDraftPeer_=peer;lobbyDraftConversation_=conversation;return;}
    if(viewedLobby().hosting()) {
        if(chat["kind"]=="groups") {
            if(viewedLobby().state()["conversation"].toString().isEmpty())viewedLobby().setConversation(conversation);
            for(const auto& v:chat["members"].toList()) {
                const auto id=v.toMap()["id"].toString();if(id!=snapshot_["userId"])viewedLobby().invite(id);
            }
        }
        if(!peer.isEmpty())viewedLobby().invite(peer);
    }
    openLobby();
}
void SocialController::openLobbyForGame(const QString& key) {
    if(snapshot_["state"]!="connected") {openLobby();return;}
    QVariantMap selected;for(const auto& v:lobbyGames_)if(v.toMap()["key"]==key) {selected=v.toMap();break;}
    if(selected.isEmpty()) {
        toastTitle_="Play together";toastText_="This game's multiplayer runtime is not available yet.";
        toastTimer_.start();emit presentationChanged();return;
    }
    if(viewedLobby().state().isEmpty())createAnotherLobby();
    else {
        openLobby();
        const auto phase=viewedLobby().state()["phase"].toString();
        if(!viewedLobby().hosting()||(phase!="gathering"&&phase!="choosing"))return;
    }
    openLobbyPicker();
    lobbyPickerAction("game:"+selected.value("adventure",selected["key"]).toString());
}
QVariantList SocialController::lobbyRows() const {
    QVariantList rows;
    const auto own=viewedLobby().state();
    rows.append(QVariantMap{{"selection","own"},{"kind",own.isEmpty()?"create":"own"},{"title",myLobbies().isEmpty()?"Create lobby":"My Lobbies"},
        {"detail",own.isEmpty()?"Choose a game together":own["game"].toObject()["title"].toString().isEmpty()?"Choosing a game":own["game"].toObject()["title"].toString()}});
    const auto invitation=lobby_.invitation();
    if(lobbyScope_!="local"&&!invitation.isEmpty())rows.append(QVariantMap{{"selection","invite:"+invitation["lobby"].toString()},{"kind","invitation"},
        {"title",invitation["title"].toString().isEmpty()?"Invitation":invitation["title"].toString()},
        {"name",invitation["name"]},{"game",invitation["game"].toVariant()},{"startsAt",invitation["startsAt"].toVariant()},{"detail","Join their lobby"}});
    for(const auto& value:lobbyDirectory()["rows"].toList()) {
        auto row=value.toMap();const auto game=row["game"].toMap();
        if(lobby_.room(row["lobby"].toString())||row["expires"].toLongLong()<=QDateTime::currentSecsSinceEpoch())continue;
        if(!lobbyTopic_.isEmpty()&&row["topic"]!=lobbyTopic_)continue;
        if(!lobbyPlatform_.isEmpty()&&game["platform"]!=lobbyPlatform_)continue;
        if(!lobbyMode_.isEmpty()&&game["mode"]!=lobbyMode_)continue;
        if(lobbyFreeOnly_&&row["status"]!="choosing"&&row["available"].toInt()==0)continue;
        const auto title=row["title"].toString().isEmpty()?row["name"].toString()+"'s lobby":row["title"].toString();
        if(!lobbySearch_.isEmpty()&&!(title+" "+game["title"].toString()+" "+row["name"].toString()+" "+game["platform"].toString()).contains(lobbySearch_,Qt::CaseInsensitive))continue;
        row["selection"]=row["lobby"];row["kind"]="public";row["title"]=title;
        row["detail"]=row["name"].toString()+(row["seats"].toInt()?" · "+QString::number(row["available"].toInt())+" seats free":QString())
            +(row["streams"].toInt()>0?" · Live":QString());rows.append(row);
    }
    return rows;
}
int SocialController::lobbyFocus() const {
    const auto rows=lobbyRows();for(int i=0;i<rows.size();++i)if(rows[i].toMap()["selection"]==lobbySelection_)return i;
    return 0;
}
QVariantList SocialController::lobbyActions() const {
    QVariantList actions;const auto add=[&](QString id,QString label,bool enabled=true){actions.append(QVariantMap{{"id",id},{"label",label},{"enabled",enabled}});};
    const auto row=lobbyRows().value(lobbyFocus()).toMap();const auto kind=row["kind"].toString();
    if(kind=="create")add("create","Create lobby");
    else if(kind=="community")add("community","Join community");
    else if(kind=="invitation") {add("accept","Join lobby");add("later","Keep for later");add("decline","Decline");}
    else if(kind=="public"){add("join",row["access"]=="approval"?"Request to join":"Join lobby",!lobby_.room(row["lobby"].toString()));if(row["type"]!="local")add("watch","Watch",!lobby_.room(row["lobby"].toString()));}
    else if(kind=="own") {
        const auto s=viewedLobby().state();const auto game=s["game"].toObject();const auto members=s["members"].toArray();
        QJsonObject mine;for(const auto& member:members)if(member.toObject()["id"]==viewedLobby().self())mine=member.toObject();
        const bool gathering=s["phase"]=="choosing"||s["phase"]=="gathering";
        if(s["startsAt"].toInteger()>QDateTime::currentSecsSinceEpoch())
            add("reminder",lobbyReminders_.contains(s["id"].toString())?"Cancel reminder":"Agree & remind me");
        if(viewedLobby().hosting()) {
            if(!viewedLobby().local())add("invite","Invite friend",gathering||s["phase"]=="playing");add("game","Choose game",gathering&&!lobbyGames_.isEmpty());
            add("options",viewedLobby().requests().isEmpty()?"Lobby options":"Options · Join requests");
        }
        if(!lobbyChatChannel_.isEmpty()) {
            const auto voice=snapshot_["voice"].toMap();
            add("call",voice["channel"].toString().isEmpty()?"Join lobby call":"Call & streams");
        }
        if(mine["spectator"].toBool()&&!viewedLobby().hosting())add("spectate","Join as player",gathering);
        if(!s["vote"].toObject().isEmpty())add("vote","Vote for a game",gathering&&!mine["spectator"].toBool());
        if(!game["id"].toString().isEmpty()) {
            if(mine["spectator"].toBool()&&!viewedLobby().hosting()) {
                if(&viewedLobby()==lobby_.activeRoom()&&lobbyRuntime_.liveAvailable())add("join-live",mine["returnSeat"].toInt()>0?"Rejoin game":"Join game now");
                else add("request-play",lobbyRuntime_.busy()?"Preparing…":"I want to play",!lobbyRuntime_.busy());
            }
            add("seat","Controller: "+(mine["seat"].toInt()?"P"+QString::number(mine["seat"].toInt()):QString("Choose")),gathering);
            if(!mine["spectator"].toBool()) {
                if(viewedLobby().readyPending())add("ready","Confirming…",false);
                else if(&viewedLobby()!=lobby_.activeRoom()||mine["prepared"].toBool())add("ready",mine["ready"].toBool()?"Not ready":(&viewedLobby()!=lobby_.activeRoom()?"Prepare here":"Ready"),gathering);
                else add("prepare",lobbyRuntime_.busy()?"Preparing…":"Prepare game",gathering&&lobbyRuntime_.available()&&!lobbyRuntime_.busy());
            }
        }
        add("leave-confirm","Leave lobby");if(viewedLobby().hosting())add("close-confirm","Close lobby for everyone");
        if(!viewedLobby().hosting()&&!s["connection"].toString().isEmpty())add("reconnect","Retry connection");
    }
    return actions;
}
void SocialController::selectLobby(int index) {
    if(index<0||index>=lobbyRows().size())return;
    if(lobbyRows()[index].toMap()["kind"]=="own"){showMyLobbies();return;}
    focusLobbyCard(index);lobbyActionFocus_=0;lobbyDetailFocused_=true;lobbyChatFocused_=false;openLobbyChat();emit changed();
    const auto row=lobbyRows()[index].toMap();
    if(row["kind"]=="own"){showMyLobbies();return;}
    if(row["kind"]=="create") {lobbyAction("create");return;}
    menuMode_="lobby";menuTitle_=row["title"].toString();menuDetail_=row["detail"].toString();menuFocus_=0;menu_.clear();menuCommands_.clear();
    const auto start=row["startsAt"].toLongLong();
    if(start>QDateTime::currentSecsSinceEpoch())menuDetail_="Gathering "+QDateTime::fromSecsSinceEpoch(start).toString("ddd d MMM, HH:mm")+". Joining sets a local reminder 15 minutes before.";
    for(const auto& v:lobbyActions()) {const auto a=v.toMap();if(a["enabled"].toBool()){menu_.append(a["label"].toString());menuCommands_.append("lobby:"+a["id"].toString());}}
    menu_.append("Back");menuCommands_.append("cancel");lobbyDetailFocused_=false;emit changed();
}
void SocialController::setLobbyGames(QVariantList games) {
    runtimeLobbyGames_=games;
    games.append(activityLobbyGames_);
    if(lobbyGames_==games)return;
    const auto focusedMode=lobbyPicker()["modes"].toList().value(lobbyPickerModeFocus_).toMap()["key"].toString();
    lobbyGames_=std::move(games);
    refreshLobbyPicker();
    if(!focusedMode.isEmpty()&&!lobbyPickerModeGame_.isEmpty()) {
        const auto modes=lobbyPicker()["modes"].toList();int index=-1;
        for(int i=0;i<modes.size();++i)if(modes[i].toMap()["key"]==focusedMode){index=i;break;}
        if(index>=0)lobbyPickerModeFocus_=index;
        else {lobbyPickerModeGame_.clear();lobbyCreationError_="This play mode is no longer available. Choose a mode again.";}
    }
    if(lobbyRuntime_.problem()=="local-content") {
        const auto game=lobby_.state()["game"].toObject();
        for(const auto& v:lobbyGames_){const auto g=v.toMap();if(g["id"].toString()==game["id"].toString()&&g["mode"].toString()==game["mode"].toString()){lobbyAutoPreparation_.clear();break;}}
    }
    emit changed();}
void SocialController::setLobbyActivities(QVariantList games) {
    activityLobbyGames_=std::move(games);
    setLobbyGames(runtimeLobbyGames_);
}
void SocialController::editLobbySearch() {textPurpose_="lobby-search";emit textRequested("Find a lobby",lobbySearch_,100);}
void SocialController::lobbyAction(QString action) {
    if(viewedLobby().local()&&(action=="invite"||action.startsWith("invite:")||action=="chat-people"||action=="chat-retry"||action=="schedule"||action=="access"||action.startsWith("confirm-access:")))return;
    if(action.startsWith("create-field:")){lobbyDraftAction(action.mid(13));return;}
    if(action=="game"){openLobbyPicker();return;}
    if(action=="vote") {if(!viewedLobby().state()["vote"].toObject()["id"].toString().isEmpty()){lobbyVoteOpen_=true;lobbyVoteFocus_=0;emit changed();}return;}
    if(action.startsWith("ballot:")||action.startsWith("winner:")) {
        const auto id=action.section(':',1,1);const int choice=action.section(':',2).toInt();
        if(action.startsWith("ballot:")){if(viewedLobby().vote(id,choice))lobbyVoteFocus_=choice;}
        else {const auto ballot=viewedLobby().state()["vote"].toObject();if(ballot["id"]!=id||choice<0||choice>=ballot["choices"].toArray().size())return;const auto g=ballot["choices"].toArray()[choice].toObject();
            for(const auto& v:availableLobbyGames()){const auto local=v.toMap();if(local["id"].toString()==g["id"].toString()&&local["mode"].toString()==g["mode"].toString()&&QJsonObject::fromVariantMap(local["runtime"].toMap())==g["runtime"].toObject()){if(viewedLobby().confirmVote(id,choice))lobbyVoteOpen_=false;break;}}}
        emit changed();return;
    }

    if(action=="room-menu"&&!viewedLobby().state().isEmpty()) {
        menuMode_="lobby";menuTitle_="Room options";menuDetail_.clear();menuFocus_=0;menu_.clear();menuCommands_.clear();
        for(const auto& v:lobbyActions()) {const auto a=v.toMap();const auto id=a["id"].toString();
            if(!a["enabled"].toBool()||id=="prepare"||id=="ready"||id=="game")continue;
            menu_.append(a["label"].toString());menuCommands_.append("lobby:"+id);}
        menu_.append("Back");menuCommands_.append("cancel");emit changed();return;
    }
    if(action=="leave"){leaveViewedLobby();return;}
    if(action=="close"){leaveViewedLobby(true);return;}
    if(action=="leave-confirm"||action=="close-confirm") {
        menuMode_="lobby";menuTitle_=action=="leave-confirm"?"Leave this lobby?":"Close this lobby for everyone?";
        menuDetail_=viewedLobby().local()?"Leaving as host closes this local room. Your running game is kept.":action=="leave-confirm"?"No other players: the lobby closes. Otherwise, ownership passes to a player.":"Game participation ends for everyone. The Fluxer conversation history is kept.";
        menu_={"Back",action=="leave-confirm"?"Leave lobby":"Close lobby"};menuCommands_={"cancel","lobby:"+action.section('-',0,0)};menuFocus_=0;emit changed();return;
    }
    if(action.startsWith("ready-switch:")) {
        const auto id=action.mid(13);if(gameActive_)return;
        auto* target=lobby_.room(id);if(!target)return;
        readySwitch_=id;readySwitchGeneration_=quint64(target->state()["generation"].toInteger());readySwitchExpires_=QDateTime::currentSecsSinceEpoch()+20;
        lobby_.ready(false);reconcileLobbyPreparation();return;
    }
    if(action=="browse"){lobbyDetailFocused_=false;lobbyChatFocused_=false;lobbyChatExpanded_=false;emit changed();return;}
    if(action=="scope-online"||action=="scope-local"){setLobbyScope(action.mid(6));return;}
    if(snapshot_["state"]!="connected"&&lobbyScope_!="local"&&!viewedLobby().local()&&action!="create"&&action!="filters")return;
    if(action=="reminder") {
        const auto s=viewedLobby().state();const auto id=s["id"].toString();
        const bool ok=lobbyReminders_.contains(id)?lobbyReminders_.cancel(id):lobbyReminders_.accept(s,QDateTime::currentSecsSinceEpoch());
        if(!ok){toastTitle_="Reminder";toastText_="Could not save the reminder on this device.";toastTimer_.start();emit presentationChanged();}
        emit changed();return;
    }
    if((action=="rename"||action=="schedule"||action=="unschedule")&&viewedLobby().hosting()) {
        const auto s=viewedLobby().state();
        if(action=="unschedule"){viewedLobby().setDetails(s["title"].toString(),0);return;}
        textPurpose_=action=="rename"?"lobby-title":"lobby-time";
        emit textRequested(action=="rename"?"Lobby name":"Start time (local): YYYY-MM-DD HH:mm",action=="rename"?s["title"].toString():
            QDateTime::fromSecsSinceEpoch(s["startsAt"].toInteger()>QDateTime::currentSecsSinceEpoch()?s["startsAt"].toInteger():QDateTime::currentSecsSinceEpoch()+3600).toString("yyyy-MM-dd HH:mm"),action=="rename"?80:16);
        return;
    }
    if(action.startsWith("category:")||action.startsWith("platform:")||action.startsWith("mode:")) {
        if(action.startsWith("category:"))lobbyTopic_=action.mid(9);
        else if(action.startsWith("platform:"))lobbyPlatform_=action.mid(9);else lobbyMode_=action.mid(5);
        lobbySelection_.clear();emit changed();return;
    }
    if(action=="filters"||action=="platforms"||action=="modes"||action=="categories") {
        menuMode_="lobby";menuFocus_=0;menuTitle_="Lobby filters";menuDetail_.clear();menu_.clear();menuCommands_.clear();
        const auto add=[&](QString text,QString command){menu_.append(text);menuCommands_.append("lobby:"+command);};
        if(action=="filters") {add(lobbyScope_=="local"?"Local (selected)":"Local - no account needed","scope-local");add(lobbyScope_=="online"?"Online (selected)":"Online","scope-online");add("Search","search");add("Category","categories");add("Platform: "+(lobbyPlatform_.isEmpty()?"All":lobbyPlatform_),"platforms");add("Multiplayer: "+(lobbyMode_.isEmpty()?"All":lobbyMode_),"modes");add(lobbyFreeOnly_?"Show full rooms too":"Free seats only","free");add("Clear filters","clear-filters");add("Refresh","refresh");}
        else if(action=="categories") {add("All categories","category:");for(const auto& t:lobbyDirectory::publicationTopics())add(lobbyDirectory::topicName(t),"category:"+t);}
        else {const auto key=action=="platforms"?QString("platform"):QString("mode");QStringList values;
            for(const auto& r:lobbyDirectory()["rows"].toList()){const auto value=r.toMap()["game"].toMap()[key].toString();if(!value.isEmpty()&&!values.contains(value))values.append(value);}
            values.sort();add("All",key+":");for(const auto& v:values)add(v,key+":"+v);}
        emit changed();return;
    }
    if(action=="clear-filters") {lobbySearch_.clear();lobbyTopic_.clear();lobbyPlatform_.clear();lobbyMode_.clear();lobbyFreeOnly_=false;lobbySelection_.clear();emit changed();return;}
    if(action=="call"&&!lobbyChatChannel_.isEmpty()) {
        const auto voice=snapshot_["voice"].toMap();
        if(voice["channel"].toString().isEmpty())emit commandRequested("voice-join",{{"channel",lobbyChatChannel_}});
        else {openLobbyChat();closeMenu();together();}
        return;
    }
    if(action=="spectate") {
        for(const auto& v:viewedLobby().state()["members"].toArray())if(v.toObject()["id"]==viewedLobby().self())
            if(v.toObject()["spectator"].toBool()&&!viewedLobby().hosting())viewedLobby().spectate(false);
        return;
    }
    if(action=="request-play"){viewedLobby().requestPlay();return;}
    if(action.startsWith("play-answer:")){viewedLobby().answerPlay(action.section(':',2),action.section(':',1,1));return;}
    if(action.startsWith("play-request:")&&viewedLobby().hosting()) {
        const auto peer=action.mid(13);menuMode_="lobby";menuTitle_="Wants to play";
        menuDetail_="The current game continues. Controller selection is available after it ends.";menuFocus_=0;
        menu_={"Accept for next game","Later","Decline","Back"};
        menuCommands_={"lobby:play-answer:accept:"+peer,"lobby:play-answer:later:"+peer,"lobby:play-answer:decline:"+peer,"cancel"};emit changed();return;
    }
    if(action=="reconnect") {viewedLobby().reconnect();return;}
    if(action.startsWith("remove:")&&viewedLobby().local()){viewedLobby().remove(action.mid(7));return;}
    if(action.startsWith("remove:")) {
        const auto channel=viewedLobby().state()["conversation"].toString();
        if(viewedLobby().hosting()&&!channel.isEmpty())emit commandRequested("remove-member",{{"channel",channel},{"id",action.mid(7)}});
        return;
    }
    if(action=="browse") {lobbyDetailFocused_=false;lobbyChatFocused_=false;lobbyChatExpanded_=false;emit changed();return;}
    if(action=="chat-retry") {lobbyChatKeys_.remove(viewedLobby().state()["id"].toString());syncMyLobbies();return;}
    if(action=="chat-people") {
        menuMode_="lobby";menuTitle_="Lobby people";menuDetail_="Fluxer groups require friendship and permission to add members.";
        menuFocus_=0;menu_.clear();menuCommands_.clear();
        for(const auto& v:viewedLobby().state()["members"].toArray()) {
            const auto m=v.toObject();if(m["id"]==viewedLobby().self())continue;
            menu_.append(m["name"].toString());menuCommands_.append("profile:"+m["id"].toString());
        }
        if(menu_.isEmpty()) {menu_.append("Back");menuCommands_.append("cancel");}
        emit changed();return;
    }
    if(action=="refresh"&&lobbyScope_=="local"){syncLocalLobby();emit changed();return;}
    if(action=="refresh") {emit commandRequested("lobby-directory-refresh",{});return;}
    if(action=="search") {editLobbySearch();return;}
    if(action=="topic") {
        const auto topics=QStringList{QString()}+lobbyDirectory::publicationTopics();
        lobbyTopic_=topics[(topics.indexOf(lobbyTopic_)+1)%topics.size()];lobbySelection_.clear();emit changed();return;
    }
    if(action=="free") {lobbyFreeOnly_=!lobbyFreeOnly_;lobbySelection_.clear();emit changed();return;}
    if(action=="access"&&viewedLobby().hosting()) {
        const QStringList choices{"private","public","approval"};const auto next=choices[(choices.indexOf(viewedLobby().state()["access"].toString())+1)%3];
        if(next=="private"){viewedLobby().setAccess(next);return;}
        menuMode_="lobby";menuTitle_=next=="public"?"Open this lobby?":"Publish this lobby?";
        menuDetail_=next=="public"?"Anyone admitted to this lobby can join its shared conversation, call and game streams.":
            "People you approve can join the shared conversation, call and game streams.";
        menuFocus_=0;menu_={next=="public"?"Open lobby & conversation":"Publish with approval","Back"};
        menuCommands_={"lobby:confirm-access:"+next,"cancel"};emit changed();return;
    }
    if(action.startsWith("confirm-access:")&&viewedLobby().hosting()) {viewedLobby().setAccess(action.mid(15));return;}
    if(action.startsWith("publish-topic:")&&viewedLobby().hosting()) {
        const auto topic=action.mid(14);
        if(lobbyDirectory::publicationTopics().contains(topic)) {lobbyPublicationTopic_=topic;viewedLobby().setPublicationTopic(topic);lobbyPublishTimer_.start();emit changed();}
        return;
    }
    if(action.startsWith("invite:")) {
        if(viewedLobby().invite(action.mid(7))) {toastTitle_="Lobby";toastText_="Sending invitation…";toastTimer_.start();emit presentationChanged();}
        emit changed();return;
    }
    if(action.startsWith("approve:")) {viewedLobby().approve(action.mid(8));return;}
    if(action.startsWith("seat:")) {
        if(!viewedLobby().seat(action.mid(5).toInt())){toastTitle_="Lobby";toastText_="That controller is already taken or unavailable.";toastTimer_.start();emit presentationChanged();}
        return;
    }
    if(action.startsWith("game:")) {
        for(const auto& v:availableLobbyGames()) {
            const auto g=v.toMap();if(g["key"]!=action.mid(5))continue;
            viewedLobby().selectGame({g["id"].toString(),g["title"].toString(),g["platform"].toString(),g["mode"].toString(),g["seats"].toInt(),QJsonObject::fromVariantMap(g["runtime"].toMap())});break;
        }
        return;
    }
    const auto room=viewedLobby().state();
    const bool relayEditable=!viewedLobby().local()&&viewedLobby().hosting()&&(room["phase"]=="choosing"||room["phase"]=="gathering")&&
        !netplayRelay::choices(room["game"].toObject()["runtime"].toObject()).isEmpty();
    if(action.startsWith("relay:")&&relayEditable) {viewedLobby().setRelay(action.mid(6));emit changed();return;}
    bool allowed=(relayEditable&&action=="relay")||(viewedLobby().hosting()&&(action=="requests"||action=="publish-topic"||action=="remove"||(action=="live-joining"&&lobbyRuntime_.liveSupported())));for(const auto& v:lobbyActions())if(v.toMap()["id"]==action&&v.toMap()["enabled"].toBool())allowed=true;
    if(!allowed)return;
    if(action=="create") {createAnotherLobby();lobbySelection_.clear();}
    else if(action=="community")emit commandRequested("lobby-directory-join",{});
    else if(action=="accept"||action=="decline") {
        const auto invite=lobby_.invitation();
        if(action=="accept")pendingLobbyReminder_={{"id",invite["lobby"]},{"owner",invite["host"]},{"startsAt",invite["startsAt"]}};
        lobby_.answer(action=="accept");if(action=="accept")viewedLobbyId_=invite["lobby"].toString();lobbySelection_.clear();lobbyDetailFocused_=action=="accept";
    }
    else if(action=="later") {lobbyDetailFocused_=false;emit lobbyDeferred();}
    else if(action=="leave") {viewedLobby().leave();lobbySelection_.clear();}
    else if(action=="import"&& &viewedLobby()==lobby_.activeRoom()&&lobbyRuntime_.problem()=="local-content")emit lobbyImportRequested();
    else if(action=="prepare"&& &viewedLobby()==lobby_.activeRoom())lobbyRuntime_.prepare();
    else if(action=="join-live"&& &viewedLobby()==lobby_.activeRoom())lobbyRuntime_.joinLive();
    else if(action=="live-joining"&& &viewedLobby()==lobby_.activeRoom())lobbyRuntime_.setLiveJoining(!viewedLobby().state()["liveJoining"].toBool());
    else if(action=="join"||action=="watch") {const auto row=lobbyRows().value(lobbyFocus()).toMap();
        pendingLobbyReminder_={{"id",row["lobby"].toString()},{"owner",row["owner"].toString()},{"startsAt",row["startsAt"].toLongLong()}};
        if(!lobby_.join(row["owner"].toString(),row["lobby"].toString(),false,action=="watch"))pendingLobbyReminder_={};else viewedLobbyId_=row["lobby"].toString();
        lobbySelection_.clear();lobbyDetailFocused_=true;}
    else if(action=="ready") {
        requestLobbyReady();
    } else {
        menuMode_="lobby";menuFocus_=0;menu_.clear();menuCommands_.clear();menuDetail_.clear();
        const auto add=[&](QString label,QString command){menu_.append(label);menuCommands_.append("lobby:"+command);};
        if(action=="invite") {menuTitle_="Invite to lobby";for(const auto& v:runtimeFriends()){const auto f=v.toMap();add(f["name"].toString(),"invite:"+f["id"].toString());}}
        else if(action=="requests") {menuTitle_="Join requests";for(const auto& v:viewedLobby().requests()){const auto p=v.toObject();add(p["name"].toString()+(p["play"].toBool()?" · Wants to play":""),(p["play"].toBool()?"play-request:":"approve:")+p["id"].toString());}}
        else if(action=="remove") {menuTitle_="Remove from lobby";for(const auto& v:viewedLobby().state()["members"].toArray()) {const auto p=v.toObject();if(p["id"]!=viewedLobby().self())add(p["name"].toString(),"remove:"+p["id"].toString());}}
        else if(action=="game") {menuTitle_="Choose a game";for(const auto& v:lobbyGames_){const auto g=v.toMap();add(g["title"].toString()+" · "+g["platform"].toString()+" · "+g.value("modeLabel","Emulator multiplayer").toString(),"game:"+g["key"].toString());}}
        else if(action=="seat") {menuTitle_="Choose your controller";for(int i=1;i<=viewedLobby().state()["game"].toObject()["seats"].toInt();++i)add("P"+QString::number(i),"seat:"+QString::number(i));}
        else if(action=="options") {
            menuTitle_="Lobby options";if(!viewedLobby().local())add("Access: "+viewedLobby().state()["access"].toString()+" · change","access");
            if(relayEditable)add("Connection settings","relay");
            if(&viewedLobby()==lobby_.activeRoom()&&lobbyRuntime_.liveSupported()&&viewedLobby().state()["phase"]!="launching")
                add(viewedLobby().state()["rejoin"].toBool()
                    ?(viewedLobby().state()["liveJoining"].toBool()?"New players during play: On · disable":"Allow new players during play")
                    :(viewedLobby().state()["liveJoining"].toBool()?"Joining during play: On · disable":"Allow joining during play"),"live-joining");
            if(viewedLobby().state()["members"].toArray().size()>1)add("Remove a member","remove");
            add("Category: "+lobbyDirectory::topicName(lobbyPublicationTopic_),"publish-topic");
            if(!viewedLobby().requests().isEmpty())add("Join requests","requests");
            if(!lobbyChatStatus_.isEmpty()&&!lobbyChatBusy_)add("Retry conversation","chat-retry");
            if(!viewedLobby().local())add("Conversation members","chat-people");
            add("Rename lobby","rename");if(!viewedLobby().local())add("Set gathering time","schedule");
            if(viewedLobby().state()["startsAt"].toInteger())add("Cancel gathering time","unschedule");
        }
        else if(action=="relay") {
            const auto profile=room["game"].toObject()["runtime"].toObject();
            menuTitle_="Connection settings";
            menuDetail_=profile["id"].toString().startsWith("runtime.ppsspp.")
                ?"Try the host relay if standard fails. Needs direct connectivity; resets Ready."
                :"Automatic chooses a relay region using every player's connection. Changing it resets Ready.";
            for(const auto& region:netplayRelay::choices(profile))add(netplayRelay::connectionLabel(profile,region)+(room["relay"].toString("auto")==region?" · Selected":""),"relay:"+region);
        }
        else if(action=="publish-topic") {menuTitle_="Lobby category";for(const auto& topic:lobbyDirectory::publicationTopics())add(lobbyDirectory::topicName(topic),"publish-topic:"+topic);}
        if(menu_.isEmpty()) {menu_.append("Close");menuCommands_.append("cancel");}
    }
    emit changed();
}
void SocialController::dispatchLobby(Action action) {
    if(!lobbyDetailFocused_||lobbyRows().value(lobbyFocus()).toMap()["kind"]!="own") {
        lobbyDetailFocused_=false;dispatchLobbyCatalogue(action);return;
    }
    dispatchLobbyRoom(action);
}

void SocialController::publishLobby() {
    if(snapshot_["state"]!="connected")return;
    QSet<QString> visible;
    for(auto* room:lobby_.rooms()) {
        const auto s=room->state();
        if(room->local()||!room->hosting()||s["access"]=="private")continue;
        // Publication identity belongs to this organizer. A provider ownership
        // transfer creates a fresh listing, never grants another account the
        // right to overwrite the former organizer's announcement.
        const auto listing=QUuid::createUuidV5(QUuid(s["id"].toString()),s["owner"].toString().toUtf8()).toString(QUuid::WithoutBraces);
        visible.insert(listing);publishedRooms_[listing]=room;
        auto game=s["game"].toObject();game.remove("runtime");const auto phase=s["phase"].toString();int used=0;
        for(const auto& m:s["members"].toArray())if(m.toObject()["seat"].toInt()>0)++used;
        const auto voice=snapshot_["voice"].toMap();int streams=0;
        if(voice["state"]=="connected"&&voice["channel"].toString()==s["conversation"].toString())
            streams=voice["streams"].toList().size()+(voice["sharing"].toBool()?1:0);
        emit commandRequested("lobby-directory-publish",{{"v",1},{"op","upsert"},{"lobby",listing},{"room",s["id"].toString()},{"revision",room->nextPublicationRevision()},
            {"status",phase=="playing"?"playing":game["id"].toString().isEmpty()?"choosing":"gathering"},{"access",s["access"].toString()},
            {"topic",room->publicationTopic()},{"seats",game["seats"].toInt()},
            {"available",qMax(0,game["seats"].toInt()-used)},{"streams",qMin(32,streams)},{"game",game.toVariantMap()},
            {"title",s["title"].toString()},{"startsAt",s["startsAt"].toInteger()},{"members",s["members"].toArray().size()}});
    }
    for(const auto& id:publishedRooms_.keys())if(!visible.contains(id)) {
        emit commandRequested("lobby-directory-publish",{{"v",1},{"op","withdraw"},{"lobby",id},{"revision",publishedRooms_[id]->nextPublicationRevision()}});
        publishedRooms_.remove(id);
    }
}
}
