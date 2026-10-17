#include "SocialController.h"
#include "integrations/social/LobbyDirectory.h"
#include <QDateTime>

namespace trainer {
namespace {
LobbySession::Game gameFrom(const QVariantMap& g) {
    return {g["id"].toString(),g["title"].toString(),g["platform"].toString(),g["mode"].toString(),g["seats"].toInt(),QJsonObject::fromVariantMap(g["runtime"].toMap())};
}
}
void SocialController::createAnotherLobby() {
    if(snapshot_["state"]!="connected")return;
    closeMenu();lobbyDraftReturn_=myLobbiesOpen_;myLobbiesOpen_=false;
    lobbyDraft_={{"title",snapshot_["name"].toString().left(65)+"'s lobby"},{"topic","other"},{"access","private"},{"startsAt",0}};
    lobbyDraftAccount_=snapshot_["userId"].toString();lobbyDraftPeer_.clear();lobbyDraftConversation_.clear();
    lobbyChoices_.clear();lobbyCreationError_.clear();lobbyDraftFocus_=0;lobbyCreateOpen_=true;emit changed();
}
QVariantMap SocialController::lobbyDraft() const {
    auto result=lobbyDraft_;result["focus"]=lobbyDraftFocus_;result["error"]=lobbyCreationError_;
    result["category"]=lobbyDirectory::topicName(lobbyDraft_["topic"].toString());
    result["accessLabel"]=lobbyDraft_["access"]=="public"?"Open":lobbyDraft_["access"]=="approval"?"By request":"Invite only";
    result["gameTitle"]=lobbyChoices_.isEmpty()?QString("Choosing Together"):lobbyChoices_.size()==1?lobbyChoices_[0].toMap()["title"].toString():QString("Vote on %1 games").arg(lobbyChoices_.size());
    result["gameDetail"]=lobbyChoices_.size()==1?lobbyChoices_[0].toMap()["modeLabel"].toString():QString("Choose together in your room");
    return result;
}
void SocialController::cancelLobbyDraft() {
    lobbyVoteOpen_=false;lobbyCreateOpen_=false;lobbyPickerOpen_=false;lobbyDraft_.clear();lobbyChoices_.clear();lobbyCreationError_.clear();
    lobbyDraftAccount_.clear();lobbyDraftPeer_.clear();lobbyDraftConversation_.clear();
    myLobbiesOpen_=lobbyDraftReturn_;lobbyDraftReturn_=false;emit changed();
}
void SocialController::lobbyDraftAction(QString action) {
    if(!lobbyCreateOpen_)return;
    lobbyCreationError_.clear();
    if(action=="cancel"){cancelLobbyDraft();return;}
    if(action=="create"){submitLobbyDraft();return;}
    if(action=="game"){lobbyDraftFocus_=1;openLobbyPicker();return;}
    if(action=="name") {lobbyDraftFocus_=0;textPurpose_="lobby-draft-name";emit textRequested("Lobby name",lobbyDraft_["title"].toString(),80);}
    else if(action=="time") {
        lobbyDraftFocus_=4;textPurpose_="lobby-draft-time";
        const auto start=lobbyDraft_["startsAt"].toLongLong();
        emit textRequested("Gathering time: YYYY-MM-DD HH:mm",QDateTime::fromSecsSinceEpoch(start?start:QDateTime::currentSecsSinceEpoch()+3600).toString("yyyy-MM-dd HH:mm"),16);
    } else if(action=="now"){lobbyDraftFocus_=4;lobbyDraft_["startsAt"]=0;}
    else if(action=="category"||action=="access") {
        lobbyDraftFocus_=action=="category"?2:3;menuMode_="lobby";menuTitle_=action=="category"?"Category":"Who can join?";
        menuDetail_.clear();menu_.clear();menuCommands_.clear();menuFocus_=0;
        const auto values=action=="category"?lobbyDirectory::publicationTopics():QStringList{"private","public","approval"};
        for(const auto& value:values){menu_.append(action=="category"?lobbyDirectory::topicName(value):value=="private"?"Invite only":value=="public"?"Open":"By request");menuCommands_.append("lobby:create-field:"+action+":"+value);}
    } else if(action.startsWith("category:")) {const auto value=action.mid(9);if(lobbyDirectory::publicationTopics().contains(value))lobbyDraft_["topic"]=value;}
    else if(action.startsWith("access:")) {const auto value=action.mid(7);if(QStringList{"private","public","approval"}.contains(value))lobbyDraft_["access"]=value;}
    emit changed();
}
void SocialController::submitLobbyDraft() {
    if(!lobbyCreateOpen_)return;
    const auto fail=[&](QString error){lobbyCreationError_=error;emit changed();};
    if(snapshot_["state"]!="connected"||lobbyDraftAccount_!=snapshot_["userId"].toString()){fail("Reconnect to your account before creating the lobby.");return;}
    const auto title=lobbyDraft_["title"].toString().trimmed();const auto starts=lobbyDraft_["startsAt"].toLongLong();const auto now=QDateTime::currentSecsSinceEpoch();
    if(title.isEmpty()||title.size()>80){fail("Give your lobby a name.");return;}
    for(const auto c:title)if(c.category()==QChar::Other_Control){fail("Use a single-line lobby name.");return;}
    if(starts&&(starts<=now||starts>now+366*86400LL)){fail("Choose a future gathering time.");return;}
    for(const auto& choice:lobbyChoices_)if(!lobbyGames_.contains(choice)){fail("A selected game or mode changed. Choose it again.");return;}
    // No room, group, invitation or public listing exists before this point.
    // Keep publication private until every supplied field is applied.
    auto* room=lobby_.createRoom();if(!room){fail("Could not create the lobby. Please try again.");return;}
    if(!room->setDetails(title,starts)){room->leave();fail("Could not apply the lobby details.");return;}
    room->setPublicationTopic(lobbyDraft_["topic"].toString());
    bool gameApplied=true;
    if(lobbyChoices_.size()==1)gameApplied=room->selectGame(gameFrom(lobbyChoices_[0].toMap()));
    else if(lobbyChoices_.size()>1){QJsonArray choices;for(const auto& c:lobbyChoices_){auto g=c.toMap();g.remove("cover");g.remove("key");g.remove("adventure");g.remove("collections");choices.append(QJsonObject::fromVariantMap(g));}gameApplied=room->proposeVote(choices);}
    if(!gameApplied){room->leave();fail("The selection changed. Choose available games and modes again.");return;}
    room->setAccess(lobbyDraft_["access"].toString());
    viewedLobbyId_=room->state()["id"].toString();
    if(starts)lobbyReminders_.accept(room->state(),now);
    const auto peer=lobbyDraftPeer_,conversation=lobbyDraftConversation_;
    lobbyDraftReturn_=false;cancelLobbyDraft();openLobby();syncMyLobbies();
    if(!peer.isEmpty())room->invite(peer);
    if(!conversation.isEmpty())for(const auto& c:snapshot_["chats"].toList())if(c.toMap()["id"]==conversation)
        for(const auto& v:c.toMap()["members"].toList()){const auto id=v.toMap()["id"].toString();if(id!=snapshot_["userId"].toString())room->invite(id);}
}
QVariantMap SocialController::lobbyPicker() const {
    auto ballot=viewedLobby().state()["vote"].toObject().toVariantMap();QVariantList choices;
    for(const auto& v:ballot["choices"].toList()){auto choice=v.toMap();for(const auto& local:lobbyGames_)if(local.toMap()["id"]==choice["id"]&&local.toMap()["mode"]==choice["mode"]){choice["cover"]=local.toMap()["cover"];break;}choices.append(choice);}
    ballot["choices"]=choices;
    return {{"voteOpen",lobbyVoteOpen_&&!viewedLobby().state()["vote"].toObject().isEmpty()},{"vote",ballot},{"host",viewedLobby().hosting()},{"voteFocus",lobbyVoteFocus_},{"self",snapshot_["userId"]},{"open",lobbyPickerOpen_},{"focus",lobbyPickerFocus_},{"rail",lobbyPickerRail_},{"railFocused",lobbyPickerRailFocused_},
        {"choices",lobbyChoices_},{"error",lobbyCreationError_},{"search",lobbyPickerSearch_}};
}
QVariantList SocialController::lobbyPickerCategories() const {
    QVariantList result{QVariantMap{{"id",""},{"title","All games"}}};QStringList platforms;
    for(const auto& v:lobbyGames_){const auto p=v.toMap()["platform"].toString();if(!platforms.contains(p))platforms.append(p);}
    if(lobbyCollections)for(const auto& v:lobbyCollections()){const auto c=v.toMap();if(c["id"]!="multiverse")result.append(QVariantMap{{"id","collection:"+c["id"].toString()},{"title",c["name"]}});}
    platforms.sort();for(const auto& p:platforms)result.append(QVariantMap{{"id",p},{"title",p.toUpper()}});
    return result;
}
QVariantList SocialController::lobbyPickerRows() const {
    QVariantList result;for(const auto& v:lobbyGames_){auto g=v.toMap();
        if(lobbyPickerCategory_.startsWith("collection:")) {
            if(!lobbyCollectionContains||!lobbyCollectionContains(lobbyPickerCategory_.mid(11),g.value("adventure",g["key"]).toString()))continue;
        } else if(!lobbyPickerCategory_.isEmpty()&&g["platform"]!=lobbyPickerCategory_)continue;
        if(!lobbyPickerSearch_.isEmpty()&&!g["title"].toString().contains(lobbyPickerSearch_,Qt::CaseInsensitive))continue;
        g["selected"]=lobbyChoices_.contains(v);result.append(g);
    }return result;
}
void SocialController::openLobbyPicker() {
    if(!lobbyCreateOpen_&&!viewedLobby().hosting())return;
    if(!lobbyCreateOpen_){lobbyChoices_.clear();lobbyPickerRoom_=viewedLobby().state()["id"].toString();lobbyPickerGeneration_=quint64(viewedLobby().state()["generation"].toInteger());}
    lobbyPickerSearch_.clear();lobbyPickerCategory_.clear();lobbyPickerRail_=lobbyPickerFocus_=0;lobbyPickerRailFocused_=true;lobbyCreationError_.clear();lobbyPickerOpen_=true;emit changed();
}
void SocialController::lobbyPickerAction(QString action) {
    if(action=="vote-close"){lobbyVoteOpen_=false;emit changed();return;}
    if(!lobbyPickerOpen_)return;
    lobbyCreationError_.clear();
    if(action=="cancel"){lobbyPickerOpen_=false;if(!lobbyCreateOpen_)lobbyChoices_.clear();emit changed();return;}
    if(action=="search"){textPurpose_="lobby-picker-search";emit textRequested("Find a game",lobbyPickerSearch_,100);return;}
    if(action=="clear"){lobbyChoices_.clear();emit changed();return;}
    if(action.startsWith("category:")){lobbyPickerCategory_=action.mid(9);lobbyPickerFocus_=0;lobbyPickerRailFocused_=false;const auto rows=lobbyPickerCategories();for(int i=0;i<rows.size();++i)if(rows[i].toMap()["id"]==lobbyPickerCategory_)lobbyPickerRail_=i;}
    else if(action.startsWith("pick:")) {
        for(const auto& v:lobbyGames_)if(v.toMap()["key"]==action.mid(5)){
            int selected=-1;for(int i=0;i<lobbyChoices_.size();++i)if(lobbyChoices_[i].toMap()["id"]==v.toMap()["id"]&&lobbyChoices_[i].toMap()["mode"]==v.toMap()["mode"])selected=i;
            if(selected>=0)lobbyChoices_.removeAt(selected);
            else if(lobbyChoices_.size()<4)lobbyChoices_.append(v);else lobbyCreationError_="Choose up to four game and mode combinations.";
            break;
        }
    } else if(action=="apply") {
        for(const auto& v:lobbyChoices_)if(!lobbyGames_.contains(v)){lobbyCreationError_="A selected game is no longer available.";emit changed();return;}
        if(!lobbyCreateOpen_) {
            auto& room=viewedLobby();const auto s=room.state();
            if(!room.hosting()||s["id"].toString()!=lobbyPickerRoom_||quint64(s["generation"].toInteger())!=lobbyPickerGeneration_){lobbyCreationError_="The room changed. Reopen the game picker.";emit changed();return;}
            bool ok=false;
            if(lobbyChoices_.size()==1)ok=room.selectGame(gameFrom(lobbyChoices_[0].toMap()));
            else if(lobbyChoices_.size()>1){QJsonArray choices;for(const auto& v:lobbyChoices_){auto g=v.toMap();g.remove("cover");g.remove("key");g.remove("adventure");g.remove("collections");choices.append(QJsonObject::fromVariantMap(g));}ok=room.proposeVote(choices);}
            if(!ok){lobbyCreationError_="Choose a game, or two to four candidates for a vote.";emit changed();return;}
        }
        lobbyPickerOpen_=false;
    }
    emit changed();
}
void SocialController::dispatchLobbyCreation(Action action) {
    if(lobbyVoteOpen_) {
        const auto vote=viewedLobby().state()["vote"].toObject();if(vote.isEmpty()){lobbyVoteOpen_=false;emit changed();return;}
        if(action==Action::Back){lobbyVoteOpen_=false;emit changed();return;}
        if(action==Action::Left||action==Action::Up||action==Action::Right||action==Action::Down)lobbyVoteFocus_=qBound(0,lobbyVoteFocus_+((action==Action::Right||action==Action::Down)?1:-1),int(vote["choices"].toArray().size())-1);
        if(action==Action::Confirm||action==Action::ToggleContinue)lobbyAction(QString(action==Action::Confirm?"ballot:":"winner:")+vote["id"].toString()+":"+QString::number(lobbyVoteFocus_));
        emit changed();return;
    }
    if(lobbyPickerOpen_) {
        const auto rows=lobbyPickerRows(),categories=lobbyPickerCategories();
        if(action==Action::Back){lobbyPickerAction("cancel");return;}
        if(action==Action::ContextMenu){lobbyPickerAction("search");return;}
        if(action==Action::ToggleContinue){lobbyPickerAction("apply");return;}
        if(action==Action::Left)lobbyPickerRailFocused_=true;
        else if(action==Action::Right)lobbyPickerRailFocused_=false;
        else if(action==Action::Up||action==Action::Down){auto& index=lobbyPickerRailFocused_?lobbyPickerRail_:lobbyPickerFocus_;index=qBound(0,index+(action==Action::Down?1:-1),qMax(0,int((lobbyPickerRailFocused_?categories:rows).size())-1));}
        else if(action==Action::Confirm){if(lobbyPickerRailFocused_)lobbyPickerAction("category:"+categories.value(lobbyPickerRail_).toMap()["id"].toString());else if(!rows.isEmpty())lobbyPickerAction("pick:"+rows.value(lobbyPickerFocus_).toMap()["key"].toString());}
    } else {
        if(action==Action::Back){cancelLobbyDraft();return;}
        if(action==Action::Up||action==Action::Down)lobbyDraftFocus_=qBound(0,lobbyDraftFocus_+(action==Action::Down?1:-1),5);
        else if(action==Action::Confirm)lobbyDraftAction(QStringList{"name","game","category","access","time","create"}[lobbyDraftFocus_]);
        else if(action==Action::Left&&lobbyDraftFocus_==4)lobbyDraftAction("now");
    }
    emit changed();
}
}
