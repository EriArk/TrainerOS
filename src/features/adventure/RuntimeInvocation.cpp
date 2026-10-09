#include "RuntimeMultiplayer.h"
#include "core/model/GamePlayers.h"
#include "features/social/SocialController.h"
#include "core/navigation/AdventureLaunchController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include "platform/process/ProcessService.h"
#include <QJsonArray>
#include <QDateTime>

namespace trainer {
void RuntimeMultiplayer::beginTogether(QString game,QString peer,bool company) {
    cancelInvocation();
    if(!allowed_) {emit notice("Connect and finish the current operation before inviting players.");return;}
    if(lifecycle_.active()) {
        if(!game.isEmpty() && game!=lifecycle_.adventureId()) {emit notice("Another game is running. Exit it from Game Options before starting this invitation.");return;}
        if(!canInvite() || !lifecycle_.minimized()) {emit notice("Play together is unavailable in the current game state.");return;}
        invocationGame_=lifecycle_.adventureId();invocationPeer_=peer;invocationCompany_=company;
        invocationOwner_=social_.account()["userId"].toString();invocationAccountGeneration_=social_.accountGeneration();
        invocationProcess_=process_.processId();invocationExpires_=QDateTime::currentMSecsSinceEpoch()+10000;
        liveInvocation_=true;emit liveOptionsRequested();return;
    }
    if(party_.active()){action("multiplayer-party",true);return;}
    invocationGame_=std::move(game);invocationPeer_=std::move(peer);invocationCompany_=company;
    socialSurface_=false;
    invocationOwner_=social_.account()["userId"].toString();++invocationGeneration_;
    invocationAccountGeneration_=social_.accountGeneration();
    invocationDescriptor_=games_.value(invocationGame_);
    const auto record=library_.registration(invocationGame_);invocationRevision_=record?record->revision:0;
    showInvocation();
}
void RuntimeMultiplayer::resumeLiveInvocation() {
    if(!liveInvocation_ || !overlay_.menuOpen() || !overlay_.ready() || lifecycle_.minimized())return;
    liveInvocation_=false;
    if(invocationExpires_<QDateTime::currentMSecsSinceEpoch() || invocationProcess_!=process_.processId()
       || invocationGame_!=lifecycle_.adventureId() || invocationOwner_!=social_.account()["userId"].toString()
       || invocationAccountGeneration_!=social_.accountGeneration() || !canInvite())return;
    if(invocationPeer_.isEmpty()){action("multiplayer");return;}
    const auto candidates=invocationCompany_?social_.runtimeCompanies():social_.runtimeFriends();
    for(const auto& value:candidates)if(value.toMap()["id"]==invocationPeer_) {
        action((invocationCompany_?"multiplayer-company:":"multiplayer-friend:")+invocationPeer_);return;
    }
    emit notice("This player is no longer available.");
}
void RuntimeMultiplayer::showInvocation() {
    social_.closeMenu(); // Explicit navigation to a different invocation step.
    QVariantList rows;
    const auto prefix="together:"+QString::number(invocationGeneration_)+":";
    const auto row=[&](const QString& id,const QString& label,const QString& detail){return QVariantMap{{"id",prefix+id},{"label",label},{"detail",detail}};};
    QString caption;
    if(invocationGame_.isEmpty()) {
        caption="Choose a game for this invitation";
        for(auto it=games_.cbegin();it!=games_.cend();++it) {
            const auto record=library_.registration(it.key());
            if(record && record->contentAvailable && !record->removed)rows.append(row("game:"+it.key(),record->adventure.title,"Supported multiplayer"));
        }
        if(rows.isEmpty())caption="No installed multiplayer games are available";
    } else if(invocationDescriptor_.isEmpty())caption="Multiplayer is not available for this game";
    else {
        caption=invocationDescriptor_["label"].toString()+" · choose players";
        for(const auto& value:social_.runtimeFriends()) {const auto person=value.toMap();rows.append(row("online:"+person["id"].toString(),person["name"].toString(),"Online friend"));}
        for(const auto& value:social_.runtimeCompanies()) {const auto group=value.toMap();rows.append(row("company:"+group["id"].toString(),group["name"].toString(),"Group"));}
        for(const auto& value:nearby_.peers()) {const auto person=value.toMap();rows.append(row("nearby:"+person["id"].toString(),person["name"].toString(),"Nearby"));}
        if(rows.isEmpty())caption="Connect a friend or nearby Trainer to invite";
    }
    rows.append(row("cancel","Back",{}));
    social_.setRuntimeSurface("Play together",caption,rows);
}
bool RuntimeMultiplayer::contextAction(const QString& actionId) {
    if(actionId.startsWith("together-friend:")){beginTogether({},actionId.mid(16),false);return true;}
    if(actionId.startsWith("together-company:")){beginTogether({},actionId.mid(17),true);return true;}
    if(!actionId.startsWith("together:"))return false;
    if(actionId.section(':',1,1).toULongLong()!=invocationGeneration_ || invocationAccountGeneration_!=social_.accountGeneration() || invocationOwner_!=social_.account()["userId"].toString())return true;
    const auto kind=actionId.section(':',2,2),target=actionId.section(':',3);
    if(kind=="cancel"){++invocationGeneration_;social_.closeMenu();return true;}
    if(!allowed_ || lifecycle_.active() || party_.active()) {emit notice("The game context changed. Reopen Play together.");return true;}
    if(kind=="game") {
        const auto record=library_.registration(target);
        if(!record || record->removed || !record->contentAvailable || !games_.contains(target))return true;
        invocationGame_=target;invocationRevision_=record->revision;invocationDescriptor_=games_.value(target);
        if(invocationPeer_.isEmpty()){showInvocation();return true;}
        return contextAction("together:"+QString::number(invocationGeneration_)+":"+(invocationCompany_?"company:":"online:")+invocationPeer_);
    }
    const auto record=library_.registration(invocationGame_);
    if(!record || record->revision!=invocationRevision_ || record->removed || !record->contentAvailable || games_.value(invocationGame_)!=invocationDescriptor_ || invocationDescriptor_.isEmpty() || !permitsMultiplayer(library_.artwork(invocationGame_),invocationDescriptor_)) {
        emit notice("This game changed. Reopen Play together.");return true;
    }
    const auto candidates=kind=="company"?social_.runtimeCompanies():kind=="online"?social_.runtimeFriends():kind=="nearby"?nearby_.peers():QVariantList{};
    bool found=false;for(const auto& value:candidates)if(value.toMap()["id"]==target)found=true;
    if(!found){emit notice("This player is no longer available.");return true;}
    QJsonArray supported;for(const auto& descriptor:games_)supported.append(descriptor);
    party_.configure(trainer_,supported,invocationDescriptor_,true);
    game_=invocationGame_;descriptor_=invocationDescriptor_;socialSurface_=true;online_=kind!="nearby";partySession_=true;
    if(kind=="company") {const auto access=social_.companyAccess(target);party_.openCompany(target,access["policy"].toString(),access["allowed"].toStringList());}
    else party_.invite((online_?"online:":"nearby:")+target);
    ++invocationGeneration_;social_.closeMenu();show("multiplayer-party");return true;
}
}
