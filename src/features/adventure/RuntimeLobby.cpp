#include "RuntimeMultiplayer.h"
#include "core/model/NetplayRelay.h"
#include "features/social/SocialController.h"
#include "core/navigation/AdventureLaunchController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include <QCryptographicHash>
#include <utility>

namespace trainer {
bool RuntimeMultiplayer::opening() const {return active_&&!lifecycle_.active();}
void RuntimeMultiplayer::cancelOpening() {if(opening())fail("Opening cancelled.");}
bool RuntimeMultiplayer::supportsLive(const QJsonObject& game) const {
    // Reviewed ARM64 RetroArch shared-console builds. Never trust a peer's
    // capability flag or infer support for other cores from their platform.
    if(!allowed_||game["transport"]=="netpacket"||game["playMode"].toString("separate")!="separate")return false;
    const auto id=game["id"].toString();QString core;
    if(id=="runtime.retroarch.nes.fceumm.v1"||id.startsWith("runtime.fceumm."))
        core="1b13b00d4680394dad8000d5175f97be727107e0945bc9b412da91d70c07b267";
    else if(id=="runtime.retroarch.snes.snes9x.v1"||id.startsWith("runtime.snes9x."))
        core="e8d8f5cf93898c7be3ca2590d4d75dc8e9a89b891589c2da1a7917fa27411d21";
    else if(id=="runtime.retroarch.megadrive.genesis_plus_gx.v1"||id.startsWith("runtime.genesis."))
        core="de8ad971eee89fa5e4e00dd1c048f0a8cc53cd9558b8c143aa022f5d7623d273";
    if(core.isEmpty())return false;
    for(const auto& local:games_)if(local["runtime"]=="8d57b47c88d416c05145187c8c906b7ab0013edb7e45af69d9b6bb841aea4a24"
        &&local["core"]==core&&supportsMultiplayerSession(local,game))return true;
    return false;
}
bool RuntimeMultiplayer::admitLive(QString ticket,QString peer,QString operation,int seat) {
    if(!active_||!host_||!partySession_||lobbyTicket_!=ticket||!process_.processId()||lifecycle_.state()!="running"
       ||!supportsLive(descriptor_)||QJsonObject::fromVariantMap(process_.runtimeControls()["netplay"].toMap())!=descriptor_)return false;
    QString name;for(const auto& v:social_.lobbyCoordinator()->state()["members"].toArray())if(v.toObject()["id"]==peer)name=v.toObject()["name"].toString();
    return !name.isEmpty()&&party_.admitLobby(ticket,"online:"+peer,name,operation,seat);
}
void RuntimeMultiplayer::releaseLive(QString peer,QString operation) {party_.releaseLobby("online:"+peer,operation);}
void RuntimeMultiplayer::prepare(QString operation,QJsonObject context,Completion done) {
    lobbyProblem_.clear();
    if(!allowed_||active_||party_.active()) {done(false,"Finish the current game party before preparing this lobby.");return;}
    if(context["self"]!=social_.account()["userId"].toString()) {done(false,"The account changed. Reopen the lobby.");return;}
    const auto wanted=context["game"].toObject();QString local;QJsonObject profile;
    const auto offered=wanted["runtime"].toObject();
    if(context.contains("liveAdmission")&&!supportsLive(offered)) {done(false,"This installed runtime cannot join a running game.");return;}
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
    if(!record||record->removed||!record->contentAvailable) {lobbyProblem_="local-content";done(false,"This game is missing or differs from the lobby's edition. Choose your local file to verify it.");return;}
    if(!permitsMultiplayer(library_.artwork(local),profile)){done(false,"This local game's metadata excludes multiplayer.");return;}
    int players=0;QSet<int> seats;bool hostReady=false;
    const int capacity=qMin(4,(offered.isEmpty()?profile:offered)["players"].toInt(2));
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
        preparation_.prepare(operation,[this,operation,context,done](bool ok,QString error){
            if(operation!=lobbyOperation_)return;
            if(ok)prepare(operation,context,done);else done(false,error);
        });return;
    }
    done(true,{});
}
void RuntimeMultiplayer::launch(QString operation,QString ticket,QJsonObject context,Completion done) {
    const auto record=library_.registration(lobbyGame_);
    if(operation!=lobbyOperation_||lifecycle_.active()||active_||party_.active()||!allowed_||games_.value(lobbyGame_)!=lobbyDescriptor_
       ||!record||record->removed||!record->contentAvailable||record->revision!=lobbyRevision_||record->contentPath!=lobbyPath_
       ||context["id"]!=lobbyContext_["lobby"]||context["generation"]!=lobbyContext_["generation"]||context["game"]!=lobbyContext_["game"]
       ||context["relay"].toString("auto")!=lobbyContext_["relay"].toString("auto")
       ||context["ticket"]!=ticket||(lobbyContext_.contains("liveAdmission")?context["phase"]!="playing":context["phase"]!="launching")) {done(false,"The game context changed. Prepare again.");return;}
    QJsonArray players;for(const auto& m:context["members"].toArray())if(!m.toObject()["spectator"].toBool())players.append(m);
    QJsonArray caps;for(const auto& p:games_)caps.append(p);
    party_.configure(trainer_,caps,lobbyDescriptor_,true);
    lobbyTicket_=ticket;lobbyCompletion_=std::move(done);online_=true;partySession_=true;
    auto selected=context["game"].toObject()["runtime"].toObject();if(selected.isEmpty())selected=lobbyDescriptor_;
    relaySelector_.stop();selectedRelay_.clear();selectedRelayEndpoint_.clear();relayWaiting_=false;relayStarted_=false;relayTicket_.clear();
    if(netplayRelay::supported(selected)&&!lobbyContext_.contains("liveAdmission")) {
        relayTicket_=ticket;relayStarted_=true;QStringList peers;
        for(const auto& player:players)peers.append(player.toObject()["id"].toString());
        relaySelector_.start(lobbyContext_["self"].toString(),context["owner"].toString(),peers,context["relay"].toString("auto"));
        if(!relayStarted_)return; // Synchronous context rejection already completed the launch.
    }
    if(!party_.bindLobby(ticket,selected,players,lobbyContext_["self"].toString(),context["owner"].toString(),lobbyContext_["liveAdmission"].toString())) {
        relaySelector_.stop();relayStarted_=relayWaiting_=false;relayTicket_.clear();
        auto completion=std::exchange(lobbyCompletion_,{});lobbyTicket_.clear();if(completion)completion(false,"The player seats changed. Prepare again.");
    }
}
void RuntimeMultiplayer::cancel(QString operation) {
    if(operation!=lobbyOperation_)return;
    relaySelector_.stop();relayStarted_=relayWaiting_=false;relayTicket_.clear();selectedRelay_.clear();selectedRelayEndpoint_.clear();
    preparation_.cancel(operation);
    lobbyOperation_.clear();lobbyContext_={};lobbyGame_.clear();lobbyDescriptor_={};lobbyCompletion_={};lobbyTicket_.clear();
    if(overlay_.panel()=="lobby-prepare")overlay_.dismissMenu();
    // A live owned process is closed only through the established Home/save flow.
    if(active_)fail("The lobby game changed. Use Home to close the current game safely.");
    else if(partySession_&&party_.active()) {partySession_=false;party_.leave();}
}
}
