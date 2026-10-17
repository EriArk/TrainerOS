#include "SocialController.h"
#include <algorithm>
#include <QJsonArray>

namespace trainer {
QVariantMap SocialController::gameChat() const {
    const auto s=lobby_.state();
    if(!gameActive_||s["phase"]!="playing"||s["conversation"].toString().isEmpty()||snapshot_["state"]!="connected")return {};
    const auto self=snapshot_["userId"].toString();bool member=false,channel=false;int unread=0;
    for(const auto& v:s["members"].toArray())if(v.toObject()["id"]==self&&!v.toObject()["spectator"].toBool())member=true;
    for(const auto& v:snapshot_["chats"].toList())if(v.toMap()["id"].toString()==s["conversation"].toString()){channel=true;unread=v.toMap()["unread"].toInt();}
    if(!member||!channel)return {};
    return {{"room",s["id"].toString()},{"channel",s["conversation"].toString()},{"name",s["title"].toString()},{"members",s["members"].toArray().toVariantList()},{"unread",unread}};
}
bool SocialController::acquireGameChat(const QString& room){
    const auto context=gameChat();if(context.isEmpty()||context["room"]!=room||!directConversationLease_.isEmpty())return false;
    previousGameChatChannel_=snapshot_["channel"].toString();directConversationLease_="lobby:"+room;selection_.stop();closeMenu();
    emit commandRequested("conversation",{{"id",context["channel"]}});return true;
}
void SocialController::releaseGameChat(){
    if(!directConversationLease_.startsWith("lobby:"))return;
    directConversationLease_.clear();const auto previous=previousGameChatChannel_;previousGameChatChannel_.clear();
    if(!previous.isEmpty())emit commandRequested("conversation",{{"id",previous}});
}
void SocialController::sendGameChat(const QString& room,const QString& channel){
    const auto context=gameChat();if(context["room"]!=room||context["channel"]!=channel||channel!=snapshot_["channel"]||directConversationLease_!="lobby:"+room)return;
    send();
}
void SocialController::composeGameChat(const QString& room,const QString& channel){
    const auto context=gameChat();if(context["room"]!=room||context["channel"]!=channel||channel!=snapshot_["channel"]||directConversationLease_!="lobby:"+room)return;
    textPurpose_="message";textChannel_=channel;emit textRequested("Lobby message",drafts_.value(channel),2000);
}
void SocialController::presentedGameChat(const QString& room,const QString& channel,const QString& message){
    const auto context=gameChat();if(context["room"]!=room||context["channel"]!=channel||snapshot_["channel"]!=channel
        ||directConversationLease_!="lobby:"+room||snapshot_["historyBusy"].toBool()||snapshot_["readTail"]!=message)return;
    emit commandRequested("read",{{"channel",channel},{"message",message}});
}
QVariantMap SocialController::directConversation(const QString& peer) const {
    if(peer.isEmpty()||peer==snapshot_["userId"].toString())return {};
    for(const auto& value:snapshot_["chats"].toList()) {
        const auto row=value.toMap();const auto members=row["members"].toList();
        if(row["kind"]=="chats"&&row["guild"].toString().isEmpty()&&members.size()==1
            &&members.first().toMap()["id"]==peer)return row;
    }
    return {};
}
QVariantList SocialController::activePlayers() const {
    QVariantList result;
    if(!gameActive_||!gameParty_["running"].toBool()||gameParty_["party"].toString().isEmpty())return result;
    const auto self=snapshot_["userId"].toString();
    const auto voice=snapshot_["voice"].toMap();const auto speakers=voice["speakers"].toStringList();
    const auto activeChannel=gameChat()["channel"].toString();
    for(const auto& value:gameParty_["members"].toList()) {
        const auto member=value.toMap();if(!member["ready"].toBool())continue;
        auto peer=member["peer"].toString();
        if(peer.isEmpty())peer=gameParty_["host"].toBool()?"online:"+self:gameParty_["hostPeer"].toString();
        const auto id=peer.startsWith("online:")?peer.mid(7):QString();
        auto chat=directConversation(id);QString avatar=chat["avatar"].toString();bool blocked=false;
        if(!id.isEmpty()&&id==self)avatar=snapshot_["profile"].toMap()["avatar"].toString();
        for(const auto& v:snapshot_["friends"].toList()) {
            const auto person=v.toMap();if(person["id"]!=id)continue;
            if(avatar.isEmpty())avatar=person["avatar"].toString();blocked=person["type"].toInt()==2;
        }
        // Nearby device identities are not Fluxer accounts. Never infer an
        // account from a display name or from an unrelated lobby roster.
        result.append(QVariantMap{{"id",id},{"name",member["name"]},{"seat",member["slot"]},
            {"avatar",blocked?QString():avatar},{"self",!id.isEmpty()&&id==self},
            {"speaking",!blocked&&!id.isEmpty()&&voice["state"]=="connected"&&(activeChannel.isEmpty()||voice["channel"]==activeChannel)&&speakers.contains(id)},
            {"canMessage",!id.isEmpty()&&id!=self&&!blocked&&snapshot_["state"]=="connected"},
            {"localMuted",voice["localMuted"].toStringList().contains(id)},
            {"unread",blocked||!activeChannel.isEmpty()?0:chat["unread"].toInt()}});
    }
    std::sort(result.begin(),result.end(),[](const QVariant& a,const QVariant& b){return a.toMap()["seat"].toInt()<b.toMap()["seat"].toInt();});
    return result;
}
bool SocialController::openDirectConversation(const QString& peer) {
    if(!directConversationLease_.isEmpty()&&directConversationLease_!=peer)return false;
    bool admitted=false;for(const auto& v:activePlayers())if(v.toMap()["id"]==peer&&v.toMap()["canMessage"].toBool())admitted=true;
    if(!admitted)return false;
    directConversationLease_=peer;selection_.stop();closeMenu();
    emit commandRequested("dm",{{"id",peer}});return true;
}
void SocialController::releaseDirectConversation(const QString& peer) {
    if(peer.isEmpty()||directConversationLease_!=peer)return;
    directConversationLease_.clear();openLobbyChat();
}
}
