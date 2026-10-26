#include "SdkNetwork.h"
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>
#include <utility>

namespace trainer {
namespace {
constexpr auto Mode="sdk.records.v1";
QJsonArray members(const QJsonArray& source) {
    QMap<int,QJsonObject> sorted;
    for(const auto& v:source){const auto m=v.toObject();if(!m["spectator"].toBool())sorted[m["seat"].toInt()]={{"id",m["id"]},{"seat",m["seat"]},{"connected",m["connected"]}};}
    QJsonArray out;for(const auto& m:sorted)out.append(m);return out;
}
QJsonObject binding(const QJsonObject& state) {
    return {{"lobby",state["id"]},{"generation",state["generation"]},{"owner",state["owner"]},
        {"game",state["game"]},{"ticket",state["ticket"]},{"members",members(state["members"].toArray())}};
}
bool integer(QJsonValue v,int lo,int hi){return v.isDouble()&&v.toDouble()==v.toInt(lo-1)&&v.toInt()>=lo&&v.toInt()<=hi;}
}
SdkNetwork::SdkNetwork(LobbyCoordinator& lobby,LobbyRuntime& runtime,QString python,QString helper,QString nearbyAddress,QObject* parent)
    :LobbyRuntimeService(parent),lobby_(lobby),runtime_(runtime),python_(std::move(python)),helper_(std::move(helper)),nearbyAddress_(std::move(nearbyAddress)) {
    runtime_.registerService(Mode,this);
    connect(&lobby_,&LobbyCoordinator::changed,this,&SdkNetwork::reconcile);
    connect(&lobby_,&LobbyCoordinator::runtimeSignal,this,&SdkNetwork::signal);
    deadline_.setSingleShot(true);deadline_.setInterval(60000);
    connect(&deadline_,&QTimer::timeout,this,[this]{end("connection_timeout");});
}
SdkNetwork::~SdkNetwork(){stop();}
bool SdkNetwork::available() const {return QFileInfo(python_).isExecutable()&&QFileInfo(helper_).isFile();}
void SdkNetwork::setInstalledActivities(const QVariantList& rows) {
    QMap<QString,QJsonObject> next;QMap<QString,QString> ids;
    for(const auto& v:rows){const auto r=v.toMap();const auto app=r["app"].toString();const auto seats=QJsonValue::fromVariant(r.value("seats",2));
        if(!integer(seats,2,MaxPlayers))continue;
        next[app]={{"title",r["title"].toString()},{"seats",seats},{"runtime",QJsonObject{{"app",app},{"protocol",r["protocol"].toString()}}}};ids[app]=r["local"].toString();}
    if(next==installed_&&ids==launchIds_)return;
    const bool changed=!app_.isEmpty()&&installed_.value(app_)!=next.value(app_);
    installed_=next;launchIds_=ids;if(changed)end("app_changed");emit activitiesChanged();
}
QVariantList SdkNetwork::activities() const {
    QVariantList rows;if(!available())return rows;
    for(auto i=installed_.begin();i!=installed_.end();++i){const auto d=i.value(),r=d["runtime"].toObject();const auto id="sdk:"+i.key();
        rows.append(QVariantMap{{"id",id},{"key",id},{"local",id},{"title",d["title"].toString()},{"platform","Apps"},{"mode",Mode},
            {"modeLabel","App · reliable messages"},{"seats",d["seats"].toInt(2)},{"runtime",r.toVariantMap()}});}
    return rows;
}
bool SdkNetwork::allConnected() const {if(links_.isEmpty())return false;for(const auto& link:links_)if(!link->connected)return false;return true;}
QJsonObject SdkNetwork::channelState() const {
    if(!announced_)return {};
    QJsonArray channels;for(const auto& l:links_)channels.append(QJsonObject{{"channel",l->channel},{"peer",l->peer}});
    QJsonObject out{{"self",self_},{"host",host_},{"hostMember",binding_["owner"]},{"members",binding_["members"]},{"channels",channels},
        {"lobby",binding_["lobby"]},{"protocol",prepared_["game"].toObject()["runtime"].toObject()["protocol"]},
        {"delivery","reliable-ordered"},{"maxPlayers",MaxPlayers},{"maxBytes",16384},{"window",64}};
    // Preserve the v1 two-peer shape for every existing game.
    if(links_.size()==1){out["channel"]=links_.first()->channel;out["peer"]=links_.first()->peer;}return out;
}
QString SdkNetwork::request(const QString& app,const QString& method,const QJsonObject& args,QJsonObject& result) {
    if(!available())return "transport_unavailable";
    if(method=="activity.register"){
        const auto title=args["title"].toString().trimmed(),protocol=args["protocol"].toString();static const QRegularExpression version("^[a-zA-Z0-9_.-]{1,64}$");
        if(title.isEmpty()||title.size()>64||!version.match(protocol).hasMatch())return "invalid_activity";
        const QJsonObject descriptor{{"title",title},{"runtime",QJsonObject{{"app",app},{"protocol",protocol}}}};
        if(installed_.contains(app)&&installed_[app]["runtime"]!=descriptor["runtime"])return "invalid_activity";
        if(app==app_&&!operation_.isEmpty()&&prepared_["game"].toObject()["runtime"]!=descriptor["runtime"])return "activity_busy";
        apps_[app]=descriptor;emit activitiesChanged();result={{"id","sdk:"+app},{"mode",Mode},{"seats",installed_.value(app)["seats"].toInt(2)}};
        QTimer::singleShot(0,this,&SdkNetwork::completeLaunch);return {};
    }
    if(method=="activity.unregister"){disconnected(app);return {};}
    if(method=="activity.open"){if(!apps_.contains(app))return "activity_missing";emit openLobbyRequested();return {};}
    if(method=="activity.get"){result={{"registered",apps_.contains(app)},{"session",app==app_?channelState():QJsonObject{}}};return {};}
    if(method!="network.send"&&method!="network.consume"&&method!="network.close")return "unsupported_method";
    if(app!=app_||!announced_||!matches())return "channel_unavailable";
    std::shared_ptr<Link> link;for(const auto& l:links_)if(args["channel"]==l->channel){link=l;break;}
    if(!link)return "channel_unavailable";
    if(method=="network.close"){end("app_closed");return {};}
    if(method=="network.send"){
        const auto decoded=QByteArray::fromBase64Encoding(args["data"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
        if(!decoded||decoded.decoded.isEmpty()||decoded.decoded.size()>16384)return "invalid_payload";
        if(link->sent-link->acked>=64)return "backpressure";const auto sequence=link->sent++;
        if(!write(*link,{{"send",QString::fromLatin1(decoded.decoded.toBase64())}}))return "channel_unavailable";
        result={{"sequence",QString::number(sequence)}};return {};
    }
    if(args["sequence"].toString()!=QString::number(link->consumed)||link->consumed>=link->received)return "invalid_sequence";
    return write(*link,{{"consume",qint64(link->consumed++)}})?QString():QString("channel_unavailable");
}
void SdkNetwork::disconnected(const QString& app){const bool removed=apps_.remove(app);if(app==app_)end("app_disconnected");if(removed)emit activitiesChanged();}
void SdkNetwork::prepare(QString operation,QJsonObject context,Completion done){
    const auto game=context["game"].toObject(),descriptor=game["runtime"].toObject();const auto app=descriptor["app"].toString();const auto local=installed_.value(app);int maximum=local["seats"].toInt(2);
    if(!available()||local.isEmpty()||local["runtime"]!=descriptor||game["mode"]!=Mode||game["id"]!="sdk:"+app||game["seats"]!=maximum){done(false,"Install the matching game version from Crates on this device.");return;}
    QSet<QString> ids;QSet<int> seats;
    for(const auto& v:context["members"].toArray()){const auto m=v.toObject();if(m["spectator"].toBool())continue;const auto id=m["id"].toString();
        if(id.isEmpty()||ids.contains(id)||!integer(m["seat"],1,maximum)||seats.contains(m["seat"].toInt())||m["connected"]!=true){done(false,"Players need unique connected seats.");return;}
        ids.insert(id);seats.insert(m["seat"].toInt());}
    if(ids.size()<2||ids.size()>maximum||!ids.contains(context["self"].toString())||!ids.contains(context["owner"].toString())){done(false,"This app needs 2 to "+QString::number(maximum)+" players, including its host.");return;}
    if(!ticket_.isEmpty()){done(false,"Finish the current app session first.");return;}
    operation_=operation;prepared_=context;app_=app;self_=context["self"].toString();done(true,{});
}
void SdkNetwork::addLink(const QString& peer){
    auto l=std::make_shared<Link>();l->peer=peer;links_[peer]=l;
    connect(&l->process,&QProcess::readyReadStandardOutput,this,[this,peer]{read(peer);});
    connect(&l->process,&QProcess::readyReadStandardError,this,[this,peer]{if(links_.contains(peer))links_[peer]->process.readAllStandardError();});
    connect(&l->process,&QProcess::errorOccurred,this,[this](auto e){if(!stopping_&&e==QProcess::FailedToStart)end("transport_unavailable");});
    connect(&l->process,&QProcess::finished,this,[this](int,QProcess::ExitStatus){if(!stopping_)end("connection_lost");});
}
void SdkNetwork::launch(QString operation,QString ticket,QJsonObject state,Completion done){
    if(operation!=operation_||ticket.isEmpty()||state["phase"]!="launching"||state["ticket"]!=ticket||state["id"]!=prepared_["lobby"]
       ||state["generation"]!=prepared_["generation"]||state["game"]!=prepared_["game"]||state["owner"]!=prepared_["owner"]
       ||members(state["members"].toArray())!=members(prepared_["members"].toArray())||(!apps_.contains(app_)&&!installed_.contains(app_))){done(false,"The app session changed. Prepare again.");return;}
    ticket_=ticket;binding_=binding(state);completion_=std::move(done);host_=state["owner"]==self_;stopping_=false;
    for(const auto& v:binding_["members"].toArray()){const auto id=v.toObject()["id"].toString();if(id!=self_&&(host_||id==state["owner"]))addLink(id);}
    deadline_.start();if(!apps_.contains(app_)&&(!launchInstalled||!launchInstalled(launchIds_.value(app_)))){end("launch_unavailable");return;}
    if(!host_){const auto l=links_.first();l->connection=QUuid::createUuid().toString(QUuid::WithoutBraces);const auto ticketCopy=ticket_;
        QTimer::singleShot(0,this,[this,l,ticketCopy]{if(ticket_==ticketCopy&&!lobby_.sendRuntimeSignal(l->peer,ticket_,l->connection,{{"kind","sdk-connect"}}))end("signaling_unavailable");});}
}
void SdkNetwork::completeLaunch(){if(!allConnected()||!apps_.contains(app_)||apps_[app_]["runtime"]!=prepared_["game"].toObject()["runtime"]||!matches())return;
    deadline_.stop();auto done=std::exchange(completion_,{});if(done)done(true,{});reconcile();}
bool SdkNetwork::matches() const {const auto state=lobby_.state();return !ticket_.isEmpty()&&(state["phase"]=="launching"||state["phase"]=="playing")
    &&!lobby_.recoveryUncertain()&&!state["recovering"].toBool()&&state["connection"].toString().isEmpty()&&binding(state)==binding_;}
void SdkNetwork::reconcile(){
    if(ticket_.isEmpty())return;if(!matches()){end("membership_changed");return;}
    if(allConnected()&&apps_.contains(app_)&&!announced_&&lobby_.state()["phase"]=="playing"){
        announced_=true;const auto ticket=ticket_;emit event(app_,{{"event","network.ready"},{"session",channelState()}});
        const auto links=links_;for(const auto& l:links){const auto packets=std::exchange(l->pending,{});for(const auto& p:packets)if(announced_&&ticket_==ticket)emit event(app_,p);}}
}
void SdkNetwork::signal(QString peer,QString ticket,QString connection,QJsonObject payload){
    if(!matches()||!links_.contains(peer)||ticket!=ticket_)return;const auto l=links_[peer];
    auto identify=[&]{auto identity=binding_;identity["connection"]=l->connection;identity["protocol"]="traineros-sdk-records-v1";
        // The old two-player identity remains compatible. Groups bind each edge.
        if(binding_["members"].toArray().size()>2)identity["edge"]=QJsonArray{binding_["owner"],host_?peer:self_};
        l->identity=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());};
    if(host_&&payload==QJsonObject{{"kind","sdk-connect"}}&&l->connection.isEmpty()&&!connection.isEmpty()){
        l->connection=connection;identify();QJsonObject config{{"host",true},{"identity",l->identity}};
        if(!nearbyAddress_.isEmpty()){config["mode"]="nearby";config["address"]=nearbyAddress_;}startHelper(*l,config);return;}
    if(!host_&&connection==l->connection&&payload["kind"]=="sdk-endpoint"&&l->process.state()==QProcess::NotRunning&&l->identity.isEmpty()){
        identify();QJsonObject config{{"host",false},{"identity",l->identity}};const auto endpoint=payload["endpoint"].toObject();
        if(endpoint["mode"]=="nearby"){if(nearbyAddress_.isEmpty()){end("route_unavailable");return;}config=endpoint;config["address"]=nearbyAddress_;config["host"]=false;config["identity"]=l->identity;}
        else{if(!nearbyAddress_.isEmpty()){end("route_unavailable");return;}config["code"]=endpoint["code"];}startHelper(*l,config);}
}
void SdkNetwork::startHelper(Link& l,const QJsonObject& config){if(l.process.state()!=QProcess::NotRunning){end("transport_busy");return;}
    l.process.start(python_,{helper_});if(l.process.write(QJsonDocument(config).toJson(QJsonDocument::Compact)+'\n')<0)end("transport_unavailable");}
bool SdkNetwork::write(Link& l,const QJsonObject& packet){if(l.process.state()==QProcess::NotRunning||l.process.bytesToWrite()>262144){end("transport_stalled");return false;}
    const auto bytes=QJsonDocument(packet).toJson(QJsonDocument::Compact)+'\n';if(l.process.write(bytes)!=bytes.size()){end("transport_stalled");return false;}return true;}
void SdkNetwork::read(const QString& peer){
    const auto l=links_.value(peer);if(!l)return;l->buffer+=l->process.readAllStandardOutput();if(l->buffer.size()>262144){end("transport_stalled");return;}
    while(l->buffer.contains('\n')&&!ticket_.isEmpty()){
        const auto at=l->buffer.indexOf('\n');const auto packet=QJsonDocument::fromJson(l->buffer.left(at)).object();l->buffer.remove(0,at+1);
        if(!matches()){end("membership_changed");return;}const auto type=packet["event"].toString();
        if(type=="ready"&&host_){const auto endpoint=nearbyAddress_.isEmpty()?QJsonObject{{"code",packet["code"]}}:packet["endpoint"].toObject();
            if(!lobby_.sendRuntimeSignal(peer,ticket_,l->connection,{{"kind","sdk-endpoint"},{"endpoint",endpoint}})){end("signaling_unavailable");return;}}
        else if(type=="connected"&&!l->connected){l->connected=true;l->channel=QUuid::createUuid().toString(QUuid::WithoutBraces);completeLaunch();}
        else if(type=="data"&&l->connected&&packet["sequence"].toInteger(-1)==qint64(l->received)&&l->received-l->consumed<64){
            ++l->received;const QJsonObject data{{"event","network.data"},{"channel",l->channel},{"peer",peer},{"sequence",QString::number(l->received-1)},{"data",packet["data"]}};
            if(announced_)emit event(app_,data);else l->pending.append(data);}
        else if(type=="ack"&&packet["sequence"].toInteger(-1)==qint64(l->acked)&&l->acked<l->sent){++l->acked;emit event(app_,{{"event","network.ack"},{"channel",l->channel},{"peer",peer},{"sequence",QString::number(l->acked-1)}});}
        else if(type!="sent"){end("connection_lost");return;}}
}
void SdkNetwork::end(QString reason){if(operation_.isEmpty())return;const auto app=app_,ticket=ticket_;const auto channel=links_.size()==1?links_.first()->channel:QString();auto done=std::exchange(completion_,{});
    stop();emit event(app,{{"event","network.closed"},{"channel",channel},{"reason",reason}});if(done)done(false,"The app connection ended: "+reason);if(!ticket.isEmpty())runtime_.finished(ticket);else runtime_.cancelPending();}
void SdkNetwork::cancel(QString operation){if(operation==operation_){const auto app=app_;const auto channel=links_.size()==1?links_.first()->channel:QString();stop();emit event(app,{{"event","network.closed"},{"channel",channel},{"reason","session_cancelled"}});}}
void SdkNetwork::stop(){
    stopping_=true;deadline_.stop();operation_.clear();ticket_.clear();completion_={};
    const auto links=std::exchange(links_,{});for(const auto& l:links)if(l->process.state()!=QProcess::NotRunning)l->process.kill();
    for(const auto& l:links)if(l->process.state()!=QProcess::NotRunning)l->process.waitForFinished(2000);
    // A helper may have called end() from its own error/data signal. Keep the
    // emitting QProcess alive until that event delivery has unwound.
    if(!links.isEmpty())QTimer::singleShot(0,this,[links]{});
    app_.clear();self_.clear();prepared_={};binding_={};announced_=false;
}
}
