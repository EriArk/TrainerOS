#include "PlayerOverlayController.h"
#include "SocialController.h"
#include "core/input/TextEntryController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include <QCryptographicHash>
#include <QSettings>
#include <cmath>

namespace trainer {
PlayerOverlayController::PlayerOverlayController(SocialController& social,TextEntryController& keyboard,
    AdventureExitPresentation& view,QObject* parent):QObject(parent),social_(social),keyboard_(keyboard),view_(view) {
    deadline_.setSingleShot(true);deadline_.setInterval(5000);
    connect(&deadline_,&QTimer::timeout,this,&PlayerOverlayController::clear);
    connect(&social_,&SocialController::changed,this,&PlayerOverlayController::sync);
    connect(&view_,&AdventureExitPresentation::changed,this,&PlayerOverlayController::sync);
    connect(&social_,&SocialController::messageQueued,this,[this](const QString& channel){
        if(open_&&conversationReady()&&social_.directConversation(peer_)["id"]==channel)close();
    });
    sync();
}
QVariantList PlayerOverlayController::players() const {return game_?social_.activePlayers():QVariantList();}
void PlayerOverlayController::setGame(qint64 pid) {if(game_==pid)return;clear();game_=pid;emit changed();}
bool PlayerOverlayController::validPeer() const {
    if(!game_||game_!=chatGame_||social_.accountGeneration()!=accountGeneration_||social_.account()["userId"]!=account_)return false;
    for(const auto& v:players())if(v.toMap()["id"]==peer_&&v.toMap()["canMessage"].toBool())return true;
    return false;
}
bool PlayerOverlayController::conversationReady() const {
    const auto chat=social_.directConversation(peer_);
    return open_&&validPeer()&&!chat.isEmpty()&&chat["id"]==social_.account()["channel"];
}
QString PlayerOverlayController::name() const {
    for(const auto& v:players())if(v.toMap()["id"]==peer_)return v.toMap()["name"].toString();
    return {};
}
void PlayerOverlayController::sync() {
    if(syncing_)return;
    syncing_=true;
    const auto trainer=social_.trainerIdentity();
    const auto key=trainer.isEmpty()?QString():"overlay/players/"+QString::fromLatin1(QCryptographicHash::hash(trainer.toUtf8(),QCryptographicHash::Sha256).toHex());
    if(key!=settingsKey_) {settingsKey_=key;const auto point=key.isEmpty()?QPointF():QSettings().value(key,QPointF()).toPointF();
        position_={std::isfinite(point.x())?qBound(0.,point.x(),1.):0.,std::isfinite(point.y())?qBound(0.,point.y(),1.):0.};}
    if((open_||pending_)&&!validPeer())clear();
    if(pendingList_&&view_.menuOpen()&&view_.ready()){pendingList_=false;deadline_.stop();showPlayers();}
    if(open_&&!view_.menuOpen())clear();
    if(open_&&keyboard_.isOpen()&&!conversationReady()){social_.preserveText(keyboard_.text());keyboard_.cancel();}
    if(pending_&&view_.menuOpen()&&view_.ready()) {
        pending_=false;deadline_.stop();open_=true;
        view_.setPanel("player-chat",{},{});view_.setContentInput(true);
        if(!social_.openDirectConversation(peer_))clear();
    }
    syncing_=false;emit changed();
}
void PlayerOverlayController::openPlayer(QString id) {
    if(id.isEmpty()||open_||pending_||keyboard_.isOpen())return;
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
        social_.releaseDirectConversation(peer_);
    }
    peer_.clear();chatGame_=0;emit changed();
}
void PlayerOverlayController::close() {clear();}
void PlayerOverlayController::setPosition(QPointF point) {
    if(!std::isfinite(point.x())||!std::isfinite(point.y()))return;
    point={qBound(0.,point.x(),1.),qBound(0.,point.y(),1.)};if(point==position_)return;
    position_=point;if(!settingsKey_.isEmpty())QSettings().setValue(settingsKey_,point);emit changed();
}
void PlayerOverlayController::compose() {if(conversationReady()&&view_.ready())social_.compose();}
void PlayerOverlayController::send() {if(conversationReady()&&view_.ready())social_.sendDraft();}
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
    actions.append(QVariantMap{{"id","player-position"},{"label","Move player bubbles"}});
    actions.append(QVariantMap{{"id","back"},{"label","Back"}});
    view_.setPanel("players","Playing together",actions);
}
bool PlayerOverlayController::menuAction(const QString& action) {
    if(action=="players")showPlayers();
    else if(action.startsWith("player:"))openPlayer(action.mid(7));
    else if(action=="player-position")view_.setPanel("player-position","Player bubbles",{
        QVariantMap{{"id","player-corner:0"},{"label","Top left"}},QVariantMap{{"id","player-corner:1"},{"label","Top right"}},
        QVariantMap{{"id","player-corner:2"},{"label","Bottom left"}},QVariantMap{{"id","player-corner:3"},{"label","Bottom right"}},
        QVariantMap{{"id","players"},{"label","Back"}}},"players");
    else if(action.startsWith("player-corner:")) {const auto corner=action.mid(14).toInt();setPosition({double(corner%2),double(corner/2)});showPlayers();}
    else return false;
    return true;
}
}
