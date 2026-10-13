#include "FluxerSession.h"
#include <QJsonArray>
#include <QUuid>

namespace trainer {
namespace {
bool chatId(const QString& value) {
    if(value.isEmpty()||value.size()>20)return false;
    for(const auto c:value)if(c<'0'||c>'9')return false;
    return true;
}
}
void FluxerSession::lobbyChatCommand(const QVariantMap& args) {
    const auto lobby=args["lobby"].toString();
    if(state_!="connected"||self_.isEmpty()||QUuid(lobby).isNull()||lobbyChatBusy_.contains(lobby))return;
    auto channel=args["channel"].toString();const bool host=args["host"].toBool();
    const auto peers=args["peers"].toStringList();
    if(peers.size()>31)return;
    QSet<QString> unique;
    for(const auto& peer:peers) {
        if(!chatId(peer)||peer==self_||unique.contains(peer)||relationships_.value(peer)["type"].toInt()==2) {
            emit lobbyChatFinished(generation_,lobby,{},"A conversation member is unavailable.");return;
        }
        unique.insert(peer);
    }
    if(channel.isEmpty())channel=lobbyChatChannels_.value(lobby);
    if(!channel.isEmpty()&&!chatId(channel))return;
    lobbyChatBusy_.insert(lobby);
    const auto failed=[this,lobby](QString error) {
        lobbyChatBusy_.remove(lobby);emit lobbyChatFinished(generation_,lobby,{},error);
    };
    if(channel.isEmpty()) {
        if(!host||peers.isEmpty()) {failed("Invite someone to start the conversation.");return;}
        if(lobbyChatUncertain_.contains(lobby)) {
            failed("The conversation may have been created. Choose it from Messages before trying again.");return;
        }
        QJsonArray recipients;for(const auto& peer:peers)recipients.append(peer);
        verifiedRequest("POST","/v1/users/@me/channels",{{"recipients",recipients}},[this,lobby,failed](Reply reply) {
            if(!lobbyChatBusy_.contains(lobby))return;
            const auto c=reply.body.object();const auto id=c["id"].toString();
            if(reply.status<200||reply.status>=300||!chatId(id)||c["type"]!=3) {
                const bool uncertain=reply.status==0||reply.status>=500||(reply.status>=200&&reply.status<300);
                if(uncertain)lobbyChatUncertain_.insert(lobby);
                failed(uncertain?"The conversation may have been created. Choose it from Messages before trying again.":"Couldn't create the conversation. Add lobby members as Fluxer friends, check group permissions, then retry.");return;
            }
            channels_[id]=c;++channelRevision_;lobbyChatChannels_[lobby]=id;lobbyChatBusy_.remove(lobby);
            publish();emit lobbyChatFinished(generation_,lobby,id,{});
        });return;
    }
    // A host snapshot is not a provider access grant. Read the channel using the
    // local account before exposing history or enabling any composer/call action.
    request("GET","/v1/channels/"+channel,{},[this,lobby,channel,host,peers,failed](Reply reply) {
        if(!lobbyChatBusy_.contains(lobby))return;
        const auto c=reply.body.object();
        if(reply.status!=200||c["id"]!=channel||c["type"]!=3||!c["guild_id"].toString().isEmpty()) {
            failed("Conversation unavailable. The host can check its members and retry.");return;
        }
        channels_[channel]=c;++channelRevision_;lobbyChatChannels_[lobby]=channel;
        if(host) {addLobbyChatMembers(lobby,channel,peers);return;}
        lobbyChatBusy_.remove(lobby);publish();emit lobbyChatFinished(generation_,lobby,channel,{});
    });
}
void FluxerSession::addLobbyChatMembers(QString lobby,QString channel,QStringList peers) {
    if(!lobbyChatBusy_.contains(lobby))return;
    const auto recipients=channels_.value(channel)["recipients"].toArray();
    peers.removeIf([&](const QString& peer){for(const auto& r:recipients)if(r.toObject()["id"]==peer)return true;return false;});
    if(peers.isEmpty()) {
        lobbyChatBusy_.remove(lobby);publish();emit lobbyChatFinished(generation_,lobby,channel,{});return;
    }
    const auto peer=peers.takeFirst();
    if(recipients.size()>=49||relationships_.value(peer)["type"].toInt()==2) {
        lobbyChatBusy_.remove(lobby);emit lobbyChatFinished(generation_,lobby,channel,"Couldn't add a lobby member to the conversation.");return;
    }
    verifiedRequest("PUT","/v1/channels/"+channel+"/recipients/"+peer,{},[this,lobby,channel,peer,peers](Reply reply) {
        if(!lobbyChatBusy_.contains(lobby))return;
        if(reply.status<200||reply.status>=300) {
            lobbyChatBusy_.remove(lobby);publish();emit lobbyChatFinished(generation_,lobby,channel,"Couldn't add a lobby member. Add them as a Fluxer friend, check group permissions, then retry.");return;
        }
        auto c=channels_.value(channel);auto recipients=c["recipients"].toArray();
        bool exists=false;for(const auto& r:recipients)if(r.toObject()["id"]==peer)exists=true;
        if(!exists)recipients.append(QJsonObject{{"id",peer}});
        c["recipients"]=recipients;channels_[channel]=c;++channelRevision_;
        addLobbyChatMembers(lobby,channel,peers);
    });
}
}
