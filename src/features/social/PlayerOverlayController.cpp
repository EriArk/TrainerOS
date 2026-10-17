#include "PlayerOverlayController.h"
#include "SocialController.h"
#include "core/input/TextEntryController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include <QCryptographicHash>
#include <QSettings>
#include <cmath>
#include <QDateTime>

namespace trainer {
PlayerOverlayController::PlayerOverlayController(SocialController& social,TextEntryController& keyboard,
    AdventureExitPresentation& view,QObject* parent):QObject(parent),social_(social),keyboard_(keyboard),view_(view) {
    deadline_.setSingleShot(true);deadline_.setInterval(5000);
    connect(&deadline_,&QTimer::timeout,this,&PlayerOverlayController::clear);
    connect(&social_,&SocialController::changed,this,&PlayerOverlayController::sync);
    connect(&view_,&AdventureExitPresentation::changed,this,&PlayerOverlayController::sync);
    balloonTimer_.setInterval(250);
    connect(&balloonTimer_,&QTimer::timeout,this,[this]{
        const auto now=QDateTime::currentMSecsSinceEpoch();
        const auto before=balloons_.size();balloons_.removeIf([&](const QVariant& v){return v.toMap()["expires"].toLongLong()<=now;});
        if(balloons_.isEmpty())balloonTimer_.stop();if(before!=balloons_.size())emit changed();
    });
    connect(&social_,&SocialController::gameMessage,this,[this](QVariantMap message){
        if(!game_||message["room"]!=social_.gameChat()["room"]||open_)return;
        if(message["spectator"].toBool()&&!spectators_)return;
        if(!message["spectator"].toBool()) {bool found=false;for(const auto& p:players())if(p.toMap()["id"]==message["author"])found=true;if(!found)return;}
        for(const auto& v:balloons_)if(v.toMap()["id"]==message["id"])return;
        balloons_.removeIf([&](const QVariant& v){return v.toMap()["author"]==message["author"];});
        message["expires"]=QDateTime::currentMSecsSinceEpoch()+3800;balloons_.append(message);
        while(balloons_.size()>2)balloons_.removeFirst();balloonTimer_.start();emit changed();
    });
    sync();
}
QVariantList PlayerOverlayController::players() const {return game_?social_.activePlayers():QVariantList();}
void PlayerOverlayController::setGame(qint64 pid) {if(game_==pid)return;clear();balloons_.clear();game_=pid;emit changed();}
bool PlayerOverlayController::validPeer() const {
    const auto context=social_.gameChat();
    return game_&&game_==chatGame_&&social_.accountGeneration()==accountGeneration_&&social_.account()["userId"]==account_
        &&!room_.isEmpty()&&context["room"]==room_&&context["channel"]==channel_;
}
bool PlayerOverlayController::conversationReady() const {
    return open_&&validPeer()&&social_.account()["channel"]==channel_;
}
QString PlayerOverlayController::name() const {return social_.gameChat()["name"].toString();}
QVariantMap PlayerOverlayController::voice() const {
    const auto v=social_.account()["voice"].toMap();
    return !social_.gameChat().isEmpty()&&v["channel"]==social_.gameChat()["channel"]?v:QVariantMap();
}
int PlayerOverlayController::unread() const {return social_.gameChat()["unread"].toInt();}
void PlayerOverlayController::presented(QString message){if(conversationReady()&&view_.ready())social_.presentedGameChat(room_,channel_,message);}
void PlayerOverlayController::setDisplay(QString display){if(display_==display)return;display_=display.left(40);sync();}
void PlayerOverlayController::voiceAction(QString action){
    if(!game_||voice().isEmpty()||(action!="mute"&&action!="output"))return;
    social_.runtimeCommand("voice-"+action);
}
void PlayerOverlayController::mutePlayer(QString id){
    if(voice().isEmpty())return;
    for(const auto& v:players())if(v.toMap()["id"]==id&&!v.toMap()["self"].toBool()){social_.runtimeCommand("voice-local-mute",{{"id",id}});return;}
}
void PlayerOverlayController::toggleSpectators(){
    spectators_=!spectators_;if(!settingsKey_.isEmpty())QSettings().setValue(settingsKey_+"/spectators",spectators_);
    if(!spectators_)balloons_.removeIf([](const QVariant& v){return v.toMap()["spectator"].toBool();});emit changed();
}
void PlayerOverlayController::sync() {
    if(syncing_)return;
    syncing_=true;
    const auto trainer=social_.trainerIdentity();
    const auto key=trainer.isEmpty()?QString():"overlay/players/"+QString::fromLatin1(QCryptographicHash::hash((trainer+display_).toUtf8(),QCryptographicHash::Sha256).toHex());
    if(key!=settingsKey_) {settingsKey_=key;const auto point=key.isEmpty()?QPointF():QSettings().value(key,QPointF()).toPointF();
        spectators_=QSettings().value(key+"/spectators",true).toBool();
        position_={std::isfinite(point.x())?qBound(0.,point.x(),1.):0.,std::isfinite(point.y())?qBound(0.,point.y(),1.):0.};}
    const auto currentRoom=social_.gameChat()["room"].toString();
    if(observedRoom_!=currentRoom){observedRoom_=currentRoom;balloons_.clear();}
    if((open_||pending_)&&!validPeer())clear();
    if(pendingList_&&view_.menuOpen()&&view_.ready()){pendingList_=false;deadline_.stop();showPlayers();}
    if(open_&&!view_.menuOpen())clear();
    if(open_&&keyboard_.isOpen()&&!conversationReady()){social_.preserveText(keyboard_.text());keyboard_.cancel();}
    if(pending_&&view_.menuOpen()&&view_.ready()) {
        pending_=false;deadline_.stop();open_=true;
        view_.setPanel("player-chat",{},{});view_.setContentInput(true);
        if(!social_.acquireGameChat(room_))clear();
    }
    syncing_=false;emit changed();
}
void PlayerOverlayController::openPlayer(QString id) {
    if(id.isEmpty()||open_||pending_||keyboard_.isOpen())return;
    bool admitted=false;for(const auto& p:players())if(p.toMap()["id"]==id)admitted=true;if(!admitted)return;
    room_=social_.gameChat()["room"].toString();channel_=social_.gameChat()["channel"].toString();
    peer_=std::move(id);chatGame_=game_;accountGeneration_=social_.accountGeneration();account_=social_.account()["userId"].toString();
    if(!validPeer()){peer_.clear();return;}
    pending_=true;deadline_.start();sync();if(pending_)emit optionsRequested();
}
void PlayerOverlayController::clear() {
    const bool owned=open_;open_=pending_=pendingList_=false;deadline_.stop();
    if(owned) {
        if(keyboard_.isOpen()){
            if(social_.accountGeneration()==accountGeneration_&&social_.account()["userId"]==account_)social_.preserveText(keyboard_.text());
            keyboard_.cancel();
        }
        view_.setContentInput(false);view_.dismissMenu();
        social_.releaseGameChat();
    }
    peer_.clear();room_.clear();channel_.clear();chatGame_=0;emit changed();
}
void PlayerOverlayController::close() {clear();}
void PlayerOverlayController::setPosition(QPointF point) {
    if(!std::isfinite(point.x())||!std::isfinite(point.y()))return;
    point={qBound(0.,point.x(),1.),qBound(0.,point.y(),1.)};if(point==position_)return;
    position_=point;if(!settingsKey_.isEmpty())QSettings().setValue(settingsKey_,point);emit changed();
}
void PlayerOverlayController::compose() {if(conversationReady()&&view_.ready())social_.composeGameChat(room_,channel_);}
void PlayerOverlayController::send() {if(conversationReady()&&view_.ready())social_.sendGameChat(room_,channel_);}
void PlayerOverlayController::dispatch(Action action) {
    if(!open_||!view_.ready())return;
    if(action==Action::Home){close();return;}
    if(keyboard_.isOpen()) {
        if(action==Action::Back)social_.preserveText(keyboard_.text());
        keyboard_.dispatch(action);return;
    }
    if(action==Action::Back)close();
    else if(action==Action::Confirm)compose();
    else if(action==Action::Secondary)send();
    else if(conversationReady()&&(action==Action::Up||action==Action::Down))
        social_.selectMessage(qBound(0,social_.messageIndex()+(action==Action::Up?-1:1),qMax(0,int(social_.messages().size())-1)));
}
void PlayerOverlayController::requestPlayers() {
    if(players().isEmpty()||open_||pending_||keyboard_.isOpen())return;
    pendingList_=true;deadline_.start();sync();if(pendingList_)emit optionsRequested();
}
void PlayerOverlayController::showPlayers() {
    QVariantList actions;
    for(const auto& v:players()) {const auto p=v.toMap();actions.append(QVariantMap{{"id","player:"+p["id"].toString()},
        {"label","P"+p["seat"].toString()+" · "+p["name"].toString()},{"readOnly",!p["canMessage"].toBool()}});}
    actions.append(QVariantMap{{"id","player-mic"},{"label",voice()["muted"].toBool()?"Unmute microphone":"Mute microphone"},{"readOnly",voice().isEmpty()}});
    actions.append(QVariantMap{{"id","player-deaf"},{"label",voice()["deaf"].toBool()?"Hear conversation":"Deafen"},{"readOnly",voice().isEmpty()}});
    actions.append(QVariantMap{{"id","player-spectators"},{"label",spectators_?"Spectator comments: On":"Spectator comments: Off"}});
    for(const auto& v:players()){const auto p=v.toMap();if(!p["self"].toBool()&&!p["id"].toString().isEmpty())actions.append(QVariantMap{{"id","player-mute:"+p["id"].toString()},{"label",(p["localMuted"].toBool()?"Hear ":"Mute ")+p["name"].toString()},{"readOnly",voice().isEmpty()}});}
    actions.append(QVariantMap{{"id","back"},{"label","Back"}});
    view_.setPanel("players","Playing together",actions);
}
bool PlayerOverlayController::menuAction(const QString& action) {
    if(action=="players")showPlayers();
    else if(action.startsWith("player:"))openPlayer(action.mid(7));
    else if(action=="player-mic"){voiceAction("mute");showPlayers();}
    else if(action=="player-deaf"){voiceAction("output");showPlayers();}
    else if(action=="player-spectators"){toggleSpectators();showPlayers();}
    else if(action.startsWith("player-mute:")){mutePlayer(action.mid(12));showPlayers();}
    else return false;
    return true;
}
}
