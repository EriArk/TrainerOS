#include "SocialController.h"
#include <algorithm>

namespace trainer {
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
            {"canMessage",!id.isEmpty()&&id!=self&&!blocked&&snapshot_["state"]=="connected"},
            {"unread",blocked?0:chat["unread"].toInt()}});
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
