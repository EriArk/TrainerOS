#include "FluxerSession.h"
#include "LobbyDirectory.h"
#include <QDateTime>
#include <QJsonArray>
#include <QDebug>

namespace trainer {
void FluxerSession::refreshContactHolds() {
    auto held=contactGamePeers_;
    if(online_.state()["stage"]!="idle"&&!online_.peerAccount().isEmpty())held.insert(online_.peerAccount());
    if(!voiceChannel_.isEmpty())for(const auto& v:channels_.value(voiceChannel_)["recipients"].toArray())held.insert(v.toObject()["id"].toString());
    temporaryContacts_.setHeld(std::move(held));
}
void FluxerSession::sendLobby(QString peer,QJsonObject packet) {
    bool valid=!peer.isEmpty()&&peer.size()<=20&&peer!=self_;
    for(const auto c:peer)if(c<'0'||c>'9')valid=false;
    if(!valid||packet["ns"]!="org.traineros.lobby"||packet["v"]!=1||state_!="connected"||self_.isEmpty()||relationships_.value(peer)["type"].toInt()==2||lobbyDeliveries_.size()>=128) {
        emit lobbyFailed(generation_,peer);return;
    }
    const auto frames=LobbyWire::encode(packet,QDateTime::currentSecsSinceEpoch());
    if(frames.isEmpty()) {emit lobbyFailed(generation_,peer);return;}
    // Replace only an untouched snapshot for the same recipient/lobby. Admission
    // and readiness commands, and an in-flight fragmented snapshot, keep order.
    if(packet["kind"]=="state") {
        for(int i=lobbyDeliveries_.size()-1;i>=1;--i) {
            auto& delivery=lobbyDeliveries_[i];
            if(delivery.peer==peer&&delivery.position==0) {
                LobbyWire decoder;QJsonObject previous;
                for(const auto& frame:delivery.frames)previous=decoder.receive(peer,LobbyWire::frame(frame),QDateTime::currentSecsSinceEpoch());
                if(previous["kind"]=="state"&&previous["lobby"]==packet["lobby"]) {delivery.frames=frames;return;}
                // Serial numbers cover every packet for this peer. Moving a
                // newer snapshot ahead of an intervening command makes that
                // command look stale at the receiver (including ICE connect).
                break;
            }
            if(delivery.peer==peer)break;
        }
    }
    lobbyDeliveries_.append({peer,frames,0});
    if(!lobbySending_&&!lobbySendTimer_.isActive())sendNextLobbyFrame();
}
void FluxerSession::sendNextLobbyFrame() {
    if(lobbySending_||lobbyDeliveries_.isEmpty()||state_!="connected")return;
    auto& delivery=lobbyDeliveries_.first();const auto peer=delivery.peer;
    if(relationships_.value(peer)["type"].toInt()==2||LobbyWire::frame(delivery.frames.value(delivery.position))["expires"].toInteger()<=QDateTime::currentSecsSinceEpoch()) {
        lobbyDeliveries_.removeFirst();emit lobbyFailed(generation_,peer);lobbySendTimer_.start();return;
    }
    QString channel;
    for(auto it=channels_.cbegin();it!=channels_.cend();++it) {
        const auto recipients=it.value()["recipients"].toArray();
        if(it.value()["type"]==1&&recipients.size()==1&&recipients.first().toObject()["id"]==peer) {channel=it.key();break;}
    }
    lobbySending_=true;
    if(channel.isEmpty()) {
        request("POST","/v1/users/@me/channels",{{"recipient_id",peer}},[this,peer](Reply reply) {
            lobbySending_=false;const auto channel=reply.body.object();const auto recipients=channel["recipients"].toArray();
            if(reply.status>=200&&reply.status<300&&channel["type"]==1&&recipients.size()==1
               &&recipients.first().toObject()["id"]==peer&&!channel["id"].toString().isEmpty())channels_[channel["id"].toString()]=channel;
            else {qWarning()<<"Lobby DM admission failed, HTTP"<<reply.status;lobbyDeliveries_.removeIf([&](const auto& row){return row.peer==peer;});emit lobbyFailed(generation_,peer);}
            lobbySendTimer_.start();
        });return;
    }
    const auto content=delivery.frames[delivery.position];
    request("POST","/v1/channels/"+channel+"/messages",{{"content",content},{"flags",1<<12},
        {"allowed_mentions",QJsonObject{{"parse",QJsonArray{}}}}},[this,peer](Reply reply) {
            lobbySending_=false;
            if(reply.status>=200&&reply.status<300&&!lobbyDeliveries_.isEmpty()) {
                auto& row=lobbyDeliveries_.first();if(++row.position>=row.frames.size())lobbyDeliveries_.removeFirst();
            } else {qWarning()<<"Lobby packet delivery failed, HTTP"<<reply.status;lobbyDeliveries_.removeIf([&](const auto& row){return row.peer==peer;});emit lobbyFailed(generation_,peer);}
            lobbySendTimer_.start();
        });
}
void FluxerSession::lobbyDirectoryCommand(const QString& operation,const QVariantMap& args) {
    if(state_!="connected"||self_.isEmpty())return;
    if(operation=="lobby-directory-refresh") {
        if(lobbyDirectoryBusy_)return;
        lobbyDirectoryBusy_=true;lobbyDirectoryStatus_="Refreshing lobbies…";lobbyDirectoryPending_.clear();
        ++lobbyDirectoryRevision_;lobbyDirectoryPages_=0;publish();loadLobbyDirectory(0,{});return;
    }
    if(operation=="lobby-directory-ensure"||operation=="lobby-directory-join") {
        if(operation=="lobby-directory-ensure"&&(lobbyMembershipReady_||lobbyMembershipRetry_.isActive()))return;
        if(lobbyDirectoryBusy_)return;
        lobbyMembershipRetry_.stop();
        if(guilds_.contains(lobbyDirectory::guild)){lobbyMembershipReady_=true;lobbyDirectoryNeedsJoin_=false;lobbyDirectoryCommand("lobby-directory-refresh",{});return;}
        lobbyDirectoryBusy_=true;lobbyDirectoryStatus_="Joining TrainerOS community…";publish();
        request("POST","/v1/invites/"+lobbyDirectory::invite,{},[this](Reply reply) {
            lobbyDirectoryBusy_=false;
            if(reply.status>=200&&reply.status<300) {lobbyMembershipReady_=true;lobbyDirectoryNeedsJoin_=false;lobbyDirectoryCommand("lobby-directory-refresh",{});}
            else {
                const bool retry=reply.status==0||reply.status==429||reply.status>=500;
                lobbyDirectoryNeedsJoin_=true;
                lobbyDirectoryStatus_=retry?"Connecting to the lobby community. We'll retry automatically.":"The lobby community is unavailable for this account.";
                if(retry)lobbyMembershipRetry_.start(qMax(30000,qMin(reply.retrySeconds,3600)*1000));
                publish();
            }
        });return;
    }
    if(operation=="lobby-directory-publish") {
        const auto packet=QJsonObject::fromVariantMap(args);
        const auto content=lobbyDirectory::encode(packet);
        if(packet["v"]!=1||content.isEmpty()||(packet["op"]!="upsert"&&packet["op"]!="close"&&packet["op"]!="withdraw"))return;
        const auto send=[this,content](QString channel) {
            request("POST","/v1/channels/"+channel+"/messages",{{"content",content},{"flags",1<<12},
                {"allowed_mentions",QJsonObject{{"parse",QJsonArray{}}}}},[this](Reply result) {
                if(result.status<200||result.status>=300) {lobbyDirectoryStatus_="Lobby announcement wasn't delivered. It will be retried.";publish();}
            });
        };
        if(!lobbyBotChannel_.isEmpty()) {send(lobbyBotChannel_);return;}
        request("POST","/v1/users/@me/channels",{{"recipient_id",lobbyDirectory::bot}},[this,send](Reply reply) {
            const auto channel=reply.body.object();const auto recipients=channel["recipients"].toArray();
            if(reply.status<200||reply.status>=300||channel["type"]!=1||recipients.size()!=1||recipients.first().toObject()["id"]!=lobbyDirectory::bot) {
                lobbyDirectoryStatus_="Couldn't reach the lobby directory.";publish();return;
            }
            lobbyBotChannel_=channel["id"].toString();send(lobbyBotChannel_);
        });
    }
}
void FluxerSession::loadLobbyDirectory(int channel,QString before) {
    if(channel>=lobbyDirectory::channels.size()) {
        lobbyDirectoryRows_=lobbyDirectoryPending_;lobbyDirectoryPending_.clear();lobbyDirectoryBusy_=false;
        lobbyMembershipReady_=true;lobbyMembershipRetry_.stop();lobbyDirectoryNeedsJoin_=false;lobbyDirectoryStatus_=lobbyDirectoryRows_.isEmpty()?"No open lobbies yet.":QString();publish();return;
    }
    const auto revision=lobbyDirectoryRevision_;
    const auto path="/v1/channels/"+lobbyDirectory::channels[channel]+"/messages?limit=100"+(before.isEmpty()?QString():"&before="+before);
    request("GET",path,{},[this,revision,channel,before](Reply reply) {
        if(revision!=lobbyDirectoryRevision_)return;
        if(reply.status!=200||!reply.body.isArray()) {
            lobbyDirectoryBusy_=false;lobbyDirectoryNeedsJoin_=reply.status==403||reply.status==404;
            lobbyDirectoryStatus_=lobbyDirectoryNeedsJoin_?"The lobby community is unavailable for this account.":"Couldn't refresh lobbies. Try again.";
            lobbyDirectoryPending_.clear();publish();
            if(lobbyDirectoryNeedsJoin_&&!lobbyMembershipReady_)lobbyDirectoryCommand("lobby-directory-ensure",{});
            return;
        }
        const auto messages=reply.body.array();
        for(const auto& message:messages) {
            auto row=lobbyDirectory::listing(message.toObject(),QDateTime::currentSecsSinceEpoch());
            if(row.isEmpty())continue;
            row["topic"]=lobbyDirectory::topics[channel]=="choosing"?QString("other"):lobbyDirectory::topics[channel];
            const auto id=row["lobby"].toString();
            const auto previous=lobbyDirectoryPending_.value(id);
            if(previous.isEmpty()||(previous["owner"]==row["owner"]?row["revision"].toInteger()>previous["revision"].toInteger():row["listedAt"].toInteger()>previous["listedAt"].toInteger()))lobbyDirectoryPending_[id]=row;
        }
        if(++lobbyDirectoryPages_>100||lobbyDirectoryPending_.size()>5000) {
            lobbyDirectoryBusy_=false;lobbyDirectoryPending_.clear();lobbyDirectoryStatus_="The directory is too large to load. Try again later.";publish();return;
        }
        const auto last=messages.isEmpty()?QString():messages.last().toObject()["id"].toString();
        bool validId=!last.isEmpty()&&last.size()<=20;for(const auto c:last)if(c<'0'||c>'9')validId=false;
        if(messages.size()==100&&validId&&last!=before)loadLobbyDirectory(channel,last);
        else loadLobbyDirectory(channel+1,{});
    });
}
QVariantMap FluxerSession::lobbyDirectoryState() const {
    QVariantList rows;const auto now=QDateTime::currentSecsSinceEpoch();
    for(const auto& row:lobbyDirectoryRows_)if(row["expires"].toInteger()>now)rows.append(row.toVariantMap());
    return {{"rows",rows},{"busy",lobbyDirectoryBusy_},{"needsJoin",lobbyDirectoryNeedsJoin_},{"status",lobbyDirectoryStatus_}};
}
}
