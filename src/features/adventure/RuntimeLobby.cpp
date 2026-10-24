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
    if(!allowed_||game.value("lateJoin")==false||game["transport"]=="netpacket"||game["playMode"].toString("separate")!="separate")return false;
    const auto id=game["id"].toString();QString name;
    const auto parts=id.split('.');
    if(parts.size()==5&&parts[0]=="runtime"&&parts[1]=="retroarch"&&parts[4]=="v1") {
        // Reuse the normal NetPlay platform/core contract, including each
        // core's supported secondary systems. A core name alone is insufficient.
        if(!retroarch::netplaySupported(parts[2],parts[3]))return false;
        name=parts[3];
    } else if(id.startsWith("runtime.fceumm."))name="fceumm";
    else if(id.startsWith("runtime.snes9x."))name="snes9x";
    else if(id.startsWith("runtime.genesis."))name="genesis_plus_gx";
    static const QHash<QString,QString> reviewed{
        {"fceumm","1b13b00d4680394dad8000d5175f97be727107e0945bc9b412da91d70c07b267"},
        {"snes9x","e8d8f5cf93898c7be3ca2590d4d75dc8e9a89b891589c2da1a7917fa27411d21"},
        {"genesis_plus_gx","de8ad971eee89fa5e4e00dd1c048f0a8cc53cd9558b8c143aa022f5d7623d273"},
        {"picodrive","6b0dbe4e21f0e47afbf8a89d59b92e5b48a316dd7c5fae8c74d7351f341de044"},
        {"mednafen_pce_fast","f7686b2bfab6a9ca34d3d48e6eda5c41ae37c430cf5c73b0f256cefd54990a51"},
        {"mednafen_supergrafx","574c4b61e83b1a4034a42cfceed62e09ae8f0809c85527ca979fe9c5e18865e3"},
        {"stella","bf2cf27781f9e393079c9a53c71d8676d638025086955498641723a8e121228a"},
        {"pcsx_rearmed","25bfeb6336d7ef573903775fe53733af974c90edd268cf4fb9afa497e188609d"},
        {"fbneo","ffb37315a609317e1a00bf32c0e3d66c5a848d052d1260f4ab7f42da0f9fb9c3"}};
    const auto core=reviewed.value(name);
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
    if(!netplayRelay::compatible(profile,context["relay"].toString("auto"))) {done(false,"This connection type isn't supported by the selected emulator.");return;}
    if(context["relay"]=="psp-host"&&(players!=2||ppsspp::hostRelayRoot().isEmpty())) {
        done(false,"The handheld relay needs two players and the PSP connection package on both devices. Choose the standard relay otherwise.");return;
    }
    if(requiresLinkedSavePreparation(profile)) {
        if(players<2) {done(false,"This linked-machine mode needs another player.");return;}
        for(int i=1;i<=players;++i)if(!seats.contains(i)) {done(false,"Linked machines need consecutive seats starting at P1.");return;}
    }
    lobbyOperation_=operation;lobbyContext_=context;lobbyGame_=local;lobbyDescriptor_=profile;
    lobbyRevision_=record->revision;lobbyPath_=record->contentPath;
    if(lifecycle_.active()) {
        auto completed=[this,operation,context,done](bool ok,QString error){
            if(operation!=lobbyOperation_)return;
            if(ok)prepare(operation,context,done);else done(false,error);
        };
        // Join is an explicit player action. Only their matching temporary
        // NetPlay child may close without asking about an ordinary save.
        if(context.contains("liveAdmission")&&lifecycle_.exitController().temporaryProgress()
           &&process_.runtimeControls()["game"].toString()==local
           &&QJsonObject::fromVariantMap(process_.runtimeControls()["netplay"].toMap())==profile)
            preparation_.prepareReturn(operation,process_.processId(),completed);
        else preparation_.prepare(operation,completed);
        return;
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
    if(selected["id"]=="runtime.armsx2.lan.v1") {
        ps2Connection_=true;nativeLaunchContext_=context;
        if(!nativeConnection_.start(lobbyContext_["self"].toString(),ticket,ps2lan::bundleRoot()+"/stream_transport",{"stun:stun.l.google.com:19302"},"ps2"))
            fail("Couldn't prepare the PS2 LAN connection. Reopen this game in the lobby.");
        return;
    }
    if(context["relay"]=="psp-host") {
        const auto root=ppsspp::hostRelayRoot();
        if(root.isEmpty()){fail("The PSP connection package is unavailable. Choose the standard relay.");return;}
        pspDirect_=true;nativeLaunchContext_=context;
        if(context["owner"]==lobbyContext_["self"])pspRelay_.start(root+"/psp_relay",{});
        if(!nativeConnection_.start(lobbyContext_["self"].toString(),ticket,root+"/stream_transport",{"stun:stun.l.google.com:19302"},"psp"))
            fail("Couldn't prepare the direct PSP connection. Choose the standard relay in Connection settings.");
        return;
    }
    bindLobbyPlayers(context);
}
void RuntimeMultiplayer::bindLobbyPlayers(const QJsonObject& context) {
    QJsonArray players;for(const auto& m:context["members"].toArray())if(!m.toObject()["spectator"].toBool())players.append(m);
    auto selected=context["game"].toObject()["runtime"].toObject();if(selected.isEmpty())selected=lobbyDescriptor_;
    if(!party_.bindLobby(lobbyTicket_,selected,players,lobbyContext_["self"].toString(),context["owner"].toString(),lobbyContext_["liveAdmission"].toString())) {
        stopNativeConnection();
        relaySelector_.stop();relayStarted_=relayWaiting_=false;relayTicket_.clear();
        auto completion=std::exchange(lobbyCompletion_,{});lobbyTicket_.clear();if(completion)completion(false,"The player seats changed. Prepare again.");
    }
}
void RuntimeMultiplayer::stopNativeConnection() {
    ps2Connection_=false;ps2ConnectionConfig_.clear();
    pspDirect_=false;nativeLaunchContext_={};nativeConnection_.stop();
    if(pspRelay_.state()!=QProcess::NotRunning) {
        pspRelay_.terminate();if(!pspRelay_.waitForFinished(1500)){pspRelay_.kill();pspRelay_.waitForFinished(1000);}
    }
}
void RuntimeMultiplayer::cancel(QString operation) {
    if(operation!=lobbyOperation_)return;
    stopNativeConnection();
    relaySelector_.stop();relayStarted_=relayWaiting_=false;relayTicket_.clear();selectedRelay_.clear();selectedRelayEndpoint_.clear();
    preparation_.cancel(operation);
    lobbyOperation_.clear();lobbyContext_={};lobbyGame_.clear();lobbyDescriptor_={};lobbyCompletion_={};lobbyTicket_.clear();
    if(overlay_.panel()=="lobby-prepare")overlay_.dismissMenu();
    // A live owned process is closed only through the established Home/save flow.
    if(active_)fail("The lobby game changed. Use Home to close the current game safely.");
    else if(partySession_&&party_.active()) {partySession_=false;party_.leave();}
}
}
