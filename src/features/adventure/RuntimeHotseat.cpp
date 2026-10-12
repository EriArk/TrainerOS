#include "RuntimeMultiplayer.h"
#include "features/adventure/AdventureExitPresentation.h"
#include "core/navigation/AdventureLaunchController.h"
#include "platform/process/ProcessService.h"
#include "features/social/SocialController.h"
#include <QJsonDocument>

namespace trainer {
QJsonObject RuntimeMultiplayer::selectedControllerProfile(const QJsonObject& base) const {
    if(controllerMode_=="pass-p1"&&base["controllerModes"].toArray().contains("pass-p1"))
        return multiplayerModeProfile(base,controllerMode_);
    return base;
}
QString RuntimeMultiplayer::controllerModeLabel() const {
    return controllerMode_=="pass-p1"?"Take turns with one controller":"Separate controllers";
}
QString RuntimeMultiplayer::hotseatPlayerName(int slot) const {
    for(const auto& value:party_.state()["members"].toArray()) {
        const auto member=value.toObject();
        if(member["slot"].toInt()==slot)return member["name"].toString();
    }
    return "Player "+QString::number(slot);
}
QVariantList RuntimeMultiplayer::gameMenuActions() const {
    QVariantList rows;
    if(active_&&hotseat()&&process_.runtimeControls().contains("hotseat")) {
        QVariantMap control{{"readOnly",!handoff_.canPass()}};
        if(handoff_.canPass()) {
            control["id"]="multiplayer-pass:"+party_.state()["party"].toString()+":"+QString::number(handoff_.epoch());
            control["label"]="Pass controller to "+hotseatPlayerName(3-handoff_.owner());
            control["detail"]="Your turn is finished";
        } else {
            control["id"]="multiplayer-controller-info";
            control["label"]=handoff_.ready()?hotseatPlayerName(handoff_.owner())+" has the controller":"Connecting controller...";
            control["detail"]=handoff_.ready()?"Home and touch controls remain available":"Waiting for the game to confirm";
        }
        rows.append(control);
    }
    if(canInvite()||!invitationId().isEmpty())rows.append(QVariantMap{{"id","multiplayer"},{"label",menuLabel()}});
    return rows;
}
void RuntimeMultiplayer::setupHotseat() {
    handoff_.requestNativeOwner=[this](int owner,quint64 sequence) {
        const auto packet=QJsonDocument(QJsonObject{{"target",owner},{"sequence",qint64(sequence)}}).toJson(QJsonDocument::Compact);
        return active_&&hotseat()&&dolphin()&&host_&&party_.running()&&
            process_.runtimeCommand("hotseat-pass",QString::fromUtf8(packet));
    };
    handoff_.requestNativeRole=[this](bool playing) {
        return active_&&hotseat()&&party_.running()&&process_.runtimeControls()["hotseat"]=="retroarch-p1-v1"&&
            process_.runtimeCommand(playing?"hotseat-play":"hotseat-spectate");
    };
    connect(&handoff_,&HotseatHandoff::outgoing,this,[this](int slot,QJsonObject control) {
        if(active_&&hotseat()&&!party_.sendRuntimeControl(slot,control))
            fail("The controller handoff connection ended. Invite your friend again.");
    });
    connect(&party_,&GameParty::runtimeControlReceived,this,[this](int sender,QJsonObject control) {
        if(active_&&hotseat())handoff_.receive(sender,control);
    });
    connect(&client_,&retroarch::NetplayClient::hotseatOwnership,this,[this](quint64 sequence,quint32,int owner,bool initial) {
        if(active_&&hotseat()&&!host_)handoff_.observed(sequence,owner,initial);
    });
    connect(&handoff_,&HotseatHandoff::failed,this,[this](QString reason){if(active_&&hotseat())fail(reason);});
    connect(&handoff_,&HotseatHandoff::changed,this,[this] {
        if(!active_||!hotseat())return;
        if(handoff_.ready()) {deadline_=0;timer_.stop();status_="Playing in turns";}
        else status_="Confirming controller handoff...";
        if(socialSurface_&&social_.runtimeSurfaceOpen()&&socialPanel_=="multiplayer-party")show(socialPanel_);
        else if(overlay_.panel()=="multiplayer-party")show("multiplayer-party");
        emit changed();
    });
}
}
