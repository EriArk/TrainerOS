#include "SocialController.h"
#include "integrations/social/LobbyDirectory.h"
#include <QDateTime>

namespace trainer {
void SocialController::syncLobbyReminder() {
    const auto s=lobby_.state();
    if(s["id"]==pendingLobbyReminder_["id"]&&s["owner"]==pendingLobbyReminder_["owner"]&&s["phase"]!="joining") {
        if(s["startsAt"]==pendingLobbyReminder_["startsAt"])
            lobbyReminders_.accept(s,QDateTime::currentSecsSinceEpoch());
        pendingLobbyReminder_={};
    }
    lobbyReminders_.observe(s);
}
void SocialController::tickLobbyReminders() {
    if(owner_.isEmpty()||snapshot_["userId"].toString().isEmpty()||snapshot_["state"]=="signed-out")return;
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
    lobbyChatFocused_=true;reading_=true;emit changed();
}
void SocialController::openLobbyChat() {
    if(!directConversationLease_.isEmpty())return;
    if(face_!="lobby"||lobbyRows().value(lobbyFocus()).toMap()["kind"]!="own"||lobbyChatChannel_.isEmpty())return;
    selection_.stop();
    if(snapshot_["channel"]!=lobbyChatChannel_)emit commandRequested("open",{{"id",lobbyChatChannel_}});
}
void SocialController::syncLobbyChat() {
    const auto s=lobby_.state();const auto room=s["id"].toString();
    if(room!=lobbyChatRoom_) {
        if(!lobbyChatRoom_.isEmpty())emit commandRequested("lobby-chat-cancel",{{"lobby",lobbyChatRoom_}});
        lobbyChatRoom_=room;lobbyChatKey_.clear();lobbyChatChannel_.clear();lobbyChatStatus_.clear();
        lobbyChatBusy_=false;lobbyChatFocused_=false;
    }
    if(room.isEmpty()||s["phase"]=="joining"||s["recovering"].toBool()||snapshot_["state"]!="connected"||lobbyChatBusy_)return;
    QStringList peers;for(const auto& m:s["members"].toArray())if(m.toObject()["id"]!=snapshot_["userId"].toString())peers.append(m.toObject()["id"].toString());
    peers.sort();const auto channel=s["conversation"].toString();
    if(channel.isEmpty()&&(!lobby_.hosting()||peers.isEmpty()))return;
    const auto key=channel+"/"+peers.join(',');if(lobbyChatKey_==key)return;
    lobbyChatKey_=key;lobbyChatBusy_=true;lobbyChatStatus_="Opening conversation…";
    emit commandRequested("lobby-chat",{{"lobby",room},{"channel",channel},{"host",lobby_.hosting()},{"peers",lobby_.hosting()?peers:QStringList{}}});
}
void SocialController::openLobby() {
    closeMenu();setFace("lobby");lobbySelection_.clear();lobbyActionFocus_=0;lobbyDetailFocused_=true;lobbyChatFocused_=false;openLobbyChat();emit lobbyRequested();emit changed();
}
void SocialController::openLobbyForPeople(QString peer,QString conversation) {
    if(snapshot_["state"]!="connected") {openLobby();return;}
    QVariantMap chat;
    if(!conversation.isEmpty())for(const auto& v:snapshot_["chats"].toList())if(v.toMap()["id"]==conversation) {chat=v.toMap();break;}
    if(!conversation.isEmpty()&&chat.isEmpty())return;
    if(lobby_.state().isEmpty()&&!lobby_.create())return;
    if(lobby_.hosting()) {
        if(chat["kind"]=="groups") {
            if(lobby_.state()["conversation"].toString().isEmpty())lobby_.setConversation(conversation);
            for(const auto& v:chat["members"].toList()) {
                const auto id=v.toMap()["id"].toString();if(id!=snapshot_["userId"])lobby_.invite(id);
            }
        }
        if(!peer.isEmpty())lobby_.invite(peer);
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
    if(lobby_.state().isEmpty())lobby_.create();
    openLobby();
    if(!lobby_.hosting())return;
    const auto current=lobby_.state()["game"].toObject();
    QVariantList modes;for(const auto& v:lobbyGames_) {
        const auto g=v.toMap();if(g.value("adventure",g["key"]).toString()==key)modes.append(g);
    }
    if(modes.size()>1) {
        for(const auto& v:modes)if(v.toMap()["id"].toString()==current["id"].toString())return;
        if(lobby_.state()["phase"]!="gathering"&&lobby_.state()["phase"]!="choosing")return;
        menuMode_="lobby";menuTitle_="How to play";menuDetail_=selected["title"].toString();menuFocus_=0;menu_.clear();menuCommands_.clear();
        for(const auto& v:modes){const auto g=v.toMap();menu_.append(g.value("modeLabel","Emulator multiplayer").toString());menuCommands_.append("lobby:game:"+g["key"].toString());}
        emit changed();return;
    }
    if(current["id"].toString().isEmpty()) {lobbyAction("game:"+key);return;}
    if(current["id"]==selected["id"].toString())return;
    if(lobby_.state()["phase"]!="gathering"&&lobby_.state()["phase"]!="choosing")return;
    menuMode_="lobby";menuTitle_="Choose a game";menuDetail_=selected["title"].toString();menuFocus_=0;
    menu_={"Keep "+current["title"].toString(),"Choose "+selected["title"].toString()};
    menuCommands_={"cancel","lobby:game:"+key};emit changed();
}
QVariantList SocialController::lobbyRows() const {
    QVariantList rows;
    const auto own=lobby_.state();
    rows.append(QVariantMap{{"selection","own"},{"kind",own.isEmpty()?"create":"own"},{"title",own.isEmpty()?"Create lobby":"My lobby"},
        {"detail",own.isEmpty()?"Choose a game together":own["game"].toObject()["title"].toString().isEmpty()?"Choosing a game":own["game"].toObject()["title"].toString()}});
    const auto invitation=lobby_.invitation();
    if(!invitation.isEmpty())rows.append(QVariantMap{{"selection","invite:"+invitation["lobby"].toString()},{"kind","invitation"},
        {"title",invitation["title"].toString().isEmpty()?"Invitation":invitation["title"].toString()},
        {"name",invitation["name"]},{"game",invitation["game"].toVariant()},{"startsAt",invitation["startsAt"].toVariant()},{"detail","Join their lobby"}});
    for(const auto& value:lobbyDirectory()["rows"].toList()) {
        auto row=value.toMap();const auto game=row["game"].toMap();
        if(row["lobby"]==own["id"].toVariant()||row["expires"].toLongLong()<=QDateTime::currentSecsSinceEpoch())continue;
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
    else if(kind=="public")add("join",row["access"]=="approval"?"Request to join":"Join lobby",lobby_.state().isEmpty());
    else if(kind=="own") {
        const auto s=lobby_.state();const auto game=s["game"].toObject();const auto members=s["members"].toArray();
        QJsonObject mine;for(const auto& member:members)if(member.toObject()["id"]==snapshot_["userId"].toString())mine=member.toObject();
        const bool gathering=s["phase"]=="choosing"||s["phase"]=="gathering";
        if(s["startsAt"].toInteger()>QDateTime::currentSecsSinceEpoch())
            add("reminder",lobbyReminders_.contains(s["id"].toString())?"Cancel reminder":"Agree & remind me");
        if(lobby_.hosting()) {
            add("invite","Invite friend",gathering||s["phase"]=="playing");add("game","Choose game",gathering&&!lobbyGames_.isEmpty());
            add("options",lobby_.requests().isEmpty()?"Lobby options":"Options · Join requests");
        }
        if(!lobbyChatChannel_.isEmpty()) {
            const auto voice=snapshot_["voice"].toMap();
            add("call",voice["channel"].toString().isEmpty()?"Join lobby call":"Call & streams");
        }
        if(!game["id"].toString().isEmpty()) {
            if(mine["spectator"].toBool()&&!lobby_.hosting()) {
                if(lobbyRuntime_.liveAvailable())add("join-live","Join game now");
                else add("request-play",lobbyRuntime_.busy()?"Preparing…":"I want to play",!lobbyRuntime_.busy());
            }
            add("spectate",mine["spectator"].toBool()?"Join as player":"Be a spectator",gathering);
            add("seat","Controller: "+(mine["seat"].toInt()?"P"+QString::number(mine["seat"].toInt()):QString("Choose")),gathering);
            if(!mine["spectator"].toBool()) {
                if(mine["prepared"].toBool())add("ready",mine["ready"].toBool()?"Not ready":"Ready",gathering);
                else add("prepare",lobbyRuntime_.busy()?"Preparing…":"Prepare game",gathering&&lobbyRuntime_.available()&&!lobbyRuntime_.busy());
            }
        }
        add("leave",lobby_.hosting()?"Close lobby":"Leave lobby");
        if(!lobby_.hosting()&&!s["connection"].toString().isEmpty())add("reconnect","Retry connection");
    }
    return actions;
}
void SocialController::selectLobby(int index) {
    if(index<0||index>=lobbyRows().size())return;
    focusLobbyCard(index);lobbyActionFocus_=0;lobbyDetailFocused_=true;lobbyChatFocused_=false;openLobbyChat();emit changed();
    const auto row=lobbyRows()[index].toMap();
    if(row["kind"]=="own")return;
    if(row["kind"]=="create") {lobbyAction("create");return;}
    menuMode_="lobby";menuTitle_=row["title"].toString();menuDetail_=row["detail"].toString();menuFocus_=0;menu_.clear();menuCommands_.clear();
    const auto start=row["startsAt"].toLongLong();
    if(start>QDateTime::currentSecsSinceEpoch())menuDetail_="Gathering "+QDateTime::fromSecsSinceEpoch(start).toString("ddd d MMM, HH:mm")+". Joining sets a local reminder 15 minutes before.";
    for(const auto& v:lobbyActions()) {const auto a=v.toMap();if(a["enabled"].toBool()){menu_.append(a["label"].toString());menuCommands_.append("lobby:"+a["id"].toString());}}
    menu_.append("Back");menuCommands_.append("cancel");lobbyDetailFocused_=false;emit changed();
}
void SocialController::setLobbyGames(QVariantList games) {if(lobbyGames_==games)return;lobbyGames_=std::move(games);emit changed();}
void SocialController::editLobbySearch() {textPurpose_="lobby-search";emit textRequested("Find a lobby",lobbySearch_,100);}
void SocialController::lobbyAction(QString action) {
    if(action=="leave") {lobby_.leave();lobbySelection_.clear();return;}
    if(snapshot_["state"]!="connected")return;
    if(action=="reminder") {
        const auto s=lobby_.state();const auto id=s["id"].toString();
        const bool ok=lobbyReminders_.contains(id)?lobbyReminders_.cancel(id):lobbyReminders_.accept(s,QDateTime::currentSecsSinceEpoch());
        if(!ok){toastTitle_="Reminder";toastText_="Could not save the reminder on this device.";toastTimer_.start();emit presentationChanged();}
        emit changed();return;
    }
    if((action=="rename"||action=="schedule"||action=="unschedule")&&lobby_.hosting()) {
        const auto s=lobby_.state();
        if(action=="unschedule"){lobby_.setDetails(s["title"].toString(),0);return;}
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
        if(action=="filters") {add("Search","search");add("Category","categories");add("Platform: "+(lobbyPlatform_.isEmpty()?"All":lobbyPlatform_),"platforms");add("Multiplayer: "+(lobbyMode_.isEmpty()?"All":lobbyMode_),"modes");add(lobbyFreeOnly_?"Show full rooms too":"Free seats only","free");add("Clear filters","clear-filters");add("Refresh","refresh");}
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
        for(const auto& v:lobby_.state()["members"].toArray())if(v.toObject()["id"]==snapshot_["userId"].toString())
            lobby_.spectate(!v.toObject()["spectator"].toBool());
        return;
    }
    if(action=="request-play"){lobby_.requestPlay();return;}
    if(action.startsWith("play-answer:")){lobby_.answerPlay(action.section(':',2),action.section(':',1,1));return;}
    if(action.startsWith("play-request:")&&lobby_.hosting()) {
        const auto peer=action.mid(13);menuMode_="lobby";menuTitle_="Wants to play";
        menuDetail_="The current game continues. Controller selection is available after it ends.";menuFocus_=0;
        menu_={"Accept for next game","Later","Decline","Back"};
        menuCommands_={"lobby:play-answer:accept:"+peer,"lobby:play-answer:later:"+peer,"lobby:play-answer:decline:"+peer,"cancel"};emit changed();return;
    }
    if(action=="reconnect") {lobby_.reconnect();return;}
    if(action.startsWith("remove:")) {lobby_.remove(action.mid(7));return;}
    if(action=="browse") {lobbyDetailFocused_=false;lobbyChatFocused_=false;emit changed();return;}
    if(action=="chat-retry") {lobbyChatKey_.clear();syncLobbyChat();return;}
    if(action=="chat-people") {
        menuMode_="lobby";menuTitle_="Lobby people";menuDetail_="Fluxer groups require friendship and permission to add members.";
        menuFocus_=0;menu_.clear();menuCommands_.clear();
        for(const auto& v:lobby_.state()["members"].toArray()) {
            const auto m=v.toObject();if(m["id"]==snapshot_["userId"].toString())continue;
            menu_.append(m["name"].toString());menuCommands_.append("profile:"+m["id"].toString());
        }
        if(menu_.isEmpty()) {menu_.append("Back");menuCommands_.append("cancel");}
        emit changed();return;
    }
    if(action.startsWith("chat:")) {
        const auto channel=action.mid(5);
        for(const auto& v:snapshot_["chats"].toList())if(v.toMap()["id"]==channel&&v.toMap()["kind"]=="groups") {lobby_.setConversation(channel);break;}
        return;
    }
    if(action=="refresh") {emit commandRequested("lobby-directory-refresh",{});return;}
    if(action=="search") {editLobbySearch();return;}
    if(action=="topic") {
        const auto topics=QStringList{QString()}+lobbyDirectory::publicationTopics();
        lobbyTopic_=topics[(topics.indexOf(lobbyTopic_)+1)%topics.size()];lobbySelection_.clear();emit changed();return;
    }
    if(action=="free") {lobbyFreeOnly_=!lobbyFreeOnly_;lobbySelection_.clear();emit changed();return;}
    if(action=="access"&&lobby_.hosting()) {
        const QStringList choices{"private","public","approval"};const auto next=choices[(choices.indexOf(lobby_.state()["access"].toString())+1)%3];
        if(next=="private"){lobby_.setAccess(next);return;}
        menuMode_="lobby";menuTitle_=next=="public"?"Open this lobby?":"Publish this lobby?";
        menuDetail_=next=="public"?"Anyone admitted to this lobby can join its shared conversation, call and game streams.":
            "People you approve can join the shared conversation, call and game streams.";
        menuFocus_=0;menu_={next=="public"?"Open lobby & conversation":"Publish with approval","Back"};
        menuCommands_={"lobby:confirm-access:"+next,"cancel"};emit changed();return;
    }
    if(action.startsWith("confirm-access:")&&lobby_.hosting()) {lobby_.setAccess(action.mid(15));return;}
    if(action.startsWith("publish-topic:")&&lobby_.hosting()) {
        const auto topic=action.mid(14);
        if(lobbyDirectory::publicationTopics().contains(topic)) {lobbyPublicationTopic_=topic;lobby_.setPublicationTopic(topic);lobbyPublishTimer_.start();emit changed();}
        return;
    }
    if(action.startsWith("invite:")) {
        if(lobby_.invite(action.mid(7))) {toastTitle_="Lobby";toastText_="Sending invitation…";toastTimer_.start();emit presentationChanged();}
        return;
    }
    if(action.startsWith("approve:")) {lobby_.approve(action.mid(8));return;}
    if(action.startsWith("seat:")) {
        if(!lobby_.seat(action.mid(5).toInt())){toastTitle_="Lobby";toastText_="That controller is already taken or unavailable.";toastTimer_.start();emit presentationChanged();}
        return;
    }
    if(action.startsWith("game:")) {
        for(const auto& v:lobbyGames_) {
            const auto g=v.toMap();if(g["key"]!=action.mid(5))continue;
            lobby_.selectGame({g["id"].toString(),g["title"].toString(),g["platform"].toString(),g["mode"].toString(),g["seats"].toInt(),QJsonObject::fromVariantMap(g["runtime"].toMap())});break;
        }
        return;
    }
    bool allowed=lobby_.hosting()&&(action=="requests"||action=="publish-topic"||action=="chat-choose"||action=="remove"||(action=="live-joining"&&lobbyRuntime_.liveSupported()));for(const auto& v:lobbyActions())if(v.toMap()["id"]==action&&v.toMap()["enabled"].toBool())allowed=true;
    if(!allowed)return;
    if(action=="create") {lobby_.create();lobbySelection_.clear();}
    else if(action=="community")emit commandRequested("lobby-directory-join",{});
    else if(action=="accept"||action=="decline") {
        const auto invite=lobby_.invitation();
        if(action=="accept")pendingLobbyReminder_={{"id",invite["lobby"]},{"owner",invite["host"]},{"startsAt",invite["startsAt"]}};
        lobby_.answer(action=="accept");lobbySelection_.clear();lobbyDetailFocused_=action=="accept";
    }
    else if(action=="later") {lobbyDetailFocused_=false;emit lobbyDeferred();}
    else if(action=="leave") {lobby_.leave();lobbySelection_.clear();}
    else if(action=="prepare")lobbyRuntime_.prepare();
    else if(action=="join-live")lobbyRuntime_.joinLive();
    else if(action=="live-joining")lobbyRuntime_.setLiveJoining(!lobby_.state()["liveJoining"].toBool());
    else if(action=="join") {const auto row=lobbyRows().value(lobbyFocus()).toMap();
        pendingLobbyReminder_={{"id",row["lobby"].toString()},{"owner",row["owner"].toString()},{"startsAt",row["startsAt"].toLongLong()}};
        if(!lobby_.join(row["owner"].toString(),row["lobby"].toString()))pendingLobbyReminder_={};
        lobbySelection_.clear();lobbyDetailFocused_=true;}
    else if(action=="ready") {
        bool ready=false;for(const auto& m:lobby_.state()["members"].toArray())if(m.toObject()["id"]==snapshot_["userId"].toString())ready=m.toObject()["ready"].toBool();
        lobby_.ready(!ready);
    } else {
        menuMode_="lobby";menuFocus_=0;menu_.clear();menuCommands_.clear();menuDetail_.clear();
        const auto add=[&](QString label,QString command){menu_.append(label);menuCommands_.append("lobby:"+command);};
        if(action=="invite") {menuTitle_="Invite to lobby";for(const auto& v:runtimeFriends()){const auto f=v.toMap();add(f["name"].toString(),"invite:"+f["id"].toString());}}
        else if(action=="requests") {menuTitle_="Join requests";for(const auto& v:lobby_.requests()){const auto p=v.toObject();add(p["name"].toString()+(p["play"].toBool()?" · Wants to play":""),(p["play"].toBool()?"play-request:":"approve:")+p["id"].toString());}}
        else if(action=="remove") {menuTitle_="Remove from lobby";for(const auto& v:lobby_.state()["members"].toArray()) {const auto p=v.toObject();if(p["id"]!=snapshot_["userId"].toString())add(p["name"].toString(),"remove:"+p["id"].toString());}}
        else if(action=="game") {menuTitle_="Choose a game";for(const auto& v:lobbyGames_){const auto g=v.toMap();add(g["title"].toString()+" · "+g["platform"].toString()+" · "+g.value("modeLabel","Emulator multiplayer").toString(),"game:"+g["key"].toString());}}
        else if(action=="seat") {menuTitle_="Choose your controller";for(int i=1;i<=lobby_.state()["game"].toObject()["seats"].toInt();++i)add("P"+QString::number(i),"seat:"+QString::number(i));}
        else if(action=="options") {
            menuTitle_="Lobby options";add("Access: "+lobby_.state()["access"].toString()+" · change","access");
            if(lobbyRuntime_.liveSupported()&&lobby_.state()["phase"]!="launching")
                add(lobby_.state()["liveJoining"].toBool()?"Joining during play: On · disable":"Allow joining during play","live-joining");
            if(lobby_.state()["members"].toArray().size()>1)add("Remove a member","remove");
            add("Category: "+lobbyDirectory::topicName(lobbyPublicationTopic_),"publish-topic");
            if(!lobby_.requests().isEmpty())add("Join requests","requests");
            if(lobby_.state()["conversation"].toString().isEmpty())add("Use an existing group conversation","chat-choose");
            if(!lobbyChatStatus_.isEmpty()&&!lobbyChatBusy_)add("Retry conversation","chat-retry");
            add("Conversation members","chat-people");
            add("Rename lobby","rename");add("Set gathering time","schedule");
            if(lobby_.state()["startsAt"].toInteger())add("Cancel gathering time","unschedule");
        }
        else if(action=="chat-choose") {
            menuTitle_="Conversation for this lobby";
            for(const auto& v:snapshot_["chats"].toList())if(v.toMap()["kind"]=="groups")add(v.toMap()["name"].toString(),"chat:"+v.toMap()["id"].toString());
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
    if(lobbyChatVisible()&&lobbyChatFocused_) {
        if(action==Action::Back||action==Action::Left) {lobbyChatFocused_=false;reading_=false;}
        else if(action==Action::Secondary)compose();
        else if(action==Action::ToggleContinue)call();
        else if(action==Action::ContextMenu||action==Action::LocalAction)contextRow();
        else if(action==Action::Confirm) {if(messages().isEmpty())compose();else selectMessage(messageFocus_);}
        else if(action==Action::Up||action==Action::Down) {
            reading_=true;messageFocus_=qBound(0,messageFocus_+(action==Action::Down?1:-1),qMax(0,int(messages().size())-1));
            if(!messages().isEmpty())retainMessagePosition(messageFocus_,messageFocus_==messages().size()-1);
            if(messageFocus_==0&&action==Action::Up)earlierMessages();
        }
        emit changed();return;
    }
    if(lobbyChatVisible()&&action==Action::Secondary) {focusLobbyChat();compose();return;}
    if(lobbyChatVisible()&&action==Action::ToggleContinue) {focusLobbyChat();call();return;}
    if(lobbyDetailFocused_&&lobbyRows().value(lobbyFocus()).toMap()["kind"]=="own"&&(action==Action::ContextMenu||action==Action::LocalAction)) {
        lobbyAction(lobby_.hosting()?"options":"chat-people");return;
    }
    if(action==Action::ContextMenu||action==Action::LocalAction) {
        lobbyAction("filters");return;
    }
    else if(action==Action::Secondary)editLobbySearch();
    else if(action==Action::ToggleContinue)lobbyAction("refresh");
    else if(action==Action::Back) {
        if(lobbyRows().value(lobbyFocus()).toMap()["kind"]=="invitation") {closeMenu();lobbyAction("later");}
        else lobbyDetailFocused_=false;
    }
    else if(action==Action::Left) {
        if(!lobbyDetailFocused_){turnLobbyPage(-1);return;}
        if(lobbyDetailFocused_&&lobbyActionFocus_%2) --lobbyActionFocus_;
        else lobbyDetailFocused_=false;
    }
    else if(action==Action::Right) {
        if(!lobbyDetailFocused_) {turnLobbyPage(1);return;}
        else if(lobbyActionFocus_%2==0&&lobbyActionFocus_+1<lobbyActions().size())++lobbyActionFocus_;
        else if(lobbyChatVisible())focusLobbyChat();
    }
    else if(action==Action::Up||action==Action::Down) {
        auto focus=lobbyDetailFocused_?lobbyActionFocus_:lobbyFocus();
        const auto size=lobbyDetailFocused_?lobbyActions().size():lobbyRows().size();
        focus=qBound(0,focus+(action==Action::Down?1:-1)*(lobbyDetailFocused_?2:1),qMax(0,int(size)-1));
        if(lobbyDetailFocused_)lobbyActionFocus_=focus;else lobbySelection_=lobbyRows().value(focus).toMap()["selection"].toString();
    } else if(action==Action::Confirm) {
        if(lobbyDetailFocused_)lobbyAction(lobbyActions().value(lobbyActionFocus_).toMap()["id"].toString());
        else selectLobby(lobbyFocus());
    }
    emit changed();
}
void SocialController::publishLobby() {
    if(snapshot_["state"]!="connected")return;
    const auto s=lobby_.state();const bool publish=lobby_.hosting()&&s["access"]!="private";
    if(!publishedLobby_.isEmpty()&&publishedLobby_!=s["id"].toString()) {
            emit commandRequested("lobby-directory-publish",{{"v",1},{"op","close"},{"lobby",publishedLobby_},{"revision",lobby_.nextPublicationRevision()}});
        publishedLobby_.clear();lobbyPublicationVisible_=false;
    }
    if(!publish&&!publishedLobby_.isEmpty()&&lobbyPublicationVisible_) {
        emit commandRequested("lobby-directory-publish",{{"v",1},{"op","withdraw"},{"lobby",publishedLobby_},{"revision",lobby_.nextPublicationRevision()}});
        lobbyPublicationVisible_=false;
    }
    if(!publish)return;
    if(publishedLobby_.isEmpty())publishedLobby_=s["id"].toString();
    lobbyPublicationVisible_=true;
    auto game=s["game"].toObject();game.remove("runtime");const auto phase=s["phase"].toString();int used=0;
    for(const auto& m:s["members"].toArray())if(m.toObject()["seat"].toInt()>0)++used;
    const auto voice=snapshot_["voice"].toMap();
    int streams=0;
    if(voice["state"]=="connected"&&voice["channel"].toString()==s["conversation"].toString())
        streams=voice["streams"].toList().size()+(voice["sharing"].toBool()?1:0);
    emit commandRequested("lobby-directory-publish",{{"v",1},{"op","upsert"},{"lobby",publishedLobby_},{"revision",lobby_.nextPublicationRevision()},
        {"status",phase=="playing"?"playing":game["id"].toString().isEmpty()?"choosing":"gathering"},{"access",s["access"].toString()},
        {"topic",lobbyPublicationTopic_},{"seats",game["seats"].toInt()},
        {"available",qMax(0,game["seats"].toInt()-used)},{"streams",qMin(32,streams)},{"game",game.toVariantMap()},
        {"title",s["title"].toString()},{"startsAt",s["startsAt"].toInteger()},{"members",s["members"].toArray().size()}});
}
}
