#include "RuntimeMultiplayer.h"
#include "features/social/SocialController.h"
#include "core/navigation/AdventureLaunchController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include <QCryptographicHash>
#include <utility>

namespace trainer {
void RuntimeMultiplayer::prepare(QString operation,QJsonObject context,Completion done) {
    if(!allowed_||active_||party_.active()) {done(false,"Finish the current game party before preparing this lobby.");return;}
    if(context["self"]!=social_.account()["userId"].toString()) {done(false,"The account changed. Reopen the lobby.");return;}
    const auto wanted=context["game"].toObject();QString local;QJsonObject profile;
    const auto offered=wanted["runtime"].toObject();
    if(!offered.isEmpty()) {
        const auto digest=QString::fromLatin1(QCryptographicHash::hash((offered["id"].toString()+"/"+offered["content"].toString()).toUtf8(),QCryptographicHash::Sha256).toHex());
        if(digest!=wanted["id"]||offered["id"]!=wanted["mode"]) {done(false,"The selected game identity changed. Choose the game again.");return;}
    }
    for(auto it=games_.cbegin();it!=games_.cend();++it) {
        const auto p=it.value();
        const auto digest=QString::fromLatin1(QCryptographicHash::hash((p["id"].toString()+"/"+p["content"].toString()).toUtf8(),QCryptographicHash::Sha256).toHex());
        const bool compatible=offered.isEmpty()?digest==wanted["id"]:supportsMultiplayerSession(p,offered);
        if(compatible&&p["id"]==wanted["mode"]&&multiplayerConnectionAllowed(p,"online")) {
            if(local.isEmpty()||p["content"]==offered["content"]) {local=it.key();profile=p;}
            if(offered.isEmpty()||p["content"]==offered["content"])break;
        }
    }
    const auto record=library_.registration(local);
    if(!record||record->removed||!record->contentAvailable||!permitsMultiplayer(library_.artwork(local),profile)) {done(false,"This exact game and its multiplayer runtime must be installed locally.");return;}
    int players=0;QSet<int> seats;bool hostReady=false;
    const int capacity=qMin(4,(offered.isEmpty()?profile:offered)["players"].toInt());
    for(const auto& v:context["members"].toArray()) {
        const auto m=v.toObject();if(m["spectator"].toBool())continue;
        ++players;const int seat=m["seat"].toInt();
        if(!m["connected"].toBool()||seat<1||seat>capacity||seats.contains(seat)) {done(false,"Every player needs a connected, separate controller seat.");return;}
        seats.insert(seat);if(m["id"]==context["owner"]&&seat==1)hostReady=true;
    }
    if(!hostReady) {done(false,"This runtime needs the organizer on P1. Choose controller seats first.");return;}
    if(requiresLinkedSavePreparation(profile)) {
        if(players<2) {done(false,"This linked-machine mode needs another player.");return;}
        for(int i=1;i<=players;++i)if(!seats.contains(i)) {done(false,"Linked machines need consecutive seats starting at P1.");return;}
    }
    lobbyOperation_=operation;lobbyContext_=context;lobbyGame_=local;lobbyDescriptor_=profile;
    lobbyRevision_=record->revision;lobbyPath_=record->contentPath;
    if(lifecycle_.active()) {
        if(lifecycle_.state()!="running"||!lifecycle_.exitController().available()) {done(false,"Game controls are temporarily unavailable. Save first and try again.");return;}
        lobbyPreparation_=std::move(done);lobbyProcess_=process_.processId();lobbyAccount_=social_.accountGeneration();
        if(overlay_.menuOpen()&&!lifecycle_.minimized())showLobbyExit();else emit invitationOptionsRequested();
        return;
    }
    done(true,{});
}
void RuntimeMultiplayer::showLobbyExit() {
    if(!lobbyPreparation_||lobbyExiting_||!overlay_.menuOpen()||!overlay_.ready()||lifecycle_.minimized())return;
    if(overlay_.panel()=="lobby-prepare")return;
    overlay_.setPanel("lobby-prepare","Save in your current game before preparing the lobby. Closing the game keeps the lobby open.",
        {QVariantMap{{"id","multiplayer-lobby-saved:"+lobbyOperation_},{"label","Saved — prepare game"}},
         QVariantMap{{"id","multiplayer-lobby-later:"+lobbyOperation_},{"label","Keep playing"}}},"multiplayer-lobby-later:"+lobbyOperation_);
}
void RuntimeMultiplayer::launch(QString operation,QString ticket,QJsonObject context,Completion done) {
    const auto record=library_.registration(lobbyGame_);
    if(operation!=lobbyOperation_||lifecycle_.active()||active_||party_.active()||!allowed_||games_.value(lobbyGame_)!=lobbyDescriptor_
       ||!record||record->removed||!record->contentAvailable||record->revision!=lobbyRevision_||record->contentPath!=lobbyPath_
       ||context["id"]!=lobbyContext_["lobby"]||context["generation"]!=lobbyContext_["generation"]||context["game"]!=lobbyContext_["game"]
       ||context["ticket"]!=ticket||context["phase"]!="launching") {done(false,"The game context changed. Prepare again.");return;}
    QJsonArray players;for(const auto& m:context["members"].toArray())if(!m.toObject()["spectator"].toBool())players.append(m);
    QJsonArray caps;for(const auto& p:games_)caps.append(p);
    party_.configure(trainer_,caps,lobbyDescriptor_,true);
    lobbyTicket_=ticket;lobbyCompletion_=std::move(done);online_=true;partySession_=true;
    auto selected=context["game"].toObject()["runtime"].toObject();if(selected.isEmpty())selected=lobbyDescriptor_;
    if(!party_.bindLobby(ticket,selected,players,lobbyContext_["self"].toString(),context["owner"].toString())) {
        auto completion=std::exchange(lobbyCompletion_,{});lobbyTicket_.clear();if(completion)completion(false,"The player seats changed. Prepare again.");
    }
}
void RuntimeMultiplayer::cancel(QString operation) {
    if(operation!=lobbyOperation_)return;
    const bool wasExiting=lobbyExiting_;lobbyExiting_=false;lobbyPreparation_={};
    lobbyOperation_.clear();lobbyContext_={};lobbyGame_.clear();lobbyDescriptor_={};lobbyCompletion_={};lobbyTicket_.clear();
    if(wasExiting)lifecycle_.exitController().cancel();
    if(overlay_.panel()=="lobby-prepare")overlay_.dismissMenu();
    // A live owned process is closed only through the established Home/save flow.
    if(active_)fail("The lobby game changed. Use Home to close the current game safely.");
    else if(partySession_&&party_.active()) {partySession_=false;party_.leave();}
}
}
