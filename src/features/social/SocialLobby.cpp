#include "SocialController.h"
#include "integrations/social/LobbyDirectory.h"
#include <QDateTime>

namespace trainer {
void SocialController::openLobby() {
    closeMenu();setFace("lobby");lobbySelection_.clear();lobbyActionFocus_=0;lobbyDetailFocused_=true;emit lobbyRequested();emit changed();
}
QVariantList SocialController::lobbyRows() const {
    QVariantList rows;
    const auto own=lobby_.state();
    rows.append(QVariantMap{{"selection","own"},{"kind",own.isEmpty()?"create":"own"},{"title",own.isEmpty()?"Create lobby":"My lobby"},
        {"detail",own.isEmpty()?"Choose a game together":own["game"].toObject()["title"].toString().isEmpty()?"Choosing a game":own["game"].toObject()["title"].toString()}});
    const auto invitation=lobby_.invitation();
    if(!invitation.isEmpty())rows.append(QVariantMap{{"selection","invite:"+invitation["lobby"].toString()},{"kind","invitation"},{"title","Invitation from "+invitation["name"].toString()},{"detail","Join their lobby"}});
    if(lobbyDirectory()["needsJoin"].toBool())rows.append(QVariantMap{{"selection","community"},{"kind","community"},{"title","Join TrainerOS community"},{"detail","Browse and publish public lobbies"}});
    for(const auto& value:lobbyDirectory()["rows"].toList()) {
        auto row=value.toMap();const auto game=row["game"].toMap();
        if(row["lobby"]==own["id"].toVariant()||row["expires"].toLongLong()<=QDateTime::currentSecsSinceEpoch())continue;
        if(!lobbyTopic_.isEmpty()&&row["topic"]!=lobbyTopic_)continue;
        if(lobbyFreeOnly_&&row["status"]!="choosing"&&row["available"].toInt()==0)continue;
        const auto title=game["title"].toString().isEmpty()?QString("Choosing a game"):game["title"].toString();
        if(!lobbySearch_.isEmpty()&&!(title+" "+row["name"].toString()+" "+game["platform"].toString()).contains(lobbySearch_,Qt::CaseInsensitive))continue;
        row["selection"]=row["lobby"];row["kind"]="public";row["title"]=title;
        row["detail"]=row["name"].toString()+(row["seats"].toInt()?" · "+QString::number(row["available"].toInt())+" seats free":QString());rows.append(row);
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
    else if(kind=="invitation") {add("accept","Accept invitation");add("decline","Decline");}
    else if(kind=="public")add("join",row["access"]=="approval"?"Request to join":"Join lobby",lobby_.state().isEmpty());
    else if(kind=="own") {
        const auto s=lobby_.state();const auto game=s["game"].toObject();const auto members=s["members"].toArray();
        QJsonObject mine;for(const auto& member:members)if(member.toObject()["id"]==snapshot_["userId"].toString())mine=member.toObject();
        const bool gathering=s["phase"]=="choosing"||s["phase"]=="gathering";
        if(lobby_.hosting()) {
            add("invite","Invite friend",gathering);add("game","Choose game",gathering&&!lobbyGames_.isEmpty());
            add("options",lobby_.requests().isEmpty()?"Lobby options":"Options · Join requests");
        }
        if(!game["id"].toString().isEmpty()) {
            add("seat","Controller: "+(mine["seat"].toInt()?"P"+QString::number(mine["seat"].toInt()):QString("Choose")),gathering);
            add("ready",mine["ready"].toBool()?"Not ready":"Ready",gathering&&mine["prepared"].toBool());
        }
        add("leave",lobby_.hosting()?"Close lobby":"Leave lobby");
    }
    return actions;
}
void SocialController::selectLobby(int index) {
    if(index<0||index>=lobbyRows().size())return;
    lobbySelection_=lobbyRows()[index].toMap()["selection"].toString();lobbyActionFocus_=0;lobbyDetailFocused_=true;emit changed();
}
void SocialController::setLobbyGames(QVariantList games) {if(lobbyGames_==games)return;lobbyGames_=std::move(games);emit changed();}
void SocialController::editLobbySearch() {textPurpose_="lobby-search";emit textRequested("Find a lobby",lobbySearch_,100);}
void SocialController::lobbyAction(QString action) {
    if(snapshot_["state"]!="connected")return;
    if(action=="refresh") {emit commandRequested("lobby-directory-refresh",{});return;}
    if(action=="search") {editLobbySearch();return;}
    if(action=="topic") {
        const auto topics=QStringList{QString()}+lobbyDirectory::topics;
        lobbyTopic_=topics[(topics.indexOf(lobbyTopic_)+1)%topics.size()];lobbySelection_.clear();emit changed();return;
    }
    if(action=="free") {lobbyFreeOnly_=!lobbyFreeOnly_;lobbySelection_.clear();emit changed();return;}
    if(action=="access"&&lobby_.hosting()) {
        const QStringList choices{"private","public","approval"};lobby_.setAccess(choices[(choices.indexOf(lobby_.state()["access"].toString())+1)%3]);return;
    }
    if(action.startsWith("publish-topic:")&&lobby_.hosting()) {
        const auto topic=action.mid(14);
        if(lobbyDirectory::topics.contains(topic)&&topic!="choosing") {lobbyPublicationTopic_=topic;lobbyPublishTimer_.start();emit changed();}
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
            lobby_.selectGame({g["id"].toString(),g["title"].toString(),g["platform"].toString(),g["mode"].toString(),g["seats"].toInt()});break;
        }
        return;
    }
    bool allowed=lobby_.hosting()&&(action=="requests"||action=="publish-topic");for(const auto& v:lobbyActions())if(v.toMap()["id"]==action&&v.toMap()["enabled"].toBool())allowed=true;
    if(!allowed)return;
    if(action=="create") {lobby_.create();lobbySelection_.clear();}
    else if(action=="community")emit commandRequested("lobby-directory-join",{});
    else if(action=="accept"||action=="decline") {lobby_.answer(action=="accept");lobbySelection_.clear();}
    else if(action=="leave") {lobby_.leave();lobbySelection_.clear();}
    else if(action=="join") {const auto row=lobbyRows().value(lobbyFocus()).toMap();lobby_.join(row["owner"].toString(),row["lobby"].toString());lobbySelection_.clear();}
    else if(action=="ready") {
        bool ready=false;for(const auto& m:lobby_.state()["members"].toArray())if(m.toObject()["id"]==snapshot_["userId"].toString())ready=m.toObject()["ready"].toBool();
        lobby_.ready(!ready);
    } else {
        menuMode_="lobby";menuFocus_=0;menu_.clear();menuCommands_.clear();menuDetail_.clear();
        const auto add=[&](QString label,QString command){menu_.append(label);menuCommands_.append("lobby:"+command);};
        if(action=="invite") {menuTitle_="Invite to lobby";for(const auto& v:runtimeFriends()){const auto f=v.toMap();add(f["name"].toString(),"invite:"+f["id"].toString());}}
        else if(action=="requests") {menuTitle_="Join requests";for(const auto& v:lobby_.requests()){const auto p=v.toObject();add(p["name"].toString(),"approve:"+p["id"].toString());}}
        else if(action=="game") {menuTitle_="Choose a game";for(const auto& v:lobbyGames_){const auto g=v.toMap();add(g["title"].toString()+" · "+g["platform"].toString(),"game:"+g["key"].toString());}}
        else if(action=="seat") {menuTitle_="Choose your controller";for(int i=1;i<=lobby_.state()["game"].toObject()["seats"].toInt();++i)add("P"+QString::number(i),"seat:"+QString::number(i));}
        else if(action=="options") {
            menuTitle_="Lobby options";add("Access: "+lobby_.state()["access"].toString()+" · change","access");
            add("Category: "+lobbyPublicationTopic_,"publish-topic");
            if(!lobby_.requests().isEmpty())add("Join requests","requests");
        }
        else if(action=="publish-topic") {menuTitle_="Lobby category";for(const auto& topic:lobbyDirectory::topics)if(topic!="choosing")add(topic,"publish-topic:"+topic);}
        if(menu_.isEmpty()) {menu_.append("Close");menuCommands_.append("cancel");}
    }
    emit changed();
}
void SocialController::dispatchLobby(Action action) {
    if(action==Action::ContextMenu||action==Action::LocalAction) {
        menuMode_="lobby";menuFocus_=0;menuTitle_="Lobby filters";menuDetail_.clear();
        menu_={"Find a lobby","Change topic",lobbyFreeOnly_?"Show all lobbies":"Free seats only","Refresh"};
        menuCommands_={"lobby:search","lobby:topic","lobby:free","lobby:refresh"};
    }
    else if(action==Action::Secondary)editLobbySearch();
    else if(action==Action::ToggleContinue)lobbyAction("refresh");
    else if(action==Action::Back)lobbyDetailFocused_=false;
    else if(action==Action::Left) {
        if(lobbyDetailFocused_&&lobbyActionFocus_%2) --lobbyActionFocus_;
        else lobbyDetailFocused_=false;
    }
    else if(action==Action::Right) {
        if(!lobbyDetailFocused_) {lobbyDetailFocused_=true;lobbyActionFocus_=0;}
        else if(lobbyActionFocus_%2==0&&lobbyActionFocus_+1<lobbyActions().size())++lobbyActionFocus_;
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
        emit commandRequested("lobby-directory-publish",{{"v",1},{"op","close"},{"lobby",publishedLobby_},{"revision",qint64(++lobbyPublicationRevision_)}});
        publishedLobby_.clear();lobbyPublicationVisible_=false;
    }
    if(!publish&&!publishedLobby_.isEmpty()&&lobbyPublicationVisible_) {
        emit commandRequested("lobby-directory-publish",{{"v",1},{"op","withdraw"},{"lobby",publishedLobby_},{"revision",qint64(++lobbyPublicationRevision_)}});
        lobbyPublicationVisible_=false;
    }
    if(!publish)return;
    if(publishedLobby_.isEmpty()) {publishedLobby_=s["id"].toString();lobbyPublicationRevision_=0;}
    lobbyPublicationVisible_=true;
    const auto game=s["game"].toObject();const auto phase=s["phase"].toString();int used=0;
    for(const auto& m:s["members"].toArray())if(m.toObject()["seat"].toInt()>0)++used;
    emit commandRequested("lobby-directory-publish",{{"v",1},{"op","upsert"},{"lobby",publishedLobby_},{"revision",qint64(++lobbyPublicationRevision_)},
        {"status",phase=="playing"?"playing":game["id"].toString().isEmpty()?"choosing":"gathering"},{"access",s["access"].toString()},
        {"topic",game["id"].toString().isEmpty()?"choosing":lobbyPublicationTopic_},{"seats",game["seats"].toInt()},
        {"available",qMax(0,game["seats"].toInt()-used)},{"game",game.toVariantMap()}});
}
}
