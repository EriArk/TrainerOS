#include "OnlineLink.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QUuid>
#include <QCryptographicHash>
namespace trainer {
namespace {
QString uuid(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
qint64 now(){return QDateTime::currentSecsSinceEpoch();}
bool validUuid(const QString& s){return s.size()==36&&!QUuid(s).isNull();}
}
OnlineLink::OnlineLink(QObject* parent):QObject(parent) {
    timer_.setParent(this);timer_.setInterval(1000);
    connect(&timer_,&QTimer::timeout,this,&OnlineLink::tick);
}
void OnlineLink::bind(QString account,QString endpoint) {
    close();account_=std::move(account);endpoint_=std::move(endpoint);boot_=uuid();
    lastProbe_=0;seenProbes_.clear();status_.clear();if(account_.isEmpty())timer_.stop();else timer_.start();
}
QString OnlineLink::identity(const QString& account,const QString& endpoint) {
    auto b=QCryptographicHash::hash(("https://api.fluxer.app/"+account+"/"+endpoint).toUtf8(),QCryptographicHash::Sha256).left(16);
    b[6]=char((quint8(b[6])&15)|0x50);b[8]=char((quint8(b[8])&63)|0x80);
    return QUuid::fromRfc4122(b).toString(QUuid::WithoutBraces);
}
QString OnlineLink::encode(const QJsonObject& e) {
    const auto kind=e["kind"].toString();
    const QString label=kind=="probe"?"Checking TrainerOS availability."
        :kind=="capabilities"?"TrainerOS availability reply."
        :kind=="invite"?"TrainerOS invitation: open TrainerOS to accept or decline."
        :kind=="accept"||kind=="ready"?"TrainerOS invitation accepted."
        :kind=="close"?"TrainerOS session ended.":"TrainerOS exchange update.";
    return label+"\n```json\n"+QString::fromUtf8(QJsonDocument(e).toJson(QJsonDocument::Compact))+"\n```";
}
QJsonObject OnlineLink::decode(const QString& text) {
    if(text.size()>2000||!text.endsWith("\n```"))return {};
    const auto at=text.indexOf("\n```json\n");if(at<0)return {};
    const auto e=QJsonDocument::fromJson(text.mid(at+9,text.size()-at-13).toUtf8()).object();
    return e["ns"]=="org.traineros.link"&&e["v"].toInt()==1?e:QJsonObject{};
}
bool OnlineLink::supports(const QJsonArray& list,const QJsonObject& descriptor) const {
    return !descriptor.isEmpty()&&list.contains(descriptor);
}
QJsonObject OnlineLink::activity(QString id) const {
    for(const auto& c:capabilities_)if(c.toObject()["id"]==id&&supports(remoteCapabilities_,c.toObject()))return c.toObject();
    return {};
}
void OnlineLink::setCapabilities(QJsonArray capabilities) {if(capabilities_==capabilities)return;capabilities_=std::move(capabilities);emit changed();}
void OnlineLink::setAvailable(bool available){
    available_=available;
    if(!available&&(stage_=="incoming"||stage_=="inviting"||stage_=="accepting"))answer(false);
}
void OnlineLink::emitPacket(QString kind,QJsonObject e) {
    e["ns"]="org.traineros.link";e["v"]=1;e["kind"]=kind;e["to"]=peer_;
    e["endpoint"]=endpoint_;e["boot"]=boot_;e["target"]=peerBoot_;
    e["request"]=request_;e["session"]=session_;e["expires"]=now()+90;
    const auto content=encode(e);
    if(content.size()>2000){close("This activity exceeds the online message limit.");return;}
    emit outgoing(channel_,content);
}
void OnlineLink::probe(QString channel,QString peer) {
    if(account_.isEmpty()||peer==account_||!validUuid(endpoint_)||stage_=="connected"||stage_=="incoming"||stage_=="inviting"||stage_=="accepting")return;
    if(now()-lastProbe_<10)return;
    lastProbe_=now();close();channel_=channel;peer_=peer;request_=uuid();initiator_=true;
    stage_="checking";status_="Checking your friend's TrainerOS...";deadline_=now()+30;
    emitPacket("probe");emit changed();
}
void OnlineLink::invite(QString id) {
    if(stage_!="available"||!available_||deadline_<now())return;
    selected_=activity(id);if(selected_.isEmpty())return;
    session_=uuid();stage_="inviting";initiator_=true;deadline_=now()+60;
    status_="Waiting for "+peerName_+"...";emitPacket("invite",{{"activity",selected_}});emit changed();
}
void OnlineLink::answer(bool accept) {
    if(!accept){if(stage_!="idle")emitPacket("close");close("Invitation closed");return;}
    if(stage_!="incoming"||!available_||deadline_<now()||!supports(capabilities_,selected_))return;
    stage_="accepting";deadline_=now()+30;status_="Connecting...";emitPacket("accept",{{"activity",selected_}});emit changed();
}
void OnlineLink::activate() {
    stage_="connected";status_="Connected with "+peerName_;deadline_=now()+600;sent_=received_=0;parts_.clear();
    emit established(identity(account_,endpoint_),identity(peer_,peerEndpoint_),peerName_,selected_["id"].toString(),initiator_);emit changed();
}
void OnlineLink::close(QString reason) {
    const bool active=stage_=="connected";
    stage_="idle";status_=std::move(reason);request_.clear();session_.clear();peer_.clear();peerBoot_.clear();peerEndpoint_.clear();channel_.clear();
    selected_={};remoteCapabilities_={};parts_.clear();deadline_=0;
    if(active)emit ended();emit changed();
}
void OnlineLink::tick() {
    if(deadline_&&deadline_<now())close(stage_=="checking"?"No active TrainerOS reply. Your chat is still available.":stage_=="connected"?"Online activity timed out. Reconnect to recover.":"Invitation expired");
}
QVariantMap OnlineLink::state() const {
    QVariantList actions;
    if(stage_=="available"&&available_)for(const auto& c:capabilities_)if(supports(remoteCapabilities_,c.toObject()))actions.append(c.toObject().toVariantMap());
    return {{"stage",stage_},{"status",status_},{"actions",actions},{"peer",peer_},{"channel",channel_},{"name",peerName_},
        {"session",session_},{"activity",selected_.toVariantMap()},{"incoming",stage_=="incoming"},{"open",stage_=="incoming"||stage_=="inviting"||stage_=="accepting"}};
}
void OnlineLink::receive(QString channel,QString author,QString name,const QJsonObject& e) {
    if(account_.isEmpty()||author==account_||e["ns"]!="org.traineros.link"||e["v"].toInt()!=1||e["to"]!=account_)return;
    const auto expiry=e["expires"].toInteger();if(expiry<now()||expiry>now()+120)return;
    const auto endpoint=e["endpoint"].toString(),boot=e["boot"].toString(),request=e["request"].toString(),kind=e["kind"].toString();
    if(!validUuid(endpoint)||!validUuid(boot)||!validUuid(request))return;
    if(kind=="probe") {
        if(!available_||(now()-lastProbe_<10&&stage_!="checking")||seenProbes_.contains(author+request)||stage_=="connected"||stage_=="incoming"||stage_=="inviting"||stage_=="accepting")return;
        // A deterministic winner avoids two simultaneously initiated checks.
        if(stage_=="checking"&&account_<author)return;
        seenProbes_.append(author+request);while(seenProbes_.size()>128)seenProbes_.removeFirst();
        lastProbe_=now();close();channel_=channel;peer_=author;peerName_=name.left(100);peerEndpoint_=endpoint;peerBoot_=boot;request_=request;
        initiator_=false;stage_="offered";deadline_=now()+90;
        emitPacket("capabilities",{{"activities",capabilities_}});emit changed();return;
    }
    if(author!=peer_||channel!=channel_||request!=request_||e["target"]!=boot_)return;
    if(kind=="capabilities"&&stage_=="checking") {
        const auto caps=e["activities"].toArray();if(caps.size()>12)return;
        peerName_=name.left(100);peerEndpoint_=endpoint;peerBoot_=boot;remoteCapabilities_=caps;
        stage_="available";status_="TrainerOS is available";deadline_=now()+90;emit changed();return;
    }
    if(boot!=peerBoot_||endpoint!=peerEndpoint_)return;
    if(kind=="invite"&&stage_=="offered"&&available_) {
        const auto a=e["activity"].toObject();if(!supports(capabilities_,a)||!validUuid(e["session"].toString()))return;
        session_=e["session"].toString();selected_=a;stage_="incoming";deadline_=now()+60;
        status_=peerName_+" invites you: "+a["label"].toString();emit changed();return;
    }
    if(e["session"]!=session_||session_.isEmpty())return;
    if(kind=="close"){close("Your friend ended the invitation or session");return;}
    if(kind=="accept"&&stage_=="inviting"&&available_&&e["activity"]==selected_&&supports(capabilities_,selected_)) {
        emitPacket("ready",{{"activity",selected_}});activate();return;
    }
    if(kind=="ready"&&stage_=="accepting"&&available_&&e["activity"]==selected_&&supports(capabilities_,selected_)){activate();return;}
    if(kind!="frame"||stage_!="connected")return;
    const int seq=e["seq"].toInt(),part=e["part"].toInt(-1),count=e["count"].toInt();const auto data=e["data"].toString();
    if(seq<=received_)return;
    if(seq>received_+8||count<1||count>16||part<0||part>=count||data.size()>1000){close("Online activity data was invalid");return;}
    auto& parts=parts_[seq];if(parts.total&&parts.total!=count){close("Online activity data changed");return;}
    if(parts.data.contains(part)&&parts.data[part]!=data){close("Online activity data changed");return;}
    parts.total=count;parts.data[part]=data;
    while(parts_.contains(received_+1)&&parts_[received_+1].data.size()==parts_[received_+1].total) {
        const auto complete=parts_.take(++received_);QByteArray encoded;
        for(int i=0;i<complete.total;++i)encoded+=complete.data[i].toLatin1();
        const auto bytes=QByteArray::fromBase64Encoding(encoded,QByteArray::AbortOnBase64DecodingErrors);
        const auto frame=QJsonDocument::fromJson(bytes.decoded).object();
        if(!bytes||frame.isEmpty()||bytes.decoded.size()>12000){close("Online activity data was invalid");return;}
        emit frameReceived(frame);
    }
    deadline_=now()+(parts_.isEmpty()?600:90);
}
void OnlineLink::sendFrame(QJsonObject frame) {
    if(stage_!="connected")return;
    const auto bytes=QJsonDocument(frame).toJson(QJsonDocument::Compact);
    if(bytes.size()>12000){close("This activity exceeds the online message limit.");return;}
    const auto encoded=bytes.toBase64();const int count=(encoded.size()+999)/1000;const auto seq=++sent_;
    for(int i=0;i<count&&stage_=="connected";++i)emitPacket("frame",{{"seq",seq},{"part",i},{"count",count},{"data",QString::fromLatin1(encoded.mid(i*1000,1000))}});
}
}
