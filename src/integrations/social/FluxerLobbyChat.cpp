#include "FluxerSession.h"
#include "LobbyChatIdentity.h"
#include <QJsonArray>
#include <QSettings>
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
    if(!channel.isEmpty()&&!lobbyChatRecords_.value(channel).value("cleanup").toString().isEmpty())return;
    lobbyChatBusy_.insert(lobby);
    const auto failed=[this,lobby](QString error) {
        lobbyChatBusy_.remove(lobby);emit lobbyChatFinished(generation_,lobby,{},error);
    };
    if(channel.isEmpty()) {
        if(!host) {failed("Invite someone to start the conversation.");return;}
        if(lobbyChatUncertain_.contains(lobby)) {
            failed("The conversation may have been created. Choose it from Messages before trying again.");return;
        }
        // Stamp the private, owner-only group before admitting anyone. Guests
        // never receive an unclassified CHANNEL_CREATE for an ordinary group.
        verifiedRequest("POST","/v1/users/@me/channels",{{"recipients",QJsonArray{}}},[this,lobby,peers,failed](Reply reply) {
            if(!lobbyChatBusy_.contains(lobby))return;
            const auto c=reply.body.object();const auto id=c["id"].toString();
            if(reply.status<200||reply.status>=300||!chatId(id)||c["type"]!=3) {
                const bool uncertain=reply.status==0||reply.status>=500||(reply.status>=200&&reply.status<300);
                if(uncertain)lobbyChatUncertain_.insert(lobby);
                failed(uncertain?"The conversation may have been created. Choose it from Messages before trying again.":"Couldn't create the conversation. Check group permissions, then retry.");return;
            }
            channels_[id]=c;++channelRevision_;lobbyChatChannels_[lobby]=id;
            lobbyChatRecords_[id]={{"lobby",lobby},{"managed",true},{"markerPending",true}};saveLobbyChats();
            markLobbyChat(lobby,id,peers);
        });return;
    }
    // A host snapshot is not a provider access grant. Read the channel using the
    // local account before exposing history or enabling any composer/call action.
    request("GET","/v1/channels/"+channel,{},[this,lobby,channel,host,peers,failed](Reply reply) {
        if(!lobbyChatBusy_.contains(lobby))return;
        const auto c=reply.body.object();
        if(reply.status!=200||c["id"]!=channel||c["type"]!=3||!c["guild_id"].toString().isEmpty()) {
            // A new guest can probe before the organizer's recipient-add completes.
            // Only previously verified access can be revoked by this response.
            if((reply.status==403||reply.status==404)&&lobbyChatChannels_.value(lobby)==channel) {
                emit groupMembershipEnded(generation_,channel);emit partyFailed(generation_,"company:"+channel);
            }
            failed("Conversation unavailable. The host can check its members and retry.");return;
        }
        channels_[channel]=c;++channelRevision_;lobbyChatChannels_[lobby]=channel;
        // Binding supplied by our accepted lobby state, never a title match.
        auto record=lobbyChatRecords_.value(channel);record["lobby"]=lobby;lobbyChatRecords_[channel]=record;saveLobbyChats();
        if(record.value("markerPending").toBool()&&host){markLobbyChat(lobby,channel,peers);return;}
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
    int ready=-1;for(int i=0;i<peers.size();++i)if(relationships_.value(peers[i])["type"]==1){ready=i;break;}
    if(ready<0) {
        lobbyChatBusy_.remove(lobby);emit lobbyChatFinished(generation_,lobby,channel,"Connecting a new lobby contact. Check Recently together if it needs attention.");return;
    }
    const auto peer=peers.takeAt(ready);
    if(recipients.size()>=49||relationships_.value(peer)["type"].toInt()==2) {
        lobbyChatBusy_.remove(lobby);emit lobbyChatFinished(generation_,lobby,channel,"Couldn't add a lobby member to the conversation.");return;
    }
    verifiedRequest("PUT","/v1/channels/"+channel+"/recipients/"+peer,{},[this,lobby,channel,peer,peers](Reply reply) {
        if(!lobbyChatBusy_.contains(lobby))return;
        if(reply.status<200||reply.status>=300) {
            lobbyChatBusy_.remove(lobby);publish();emit lobbyChatFinished(generation_,lobby,channel,"Couldn't add a lobby member. Check group permissions, then retry.");return;
        }
        auto c=channels_.value(channel);auto recipients=c["recipients"].toArray();
        bool exists=false;for(const auto& r:recipients)if(r.toObject()["id"]==peer)exists=true;
        if(!exists)recipients.append(QJsonObject{{"id",peer}});
        c["recipients"]=recipients;channels_[channel]=c;++channelRevision_;
        addLobbyChatMembers(lobby,channel,peers);
    });
}

bool FluxerSession::hiddenSocialGroup(const QString& channel) const {
    return channels_.value(channel)["type"]==3 && (lobbyChatRecords_.contains(channel)||!ordinaryGroups_.contains(channel));
}
bool FluxerSession::saveLobbyChats() {
    if(navigationKey_.isEmpty())return bool(transport_);
    QVariantMap records;for(auto it=lobbyChatRecords_.cbegin();it!=lobbyChatRecords_.cend();++it)records[it.key()]=it.value();
    QSettings settings;settings.setValue(navigationKey_+"lobbyChats",records);settings.sync();return settings.status()==QSettings::NoError;
}
void FluxerSession::restoreLobbyChats() {
    lobbyChatRecords_.clear();lobbyChatChannels_.clear();
    if(navigationKey_.isEmpty())return;
    const auto records=QSettings().value(navigationKey_+"lobbyChats").toMap();
    for(auto it=records.cbegin();it!=records.cend();++it){const auto record=it.value().toMap();
        if(!chatId(it.key())||QUuid(record["lobby"].toString()).isNull())continue;
        lobbyChatRecords_[it.key()]=record;lobbyChatChannels_[record["lobby"].toString()]=it.key();
    }
}
void FluxerSession::checkLobbyGroups() {
    if(state_!="connected")return;
    const auto records=lobbyChatRecords_;for(auto it=records.cbegin();it!=records.cend();++it)
        if(!it.value().value("cleanup").toString().isEmpty())cleanupLobbyChat(it.key());
    const auto channels=channels_;
    for(const auto& lobby:lobbyChatBusy_)if(!lobbyChatChannels_.contains(lobby))return;
    for(auto it=channels.cbegin();it!=channels.cend();++it) {
        const auto channel=it.key();
        const auto record=lobbyChatRecords_.value(channel);
        if(it.value()["type"]!=3||record.value("managed").toBool()||record.value("markerPending").toBool()||ordinaryGroups_.contains(channel)||checkingGroups_.contains(channel))continue;
        if(checkingGroups_.size()>=4)break;
        checkingGroups_.insert(channel);
        request("GET","/v1/channels/"+channel+"/messages/pins?limit=50",{},[this,channel](Reply r){
            checkingGroups_.remove(channel);if(r.status!=200||!r.body.object()["items"].isArray()||!channels_.contains(channel))return;
            QJsonObject identity;
            for(const auto& item:r.body.object()["items"].toArray()) {
                const auto candidate=lobbyChatIdentity::identity(item.toObject()["message"].toObject(),channel);
                if(!candidate.isEmpty()){identity=candidate;break;}
            }
            auto record=lobbyChatRecords_.value(channel);
            if(!identity.isEmpty()&&(record.isEmpty()||record["lobby"]==identity["lobby"].toString())) {
                // Discovery hides a service conversation. It alone NEVER grants
                // permission to destroy it; closure additionally requires our
                // accepted room binding and provider ownership on every retry.
                record["lobby"]=identity["lobby"].toString();record["managed"]=true;lobbyChatRecords_[channel]=record;
                lobbyChatChannels_[identity["lobby"].toString()]=channel;saveLobbyChats();
            } else ordinaryGroups_.insert(channel);
            ensureConversation();publish();
        });
    }
}
void FluxerSession::markLobbyChat(QString lobby,QString channel,QStringList peers) {
    if(!lobbyChatBusy_.contains(lobby))return;
    const auto fail=[this,lobby,channel] {lobbyChatBusy_.remove(lobby);emit lobbyChatFinished(generation_,lobby,channel,"Preparing the lobby conversation. Retry after reconnecting.");publish();};
    if(!saveLobbyChats()){fail();return;}
    const auto message=lobbyChatRecords_.value(channel).value("marker").toString();
    if(message.isEmpty()) {
        verifiedRequest("POST","/v1/channels/"+channel+"/messages",{{"content",lobbyChatIdentity::content(lobby,channel,self_)},{"flags",1<<12},{"allowed_mentions",QJsonObject{{"parse",QJsonArray{}}}}},[this,lobby,channel,peers,fail](Reply r){
            if(!lobbyChatBusy_.contains(lobby))return;
            const auto id=r.body.object()["id"].toString();
            if(r.status<200||r.status>=300||!chatId(id)){fail();return;}
            lobbyChatRecords_[channel]["marker"]=id;saveLobbyChats();markLobbyChat(lobby,channel,peers);
        });return;
    }
    verifiedRequest("PUT","/v1/channels/"+channel+"/pins/"+message,{},[this,lobby,channel,peers,fail](Reply r){
        if(!lobbyChatBusy_.contains(lobby))return;
        if(r.status<200||r.status>=300){fail();return;}
        lobbyChatRecords_[channel]["markerPending"]=false;if(!saveLobbyChats()){fail();return;}addLobbyChatMembers(lobby,channel,peers);
    });
}
void FluxerSession::closeLobbyChat(const QVariantMap& args) {
    const auto lobby=args["lobby"].toString(),channel=args["channel"].toString();
    if(QUuid(lobby).isNull()||lobbyChatRecords_.value(channel).value("lobby")!=lobby) {
        emit mutationFinished(generation_,"lobby-chat-close",channel,{},false);return;
    }
    lobbyChatBusy_.remove(lobby);
    lobbyChatRecords_[channel]["cleanup"]=!args["close"].toBool()?"leave":lobbyChatRecords_[channel].value("managed").toBool()?"close":ordinaryGroups_.contains(channel)?"leave":"classify-close";
    if(!saveLobbyChats()){status_="Couldn't save lobby cleanup. Check device storage.";publish();return;}cleanupLobbyChat(channel);
}
void FluxerSession::cleanupLobbyChat(QString channel) {
    if(state_!="connected"||cleaningGroups_.contains(channel))return;
    const auto record=lobbyChatRecords_.value(channel);if(record.value("cleanup").toString().isEmpty())return;
    if(!saveLobbyChats())return;
    cleaningGroups_.insert(channel);
    // A guest may become owner before its background pin probe finishes. Resolve
    // that identity before choosing close-all versus leaving a legacy group.
    if(record.value("cleanup")=="classify-close") {
        request("GET","/v1/channels/"+channel+"/messages/pins?limit=50",{},[this,channel,record](Reply r){
            cleaningGroups_.remove(channel);
            if(r.status==403||r.status==404){lobbyChatRecords_[channel]["cleanup"]="leave";cleanupLobbyChat(channel);return;}
            if(r.status!=200||!r.body.object()["items"].isArray())return;
            bool managed=false;
            for(const auto& item:r.body.object()["items"].toArray()) {
                const auto identity=lobbyChatIdentity::identity(item.toObject()["message"].toObject(),channel);
                if(!identity.isEmpty()&&identity["lobby"].toString()==record["lobby"].toString()){managed=true;break;}
            }
            lobbyChatRecords_[channel]["managed"]=managed;
            lobbyChatRecords_[channel]["cleanup"]=managed?"close":"leave";cleanupLobbyChat(channel);
        });return;
    }
    const auto done=[this,channel] {
        cleaningGroups_.remove(channel);lobbyChatRecords_[channel].remove("cleanup");saveLobbyChats();
        channels_.remove(channel);historyCache_.remove(channel);historyLru_.removeAll(channel);unread_.remove(channel);callNotices_.remove(channel);++channelRevision_;
        if(voiceChannel_==channel)leaveVoice();
        if(channel_==channel){channel_.clear();messages_.clear();messageOrder_.clear();}
        emit mutationFinished(generation_,"lobby-chat-close",channel,{},true);ensureConversation();publish();
    };
    request("GET","/v1/channels/"+channel,{},[this,channel,record,done](Reply r){
        if(r.status==403||r.status==404){done();return;}
        const auto c=r.body.object();
        if(r.status!=200||c["id"]!=channel||c["type"]!=3){cleaningGroups_.remove(channel);return;}
        QString peer;
        if(record.value("cleanup")=="close"&&c["owner_id"]==self_)
            for(const auto& p:c["recipients"].toArray())if(p.toObject()["id"]!=self_){peer=p.toObject()["id"].toString();break;}
        const auto path="/v1/channels/"+channel+(peer.isEmpty()?QString():"/recipients/"+peer);
        verifiedRequest("DELETE",path,{},[this,channel,peer,done](Reply reply){
            cleaningGroups_.remove(channel);
            if(reply.status<200||reply.status>=300)return; // Re-read on the maintenance timer, including an ambiguous timeout.
            if(peer.isEmpty())done();else cleanupLobbyChat(channel);
        });
    });
}
}
