#include "FluxerSession.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QWebSocket>
#include <QJsonArray>
#include <QDateTime>
#include <QRandomGenerator>
#include <QUuid>
#include <QCryptographicHash>
#include <QPointer>
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
constexpr int responseLimit = 2 * 1024 * 1024;
}
FluxerSession::FluxerSession(QObject* parent) : QObject(parent) {}
FluxerSession::~FluxerSession() { reset(); }
void FluxerSession::reset() {
    ++epoch_;
    if (poll_) poll_->stop();
    if (heartbeat_) heartbeat_->stop();
    if (reconnect_) reconnect_->stop();
    if (socket_) { socket_->disconnect(this); socket_->abort(); socket_->deleteLater(); socket_ = nullptr; }
    if (network_) {
        for(auto* reply:network_->findChildren<QNetworkReply*>())reply->abort();
        network_->deleteLater(); network_ = nullptr;
    }
    token_.clear(); self_.clear(); name_.clear(); code_.clear(); pollSecret_.clear();
    gatewaySession_.clear(); channel_.clear(); relationships_.clear(); channels_.clear();
    messages_.clear(); messageOrder_.clear(); pendingNonces_.clear(); unread_.clear(); guilds_.clear(); guild_.clear();
    polling_ = refreshing_ = awaitingAck_ = remembered_ = false;
    reconnectAttempt_ = 0; sequence_ = 0; blockedUntil_ = 0;
}
void FluxerSession::stop() { reset(); }
void FluxerSession::setOwner(QString owner, quint64 generation) {
    reset(); owner_ = std::move(owner); generation_ = generation;
    state_ = "signed-out"; status_ = "Sign in to Fluxer"; publish();
    if (!owner_.isEmpty() && !transport_) credential();
}
QString FluxerSession::label(const QJsonObject& user) const {
    auto name = user["global_name"].toString();
    return (name.isEmpty() ? user["username"].toString() : name).left(100);
}
void FluxerSession::publish() {
    QVariantList friends, chats, messages, communities;
    for (auto it = relationships_.cbegin(); it != relationships_.cend(); ++it) {
        const auto user = it.value()["user"].toObject(); const int type = it.value()["type"].toInt();
        friends.append(QVariantMap{{"id",it.key()},{"name",label(user)}, {"type",type},
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
        chats.append(QVariantMap{{"id",it.key()},{"name",name.isEmpty()?QString("Conversation"):name.left(120)},
            {"detail",unread_.value(it.key())?QString("New messages"):QString("Private conversation")},
            {"kind",c["guild_id"].toString().isEmpty() ? (c["type"].toInt()==3 ? "groups" : "chats") : "community-channel"},{"guild",c["guild_id"].toString()},{"unread",unread_.value(it.key())},{"last",c["last_message_id"].toString()}});
    }
    std::sort(chats.begin(),chats.end(),[](const QVariant& a,const QVariant& b){
        const QString aa=a.toMap()["last"].toString(),bb=b.toMap()["last"].toString();
        return aa.size()==bb.size()?aa>bb:aa.size()>bb.size();
    });
    for (const auto& id:messageOrder_) {
        const auto m=messages_.value(id);
        messages.append(QVariantMap{{"id",id},{"name",label(m["author"].toObject())},
            {"mine",m["author"].toObject()["id"]==self_},{"text",m["content"].toString().left(4000)},
            {"delivery",m["local_delivery"].toString()}, {"media",!m["attachments"].toArray().isEmpty()}});
    }
    for(auto it=guilds_.cbegin();it!=guilds_.cend();++it)communities.append(QVariantMap{{"id",it.key()},{"name",it.value()["name"].toString()},{"kind","community"},{"detail","Community"}});
    std::sort(communities.begin(),communities.end(),[](const QVariant& a,const QVariant& b){return a.toMap()["name"].toString()<b.toMap()["name"].toString();});
    emit snapshot(generation_, {{"state",state_},{"status",status_},{"name",name_},{"code",code_},
        {"guild",guild_},{"communities",communities},{"remembered",remembered_},{"friends",friends},{"chats",chats},{"messages",messages},{"channel",channel_}});
}
void FluxerSession::request(QByteArray method, QString path, QJsonObject body, Completion done, bool anonymous) {
    const auto epoch=epoch_; QPointer<FluxerSession> guard(this);
    auto complete=[this,guard,epoch,done=std::move(done)](Reply r) {
        if(!guard || epoch!=epoch_) return;
        if(r.status==429) blockedUntil_=qMax(blockedUntil_,QDateTime::currentMSecsSinceEpoch()+qMax(1,r.retrySeconds)*1000LL);
        done(std::move(r));
    };
    if(QDateTime::currentMSecsSinceEpoch()<blockedUntil_) { complete({429,{ },1}); return; }
    if(transport_) { transport_(method,path,body,std::move(complete)); return; }
    if(!network_) network_=new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl("https://api.fluxer.app"+path));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    req.setRawHeader("User-Agent","TrainerOS/0.1 (native user client)");
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
void FluxerSession::fail(const Reply& r, QString fallback) {
    if(r.status==401) { credential(false,true); reset(); state_="signed-out"; status_="Please sign in again"; }
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
    if(transport_ || owner_.isEmpty() || !credentialStoreAvailable()) return;
    const auto epoch=epoch_;
    const auto key=QString::fromLatin1(QCryptographicHash::hash(("https://fluxer.app\n"+owner_).toUtf8(),QCryptographicHash::Sha256).toHex());
    QKeychain::Job* job;
    if(remove) { auto* j=new QKeychain::DeletePasswordJob("TrainerOS.Fluxer",this); j->setKey(key); job=j; }
    else if(write) { auto* j=new QKeychain::WritePasswordJob("TrainerOS.Fluxer",this); j->setKey(key); j->setTextData(token_); job=j; }
    else { auto* j=new QKeychain::ReadPasswordJob("TrainerOS.Fluxer",this); j->setKey(key); job=j; }
    job->setInsecureFallback(false);
    connect(job,&QKeychain::Job::finished,this,[this,epoch,write,remove](QKeychain::Job* j){
        if(epoch!=epoch_ || remove) return;
        if(j->error()!=QKeychain::NoError) { remembered_=false; publish(); return; }
        remembered_=true;
        if(!write) { token_=static_cast<QKeychain::ReadPasswordJob*>(j)->textData(); if(!token_.isEmpty()) authenticated(); }
        else publish();
    });
    job->start();
}
void FluxerSession::login() {
    if(owner_.isEmpty() || state_=="authorizing" || !token_.isEmpty()) return;
    state_="authorizing"; status_="Preparing browser sign-in…"; publish();
    // Public instance only for this increment. Validate advertised endpoints before authentication.
    request("GET","/.well-known/fluxer",{},[this](Reply r){
        const auto endpoints=r.body.object()["endpoints"].toObject();
        if(r.status!=200 || endpoints["api_public"]!="https://api.fluxer.app" || endpoints["gateway"]!="wss://gateway.fluxer.app") {
            state_="signed-out"; fail(r,"Fluxer sign-in is unavailable"); return;
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
    state_="connecting";status_="Connecting…";publish();
    request("GET","/v1/users/@me",{},[this](Reply r){
        if(r.status!=200||!idValid(r.body.object()["id"].toString())) {fail(r,"Could not verify your account");return;}
        self_=r.body.object()["id"].toString();name_=label(r.body.object());
        state_="connected";status_="Connected";credential(true);refresh();openGateway();publish();
    });
}
void FluxerSession::refresh() {
    if(self_.isEmpty()||refreshing_)return;
    refreshing_=true;
    request("GET","/v1/users/@me/relationships",{},[this](Reply r){
        refreshing_=false;
        if(r.status!=200) {fail(r,"Could not load friends");return;}
        relationships_.clear();
        for(const auto& v:r.body.array()) {auto o=v.toObject();auto id=o["id"].toString();if(idValid(id)&&relationships_.size()<2000)relationships_[id]=o;}
        publish();
    });
    request("GET","/v1/users/@me/channels",{},[this](Reply r){
        if(r.status!=200) {fail(r,"Could not load conversations");return;}
        for(auto it=channels_.begin();it!=channels_.end();) {if(it.value()["guild_id"].toString().isEmpty())it=channels_.erase(it);else ++it;}
        for(const auto& v:r.body.array()) {auto o=v.toObject();auto id=o["id"].toString();if(idValid(id)&&channels_.size()<500)channels_[id]=o;}
        if(!channel_.isEmpty()&&!channels_.contains(channel_)) {channel_.clear();messages_.clear();messageOrder_.clear();}
        status_="Connected";publish();
    });
    request("GET","/v1/users/@me/guilds?limit=100",{},[this](Reply r){
        if(r.status!=200){fail(r,"Could not load communities");return;}
        guilds_.clear();for(const auto& v:r.body.array()){auto g=v.toObject();if(idValid(g["id"].toString()))guilds_[g["id"].toString()]=g;}
        for(auto it=channels_.begin();it!=channels_.end();) {auto guild=it.value()["guild_id"].toString();if(!guild.isEmpty()&&!guilds_.contains(guild))it=channels_.erase(it);else ++it;}
        if(!guild_.isEmpty()&&!guilds_.contains(guild_)){guild_.clear();channel_.clear();messages_.clear();messageOrder_.clear();}
        publish();
    });
    if(!channel_.isEmpty())loadMessages(channel_);
}
void FluxerSession::loadMessages(QString channel) {
    const auto revision=messageRevision_;
    request("GET","/v1/channels/"+channel+"/messages?limit=50",{},[this,channel,revision](Reply r){
        if(channel!=channel_)return;
        if(revision!=messageRevision_)return; // Never resurrect a deleted message or erase a fresh event.
        if(r.status!=200) {if(r.status==403||r.status==404){messages_.clear();messageOrder_.clear();}fail(r,"Could not load messages");return;}
        // Keep uncertain sends; the server response is authoritative for remote history.
        auto old=messages_;messages_.clear();messageOrder_.clear();
        const auto list=r.body.array();for(auto i=list.size();i>0;--i)mergeMessage(list.at(i-1).toObject());
        for(auto it=old.cbegin();it!=old.cend();++it) if(!it.value()["local_delivery"].toString().isEmpty()&&pendingNonces_.contains(it.key()))mergeMessage(it.value());
        unread_[channel]=0;publish();
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
void FluxerSession::command(QString operation, QVariantMap args) {
    if(operation=="login") {login();return;}
    if(operation=="cancel-login") {
        if(poll_)poll_->stop();
        if(!code_.isEmpty())request("DELETE","/v1/auth/handoff/"+code_,{{"poll_secret",pollSecret_}},[](Reply){},true);
        ++epoch_;polling_=false;code_.clear();pollSecret_.clear();state_="signed-out";status_="Sign in to Fluxer";publish();return;
    }
    if(operation=="logout") {
        // Retain session until revocation succeeds; offline logout must not silently restore it next boot.
        if(token_.isEmpty())return;
        request("POST","/v1/auth/logout",{},[this](Reply r){
            if(r.status!=204&&r.status!=200&&r.status!=401){fail(r,"Could not sign out. Please try again");return;}
            credential(false,true);reset();state_="signed-out";status_="Signed out";publish();
        });return;
    }
    if(self_.isEmpty())return;
    if(operation=="refresh") {refresh();return;}
    if(operation=="close") {channel_.clear();messages_.clear();messageOrder_.clear();publish();return;}
    const auto id=args["id"].toString();
    if(operation=="guild"&&idValid(id)&&guilds_.contains(id)) {
        guild_=id;publish();
        request("GET","/v1/guilds/"+id,{},[this,id](Reply r){
            if(id!=guild_)return;
            if(r.status!=200){fail(r,"Could not open this community");return;}
            for(auto it=channels_.begin();it!=channels_.end();) {if(it.value()["guild_id"]==id)it=channels_.erase(it);else ++it;}
            for(const auto& v:r.body.object()["channels"].toArray()){auto c=v.toObject();if(c["type"].toInt(-1)==0&&idValid(c["id"].toString())&&channels_.size()<1000){c["guild_id"]=id;channels_[c["id"].toString()]=c;}}
            publish();
        });return;
    }
    if(operation=="open"&&idValid(id)&&channels_.contains(id)) {channel_=id;messages_.clear();messageOrder_.clear();publish();loadMessages(id);return;}
    if(operation=="dm"&&idValid(id)&&relationships_.value(id)["type"]==1) {
        request("POST","/v1/users/@me/channels",{{"recipient_id",id}},[this](Reply r){
            const auto c=r.body.object();const auto id=c["id"].toString();
            if(r.status<200||r.status>=300||!idValid(id)){fail(r,"Could not open conversation");return;}
            channels_[id]=c;channel_=id;messages_.clear();messageOrder_.clear();publish();loadMessages(id);
        });return;
    }
    if(operation=="send") {
        auto content=args["text"].toString().trimmed();
        if(content.isEmpty()||content.size()>2000||!idValid(channel_))return;
        const auto channel=channel_; const auto nonce=QUuid::createUuid().toString(QUuid::Id128);
        if(pendingNonces_.size()>=100)pendingNonces_.erase(pendingNonces_.begin());
        pendingNonces_[nonce]=channel;
        mergeMessage({{"id",nonce},{"channel_id",channel},{"content",content},{"local_delivery","Sending…"},{"author",QJsonObject{{"id",self_},{"global_name",name_}}}});publish();
        request("POST","/v1/channels/"+channel+"/messages",{{"content",content},{"nonce",nonce}},[this,channel,nonce,content](Reply r){
            if(r.status>=200&&r.status<300) {
                messages_.remove(nonce);messageOrder_.removeAll(nonce);pendingNonces_.remove(nonce);
                if(channel==channel_)mergeMessage(r.body.object());
                publish();
            } else {
                if(messages_.contains(nonce))messages_[nonce]["local_delivery"]=r.status==0?"Delivery unknown — check before resending":"Not sent";
                if(r.status!=0)pendingNonces_.remove(nonce);
                if(r.status>=400 && r.status!=401)emit sendFailed(generation_,channel,content);
                fail(r,"Message was not sent");
            }
        });return;
    }
    if(operation=="add") {
        const auto tag=args["text"].toString().trimmed();const auto split=tag.lastIndexOf('#');
        if(split<1||tag.size()-split!=5){status_="Enter username#1234";publish();return;}
        request("POST","/v1/users/@me/relationships",{{"username",tag.left(split)},{"discriminator",tag.mid(split+1)}},[this](Reply r){
            if(r.status>=200&&r.status<300){status_="Friend request sent";refresh();}else fail(r,"Could not send friend request. Check the name and tag");
        });return;
    }
    if(idValid(id)&&relationships_.contains(id)&&(operation=="accept"||operation=="remove"||operation=="block")) {
        request(operation=="remove"?"DELETE":"PUT","/v1/users/@me/relationships/"+id,
            operation=="remove"?QJsonObject():QJsonObject{{"type",operation=="block"?2:1}},[this](Reply r){
                if(r.status>=200&&r.status<300)refresh();else fail(r,"Could not update this friendship");
            });
    }
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
        else gatewaySend(2,QJsonObject{{"token",token_},{"properties",QJsonObject{{"os","Linux"},{"browser","TrainerOS"},{"device","handheld"}}}});
    } else if(op==11)awaitingAck_=false;
    else if(op==1)gatewaySend(1,sequence_);
    else if(op==7)socket_->abort();
    else if(op==9) {gatewaySession_.clear();sequence_=0;socket_->abort();request("GET","/v1/users/@me",{},[this](Reply r){if(r.status!=200)fail(r,"Session interrupted");});}
    else if(op==0) {
        sequence_=event["s"].toInteger();const auto type=event["t"].toString();
        if(type=="READY"||type=="RESUMED") {if(type=="READY")gatewaySession_=d["session_id"].toString();reconnectAttempt_=0;status_="Connected";refresh();}
        else if(type=="MESSAGE_CREATE"||type=="MESSAGE_UPDATE") {
            ++messageRevision_;
            mergeMessage(d);const auto channel=d["channel_id"].toString();
            if(type=="MESSAGE_CREATE"&&channels_.contains(channel)) {channels_[channel]["last_message_id"]=d["id"];if(channel!=channel_&&d["author"].toObject()["id"]!=self_)unread_[channel]++;}
        } else if(type=="MESSAGE_DELETE") {++messageRevision_;const auto id=d["id"].toString();messages_.remove(id);messageOrder_.removeAll(id);}
        else if(type.startsWith("RELATIONSHIP_")||type.startsWith("CHANNEL_")||type.startsWith("GUILD_")) {
            if(type=="CHANNEL_DELETE") {channels_.remove(d["id"].toString());if(d["id"]==channel_){channel_.clear();messages_.clear();messageOrder_.clear();}}
            refresh();
            if(!guild_.isEmpty())command("guild",{{"id",guild_}});
        } else return; // Presence/guild chatter outside this surface must not repaint the shell.
        publish();
    }
}
}
