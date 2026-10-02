#include "FluxerSession.h"
#include "AltchaProof.h"
#include "CommunityIdentity.h"
#include "platform/storage/EncryptedCredentials.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QWebSocket>
#include <QJsonArray>
#include <QDateTime>
#include <QLocale>
#include <QRandomGenerator>
#include <QUuid>
#include <QCryptographicHash>
#include <QPointer>
#include <QSettings>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <qt6keychain/keychain.h>
#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#endif
#include <algorithm>

namespace trainer {
namespace {
bool idValid(const QString& id) {
    return !id.isEmpty() && id.size() <= 20 && id.front() != QLatin1Char('0')
        && std::all_of(id.begin(),id.end(),[](QChar c){return c >= QLatin1Char('0') && c <= QLatin1Char('9');});
}
QString avatar(const QJsonObject& user) {
    const auto id=user["id"].toString(),hash=user["avatar"].toString();
    // Fluxer also returns short asset hashes; do not assume Discord's length.
    static const QRegularExpression valid("^(a_)?[a-fA-F0-9]{1,128}$");
    if(!idValid(id)||!valid.match(hash).hasMatch())return {};
    return "https://fluxerusercontent.com/avatars/"+id+"/"+hash+".png?size=64";
}
constexpr int responseLimit = 2 * 1024 * 1024;
bool newer(const QString& a, const QString& b) {
    return !a.isEmpty() && (a.size() == b.size() ? a > b : a.size() > b.size());
}
}
FluxerSession::FluxerSession(QObject* parent) : QObject(parent) {
    voiceHeartbeat_.setInterval(3000);voiceDeadline_.setSingleShot(true);voiceDeadline_.setInterval(20000);
    historySave_.setSingleShot(true);historySave_.setInterval(1200);
    connect(&historySave_,&QTimer::timeout,this,&FluxerSession::saveHistoryCache);
    connect(&voiceHeartbeat_,&QTimer::timeout,this,[this]{voiceWrite({{"op","ping"}});});
    connect(&voiceDeadline_,&QTimer::timeout,this,[this]{leaveVoice();voiceStatus_="Couldn't connect the call. Try again.";publish();});
    connect(&voiceProcess_,&QProcess::readyReadStandardError,this,[this]{voiceProcess_.readAllStandardError();});
    connect(&voiceProcess_,&QProcess::readyReadStandardOutput,this,[this]{
        voiceBuffer_+=voiceProcess_.readAllStandardOutput();if(voiceBuffer_.size()>65536){leaveVoice();return;}
        while(voiceBuffer_.contains('\n')) {
            const auto at=voiceBuffer_.indexOf('\n');const auto event=QJsonDocument::fromJson(voiceBuffer_.left(at)).object();voiceBuffer_.remove(0,at+1);
            const auto type=event["event"].toString();
            if(type=="connected"||type=="reconnected"){
                voiceState_="connected";voiceStatus_=voiceMuted_?"Connected · Microphone off":"Connected · Microphone on";voiceDeadline_.stop();
                voiceWrite({{"op","mute"},{"value",voiceMuted_}});voiceWrite({{"op","deaf"},{"value",voiceDeaf_}});voiceStateUpdate();
            }
            else if(type=="reconnecting"){voiceState_="reconnecting";voiceStatus_="Reconnecting audio...";voiceDeadline_.start(45000);}
            else if(type=="participants")voiceParticipants_=qBound(1,event["count"].toInt(),100);
            else if(type=="input-error"){voiceMuted_=true;voiceWrite({{"op","mute"},{"value",true}});voiceStateUpdate();voiceStatus_="Microphone unavailable";}
            else if(type=="output-error")voiceStatus_="Audio output unavailable";
            else if(type=="output-restored")voiceStatus_=voiceMuted_?"Connected · Microphone off":"Connected · Microphone on";
            else if(type=="failed"){qWarning()<<"Voice worker failed:"<<event["reason"].toString().left(48);leaveVoice();voiceStatus_="Couldn't connect audio. Try again.";}
            publish();
        }
    });
    connect(&voiceProcess_,&QProcess::finished,this,[this]{if(!voiceReplacing_&&!voiceChannel_.isEmpty()){leaveVoice();voiceStatus_="Call ended";publish();}});
    connect(&voiceProcess_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart){leaveVoice();voiceStatus_="Voice service unavailable";publish();}});
    onlineSendTimer_.setSingleShot(true);onlineSendTimer_.setInterval(1000);
    connect(&onlineSendTimer_,&QTimer::timeout,this,&FluxerSession::sendOnline);
    connect(&online_,&OnlineLink::changed,this,[this]{
        if(online_.state()["stage"]=="idle") {
            // Keep an explicit final close, but never send queued exchange data
            // after cancellation/block/expiry. In-flight delivery is uncertain.
            onlineQueue_.removeIf([](const auto& entry){return OnlineLink::decode(entry.second)["kind"]!="close";});
        }
        publish();
    });
    connect(&online_,&OnlineLink::established,this,[this](QString self,QString peer,QString name,QString activity,bool initiator){emit onlineEstablished(generation_,self,peer,name,activity,initiator);});
    connect(&online_,&OnlineLink::frameReceived,this,[this](QJsonObject frame){emit onlineFrame(generation_,frame);});
    connect(&online_,&OnlineLink::ended,this,[this]{emit onlineEnded(generation_);});
    connect(&online_,&OnlineLink::outgoing,this,[this](QString channel,QString content){
        if(onlineQueue_.size()>=32){onlineQueue_.clear();online_.close("Online activity queue is full. Reconnect to recover.");return;}
        onlineQueue_.append({channel,content});if(!onlineSending_&&!onlineSendTimer_.isActive())sendOnline();
    });
}
void FluxerSession::bindOnline() {
    QSettings settings;
    const auto key="social/endpoint/"+owner_+"/"+self_;
    auto endpoint=settings.value(key).toString();
    if(QUuid(endpoint).isNull()){endpoint=QUuid::createUuid().toString(QUuid::WithoutBraces);settings.setValue(key,endpoint);}
    online_.bind(self_,endpoint);
}
void FluxerSession::sendOnline() {
    if(onlineSending_||onlineQueue_.isEmpty()||self_.isEmpty())return;
    const auto entry=onlineQueue_.takeFirst();onlineSending_=true;const auto revision=onlineSendRevision_;
    const auto nonce=QUuid::createUuid().toString(QUuid::Id128);
    request("POST","/v1/channels/"+entry.first+"/messages",{{"content",entry.second},{"nonce",nonce},{"flags",1<<12},
        {"allowed_mentions",QJsonObject{{"parse",QJsonArray{}}}}},[this,revision](Reply r){
        if(revision!=onlineSendRevision_)return;onlineSending_=false;
        if(r.status<200||r.status>=300){onlineQueue_.clear();online_.close("Online delivery stopped. Reconnect to recover; nothing is automatically resent.");return;}
        onlineSendTimer_.start();
    });
}
FluxerSession::~FluxerSession() { reset(); }
void FluxerSession::reset() {
    retainHistory();saveHistoryCache();historySave_.stop();
    historyCache_.clear();historyLru_.clear();historyFile_.clear();historyAnchor_.clear();
    ++reviewRevision_;reviewBusy_=false;reviewIdentity_.clear();reviewRows_.clear();ownReview_.clear();
    leaveVoice();calls_.clear();callNotices_.clear();voiceStatus_.clear();
    ++epoch_;
    attachmentBusy_=false;attachmentReply_=nullptr;notificationSound_=true;
    profile_={};profileStatus_.clear();profileBusy_=false;
    voiceInput_.clear();voiceOutput_.clear();voiceVolume_=100;
    ++onlineSendRevision_;onlineSendTimer_.stop();onlineQueue_.clear();onlineSending_=false;online_.bind({},{});
    delete proof_; proof_=nullptr;
    readThrough_.clear();quietThrough_.clear();readRevision_.clear();muted_.clear();mentions_.clear();
    readsReady_=ackBusy_=doNotDisturb_=false;privatePreviews_=true;
    mutationBusy_=channelsLoading_=false;++channelRevision_;
    ++guildListRevision_;
    communityOnly_=communityChecking_=false;communityChecked_.clear();communityMarked_.clear();
    communityRevision_.clear();communityChannels_.clear();communityStatus_.clear();communityInvite_.clear();
    if(encryptedCredentials_)encryptedCredentials_->cancel();
    credentialLoading_=false;
    ++historyRequest_;historyBusy_=historyMore_=historyPast_=false;
    if (poll_) poll_->stop();
    if (heartbeat_) heartbeat_->stop();
    if (reconnect_) reconnect_->stop();
    if (socket_) { socket_->disconnect(this); socket_->abort(); socket_->deleteLater(); socket_ = nullptr; }
    if (network_) {
        for(auto* reply:network_->findChildren<QNetworkReply*>())reply->abort();
        network_->deleteLater(); network_ = nullptr;
    }
    channelsLoaded_=friendsLoaded_=openingDm_=openingGuild_=false;preferred_.clear();navigationKey_.clear();
    ++searchRevision_;searchResults_.clear();searchText_.clear();searchStatus_.clear();searching_=false;searchTotal_=searchOffset_=0;
    token_.clear(); self_.clear(); name_.clear(); code_.clear(); pollSecret_.clear();
    gatewaySession_.clear(); channel_.clear(); relationships_.clear(); channels_.clear();
    messages_.clear(); messageOrder_.clear(); pendingNonces_.clear(); unread_.clear(); guilds_.clear(); guild_.clear();
    // Teardown emits voice/Link state changes while the old channel still
    // exists. Do not let those intermediate snapshots repopulate its cache.
    historyCache_.clear();historyLru_.clear();historySave_.stop();historyAnchor_.clear();
    polling_ = refreshing_ = awaitingAck_ = remembered_ = false;
    reconnectAttempt_ = 0; sequence_ = 0; blockedUntil_ = 0;
}
void FluxerSession::stop() { reset(); }
void FluxerSession::setOwner(QString owner, quint64 generation) {
    reset(); owner_ = std::move(owner); generation_ = generation;
    state_ = "signed-out"; status_ = "Sign in"; publish();
    if (!owner_.isEmpty() && !transport_) credential();
}
QString FluxerSession::label(const QJsonObject& user) const {
    auto name = user["global_name"].toString();
    return (name.isEmpty() ? user["username"].toString() : name).left(100);
}
void FluxerSession::publish() {
    retainHistory();
    QVariantList friends, chats, messages, communities;
    QString voiceName;
    for (auto it = relationships_.cbegin(); it != relationships_.cend(); ++it) {
        const auto user = it.value()["user"].toObject(); const int type = it.value()["type"].toInt();
        friends.append(QVariantMap{{"id",it.key()},{"name",label(user)},{"avatar",avatar(user)}, {"type",type},
            {"detail",type==3?"Friend request":type==4?"Request sent":type==2?"Blocked":"Friend"}});
    }
    std::sort(friends.begin(),friends.end(),[](const QVariant& a,const QVariant& b){
        const auto aa=a.toMap(),bb=b.toMap();
        if ((aa["type"].toInt()==3)!=(bb["type"].toInt()==3)) return aa["type"].toInt()==3;
        return aa["name"].toString().localeAwareCompare(bb["name"].toString())<0;
    });
    for (auto it = channels_.cbegin(); it != channels_.cend(); ++it) {
        const auto c=it.value(); QString name=c["name"].toString();
        if(name.isEmpty()) { QStringList names; for(const auto& v:c["recipients"].toArray()) if(v.toObject()["id"]!=self_) names<<label(v.toObject()); name=names.join(", "); }
        if(it.key()==voiceChannel_)voiceName=name.isEmpty()?QString("Voice call"):name.left(100);
        const auto recipients=c["recipients"].toArray();
        const auto user=recipients.size()==1?recipients.first().toObject():QJsonObject();
        QString preview;const auto cached=historyCache_.value(it.key())["rows"].toArray();
        if(!privatePreviews_)for(auto i=cached.size();i>0;--i){const auto m=cached[i-1].toObject();if(OnlineLink::decode(m["content"].toString()).isEmpty()){preview=m["content"].toString().left(100);break;}}
        chats.append(QVariantMap{{"avatar",avatar(user)},{"id",it.key()},{"name",name.isEmpty()?QString("Conversation"):name.left(120)},
            {"call",calls_.contains(it.key())&&!calls_.value(it.key())["unavailable"].toBool()},
            {"ringing",!calls_.value(it.key())["unavailable"].toBool()&&calls_.value(it.key())["ringing"].toArray().contains(self_)&&voiceChannel_!=it.key()},
            {"missedCall",callNotices_.value(it.key())["missed"].toBool()?callNotices_.value(it.key())["message"].toString():QString()},
            {"owner",c["owner_id"].toString()},{"members",c["recipients"].toArray().toVariantList()},
            {"friend",c["type"].toInt(-1)==1&&c["recipients"].toArray().size()==1&&relationships_.value(c["recipients"].toArray().first().toObject()["id"].toString())["type"].toInt()==1},
            {"muted",muted_.contains(it.key())},
            {"mentions",mentions_.value(it.key())},
            {"detail",muted_.contains(it.key())?QString("Muted"):unread_.value(it.key())?QString("New messages"):(preview.isEmpty()?QString("Private conversation"):preview)},
            {"kind",c["guild_id"].toString().isEmpty() ? (c["type"].toInt()==3 ? "groups" : "chats") : "community-channel"},{"guild",c["guild_id"].toString()},{"unread",unread_.value(it.key())},{"last",c["last_message_id"].toString()}});
    }
    std::sort(chats.begin(),chats.end(),[](const QVariant& a,const QVariant& b){
        const QString aa=a.toMap()["last"].toString(),bb=b.toMap()["last"].toString();
        return aa.size()==bb.size()?aa>bb:aa.size()>bb.size();
    });
    for (const auto& id:messageOrder_) {
        const auto m=messages_.value(id);
        if(relationships_.value(m["author"].toObject()["id"].toString())["type"].toInt()==2)continue;
        const auto onlineEnvelope=OnlineLink::decode(m["content"].toString());
        // Link traffic has its own invitation/session surface. Keep its IDs in
        // the provider window for paging/acknowledgement, never as chat bubbles.
        if(!onlineEnvelope.isEmpty())continue;
        const auto guild=channels_.value(channel_)["guild_id"].toString();
        const bool welcome=communityIdentity::matches(m,guild,guilds_.value(guild)["owner_id"].toString(),communityChannels_.value(guild));
        QString text=welcome?QString("A gathering place for TrainerOS players. Welcome!"):m["content"].toString().left(4000);
        const bool callEvent=m["type"].toInt()==3;
        QString callDetail;
        bool missedCall=false;
        if(callEvent) {
            const auto notice=callNotices_.value(channel_);
            missedCall=notice["missed"].toBool()&&notice["message"]==id;
            const bool mine=m["author"].toObject()["id"]==self_;
            text=missedCall?"Missed call from "+label(m["author"].toObject()):
                mine?QString("You started a call"):"Call from "+label(m["author"].toObject());
            const auto started=QDateTime::fromString(m["timestamp"].toString(),Qt::ISODateWithMs);
            const auto ended=QDateTime::fromString(m["call"].toObject()["ended_timestamp"].toString(),Qt::ISODateWithMs);
            QStringList details;
            if(started.isValid())details.append(QLocale(QLocale::English).toString(started.toLocalTime(),"dd MMM yyyy · HH:mm"));
            if(ended.isValid()) {
                const auto seconds=started.secsTo(ended);
                if(started.isValid()&&seconds>=0) {
                    if(seconds<60)details.append(QString::number(seconds)+" s");
                    else if(seconds<3600)details.append(QString::number(seconds/60)+" min "+QString::number(seconds%60)+" s");
                    else details.append(QString::number(seconds/3600)+" h "+QString::number(seconds%3600/60)+" min");
                } else details.append("Call ended");
            } else if(m["call"].isObject()&&m["call"].toObject()["ended_timestamp"].isNull())details.append("Ongoing");
            callDetail=details.join(" · ");
        }
        else if(m["type"].toInt()==6)text=label(m["author"].toObject())+" pinned a message";
        else if(m["type"].toInt()==7)text=label(m["author"].toObject())+" joined the community";
        messages.append(QVariantMap{{"id",id},{"name",label(m["author"].toObject())},{"avatar",avatar(m["author"].toObject())},
            {"editable",onlineEnvelope.isEmpty()&&!welcome&&idValid(id)&&m["author"].toObject()["id"]==self_&&(m["type"].toInt()==0||m["type"].toInt()==19)&&m["message_snapshots"].toArray().isEmpty()&&m["local_delivery"].toString().isEmpty()},
            {"onlineKind",onlineEnvelope["kind"].toString()},
            {"edited",!m["edited_timestamp"].toString().isEmpty()},{"system",!onlineEnvelope.isEmpty()||(m["type"].toInt()!=0&&m["type"].toInt()!=19)},
            {"mine",m["author"].toObject()["id"]==self_},{"text",text},
            {"callEvent",callEvent},{"callDetail",callDetail},{"missedCall",missedCall},
            {"retryable",m["local_delivery"]=="Not sent"||(m["local_delivery"].toString().startsWith("Delivery unknown")&&QDateTime::currentMSecsSinceEpoch()-m["local_sent_at"].toString().toLongLong()<240000)},{"uncertain",m["local_delivery"].toString().startsWith("Delivery unknown")},
            {"delivery",m["local_delivery"].toString()}, {"attachments",m["attachments"].toArray().toVariantList()}, {"media",!m["attachments"].toArray().isEmpty()}});
    }
    for(auto it=guilds_.cbegin();it!=guilds_.cend();++it) {
        const bool marked=communityMarked_.contains(it.key());
        if(communityOnly_&&!marked)continue;
        communities.append(QVariantMap{{"id",it.key()},{"name",it.value()["name"].toString()},
            {"kind","community"},{"traineros",marked},{"detail",marked?"TrainerOS":"Community"}});
    }
    std::sort(communities.begin(),communities.end(),[](const QVariant& a,const QVariant& b){return a.toMap()["name"].toString()<b.toMap()["name"].toString();});
    int unreadCount=0;for(auto it=channels_.cbegin();it!=channels_.cend();++it)
        if(unread_.value(it.key())||callNotices_.value(it.key())["missed"].toBool())++unreadCount;
    QString readTail;
    for(auto i=messageOrder_.crbegin();i!=messageOrder_.crend();++i)if(idValid(*i)){readTail=*i;break;}
    emit snapshot(generation_, {{"state",state_},{"status",status_},{"name",name_},{"code",code_},
        {"profile",profile_.toVariantMap()},{"profileStatus",profileStatus_},{"profileBusy",profileBusy_},
        {"audio",audioConfiguration().toVariantMap()},
        {"unreadCount",unreadCount},{"doNotDisturb",doNotDisturb_},{"privatePreviews",privatePreviews_},
        {"mutationBusy",mutationBusy_},{"attachmentBusy",attachmentBusy_},{"notificationSound",notificationSound_},
        {"voice",QVariantMap{{"available",voiceAvailable()},{"channel",voiceChannel_},{"name",voiceName},
            {"summary",voiceName.isEmpty()?voiceStatus_:voiceName+" · "+voiceStatus_},
            {"state",voiceState_},{"status",voiceStatus_},{"muted",voiceMuted_},{"deaf",voiceDeaf_},{"participants",voiceParticipants_}}},
        {"communityOnly",communityOnly_},{"communityChecking",communityChecking_},{"communityStatus",communityStatus_},
        {"communityInvite",communityInvite_},{"communityOwner",guilds_.value(guild_)["owner_id"]==self_},
        {"communityMarked",communityMarked_.contains(guild_)},
        {"historyBusy",historyBusy_},{"historyMore",historyMore_},{"historyPast",historyPast_},
        {"readTail",readTail},{"historyAnchor",historyAnchor_},
        {"online",online_.state()},{"userId",self_},{"searchResults",searchResults_},{"searchStatus",searchStatus_},{"searching",searching_},{"searchTotal",searchTotal_},{"searchOffset",searchOffset_},
        {"guild",guild_},{"communities",communities},{"remembered",remembered_},{"friends",friends},{"chats",chats},{"messages",messages},{"channel",channel_}});
}
void FluxerSession::request(QByteArray method, QString path, QJsonObject body, Completion done, bool anonymous, QByteArray captcha) {
    const auto epoch=epoch_; QPointer<FluxerSession> guard(this);
    auto complete=[this,guard,epoch,done=std::move(done)](Reply r) {
        if(!guard || epoch!=epoch_) return;
        if(r.status==429||r.body.object()["code"]=="SLOWMODE_RATE_LIMITED") blockedUntil_=qMax(blockedUntil_,QDateTime::currentMSecsSinceEpoch()+qMax(1,r.retrySeconds)*1000LL);
        done(std::move(r));
    };
    if(QDateTime::currentMSecsSinceEpoch()<blockedUntil_) { complete({429,{ },1}); return; }
    if(transport_) { transport_(method,path,body,std::move(complete),captcha); return; }
    if(!network_) network_=new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl("https://api.fluxer.app"+path));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    req.setRawHeader("User-Agent","TrainerOS/0.1 (native user client)");
    if(!captcha.isEmpty())req.setRawHeader("X-Captcha-Token",captcha);
    if(!anonymous && !token_.isEmpty()) req.setRawHeader("Authorization",token_.toUtf8());
    auto* reply=network_->sendCustomRequest(req,method,method=="GET"?QByteArray():QJsonDocument(body).toJson(QJsonDocument::Compact));
    reply->setReadBufferSize(responseLimit+1);
    auto received=std::make_shared<QByteArray>();
    connect(reply,&QIODevice::readyRead,reply,[reply,received]{
        received->append(reply->readAll()); if(received->size()>responseLimit) reply->abort();
    });
    connect(reply,&QNetworkReply::finished,this,[reply,received,complete=std::move(complete)]() mutable {
        received->append(reply->readAll());
        int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(received->size()>responseLimit) status=0;
        const double retry=reply->rawHeader("Retry-After").toDouble();
        Reply result{status,status?QJsonDocument::fromJson(*received):QJsonDocument(),int(retry)+1};
        reply->deleteLater(); complete(std::move(result));
    });
}
void FluxerSession::verifiedRequest(QByteArray method, QString path, QJsonObject body, Completion done, int attempt, QByteArray captcha) {
    request(method,path,body,[this,method,path,body,done=std::move(done),attempt](Reply r) mutable {
        const auto error=r.body.object();const auto code=error["code"].toString();
        if(r.status!=400 || (code!="CAPTCHA_REQUIRED"&&code!="INVALID_CAPTCHA") || attempt>=2
            || error["captcha_provider"]!="altcha" || proof_) {done(std::move(r));return;}
        status_="Completing verification...";publish();
        const auto epoch=epoch_;
        proof_=new AltchaProof(error["altcha_challenge"].toObject(),
            [this,epoch,method,path,body,done=std::move(done),attempt,r](QByteArray token) mutable {
                proof_=nullptr;if(epoch!=epoch_)return;
                if(token.isEmpty()){done(r);return;}
                // Only retry an explicit challenge rejection. Never replay an
                // uncertain mutation or a token rejected by the provider.
                verifiedRequest(method,path,body,std::move(done),attempt+1,std::move(token));
            },this);
    },false,std::move(captcha));
}
void FluxerSession::updateUnread(const QString& channel) {
    if(!readsReady_)return;
    const auto read=readThrough_.value(channel),quiet=quietThrough_.value(channel);
    unread_[channel]=newer(channels_.value(channel)["last_message_id"].toString(),newer(quiet,read)?quiet:read)?1:0;
}
void FluxerSession::applyReadState(const QJsonObject& state, bool gateway) {
    const auto channel=state[gateway?"channel_id":"id"].toString();
    if(!idValid(channel))return;
    const auto message=state[gateway?"message_id":"last_message_id"].toString();
    if(!message.isEmpty()&&!idValid(message))return;
    if(state["manual"].toBool())quietThrough_.remove(channel);
    if(state.contains("mention_count"))mentions_[channel]=qMax(0,state["mention_count"].toInt());
    readThrough_[channel]=message;++readRevision_[channel];updateUnread(channel);
    const auto notice=callNotices_.value(channel);
    if(notice["missed"].toBool()&&!message.isEmpty()&&!newer(notice["message"].toString(),message)) {
        callNotices_.remove(channel);
        if(!historyFile_.isEmpty()&&!historySave_.isActive())historySave_.start();
    }
}
void FluxerSession::acknowledge(QString channel, QString message) {
    if(!readsReady_||channel!=channel_||!idValid(message)||!messages_.contains(message))return;
    if(!newer(message,readThrough_.value(channel))) {
        // The call message may already have been read before the caller hung up.
        // Viewing that ended call clears its local notice without repeating ACK.
        const auto notice=callNotices_.value(channel);
        if(notice["missed"].toBool()&&!newer(notice["message"].toString(),message)) {
            callNotices_.remove(channel);
            if(!historyFile_.isEmpty()&&!historySave_.isActive())historySave_.start();
            publish();
        }
        return;
    }
    if(ackBusy_)return;
    ackBusy_=true;const auto revision=readRevision_.value(channel);
    request("POST","/v1/read-states/ack",{{"read_states",QJsonArray{QJsonObject{
        {"channel_id",channel},{"message_id",message},{"mention_count",0},{"manual",false}}}}},
        [this,channel,revision](Reply r){
            ackBusy_=false;
            if(r.status==200&&revision==readRevision_.value(channel))
                for(const auto& v:r.body.object()["read_states"].toArray())if(v.toObject()["id"]==channel)applyReadState(v.toObject(),false);
            if(r.status==401){fail(r,"Please sign in again");return;}
            publish();
        });
}
void FluxerSession::fail(const Reply& r, QString fallback) {
    if(r.status==401) { clearHistoryCache(); credential(false,true); reset(); state_="signed-out"; status_="Please sign in again"; }
    else status_=r.status==429?"Please wait a moment before trying again":r.status==0?"Connection lost. Try again when online":std::move(fallback);
    publish();
}
bool FluxerSession::credentialStoreAvailable() const {
#ifdef Q_OS_LINUX
    // Never summon a desktop wallet password dialog over the handheld shell.
    auto* bus=QDBusConnection::sessionBus().interface();
    return bus && (bus->isServiceRegistered("org.freedesktop.secrets").value()
        || bus->isServiceRegistered("org.kde.kwalletd6").value());
#else
    return true;
#endif
}
void FluxerSession::credential(bool write, bool remove) {
    if(transport_ || owner_.isEmpty()) return;
    const auto epoch=epoch_;
    const auto key=QString::fromLatin1(QCryptographicHash::hash(("https://fluxer.app\n"+owner_).toUtf8(),QCryptographicHash::Sha256).toHex());
    // Prefer the already-running desktop vault. Armada gaming sessions use the
    // OS host-key store without starting a wallet/password window.
    const bool encrypted=EncryptedCredentials::available()&&(!credentialStoreAvailable()||QFileInfo::exists(EncryptedCredentials::path(key)));
    if(remove) {if(encryptedCredentials_)encryptedCredentials_->cancel();EncryptedCredentials::remove(key);}
    if(encrypted&&!remove) {
        if(!encryptedCredentials_)encryptedCredentials_=new EncryptedCredentials(this);
        if(!write){credentialLoading_=true;state_="restoring";status_="Signing in...";publish();}
        auto completed=[this,epoch,write](bool ok,QByteArray value){
            if(epoch!=epoch_)return;
            credentialLoading_=false;remembered_=ok&&(write||!value.isEmpty());
            if(!write){if(ok&&!value.isEmpty()){token_=QString::fromUtf8(value);authenticated();return;}state_="signed-out";status_="Sign in";}
            publish();
        };
        if(write)encryptedCredentials_->write(key,token_.toUtf8(),std::move(completed));
        else encryptedCredentials_->read(key,std::move(completed));
        return;
    }
    if(!credentialStoreAvailable())return;
    if(!write&&!remove){credentialLoading_=true;state_="restoring";status_="Signing in...";publish();}
    QKeychain::Job* job;
    if(remove) { auto* j=new QKeychain::DeletePasswordJob("TrainerOS.Fluxer",this); j->setKey(key); job=j; }
    else if(write) { auto* j=new QKeychain::WritePasswordJob("TrainerOS.Fluxer",this); j->setKey(key); j->setTextData(token_); job=j; }
    else { auto* j=new QKeychain::ReadPasswordJob("TrainerOS.Fluxer",this); j->setKey(key); job=j; }
    job->setInsecureFallback(false);
    connect(job,&QKeychain::Job::finished,this,[this,epoch,write,remove](QKeychain::Job* j){
        if(epoch!=epoch_ || remove) return;
        credentialLoading_=false;
        if(j->error()!=QKeychain::NoError) { remembered_=false;if(!write){state_="signed-out";status_="Sign in";} publish(); return; }
        remembered_=true;
        if(!write) { token_=static_cast<QKeychain::ReadPasswordJob*>(j)->textData(); if(!token_.isEmpty()) authenticated(); else {remembered_=false;state_="signed-out";status_="Sign in";publish();} }
        else publish();
    });
    job->start();
}
void FluxerSession::login() {
    if(owner_.isEmpty() || credentialLoading_ || state_=="authorizing") return;
    if(!token_.isEmpty()){if(self_.isEmpty())authenticated();return;}
    state_="authorizing"; status_="Preparing browser sign-in…"; publish();
    // Public instance only for this increment. Validate advertised endpoints before authentication.
    request("GET","/.well-known/fluxer",{},[this](Reply r){
        const auto endpoints=r.body.object()["endpoints"].toObject();
        if(r.status!=200 || endpoints["api_public"]!="https://api.fluxer.app" || endpoints["gateway"]!="wss://gateway.fluxer.app") {
            state_="signed-out"; fail(r,"Sign-in is unavailable"); return;
        }
        request("POST","/v1/auth/handoff/initiate",{},[this](Reply result){
            const auto b=result.body.object(); code_=b["code"].toString(); pollSecret_=b["poll_secret"].toString();
            if(result.status<200||result.status>=300||code_.isEmpty()||pollSecret_.isEmpty()) {state_="signed-out";fail(result,"Could not start sign-in");return;}
            expires_=QDateTime::currentMSecsSinceEpoch()+300000;
            status_="Open web.fluxer.app/login?handoff=1 and approve this code";
            if(!poll_) {poll_=new QTimer(this);poll_->setInterval(2200);connect(poll_,&QTimer::timeout,this,&FluxerSession::pollLogin);}
            poll_->start();publish();
        },true);
    },true);
}
void FluxerSession::pollLogin() {
    if(polling_)return;
    if(QDateTime::currentMSecsSinceEpoch()>expires_) { command("cancel-login");status_="Sign-in expired. Try again";publish();return; }
    polling_=true;
    request("POST","/v1/auth/handoff/"+code_+"/status",{{"poll_secret",pollSecret_}},[this](Reply r){
        polling_=false;
        if(r.status==429)return;
        if(r.status!=200) {poll_->stop();state_="signed-out";code_.clear();pollSecret_.clear();fail(r,"Sign-in was not completed");return;}
        const auto b=r.body.object();
        if(b["status"]=="completed" && !b["token"].toString().isEmpty()) {
            poll_->stop();token_=b["token"].toString();code_.clear();pollSecret_.clear();authenticated();
        }
    },true);
}
void FluxerSession::authenticated() {
    if(self_.isEmpty())loadHistoryCache();
    state_=self_.isEmpty()?"restoring":"connecting";status_="Signing in...";publish();
    request("GET","/v1/users/@me",{},[this](Reply r){
        if(r.status!=200||!idValid(r.body.object()["id"].toString())) {
            state_=self_.isEmpty()?"restoring":"connecting";fail(r,"Could not verify your account");
            if(!token_.isEmpty()){const auto epoch=epoch_;QTimer::singleShot(qMax(15000,r.retrySeconds*1000),this,[this,epoch]{if(epoch==epoch_&&!token_.isEmpty()&&state_!="connected")authenticated();});}
            return;
        }
        const auto verifiedId=r.body.object()["id"].toString();
        if(!self_.isEmpty()&&self_!=verifiedId){historyCache_.clear();historyLru_.clear();callNotices_.clear();historyFile_.clear();channels_.clear();relationships_.clear();guilds_.clear();messages_.clear();messageOrder_.clear();channel_.clear();}
        self_=verifiedId;updateProfile(r.body.object());bindOnline();
        navigationKey_="social/navigation/"+QString::fromLatin1(QCryptographicHash::hash((owner_+"\n"+self_).toUtf8(),QCryptographicHash::Sha256).toHex())+"/";
        if(!transport_) {QSettings settings;for(const auto& key:{"chats","groups","communities","guild"})preferred_[key]=settings.value(navigationKey_+key).toString();
            doNotDisturb_=settings.value(navigationKey_+"dnd",false).toBool();privatePreviews_=settings.value(navigationKey_+"private",true).toBool();muted_=settings.value(navigationKey_+"muted").toStringList();notificationSound_=settings.value(navigationKey_+"sound",true).toBool();}
        loadHistoryCache();
        if(!transport_){QSettings settings;voiceInput_=settings.value(navigationKey_+"voiceInput").toString();voiceOutput_=settings.value(navigationKey_+"voiceOutput").toString();voiceVolume_=qBound(0,settings.value(navigationKey_+"voiceVolume",100).toInt(),100);}
        state_="connected";status_="Connected";if(!remembered_)credential(true);refresh();openGateway();publish();
    });
}
void FluxerSession::refresh() {
    if(self_.isEmpty())return;
    refreshChannels();
    if(refreshing_)return;
    refreshing_=true;
    request("GET","/v1/users/@me/relationships",{},[this](Reply r){
        refreshing_=false;
        if(r.status!=200) {fail(r,"Could not load friends");return;}
        friendsLoaded_=true;relationships_.clear();
        for(const auto& v:r.body.array()) {auto o=v.toObject();auto id=o["id"].toString();if(idValid(id)&&relationships_.size()<2000)relationships_[id]=o;}
        ensureConversation();publish();
    });
    const auto guildRevision=++guildListRevision_;
    request("GET","/v1/users/@me/guilds?limit=100",{},[this,guildRevision](Reply r){
        if(guildRevision!=guildListRevision_)return;
        if(r.status!=200){fail(r,"Could not load communities");return;}
        auto previous=guilds_;guilds_.clear();for(const auto& v:r.body.array()){auto g=v.toObject();const auto id=g["id"].toString();if(idValid(id)){
            if(!g.contains("owner_id")&&previous.contains(id))g["owner_id"]=previous[id]["owner_id"];
            guilds_[id]=g;}}
        for(const auto& id:communityMarked_)if(!guilds_.contains(id))communityChecked_.remove(id);
        communityMarked_.intersect(QSet<QString>(guilds_.keyBegin(),guilds_.keyEnd()));
        for(auto it=channels_.begin();it!=channels_.end();) {auto guild=it.value()["guild_id"].toString();if(!guild.isEmpty()&&!guilds_.contains(guild)){historyCache_.remove(it.key());historyLru_.removeAll(it.key());it=channels_.erase(it);}else ++it;}
        if(!guild_.isEmpty()&&!guilds_.contains(guild_)){guild_.clear();channel_.clear();messages_.clear();messageOrder_.clear();}
        ensureConversation();checkCommunities();publish();
    });
    if(!channel_.isEmpty()&&!historyBusy_)loadMessages(channel_);
}
void FluxerSession::refreshChannels() {
    if(channelsLoading_)return;
    channelsLoading_=true;
    const auto channelRevision=channelRevision_;
    request("GET","/v1/users/@me/channels",{},[this,channelRevision](Reply r){
        channelsLoading_=false;
        if(channelRevision!=channelRevision_){refreshChannels();return;}
        if(r.status!=200) {fail(r,"Could not load conversations");return;}
        QStringList previousPrivate;
        for(auto it=channels_.begin();it!=channels_.end();) {if(it.value()["guild_id"].toString().isEmpty()){previousPrivate.append(it.key());it=channels_.erase(it);}else ++it;}
        for(const auto& v:r.body.array()) {auto o=v.toObject();auto id=o["id"].toString();if(idValid(id)&&channels_.size()<500)channels_[id]=o;}
        for(const auto& id:previousPrivate)if(!channels_.contains(id)){historyCache_.remove(id);historyLru_.removeAll(id);callNotices_.remove(id);}
        if(!channel_.isEmpty()&&!channels_.contains(channel_)) {channel_.clear();messages_.clear();messageOrder_.clear();}
        for(auto it=channels_.cbegin();it!=channels_.cend();++it)updateUnread(it.key());
        channelsLoaded_=true;status_="Connected";ensureConversation();publish();
    });
}
void FluxerSession::retainHistory() {
    if(channel_.isEmpty()||!channels_.contains(channel_))return;
    QJsonArray rows;
    for(const auto& id:messageOrder_)rows.append(messages_.value(id));
    historyCache_[channel_]={{"rows",rows},{"anchor",historyAnchor_},{"past",historyPast_},
        {"more",historyMore_},{"visited",QString::number(QDateTime::currentSecsSinceEpoch())}};
    historyLru_.removeAll(channel_);historyLru_.append(channel_);
    while(historyLru_.size()>24)historyCache_.remove(historyLru_.takeFirst());
    if(!historyFile_.isEmpty()&&!historySave_.isActive())historySave_.start();
}
void FluxerSession::restoreHistory(const QString& channel) {
    const auto cached=historyCache_.value(channel);
    historyAnchor_=cached["anchor"].toString();historyPast_=cached["past"].toBool();historyMore_=cached["more"].toBool();
    for(const auto& value:cached["rows"].toArray()) {
        const auto m=value.toObject();mergeMessage(m);
        if(m["local_delivery"].toString().startsWith("Delivery unknown"))pendingNonces_[m["id"].toString()]=channel;
    }
}
void FluxerSession::loadHistoryCache() {
    if(transport_||owner_.isEmpty()||token_.isEmpty())return;
    const auto ownerKey="social/cached-account/"+QString::fromLatin1(QCryptographicHash::hash(owner_.toUtf8(),QCryptographicHash::Sha256).toHex());
    const bool restoring=self_.isEmpty();
    const auto account=restoring?QSettings().value(ownerKey).toString():self_;
    if(!idValid(account))return;
    if(!restoring)QSettings().setValue(ownerKey,account);
    const auto key=QString::fromLatin1(QCryptographicHash::hash(("https://fluxer.app\n"+owner_+"\n"+account).toUtf8(),QCryptographicHash::Sha256).toHex());
    const auto path=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/social/"+key+"/history.json";
    if(path==historyFile_)return;
    historyCache_.clear();historyLru_.clear();callNotices_.clear();historyFile_=path;
    QFile input(path);
    if(QFileInfo(path).isSymLink()||input.size()>4*1024*1024||!input.open(QIODevice::ReadOnly))return;
    const auto data=QJsonDocument::fromJson(input.readAll()).object();
    if(data["account"]!=account||data["owner"]!=owner_)return;
    if(restoring) {
        // Cold offline history is available only with the same protected token
        // that last verified this account. Replacing a login cannot reveal it.
        const auto binding=QString::fromLatin1(QCryptographicHash::hash(token_.toUtf8(),QCryptographicHash::Sha256).toHex());
        if(data["binding"]!=binding)return;
        self_=account;name_=data["name"].toString();
        for(const auto& v:data["channels"].toArray()){const auto c=v.toObject();if(idValid(c["id"].toString()))channels_[c["id"].toString()]=c;}
        for(const auto& v:data["friends"].toArray()){const auto c=v.toObject();if(idValid(c["id"].toString()))relationships_[c["id"].toString()]=c;}
        for(const auto& v:data["guilds"].toArray()){const auto c=v.toObject();if(idValid(c["id"].toString()))guilds_[c["id"].toString()]=c;}
        channelsLoaded_=friendsLoaded_=true;
        preferred_["chats"]=data["selected"].toString();
    }
    const auto cutoff=QDateTime::currentSecsSinceEpoch()-30*86400;
    for(const auto& value:data["missedCalls"].toArray()) {
        const auto notice=value.toObject();const auto id=notice["channel"].toString();
        if(!idValid(id)||!idValid(notice["message"].toString())||notice["time"].toString().toLongLong()<cutoff)continue;
        callNotices_[id]={{"message",notice["message"]},{"time",notice["time"]},{"missed",true}};
        if(callNotices_.size()==32)break;
    }
    for(const auto& entry:data["conversations"].toArray()) {
        auto c=entry.toObject();const auto id=c.take("id").toString();
        if(!idValid(id)||c["visited"].toString().toLongLong()<cutoff||c["rows"].toArray().size()>100)continue;
        historyCache_[id]=c;historyLru_.append(id);
        if(historyLru_.size()==24)break;
    }
    if(restoring&&!channels_.isEmpty()){auto id=data["selected"].toString();if(!channels_.contains(id))id=*channels_.keyBegin();channel_=id;restoreHistory(id);}
}
void FluxerSession::saveHistoryCache() {
    historySave_.stop();if(historyFile_.isEmpty())return;
    QJsonArray entries;
    for(const auto& id:historyLru_) {
        auto entry=historyCache_.value(id);QJsonArray rows;
        for(const auto& value:entry["rows"].toArray()) {
            const auto m=value.toObject();
            // Persist text only: no attachment capabilities, embeds, voice grants
            // or game-activity packets in the local message cache.
            if(!OnlineLink::decode(m["content"].toString()).isEmpty())continue;
            QJsonObject clean;
            for(const auto* field:{"id","channel_id","content","author","type","timestamp","edited_timestamp","local_delivery","local_sent_at","nonce"})
                if(m.contains(field))clean[field]=m[field];
            const auto author=m["author"].toObject();
            clean["author"]=QJsonObject{{"id",author["id"]},{"username",author["username"]},{"global_name",author["global_name"]}};
            if(m["type"].toInt()==3&&m.contains("call"))clean["call"]=QJsonObject{{"ended_timestamp",m["call"].toObject()["ended_timestamp"]}};
            if(m["content"].toString().isEmpty()&&!m["attachments"].toArray().isEmpty())clean["content"]="Attachment · reconnect to view";
            if(clean["local_delivery"]=="Sending…")clean["local_delivery"]="Delivery unknown — check before resending";
            rows.append(clean);
        }
        entry["rows"]=rows;entry["id"]=id;entries.append(entry);
    }
    const auto dir=QFileInfo(historyFile_).absolutePath();
    if(QFileInfo(dir).isSymLink()||QFileInfo(historyFile_).isSymLink()||!QDir().mkpath(dir))return;
    QFile::setPermissions(dir,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    QJsonArray channels,friends,guilds;
    for(auto it=channels_.cbegin();it!=channels_.cend();++it) {
        const auto c=it.value();QJsonObject clean;
        for(const auto* field:{"id","name","type","guild_id","owner_id","recipients","last_message_id"})if(c.contains(field))clean[field]=c[field];
        channels.append(clean);
    }
    for(const auto& f:relationships_)friends.append(f);
    for(const auto& g:guilds_)guilds.append(QJsonObject{{"id",g["id"]},{"name",g["name"]}});
    QJsonArray missedCalls;
    for(auto it=callNotices_.cbegin();it!=callNotices_.cend();++it)if(it.value()["missed"].toBool()) {
        auto notice=it.value();notice["channel"]=it.key();missedCalls.append(notice);
    }
    QJsonObject data{{"owner",owner_},{"account",self_},{"name",name_},{"selected",channel_},{"missedCalls",missedCalls},
        {"binding",QString::fromLatin1(QCryptographicHash::hash(token_.toUtf8(),QCryptographicHash::Sha256).toHex())},
        {"channels",channels},{"friends",friends},{"guilds",guilds},{"conversations",entries}};
    auto bytes=QJsonDocument(data).toJson(QJsonDocument::Compact);
    while(bytes.size()>4*1024*1024&&!entries.isEmpty()){entries.removeAt(0);data["conversations"]=entries;bytes=QJsonDocument(data).toJson(QJsonDocument::Compact);}
    if(bytes.size()>4*1024*1024)return;
    QSaveFile file(historyFile_);if(!file.open(QIODevice::WriteOnly))return;
    file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    if(file.write(bytes)==bytes.size())file.commit();
}
void FluxerSession::clearHistoryCache() {
    historySave_.stop();historyCache_.clear();historyLru_.clear();callNotices_.clear();
    if(!historyFile_.isEmpty())QFile::remove(historyFile_);
    messages_.clear();messageOrder_.clear();historyAnchor_.clear();pendingNonces_.clear();
    historyFile_.clear();
}
void FluxerSession::sendText(QString channel,QString content,QString nonce) {
    if(channel!=channel_||messages_.value(nonce)["local_delivery"]=="Sending…")return;
    const auto previous=messages_.value(nonce);
    const auto first=previous["local_sent_at"].toString().toLongLong();
    if(previous["local_delivery"].toString().startsWith("Delivery unknown")&&(first<=0||QDateTime::currentMSecsSinceEpoch()-first>=240000)){status_="Check the conversation before sending this message again.";publish();return;}
    if(pendingNonces_.size()>=2400&&!pendingNonces_.contains(nonce))pendingNonces_.erase(pendingNonces_.begin());
    pendingNonces_[nonce]=channel;
    mergeMessage({{"id",nonce},{"channel_id",channel},{"content",content},{"local_delivery","Sending…"},{"local_sent_at",QString::number(first>0?first:QDateTime::currentMSecsSinceEpoch())},
        {"author",QJsonObject{{"id",self_},{"global_name",name_}}}});publish();
    request("POST","/v1/channels/"+channel+"/messages",{{"content",content},{"nonce",nonce}},[this,channel,nonce](Reply r){
        // A Gateway echo can settle the post before a failed/late HTTP reply.
        if(!pendingNonces_.contains(nonce))return;
        const bool ok=r.status>=200&&r.status<300;
        auto finish=[&](QJsonObject& entry){
            auto rows=entry["rows"].toArray();
            for(qsizetype i=0;i<rows.size();++i)if(rows[i].toObject()["id"]==nonce){
                if(ok)rows[i]=r.body.object();
                else {auto m=rows[i].toObject();m["local_delivery"]=r.status==0||r.status>=500?"Delivery unknown — check before resending":"Not sent";rows[i]=m;}
            }
            entry["rows"]=rows;
        };
        if(channel!=channel_){auto entry=historyCache_.value(channel);finish(entry);historyCache_[channel]=entry;}
        else if(ok){messages_.remove(nonce);messageOrder_.removeAll(nonce);mergeMessage(r.body.object());}
        else if(messages_.contains(nonce))messages_[nonce]["local_delivery"]=r.status==0||r.status>=500?"Delivery unknown — check before resending":"Not sent";
        if(ok||(r.status>=400&&r.status<500))pendingNonces_.remove(nonce);
        if(ok){status_="Connected";publish();}else fail(r,"Message was not sent");
    });
}
void FluxerSession::loadMessages(QString channel) {
    const auto revision=messageRevision_, ticket=++historyRequest_;
    historyBusy_=true;publish();
    const auto anchor=historyPast_&&idValid(historyAnchor_)?historyAnchor_:QString();
    request("GET","/v1/channels/"+channel+"/messages?limit=50"+(anchor.isEmpty()?QString():"&around="+anchor),{},[this,channel,revision,ticket,anchor](Reply r){
        if(channel!=channel_||ticket!=historyRequest_)return;
        historyBusy_=false;
        if(revision!=messageRevision_){loadMessages(channel);return;} // Never resurrect a deleted message or erase a fresh event.
        if(r.status!=200) {if(r.status==403||r.status==404){historyCache_.remove(channel);historyLru_.removeAll(channel);messages_.clear();messageOrder_.clear();historyAnchor_.clear();}fail(r,"Could not load messages");return;}
        historyPast_=!anchor.isEmpty();historyMore_=!r.body.array().isEmpty();
        // Keep uncertain sends; the server response is authoritative for remote history.
        auto old=messages_;messages_.clear();messageOrder_.clear();
        const auto list=r.body.array();for(auto i=list.size();i>0;--i)mergeMessage(list.at(i-1).toObject());
        for(auto it=old.cbegin();it!=old.cend();++it) if(!it.value()["local_delivery"].toString().isEmpty())mergeMessage(it.value());
        publish();
    });
}
void FluxerSession::loadOlderMessages() {
    if(historyBusy_||!historyMore_||channel_.isEmpty())return;
    QString before;for(const auto& id:messageOrder_)if(idValid(id)){before=id;break;}
    if(before.isEmpty())return;
    const auto channel=channel_;const auto revision=messageRevision_,ticket=++historyRequest_;
    historyBusy_=true;publish();
    request("GET","/v1/channels/"+channel+"/messages?limit=50&before="+before,{},[this,channel,revision,ticket](Reply r){
        if(channel!=channel_||ticket!=historyRequest_)return;
        historyBusy_=false;
        if(revision!=messageRevision_){publish();return;}
        if(r.status!=200){fail(r,"Could not load earlier messages");return;}
        QStringList added;const auto list=r.body.array();
        for(auto i=list.size();i>0;--i){const auto m=list.at(i-1).toObject();const auto id=m["id"].toString();
            if(idValid(id)&&m["channel_id"]==channel&&!messages_.contains(id)){messages_[id]=m;added.append(id);}}
        historyMore_=!added.isEmpty();
        if(!added.isEmpty()){historyPast_=true;messageOrder_=added+messageOrder_;}
        // A moving window can page indefinitely without retaining the entire account.
        while(messageOrder_.size()>100) {
            const auto id=messageOrder_.takeLast();
            if(!pendingNonces_.contains(id))messages_.remove(id);
        }
        publish();
    });
}
void FluxerSession::mergeMessage(const QJsonObject& message) {
    if(message["channel_id"].toString()!=channel_)return;
    auto id=message["id"].toString();if(id.isEmpty())return;
    const auto nonce=message["nonce"].toString();
    if(!nonce.isEmpty()&&pendingNonces_.contains(nonce)) {messages_.remove(nonce);messageOrder_.removeAll(nonce);pendingNonces_.remove(nonce);}
    if(!messages_.contains(id))messageOrder_.append(id);
    auto combined=messages_.value(id);for(auto it=message.begin();it!=message.end();++it)combined[it.key()]=it.value();messages_[id]=combined;
    while(messageOrder_.size()>100)messages_.remove(messageOrder_.takeFirst());
}
bool FluxerSession::voiceAvailable() const {
#ifdef Q_OS_LINUX
    static const bool available=QFileInfo((qEnvironmentVariable("XDG_DATA_HOME").isEmpty()?QDir::homePath()+"/.local/share":qEnvironmentVariable("XDG_DATA_HOME"))+"/traineros-voice/bin/python").isExecutable()
        &&QFileInfo("/var/opt/traineros/integrations/fluxer-voice.py").isFile();
    return available;
#else
    return false;
#endif
}
void FluxerSession::voiceWrite(const QJsonObject& command) {
    if(voiceProcess_.state()==QProcess::Running&&voiceProcess_.bytesToWrite()<65536)
        voiceProcess_.write(QJsonDocument(command).toJson(QJsonDocument::Compact)+'\n');
}
void FluxerSession::voiceStateUpdate(bool leave) {
    if(voiceChannel_.isEmpty())return;
    QJsonObject update{{"guild_id",QJsonValue::Null},{"channel_id",leave?QJsonValue(QJsonValue::Null):QJsonValue(voiceChannel_)},
        {"self_mute",voiceMuted_},{"self_deaf",voiceDeaf_},{"self_video",false},{"self_stream",false}};
    if(!voiceConnection_.isEmpty())update["connection_id"]=voiceConnection_;
    gatewaySend(4,update);
}
void FluxerSession::leaveVoice() {
    if(!voiceChannel_.isEmpty())qInfo()<<"Voice left from state"<<voiceState_;
    voiceStateUpdate(true);voiceChannel_.clear();voiceConnection_.clear();voiceState_.clear();
    voiceMuted_=true;voiceDeaf_=false;voiceParticipants_=0;voiceHeartbeat_.stop();voiceDeadline_.stop();
    voiceWrite({{"op","leave"}});if(voiceProcess_.state()!=QProcess::NotRunning)voiceProcess_.closeWriteChannel();
    if(voiceProcess_.state()!=QProcess::NotRunning){voiceProcess_.kill();voiceProcess_.waitForFinished(700);}
    voiceBuffer_.clear();voiceGrantIdentity_.clear();
}
void FluxerSession::reconcileVoice() {
    // A fresh chat session does not own the lifetime of an already connected
    // media room. Recheck access, then reattach its placement without ringing.
    if(voiceChannel_.isEmpty())return;
    const auto channel=voiceChannel_,connection=voiceConnection_;const auto epoch=epoch_;
    request("GET","/v1/channels/"+channel,{},[this,channel,connection,epoch](Reply r){
        if(epoch!=epoch_||channel!=voiceChannel_||connection!=voiceConnection_)return;
        if(r.status==401||r.status==403||r.status==404){leaveVoice();voiceStatus_="Call no longer available";publish();return;}
        if(r.status==200&&r.body.object()["id"]==channel){voiceStateUpdate();return;}
        QTimer::singleShot(qBound(5,r.retrySeconds,60)*1000,this,[this,channel,connection,epoch]{
            if(epoch==epoch_&&channel==voiceChannel_&&connection==voiceConnection_)reconcileVoice();
        });
    });
}
void FluxerSession::updateCallNotice(const QString& type,const QString& channel,const QJsonObject& event) {
    // Record only calls actually observed ringing this account. An unavailable
    // call or a Gateway outage is not evidence that someone missed a call.
    const auto before=callNotices_;
    if(type=="CALL_DELETE") {
        if(!event["unavailable"].toBool()&&callNotices_.contains(channel))callNotices_[channel]["missed"]=true;
    } else {
        const auto call=calls_.value(channel);bool joined=voiceChannel_==channel;
        for(const auto& state:call["voice_states"].toArray())if(state.toObject()["user_id"]==self_)joined=true;
        if(joined)callNotices_.remove(channel);
        else if(call["ringing"].toArray().contains(self_)&&idValid(call["message_id"].toString()))
            callNotices_[channel]={{"message",call["message_id"]},{"time",QString::number(QDateTime::currentSecsSinceEpoch())},{"missed",false}};
    }
    while(callNotices_.size()>32) {
        auto oldest=callNotices_.begin();
        for(auto it=callNotices_.begin();it!=callNotices_.end();++it)
            if(newer(oldest.value()["message"].toString(),it.value()["message"].toString()))oldest=it;
        callNotices_.erase(oldest);
    }
    if(before!=callNotices_&&!historyFile_.isEmpty()&&!historySave_.isActive())historySave_.start();
}
void FluxerSession::voiceCommand(QString operation,const QVariantMap& args) {
    if(operation=="voice-decline") {
        const auto id=args["channel"].toString();if(!calls_.contains(id))return;
        callNotices_.remove(id);
        request("POST","/v1/channels/"+id+"/call/stop-ringing",{},[this](Reply r){if(r.status<200||r.status>=300)voiceStatus_="Couldn't decline the call. Try again.";publish();});return;
    }
    if(operation=="voice-leave"){leaveVoice();voiceStatus_="Call ended";publish();return;}
    if(operation=="voice-join") {
        const auto channel=args["channel"].toString();const auto type=channels_.value(channel)["type"].toInt(-1);
        if(!voiceAvailable()||!voiceChannel_.isEmpty()||!idValid(channel)||(type!=1&&type!=3))return;
        callNotices_.remove(channel);
        voiceChannel_=channel;voiceState_="connecting";voiceStatus_="Connecting...";voiceMuted_=true;voiceDeaf_=false;
        voiceDeadline_.start(20000);publish();
        if(!calls_.contains(channel))request("POST","/v1/channels/"+channel+"/call/ring",{},[this,channel](Reply reply){
            if(voiceChannel_!=channel)return;
            if(reply.status>=200&&reply.status<300)voiceStateUpdate();
            else {leaveVoice();voiceStatus_="Couldn't start the call. Try again.";publish();}
        });
        else voiceStateUpdate();
        return;
    }
    if(voiceState_!="connected"&&voiceState_!="reconnecting")return;
    if(operation=="voice-mute") {voiceMuted_=!voiceMuted_;voiceWrite({{"op","mute"},{"value",voiceMuted_}});}
    else if(operation=="voice-output") {voiceDeaf_=!voiceDeaf_;voiceWrite({{"op","deaf"},{"value",voiceDeaf_}});}
    else if(operation=="voice-ring") {request("POST","/v1/channels/"+voiceChannel_+"/call/ring",{},[this](Reply r){if(r.status!=204)voiceStatus_="Could not ring this conversation";publish();});return;}
    else return;
    voiceStatus_=voiceMuted_?"Connected · Microphone off":"Connected · Microphone on";voiceStateUpdate();publish();
}
void FluxerSession::voiceGrant(const QJsonObject& grant) {
    qInfo()<<"Voice grant received"<<(voiceProcess_.state()!=QProcess::NotRunning);
    if(voiceChannel_.isEmpty()||grant["channel_id"]!=voiceChannel_||!voiceAvailable())return;
    const QUrl endpoint(grant["endpoint"].toString());
    if((endpoint.scheme()!="wss"&&endpoint.scheme()!="https")||!endpoint.userInfo().isEmpty()||grant["token"].toString().isEmpty()||grant["connection_id"].toString().isEmpty())return;
    // Replayed placement grants must not restart healthy audio. A genuinely new
    // placement replaces only the media worker and keeps the user's mute choice.
    const auto identity=QCryptographicHash::hash(QJsonDocument(QJsonObject{
        {"connection",grant["connection_id"]},{"endpoint",grant["endpoint"]},{"key",grant["e2ee_key"]}
    }).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);
    if(voiceProcess_.state()!=QProcess::NotRunning&&identity==voiceGrantIdentity_)return;
    if(voiceProcess_.state()!=QProcess::NotRunning){
        voiceReplacing_=true;voiceProcess_.kill();voiceProcess_.waitForFinished(700);voiceReplacing_=false;
        if(voiceProcess_.state()!=QProcess::NotRunning){leaveVoice();voiceStatus_="Couldn't restore audio. Try again.";publish();return;}
    }
    voiceConnection_=grant["connection_id"].toString();voiceGrantIdentity_=identity;voiceBuffer_.clear();
    voiceState_="connecting";voiceStatus_="Connecting audio...";voiceDeadline_.start(20000);
    voiceProcess_.start((qEnvironmentVariable("XDG_DATA_HOME").isEmpty()?QDir::homePath()+"/.local/share":qEnvironmentVariable("XDG_DATA_HOME"))+"/traineros-voice/bin/python",{"/var/opt/traineros/integrations/fluxer-voice.py"});
    auto configured=grant;configured["audio"]=audioConfiguration();
    voiceProcess_.write(QJsonDocument(configured).toJson(QJsonDocument::Compact)+'\n');voiceHeartbeat_.start();publish();
}
QJsonObject FluxerSession::audioConfiguration() const {
    return {{"input",voiceInput_},{"output",voiceOutput_},{"volume",voiceVolume_}};
}
void FluxerSession::updateProfile(const QJsonObject& user) {
    if(user["id"].toString()!=self_)return;
    // Expose presentation fields only, never the private account response.
    name_=label(user);
    profile_={{"name",user["global_name"].toString()},{"bio",user["bio"].toString()},
        {"avatar",avatar(user)},{"tag",user["username"].toString()+"#"+user["discriminator"].toString()}};
}
void FluxerSession::profileCommand(const QVariantMap& args) {
    if(self_.isEmpty()||profileBusy_)return;
    const auto field=args["field"].toString(),value=args["value"].toString();
    if(field!="global_name"&&field!="bio"&&field!="avatar")return;
    if((field=="global_name"&&value.size()>32)||(field=="bio"&&value.size()>320)
        ||(field=="avatar"&&!value.isEmpty()&&(!value.startsWith("data:image/jpeg;base64,")||value.size()>13981016)))return;
    profileBusy_=true;profileStatus_="Saving...";publish();
    verifiedRequest("PATCH","/v1/users/@me",{{field,value.isEmpty()?QJsonValue(QJsonValue::Null):QJsonValue(value)}},[this](Reply r){
        profileBusy_=false;
        if(r.status==200&&r.body.object()["id"].toString()==self_){updateProfile(r.body.object());profileStatus_="Saved";}
        else {profileStatus_=r.status==429?"Please wait before saving again.":"Couldn't save your profile. Try again.";}
        publish();
    });
}
void FluxerSession::sendAttachment(const QVariantMap& args) {
    const auto channel=args["channel"].toString();const auto bytes=args["bytes"].toByteArray();
    const bool voice=args["voice"].toBool();const auto wave=args["waveform"].toByteArray();
    if(attachmentBusy_||!idValid(channel)||channel!=channel_||bytes.isEmpty()||bytes.size()>16*1024*1024
       ||(voice&&(wave.size()!=64||args["seconds"].toInt()<1||args["seconds"].toInt()>120)))return;
    if(QDateTime::currentMSecsSinceEpoch()<blockedUntil_){emit attachmentFinished(generation_,channel,429);return;}
    const auto filename=voice?QString("voice.ogg"):QString("picture.jpg");
    const auto mime=voice?QString("audio/ogg"):QString("image/jpeg");
    QJsonObject attachment{{"id",0},{"filename",filename},{"content_type",mime}};
    if(voice){attachment["duration"]=args["seconds"].toInt();attachment["waveform"]=QString::fromLatin1(wave.toBase64());}
    const auto nonce=QUuid::createUuid().toString(QUuid::Id128);
    const QJsonObject payload{{"nonce",nonce},{"flags",voice?1<<13:0},{"attachments",QJsonArray{attachment}}};
    attachmentBusy_=true;publish();const auto epoch=epoch_;
    auto done=[this,epoch,channel](Reply result){
        if(epoch!=epoch_)return;attachmentBusy_=false;attachmentReply_=nullptr;
        if(result.status>=200&&result.status<300){if(channel==channel_)mergeMessage(result.body.object());status_="Connected";}
        else {if(result.status==429)blockedUntil_=QDateTime::currentMSecsSinceEpoch()+qMax(1,result.retrySeconds)*1000LL;
            status_=result.status==0?"Delivery unknown. Check the conversation before sending again.":"Attachment was not sent. Try again.";}
        publish();emit attachmentFinished(generation_,channel,result.status);
    };
    if(transport_){transport_("POST","/v1/channels/"+channel+"/messages",payload,std::move(done),{});return;}
    if(!network_)network_=new QNetworkAccessManager(this);
    auto* multipart=new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart json;json.setHeader(QNetworkRequest::ContentDispositionHeader,"form-data; name=\"payload_json\"");
    json.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");json.setBody(QJsonDocument(payload).toJson(QJsonDocument::Compact));multipart->append(json);
    QHttpPart file;file.setHeader(QNetworkRequest::ContentDispositionHeader,"form-data; name=\"files[0]\"; filename=\""+filename+"\"");
    file.setHeader(QNetworkRequest::ContentTypeHeader,mime);file.setBody(bytes);multipart->append(file);
    QNetworkRequest request(QUrl("https://api.fluxer.app/v1/channels/"+channel+"/messages"));
    request.setRawHeader("Authorization",token_.toUtf8());request.setRawHeader("User-Agent","TrainerOS/0.1 (native user client)");
    request.setTransferTimeout(30000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto* reply=network_->post(request,multipart);attachmentReply_=reply;multipart->setParent(reply);reply->setReadBufferSize(responseLimit+1);
    auto received=std::make_shared<QByteArray>();
    connect(reply,&QIODevice::readyRead,reply,[reply,received]{received->append(reply->readAll());if(received->size()>responseLimit)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[reply,received,done=std::move(done)]()mutable{
        received->append(reply->readAll());int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(reply->error()!=QNetworkReply::NoError&&status<400)status=0;if(received->size()>responseLimit)status=0;
        done({status,QJsonDocument::fromJson(*received),reply->rawHeader("Retry-After").toInt()});reply->deleteLater();
    });
}
void FluxerSession::command(QString operation, QVariantMap args) {
    if(operation=="profile-update"){profileCommand(args);return;}
    if(operation=="audio-settings") {
        if(self_.isEmpty())return;
        if(args.contains("input"))voiceInput_=args["input"].toString().left(512);
        if(args.contains("output"))voiceOutput_=args["output"].toString().left(512);
        if(args.contains("volume"))voiceVolume_=qBound(0,args["volume"].toInt(),100);
        if(!transport_&&!navigationKey_.isEmpty()){QSettings s;s.setValue(navigationKey_+"voiceInput",voiceInput_);s.setValue(navigationKey_+"voiceOutput",voiceOutput_);s.setValue(navigationKey_+"voiceVolume",voiceVolume_);}
        auto command=audioConfiguration();command["op"]="audio-settings";voiceWrite(command);publish();return;
    }
    if(operation.startsWith("reviews-")){reviewCommand(operation,args);return;}
    if(operation=="cancel-attachment"){if(attachmentReply_)attachmentReply_->abort();return;}
    if(operation.startsWith("voice-")){if(!self_.isEmpty())voiceCommand(operation,args);return;}
    if(operation=="attachment"){if(!self_.isEmpty())sendAttachment(args);return;}
    if(operation=="notification-sound") {
        if(self_.isEmpty())return;
        notificationSound_=!notificationSound_;
        if(!transport_&&!navigationKey_.isEmpty()){QSettings settings;settings.setValue(navigationKey_+"sound",notificationSound_);}
        publish();return;
    }
    if(operation=="conversation") {
        const auto id=args["id"].toString();if(!channels_.contains(id))return;
        face_=channelKind(channels_[id]);guild_=channels_[id]["guild_id"].toString();
        if(!guild_.isEmpty())remember("guild",guild_);
        openConversation(id);publish();return;
    }
    if(operation=="online-capabilities"){online_.setCapabilities(QJsonArray::fromVariantList(args["activities"].toList()));return;}
    if(operation=="online-available"){online_.setAvailable(args["available"].toBool());return;}
    if(operation=="online-answer"){online_.answer(args["accept"].toBool());return;}
    if(operation=="online-close"){online_.answer(false);return;}
    if(operation=="online-frame"){online_.sendFrame(QJsonObject::fromVariantMap(args));return;}
    if(operation=="online-invite"){online_.invite(args["id"].toString());return;}
    if(operation=="online-probe") {
        const auto channel=args["channel"].toString();const auto c=channels_.value(channel);
        if(c["type"].toInt(-1)!=1||c["recipients"].toArray().size()!=1)return;
        const auto peer=c["recipients"].toArray().first().toObject()["id"].toString();
        if(relationships_.value(peer)["type"].toInt()!=1)return;
        online_.probe(channel,peer);return;
    }
    if(operation=="face") {retainHistory();face_=args["face"].toString();if(face_=="communities")communityChecked_.clear();ensureConversation();checkCommunities();publish();return;}
    if(operation=="login") {login();return;}
    if(operation=="cancel-login") {
        if(poll_)poll_->stop();
        if(!code_.isEmpty())request("DELETE","/v1/auth/handoff/"+code_,{{"poll_secret",pollSecret_}},[](Reply){},true);
        ++epoch_;polling_=false;code_.clear();pollSecret_.clear();state_="signed-out";status_="Sign in";publish();return;
    }
    if(operation=="logout") {
        // Retain session until revocation succeeds; offline logout must not silently restore it next boot.
        if(token_.isEmpty())return;
        request("POST","/v1/auth/logout",{},[this](Reply r){
            if(r.status!=204&&r.status!=200&&r.status!=401){fail(r,"Could not sign out. Please try again");return;}
            clearHistoryCache();credential(false,true);reset();state_="signed-out";status_="Signed out";publish();
        });return;
    }
    if(self_.isEmpty())return;
    if(operation=="create-community"){createCommunity(args["text"].toString());return;}
    if(operation=="mark-community"){markCommunity(args["id"].toString());return;}
    if(operation=="community-filter") {
        communityOnly_=!communityOnly_;
        if(communityOnly_&&!communityMarked_.contains(guild_)){guild_.clear();channel_.clear();messages_.clear();messageOrder_.clear();}
        ensureConversation();checkCommunities();publish();return;
    }
    if(operation=="community-invite") {
        const auto guild=args["id"].toString(),channel=communityChannels_.value(guild);
        if(mutationBusy_||!guilds_.contains(guild)||!idValid(channel))return;
        mutationBusy_=true;communityInvite_.clear();publish();
        request("POST","/v1/channels/"+channel+"/invites",{{"max_age",86400},{"max_uses",0},{"temporary",false}},[this,guild](Reply r){
            mutationBusy_=false;
            const auto code=r.body.object()["code"].toString();
            static const QRegularExpression valid("^[A-Za-z0-9_-]{1,100}$");
            if(r.status>=200&&r.status<300&&valid.match(code).hasMatch()&&guild==guild_)communityInvite_="https://fluxer.gg/"+code;
            else communityStatus_="Could not create an invite. Check your community permissions.";
            publish();checkCommunities();
        });return;
    }
    if(operation=="read"){acknowledge(args["channel"].toString(),args["message"].toString());return;}
    if(operation=="mute"||operation=="dnd"||operation=="private") {
        if(operation=="mute") {const auto channel=args["channel"].toString();if(!channels_.contains(channel))return;
            if(muted_.contains(channel))muted_.removeAll(channel);else muted_.append(channel);
        } else if(operation=="dnd")doNotDisturb_=!doNotDisturb_;else privatePreviews_=!privatePreviews_;
        if(!transport_&&!navigationKey_.isEmpty()){QSettings settings;settings.setValue(navigationKey_+"dnd",doNotDisturb_);settings.setValue(navigationKey_+"private",privatePreviews_);settings.setValue(navigationKey_+"muted",muted_);}
        publish();return;
    }
    if(mutate(operation,args))return;
    if(operation=="older"){loadOlderMessages();return;}
    if(operation=="latest"&&!channel_.isEmpty()){historyPast_=false;historyAnchor_.clear();loadMessages(channel_);return;}
    if(operation=="history-position"){if(args["channel"]==channel_){historyAnchor_=args["id"].toString();retainHistory();}return;}
    if(operation=="clear-history"){clearHistoryCache();loadHistoryCache();status_="Local history cleared";publish();return;}
    if(operation=="retry-message"){
        const auto id=args["id"].toString();const auto m=messages_.value(id);
        if(args["channel"]==channel_&&(m["local_delivery"]=="Not sent"||m["local_delivery"].toString().startsWith("Delivery unknown"))&&m["author"].toObject()["id"]==self_)sendText(channel_,m["content"].toString(),id);
        return;
    }
    if(operation=="discard-message"){const auto id=args["id"].toString();if(args["channel"]==channel_&&!messages_.value(id)["local_delivery"].toString().isEmpty()){messages_.remove(id);messageOrder_.removeAll(id);pendingNonces_.remove(id);publish();}return;}
    if(operation=="search") {search(args["mode"].toString(),args["text"].toString(),args["offset"].toInt());return;}
    if(operation=="search-action") {
        const auto id=args["id"].toString();
        QVariantMap row;for(const auto& v:searchResults_)if(v.toMap()["id"]==id){row=v.toMap();break;}
        if(row.isEmpty()||searching_||row["action"]=="Joined"||row["action"]=="Sent")return;
        if(row["kind"]=="person") {command("add",{{"text",id}});return;}
        searching_=true;searchStatus_="Joining...";publish();
        const bool invite=row["kind"]=="invite";const auto revision=searchRevision_;
        request("POST",invite?"/v1/invites/"+QString::fromLatin1(QUrl::toPercentEncoding(id)):"/v1/discovery/guilds/"+id+"/join",{},[this,id,revision](Reply r){
            if(revision==searchRevision_)searching_=false;
            if(r.status<200||r.status>=300){if(revision==searchRevision_){searchStatus_="Could not join. "+r.body.object()["message"].toString().left(180);fail(r,"Could not join");}return;}
            if(revision==searchRevision_){searchStatus_="Joined";for(auto& v:searchResults_){auto row=v.toMap();if(row["id"]==id)row["action"]="Joined";v=row;}}refresh();publish();
        });return;
    }
    if(operation=="refresh") {communityChecked_.clear();communityStatus_.clear();refresh();return;}
    if(operation=="close") {channel_.clear();messages_.clear();messageOrder_.clear();publish();return;}
    const auto id=args["id"].toString();
    if(operation=="guild"&&idValid(id)&&guilds_.contains(id)) {
        if(openingGuild_&&guild_==id)return;
        guild_=id;openingGuild_=true;communityInvite_.clear();remember("guild",id);publish();
        request("GET","/v1/guilds/"+id,{},[this,id](Reply r){
            if(id!=guild_)return;
            openingGuild_=false;
            if(r.status!=200){fail(r,"Could not open this community");return;}
            guilds_[id]=r.body.object();
            communityChannels_[id]=r.body.object()["system_channel_id"].toString();
            for(auto it=channels_.begin();it!=channels_.end();) {if(it.value()["guild_id"]==id)it=channels_.erase(it);else ++it;}
            for(const auto& v:r.body.object()["channels"].toArray()){auto c=v.toObject();if(c["type"].toInt(-1)==0&&idValid(c["id"].toString())&&channels_.size()<1000){c["guild_id"]=id;channels_[c["id"].toString()]=c;}}
            ensureConversation();checkCommunities();publish();
        });return;
    }
    if(operation=="open"&&idValid(id)&&channels_.contains(id)) {openConversation(id);return;}
    if(operation=="dm"&&idValid(id)&&relationships_.value(id)["type"]==1) {
        for(auto it=channels_.cbegin();it!=channels_.cend();++it)if(it.value()["type"]==1)
            for(const auto& recipient:it.value()["recipients"].toArray())if(recipient.toObject()["id"]==id){openConversation(it.key());return;}
        if(openingDm_)return;
        openingDm_=true;
        request("POST","/v1/users/@me/channels",{{"recipient_id",id}},[this](Reply r){
            openingDm_=false;const auto c=r.body.object();const auto id=c["id"].toString();
            if(r.status<200||r.status>=300||!idValid(id)){fail(r,"Could not open conversation");return;}
            channels_[id]=c;if(face_=="chats")openConversation(id);else publish();
        });return;
    }
    if(operation=="send") {
        auto content=args["text"].toString().trimmed();
        const auto intended=args.value("channel",channel_).toString();
        if(intended!=channel_) {emit sendFailed(generation_,intended,content);return;}
        if(content.isEmpty())return;
        if(content.size()>2000||!idValid(channel_)) {
            emit sendFailed(generation_,intended,content);
            status_=content.size()>2000?"Message is too long. Shorten the draft and try again.":"This conversation is unavailable.";
            publish();return;
        }
        sendText(channel_,content,QUuid::createUuid().toString(QUuid::Id128));return;
    }
    if(operation=="add") {
        const auto tag=args["text"].toString().trimmed();const auto split=tag.lastIndexOf('#');
        if(split<1||tag.size()-split!=5){status_="Enter username#1234";publish();return;}
        const auto revision=searchRevision_;
        request("POST","/v1/users/@me/relationships",{{"username",tag.left(split)},{"discriminator",tag.mid(split+1)}},[this,tag,revision](Reply r){
            if(r.status>=200&&r.status<300){if(revision==searchRevision_){searchStatus_="Friend request sent";for(auto& v:searchResults_){auto row=v.toMap();if(row["kind"]=="person"&&row["id"]==tag)row["action"]="Sent";v=row;}}status_="Friend request sent";refresh();publish();}else {if(revision==searchRevision_)searchStatus_="Could not add this person. Check their full tag";fail(r,"Could not send friend request. Check the name and tag");}
        });return;
    }
    if(idValid(id)&&relationships_.contains(id)&&(operation=="accept"||operation=="remove"||operation=="block")) {
        request(operation=="remove"?"DELETE":"PUT","/v1/users/@me/relationships/"+id,
            operation=="remove"?QJsonObject():QJsonObject{{"type",operation=="block"?2:1}},[this](Reply r){
                if(r.status>=200&&r.status<300)refresh();else fail(r,"Could not update this friendship");
            });
    }
}

bool FluxerSession::mutate(const QString& operation,const QVariantMap& args) {
    if(!QStringList{"create-group","rename-group","add-member","remove-member","leave-group","edit-message","delete-message"}.contains(operation))return false;
    if(mutationBusy_)return true;
    const auto channel=args["channel"].toString(),id=args["id"].toString();
    const auto c=channels_.value(channel);QByteArray method;QString path;QJsonObject body;
    const bool message=operation.endsWith("message");
    if(message) {
        const auto m=messages_.value(id);const auto type=m["type"].toInt();
        if(channel!=channel_||!idValid(id)||m["author"].toObject()["id"]!=self_||(type!=0&&type!=19)||!m["local_delivery"].toString().isEmpty())return true;
        method=operation=="edit-message"?"PATCH":"DELETE";path="/v1/channels/"+channel+"/messages/"+id;
        if(operation=="edit-message") {
            const auto content=args["text"].toString().trimmed();
            if(content.isEmpty()||content.size()>2000||!m["message_snapshots"].toArray().isEmpty())return true;
            body["content"]=content; // Omitted attachments/embeds remain intact.
        }
    } else if(operation=="create-group") {
        const auto ids=args["recipients"].toStringList();QJsonArray recipients;
        if(ids.isEmpty()||ids.size()>49)return true;
        for(const auto& recipient:ids){if(!idValid(recipient)||recipient==self_||recipients.contains(recipient))return true;recipients.append(recipient);}
        method="POST";path="/v1/users/@me/channels";body["recipients"]=recipients;
    } else {
        if(!idValid(channel)||c["type"].toInt()!=3||!c["guild_id"].toString().isEmpty())return true;
        path="/v1/channels/"+channel;
        if(operation=="rename-group") {
            const auto name=args["text"].toString().trimmed();if(name.isEmpty()||name.size()>100)return true;
            method="PATCH";body["name"]=name;
        } else if(operation=="leave-group")method="DELETE";
        else {
            if(!idValid(id)||id==self_)return true;
            if(operation=="remove-member"&&c["owner_id"]!=self_)return true;
            method=operation=="add-member"?"PUT":"DELETE";path+="/recipients/"+id;
        }
    }
    mutationBusy_=true;++channelRevision_;status_="Updating...";publish();
    verifiedRequest(method,path,body,[this,operation,channel,id,message](Reply r){
        mutationBusy_=false;
        const bool ok=r.status>=200&&r.status<300;
        emit mutationFinished(generation_,operation,channel,id,ok);
        if(!ok){
            const auto code=r.body.object()["code"].toString();
            QString problem=r.body.object()["message"].toString().left(180);
            if(code=="CAPTCHA_REQUIRED"||code=="INVALID_CAPTCHA")problem="A verification challenge is required. Complete this action in the web client.";
            if(problem.isEmpty())problem="Could not complete this action";
            if(r.status==0&&operation=="create-group"){status_="Connection interrupted. Refresh Groups before trying again.";publish();return;}
            fail(r,problem);return;
        }
        status_="Connected";++channelRevision_;
        if(message) {
            ++messageRevision_;
            if(channel==channel_) {
                if(operation=="delete-message"){messages_.remove(id);messageOrder_.removeAll(id);}
                else if(messages_.contains(id))mergeMessage(r.body.object());
            }
        } else if(operation=="leave-group") {
            historyCache_.remove(channel);historyLru_.removeAll(channel);callNotices_.remove(channel);channels_.remove(channel);unread_.remove(channel);
            if(channel_==channel){channel_.clear();messages_.clear();messageOrder_.clear();++historyRequest_;historyBusy_=false;}
            ensureConversation();
        } else if(operation=="create-group"||operation=="rename-group") {
            const auto updated=r.body.object();const auto updatedId=updated["id"].toString();
            if(idValid(updatedId)&&updated["type"].toInt()==3){channels_[updatedId]=updated;if(operation=="create-group"&&face_=="groups")openConversation(updatedId);}
        } else {
            const auto revision=channelRevision_;
            request("GET","/v1/channels/"+channel,{},[this,channel,revision](Reply reply){
                if(revision!=channelRevision_)return;
                if(reply.status==200&&reply.body.object()["id"]==channel){channels_[channel]=reply.body.object();publish();}
                else if(reply.status!=200)fail(reply,"Could not refresh group members");
            });
        }
        publish();
    });
    return true;
}

QString FluxerSession::channelKind(const QJsonObject& channel) const {
    return !channel["guild_id"].toString().isEmpty()?"communities":channel["type"].toInt()==3?"groups":"chats";
}
void FluxerSession::remember(const QString& key,const QString& value) {
    preferred_[key]=value;
    if(!transport_&&!navigationKey_.isEmpty()){QSettings settings;settings.setValue(navigationKey_+key,value);}
}
void FluxerSession::openConversation(const QString& id) {
    if(!channels_.contains(id)||channel_==id)return;
    retainHistory();
    channel_=id;remember(channelKind(channels_[id]),id);
    ++historyRequest_;historyBusy_=historyMore_=historyPast_=false;
    messages_.clear();messageOrder_.clear();restoreHistory(id);publish();loadMessages(id);
}
void FluxerSession::ensureConversation() {
    if(self_.isEmpty()||face_=="friends"||!channelsLoaded_)return;
    if(face_=="communities"&&guild_.isEmpty()&&!guilds_.isEmpty()) {
        auto candidates=guilds_.keys();std::sort(candidates.begin(),candidates.end());
        if(communityOnly_)candidates.removeIf([this](const QString& id){return !communityMarked_.contains(id);});
        if(!candidates.isEmpty()) {const auto id=candidates.contains(preferred_["guild"])?preferred_["guild"]:candidates.first();
            command("guild",{{"id",id}});return;}
    }
    auto matches=[&](const QJsonObject& c){return channelKind(c)==face_&&(face_!="communities"||c["guild_id"]==guild_);};
    if(channels_.contains(channel_)&&matches(channels_[channel_]))return;
    QString choice;
    if(channels_.contains(preferred_[face_])&&matches(channels_[preferred_[face_]]))choice=preferred_[face_];
    else for(auto it=channels_.cbegin();it!=channels_.cend();++it)if(matches(it.value())) {
        const auto last=it.value()["last_message_id"].toString();const auto old=channels_.value(choice)["last_message_id"].toString();
        if(choice.isEmpty()||last.size()>old.size()||(last.size()==old.size()&&(last>old||(last==old&&it.key()<choice))))choice=it.key();
    }
    if(!choice.isEmpty()){openConversation(choice);return;}
    channel_.clear();messages_.clear();messageOrder_.clear();
    if(face_=="chats"&&friendsLoaded_&&!openingDm_) {
        QString first;for(auto it=relationships_.cbegin();it!=relationships_.cend();++it)
            if(it.value()["type"]==1&&(first.isEmpty()||label(it.value()["user"].toObject())<label(relationships_[first]["user"].toObject())))first=it.key();
        if(!first.isEmpty())command("dm",{{"id",first}});
    }
}
void FluxerSession::search(QString mode,QString text,int offset) {
    if(mode!="people"&&mode!="communities"&&mode!="traineros"&&mode!="invite")return;
    const auto revision=++searchRevision_;searchMode_=mode;searchText_=text.trimmed().left(256);
    searchOffset_=qMax(0,offset);searchTotal_=0;searchResults_.clear();searching_=false;searchStatus_.clear();
    if(mode=="people") {
        static const QRegularExpression tag("^[A-Za-z0-9_]{1,32}#[0-9]{4}$");
        if(!tag.match(searchText_).hasMatch())searchStatus_="Enter their full tag: username#1234";
        else {searchStatus_="Ready to send a friend request";searchResults_.append(QVariantMap{{"id",searchText_},{"name",searchText_},{"description","Send a friend request"},{"detail","New friend"},{"kind","person"},{"action","Add friend"}});}
        publish();return;
    }
    QString path;
    if(mode=="invite") {
        QString code=searchText_;
        if(code.contains('/')) {QUrl url(code.contains("://")?code:"https://"+code);code=url.path().section('/',-1);}
        if(code.isEmpty()){searchStatus_="Enter a group or community invite link";publish();return;}
        path="/v1/invites/"+QString::fromLatin1(QUrl::toPercentEncoding(code));
    } else {
        QUrlQuery q;if(!searchText_.isEmpty())q.addQueryItem("query",searchText_.left(100));q.addQueryItem("limit","24");q.addQueryItem("offset",QString::number(searchOffset_));
        if(mode=="traineros")q.addQueryItem("tag","traineros-v1");
        path="/v1/discovery/guilds?"+q.toString(QUrl::FullyEncoded);
    }
    searching_=true;searchStatus_="Searching...";publish();
    request("GET",path,{},[this,revision,mode](Reply r){
        if(revision!=searchRevision_)return;
        searching_=false;
        if(r.status!=200){searchStatus_=r.status==404?"Invite not found or expired":r.body.object()["message"].toString().left(180);if(searchStatus_.isEmpty())searchStatus_="Search is unavailable. Try again";if(r.status==401)fail(r,searchStatus_);else publish();return;}
        if(mode=="invite") {
            const auto b=r.body.object();const auto g=b["guild"].toObject(),c=b["channel"].toObject();
            QString name=g["name"].toString();if(name.isEmpty())name=c["name"].toString();if(name.isEmpty())name="Group conversation";
            searchResults_.append(QVariantMap{{"id",b["code"].toString()},{"name",name},{"description",QString("%1 members").arg(b["member_count"].toInt())},{"detail",g.isEmpty()?"Group invite":"Community invite"},{"kind","invite"},{"action","Join"}});searchTotal_=1;
        } else {
            const auto body=r.body.object();searchTotal_=body["total"].toInt();
            for(const auto& v:body["guilds"].toArray()){auto g=v.toObject();if(!idValid(g["id"].toString()))continue;
                searchResults_.append(QVariantMap{{"id",g["id"].toString()},{"name",g["name"].toString()},{"description",g["description"].toString()},{"detail",QString("%1 members").arg(g["member_count"].toInt())},{"kind","community"},{"action",guilds_.contains(g["id"].toString())?"Joined":"Join"}});
            }
        }
        searchStatus_=searchResults_.isEmpty()?(mode=="traineros"?"No public TrainerOS communities yet. Private communities use invite links.":"No matches"):QString("%1 results").arg(searchTotal_);publish();
    });
}

void FluxerSession::invalidateCommunity(const QString& id) {
    if(id.isEmpty())return;
    ++communityRevision_[id];communityChecked_.remove(id);communityMarked_.remove(id);
}
void FluxerSession::checkCommunities() {
    if(self_.isEmpty()||face_!="communities"||communityChecking_||mutationBusy_)return;
    QString id;
    for(auto it=guilds_.cbegin();it!=guilds_.cend();++it)if(!communityChecked_.contains(it.key())){id=it.key();break;}
    if(id.isEmpty())return;
    communityChecking_=true;communityChecked_.insert(id);
    const auto revision=communityRevision_.value(id);
    auto finish=[this,id,revision](bool marked) {
        communityChecking_=false;
        if(guilds_.contains(id)&&revision==communityRevision_.value(id)) {
            if(marked)communityMarked_.insert(id);else communityMarked_.remove(id);
        }
        if(communityOnly_&&!communityMarked_.contains(guild_)){guild_.clear();channel_.clear();messages_.clear();messageOrder_.clear();}
        ensureConversation();publish();
        // One community at a time; never fan out requests on the UI thread.
        const auto epoch=epoch_;
        QTimer::singleShot(300,this,[this,epoch]{if(epoch==epoch_)checkCommunities();});
    };
    request("GET","/v1/guilds/"+id,{},[this,id,revision,finish](Reply r){
        if(!guilds_.contains(id)||revision!=communityRevision_.value(id)||r.status!=200||r.body.object()["id"]!=id){finish(false);return;}
        const auto g=r.body.object();const auto channel=g["system_channel_id"].toString(),owner=g["owner_id"].toString();
        guilds_[id]["owner_id"]=owner;communityChannels_[id]=channel;
        bool visible=false;for(const auto& c:g["channels"].toArray())if(c.toObject()["id"]==channel&&c.toObject()["type"].toInt(-1)==0)visible=true;
        if(!visible||!idValid(channel)||!idValid(owner)){finish(false);return;}
        request("GET","/v1/channels/"+channel+"/messages/pins?limit=50",{},[id,channel,owner,finish](Reply pins){
            bool marked=false;
            if(pins.status==200)for(const auto& p:pins.body.object()["items"].toArray()) {
                const auto m=p.toObject()["message"].toObject();
                if(m["pinned"].toBool()&&communityIdentity::matches(m,id,owner,channel))marked=true;
            }
            finish(marked);
        });
    });
}
void FluxerSession::createCommunity(QString name) {
    name=name.trimmed();if(mutationBusy_||name.isEmpty()||name.size()>100)return;
    mutationBusy_=true;communityStatus_="Creating your community...";publish();
    const QJsonObject setup{{"name","TrainerOS"},{"roles",QJsonArray{}},{"system_channel_id","1"},
        {"channels",QJsonArray{QJsonObject{{"id","1"},{"type",0},{"name","general"},{"position",0}}}}};
    verifiedRequest("POST","/v1/guilds",{{"name",name},{"template",setup}},[this](Reply r){
        mutationBusy_=false;const auto g=r.body.object();const auto id=g["id"].toString();
        if(r.status<200||r.status>=300||!idValid(id)) {
            const auto code=g["code"].toString();
            communityStatus_=code=="GUILD_CREATION_EMAIL_VERIFICATION_REQUIRED"?"Verify your email before creating a community."
                :r.status==0?"Connection lost. Refresh your communities before trying again."
                :"Community was not created. "+g["message"].toString().left(140);
            fail(r,communityStatus_);return;
        }
        ++guildListRevision_;guilds_[id]=g;communityOnly_=false;
        if(face_=="communities"){guild_=id;remember("guild",id);}
        markCommunity(id);
    });
}
void FluxerSession::markCommunity(QString id) {
    if(mutationBusy_||!idValid(id)||!guilds_.contains(id))return;
    mutationBusy_=true;communityStatus_="Preparing your community...";publish();
    auto finish=[this,id](bool success,const Reply& reply) {
        mutationBusy_=false;
        communityStatus_=success?QString():"Community kept. TrainerOS setup is incomplete; retry from Options.";
        if(reply.status==401){fail(reply,communityStatus_);return;}
        invalidateCommunity(id);
        if(face_=="communities"&&guild_==id)command("guild",{{"id",id}});
        checkCommunities();publish();
    };
    request("GET","/v1/guilds/"+id,{},[this,id,finish](Reply r){
        const auto g=r.body.object();const auto channel=g["system_channel_id"].toString();
        if(r.status!=200||g["id"]!=id||g["owner_id"]!=self_||!idValid(channel)){finish(false,r);return;}
        guilds_[id]=g;communityChannels_[id]=channel;
        auto pin=[this,id,channel,finish](const QJsonObject& message) {
            const auto messageId=message["id"].toString();
            if(!idValid(messageId)||!communityIdentity::matches(message,id,self_,channel)){finish(false,{});return;}
            request("PUT","/v1/channels/"+channel+"/pins/"+messageId,{},[finish](Reply reply){finish(reply.status==204,reply);});
        };
        // Reuse a completed post after a pin failure, including across restart.
        request("GET","/v1/channels/"+channel+"/messages?limit=50",{},[this,id,channel,pin,finish](Reply history){
            if(history.status!=200){finish(false,history);return;}
            for(const auto& v:history.body.array())if(communityIdentity::matches(v.toObject(),id,self_,channel)){pin(v.toObject());return;}
            request("POST","/v1/channels/"+channel+"/messages",
                {{"content",communityIdentity::content(id)},{"allowed_mentions",QJsonObject{{"parse",QJsonArray{}}}}},
                [pin,finish](Reply message){if(message.status>=200&&message.status<300)pin(message.body.object());else finish(false,message);});
        });
    });
}

void FluxerSession::gatewaySend(int op,const QJsonValue& data) {
    if(socket_)socket_->sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"op",op},{"d",data}}).toJson(QJsonDocument::Compact)));
}
void FluxerSession::openGateway() {
    if(transport_||token_.isEmpty())return;
    if(socket_) {socket_->disconnect(this);socket_->abort();socket_->deleteLater();}
    socket_=new QWebSocket(QString(),QWebSocketProtocol::VersionLatest,this);
    socket_->setMaxAllowedIncomingMessageSize(responseLimit);socket_->setMaxAllowedIncomingFrameSize(responseLimit);
    if(!heartbeat_) {heartbeat_=new QTimer(this);connect(heartbeat_,&QTimer::timeout,this,[this]{
        if(awaitingAck_){socket_->abort();return;}awaitingAck_=true;gatewaySend(1,sequence_);
    });}
    connect(socket_,&QWebSocket::textMessageReceived,this,[this](const QString& text){gatewayEvent(QJsonDocument::fromJson(text.toUtf8()).object());});
    connect(socket_,&QWebSocket::disconnected,this,&FluxerSession::disconnected);
    connect(socket_,&QWebSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){disconnected();});
    socket_->open(QUrl("wss://gateway.fluxer.app/?v=1&encoding=json"));
}
void FluxerSession::disconnected() {
    // Chat Gateway and LiveKit are independent connections. Keep the call and
    // microphone choice while chat reconnects; the media SDK handles its route.
    ++onlineSendRevision_;onlineQueue_.clear();onlineSending_=false;onlineSendTimer_.stop();
    online_.close("Connection interrupted. Reconnect the activity to recover.");
    if(heartbeat_)heartbeat_->stop();
    awaitingAck_=false;
    if(token_.isEmpty())return;
    status_="Reconnecting…";publish();
    if(!reconnect_) {reconnect_=new QTimer(this);reconnect_->setSingleShot(true);connect(reconnect_,&QTimer::timeout,this,&FluxerSession::openGateway);}
    if(!reconnect_->isActive())reconnect_->start(qMin(60000,1000*(1<<qMin(reconnectAttempt_++,5)))+int(QRandomGenerator::global()->bounded(500U)));
}
void FluxerSession::gatewayEvent(const QJsonObject& event) {
    const auto op=event["op"].toInt(-1);const auto d=event["d"].toObject();
    if(op==10) {
        const int interval=d["heartbeat_interval"].toInt();if(interval<1000||interval>120000){socket_->abort();return;}
        awaitingAck_=false;heartbeat_->start(interval);
        if(!gatewaySession_.isEmpty())gatewaySend(6,QJsonObject{{"token",token_},{"session_id",gatewaySession_},{"seq",sequence_}});
        else gatewaySend(2,QJsonObject{{"token",token_},{"properties",QJsonObject{{"e2ee_capable",voiceAvailable()},{"os","Linux"},{"browser","TrainerOS"},{"device","handheld"}}}});
    } else if(op==11)awaitingAck_=false;
    else if(op==1)gatewaySend(1,sequence_);
    else if(op==7)socket_->abort();
    else if(op==9) {gatewaySession_.clear();sequence_=0;socket_->abort();request("GET","/v1/users/@me",{},[this](Reply r){if(r.status!=200)fail(r,"Session interrupted");});}
    else if(op==0) {
        sequence_=event["s"].toInteger();const auto type=event["t"].toString();
        if(type=="USER_UPDATE"){updateProfile(d);publish();return;}
        if(type=="VOICE_SERVER_UPDATE"){voiceGrant(d);return;}
        if(type=="VOICE_STATE_UPDATE") {
            if(!voiceConnection_.isEmpty()&&d["connection_id"]==voiceConnection_&&d["user_id"]==self_&&d["channel_id"].isNull()){leaveVoice();voiceStatus_="Call disconnected. Try again.";publish();}return;
        }
        if(type=="CALL_CREATE"||type=="CALL_UPDATE"||type=="CALL_DELETE") {
            const auto id=d["channel_id"].toString();
            const bool wasRinging=calls_.value(id)["ringing"].toArray().contains(self_);
            if(!idValid(id))return;
            if(type=="CALL_DELETE") {
                if(d["unavailable"].toBool()){calls_[id]["unavailable"]=true;calls_[id]["ringing"]=QJsonArray{};}
                else calls_.remove(id);
            } else {
                auto call=type=="CALL_CREATE"?QJsonObject{}:calls_.value(id);
                for(auto it=d.begin();it!=d.end();++it)call[it.key()]=it.value();
                calls_[id]=call;
            }
            updateCallNotice(type,id,d);
            if(type!="CALL_DELETE"&&!wasRinging&&d["ringing"].toArray().contains(self_)&&!doNotDisturb_&&!muted_.contains(id))emit incomingMessage(generation_,id,"Incoming call","Press Home to answer");
            publish();return;
        }
        if(type=="MESSAGE_UPDATE"||type=="MESSAGE_DELETE"||type=="CHANNEL_PINS_UPDATE"||type=="CHANNEL_DELETE"||type=="GUILD_UPDATE"||type=="GUILD_DELETE") {
            const auto channel=d["channel_id"].toString(d["id"].toString());
            for(auto it=communityChannels_.cbegin();it!=communityChannels_.cend();++it)
                if(it.value()==channel||it.key()==d["id"].toString())invalidateCommunity(it.key());
            checkCommunities();
        }
        const auto eventChannel=d["channel_id"].toString();
        if(eventChannel!=channel_&&historyCache_.contains(eventChannel)&&type.startsWith("MESSAGE_")) {
            auto entry=historyCache_.value(eventChannel);auto rows=entry["rows"].toArray();
            for(qsizetype i=rows.size();i>0;--i) {
                auto m=rows[i-1].toObject();
                if(m["id"]==d["id"]) {
                    if(type=="MESSAGE_DELETE")rows.removeAt(i-1);
                    else if(type=="MESSAGE_UPDATE"){for(auto it=d.begin();it!=d.end();++it)m[it.key()]=it.value();rows[i-1]=m;}
                } else if(type=="MESSAGE_CREATE"&&!d["nonce"].toString().isEmpty()&&m["id"]==d["nonce"]) {
                    pendingNonces_.remove(m["id"].toString());rows[i-1]=d;
                }
            }
            entry["rows"]=rows;historyCache_[eventChannel]=entry;
            if(!historyFile_.isEmpty()&&!historySave_.isActive())historySave_.start();
        }
        if(type=="READY"||type=="RESUMED") {
            if(type=="READY") {gatewaySession_=d["session_id"].toString();readThrough_.clear();quietThrough_.clear();unread_.clear();mentions_.clear();calls_.clear();readsReady_=true;
                for(const auto& v:d["read_states"].toArray())applyReadState(v.toObject(),false);
                for(auto it=channels_.cbegin();it!=channels_.cend();++it)updateUnread(it.key());}
            reconnectAttempt_=0;status_="Connected";refresh();
            if(type=="READY")reconcileVoice();
        }
        else if(type=="MESSAGE_ACK")applyReadState(d,true);
        else if(type=="MESSAGE_CREATE"||type=="MESSAGE_UPDATE") {
            if(type=="MESSAGE_CREATE"&&d["webhook_id"].toString().isEmpty()&&d["type"].toInt(-1)==0) {
                const auto author=d["author"].toObject();const auto id=author["id"].toString();
                const auto c=channels_.value(d["channel_id"].toString());
                if(c["type"].toInt(-1)==1&&c["recipients"].toArray().size()==1&&c["recipients"].toArray().first().toObject()["id"]==id&&relationships_.value(id)["type"].toInt()==1)
                    online_.receive(d["channel_id"].toString(),id,label(author),OnlineLink::decode(d["content"].toString()));
            }
            ++messageRevision_;
            if(!historyPast_||messages_.contains(d["id"].toString())||pendingNonces_.contains(d["nonce"].toString()))mergeMessage(d);
            const auto channel=d["channel_id"].toString();
            if(type=="MESSAGE_CREATE"&&channels_.contains(channel)) {
                const auto id=d["id"].toString();const bool fresh=newer(id,channels_[channel]["last_message_id"].toString());
                // A transport packet or our own post cannot create a new chat
                // unread badge, but must never erase an earlier unread message.
                if(fresh&&!unread_.value(channel)&&(d["author"].toObject()["id"]==self_||!OnlineLink::decode(d["content"].toString()).isEmpty()))
                    quietThrough_[channel]=id;
                if(fresh){++channelRevision_;channels_[channel]["last_message_id"]=id;}
                updateUnread(channel);
                const auto author=d["author"].toObject();
                bool mentioned=d["mention_everyone"].toBool();
                for(const auto& user:d["mentions"].toArray())if(user.toObject()["id"]==self_)mentioned=true;
                if(fresh&&mentioned&&author["id"]!=self_)++mentions_[channel];
                if(fresh&&OnlineLink::decode(d["content"].toString()).isEmpty()&&author["id"]!=self_&&relationships_.value(author["id"].toString())["type"].toInt()!=2
                    &&!muted_.contains(channel)&&!doNotDisturb_)
                    emit incomingMessage(generation_,channel,privatePreviews_?(mentioned?QString("New mention"):QString("New message")):label(author),
                        privatePreviews_?QString("Open Social to catch up"):d["content"].toString().left(160));
            }
        } else if(type=="MESSAGE_DELETE") {++messageRevision_;const auto id=d["id"].toString();messages_.remove(id);messageOrder_.removeAll(id);}
        else if(type.startsWith("RELATIONSHIP_")||type.startsWith("CHANNEL_")||type.startsWith("GUILD_")) {
            if(type=="RELATIONSHIP_ADD") {
                const auto id=d["id"].toString();const bool incoming=d["type"].toInt()==3&&relationships_.value(id)["type"].toInt()!=3;
                if(idValid(id))relationships_[id]=d;
                if(incoming&&!doNotDisturb_)emit incomingMessage(generation_,"request:"+id,"Friend request",privatePreviews_?QString("Open Home notifications"):label(d["user"].toObject()));
            }
            if(type=="CHANNEL_RECIPIENT_REMOVE"&&d["channel_id"]==voiceChannel_){
                if(d["user"].toObject()["id"]==self_||d["user_id"]==self_){leaveVoice();voiceStatus_="You left the call";}
                else reconcileVoice();
            }
            if(type.startsWith("RELATIONSHIP_")&&d["id"]==online_.peerAccount()&&(type=="RELATIONSHIP_REMOVE"||d["type"].toInt()!=1))online_.close("Friend connection ended");
            ++channelRevision_;
            if(type=="CHANNEL_DELETE") {if(d["id"]==voiceChannel_)leaveVoice();calls_.remove(d["id"].toString());callNotices_.remove(d["id"].toString());historyCache_.remove(d["id"].toString());historyLru_.removeAll(d["id"].toString());channels_.remove(d["id"].toString());if(d["id"]==channel_){channel_.clear();messages_.clear();messageOrder_.clear();}}
            refresh();
            if(!guild_.isEmpty())command("guild",{{"id",guild_}});
        } else return; // Presence/guild chatter outside this surface must not repaint the shell.
        publish();
    }
}
}
