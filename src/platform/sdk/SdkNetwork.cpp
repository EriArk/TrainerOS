#include "SdkNetwork.h"
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>
#include <utility>

namespace trainer {
namespace {
constexpr auto Mode="sdk.records.v1";
QJsonObject binding(const QJsonObject& state) {
    QJsonArray members;
    for(const auto& v:state["members"].toArray()) {
        const auto m=v.toObject();if(m["spectator"].toBool())continue;
        members.append(QJsonObject{{"id",m["id"]},{"seat",m["seat"]},{"connected",m["connected"]}});
    }
    return {{"lobby",state["id"]},{"generation",state["generation"]},{"owner",state["owner"]},
            {"game",state["game"]},{"ticket",state["ticket"]},{"members",members}};
}
}
SdkNetwork::SdkNetwork(LobbyCoordinator& lobby,LobbyRuntime& runtime,QString python,QString helper,
                       QString nearbyAddress,QObject* parent)
    :LobbyRuntimeService(parent),lobby_(lobby),runtime_(runtime),python_(std::move(python)),
      helper_(std::move(helper)),nearbyAddress_(std::move(nearbyAddress)) {
    runtime_.registerService(Mode,this);
    connect(&lobby_,&LobbyCoordinator::changed,this,&SdkNetwork::reconcile);
    connect(&lobby_,&LobbyCoordinator::runtimeSignal,this,&SdkNetwork::signal);
    connect(&process_,&QProcess::readyReadStandardOutput,this,&SdkNetwork::read);
    connect(&process_,&QProcess::readyReadStandardError,this,[this]{process_.readAllStandardError();});
    connect(&process_,&QProcess::errorOccurred,this,[this](auto error){
        if(!stopping_&&error==QProcess::FailedToStart)end("transport_unavailable");
    });
    connect(&process_,&QProcess::finished,this,[this](int,QProcess::ExitStatus){if(!stopping_)end("connection_lost");});
    deadline_.setSingleShot(true);deadline_.setInterval(60000);
    connect(&deadline_,&QTimer::timeout,this,[this]{end("connection_timeout");});
}
SdkNetwork::~SdkNetwork(){stop();}
bool SdkNetwork::available() const {return QFileInfo(python_).isExecutable()&&QFileInfo(helper_).isFile();}
QVariantList SdkNetwork::activities() const {
    QVariantList rows;
    if(!available())return rows;
    for(auto i=apps_.begin();i!=apps_.end();++i) {
        const auto descriptor=i.value();const auto id="sdk:"+i.key();
        rows.append(QVariantMap{{"id",id},{"key",id},{"local",id},{"title",descriptor["title"].toString()},
            {"platform","Apps"},{"mode",Mode},{"modeLabel","App · reliable messages"},{"seats",2},
            {"runtime",descriptor["runtime"].toObject().toVariantMap()}});
    }
    return rows;
}
QJsonObject SdkNetwork::channelState() const {
    if(!announced_)return {};
    return {{"channel",channel_},{"self",self_},{"peer",peer_},{"host",host_},
        {"lobby",binding_["lobby"]},{"protocol",prepared_["game"].toObject()["runtime"].toObject()["protocol"]},
        {"delivery","reliable-ordered"},{"maxBytes",16384},{"window",64}};
}
QString SdkNetwork::request(const QString& app,const QString& method,const QJsonObject& args,QJsonObject& result) {
    if(!available())return "transport_unavailable";
    if(method=="activity.register") {
        const auto title=args["title"].toString().trimmed(),protocol=args["protocol"].toString();
        static const QRegularExpression version("^[a-zA-Z0-9_.-]{1,64}$");
        if(title.isEmpty()||title.size()>64||!version.match(protocol).hasMatch())return "invalid_activity";
        const QJsonObject descriptor{{"title",title},{"runtime",QJsonObject{{"app",app},{"protocol",protocol}}}};
        if(app==app_&&!operation_.isEmpty()&&apps_.value(app)!=descriptor)return "activity_busy";
        apps_[app]=descriptor;emit activitiesChanged();result={{"id","sdk:"+app},{"mode",Mode},{"seats",2}};return {};
    }
    if(method=="activity.unregister"){disconnected(app);return {};}
    if(method=="activity.open") {if(!apps_.contains(app))return "activity_missing";emit openLobbyRequested();return {};}
    if(method=="activity.get") {result={{"registered",apps_.contains(app)},{"session",app==app_?channelState():QJsonObject{}}};return {};}
    if(method!="network.send"&&method!="network.consume"&&method!="network.close")return "unsupported_method";
    if(app!=app_||!announced_||args["channel"]!=channel_||!matches())return "channel_unavailable";
    if(method=="network.close"){end("app_closed");return {};}
    if(method=="network.send") {
        const auto encoded=args["data"].toString().toLatin1();
        const auto decoded=QByteArray::fromBase64Encoding(encoded,QByteArray::AbortOnBase64DecodingErrors);
        if(!decoded||decoded.decoded.isEmpty()||decoded.decoded.size()>16384)return "invalid_payload";
        if(sent_-acked_>=64)return "backpressure";
        const auto sequence=sent_++;
        if(!write({{"send",QString::fromLatin1(decoded.decoded.toBase64())}}))return "channel_unavailable";
        result={{"sequence",QString::number(sequence)}};return {};
    }
    if(args["sequence"].toString()!=QString::number(consumed_)||consumed_>=received_)return "invalid_sequence";
    const auto sequence=consumed_++;
    return write({{"consume",qint64(sequence)}})?QString():QString("channel_unavailable");
}
void SdkNetwork::disconnected(const QString& app) {
    const bool removed=apps_.remove(app);
    if(app==app_)end("app_disconnected");
    if(removed)emit activitiesChanged();
}
void SdkNetwork::prepare(QString operation,QJsonObject context,Completion done) {
    const auto game=context["game"].toObject(),descriptor=game["runtime"].toObject();
    const auto app=descriptor["app"].toString();
    if(!available()||!apps_.contains(app)||apps_[app]["runtime"]!=descriptor||game["mode"]!=Mode
        ||game["id"]!="sdk:"+app||game["seats"]!=2) {done(false,"Open the matching app and protocol version on this device.");return;}
    int players=0;bool mine=false;QSet<int> seats;
    for(const auto& v:context["members"].toArray()) {const auto m=v.toObject();if(m["spectator"].toBool())continue;
        ++players;seats.insert(m["seat"].toInt());mine|=m["id"]==context["self"];
        if(!m["connected"].toBool()){done(false,"Both players must be connected.");return;}}
    if(players!=2||seats!=QSet<int>{1,2}||!mine){done(false,"This app route needs two players in separate seats.");return;}
    operation_=operation;prepared_=context;app_=app;self_=context["self"].toString();done(true,{});
}
void SdkNetwork::launch(QString operation,QString ticket,QJsonObject state,Completion done) {
    if(operation!=operation_||ticket.isEmpty()||state["phase"]!="launching"||state["ticket"]!=ticket
       ||state["id"]!=prepared_["lobby"]||state["generation"]!=prepared_["generation"]||state["game"]!=prepared_["game"]
       ||!apps_.contains(app_)){done(false,"The app session changed. Prepare again.");return;}
    ticket_=ticket;binding_=binding(state);completion_=std::move(done);host_=state["owner"]==self_;stopping_=false;
    for(const auto& v:binding_["members"].toArray())if(v.toObject()["id"]!=self_)peer_=v.toObject()["id"].toString();
    deadline_.start();
    if(!host_) {
        connection_=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QTimer::singleShot(0,this,[this]{if(!ticket_.isEmpty()&&!lobby_.sendRuntimeSignal(peer_,ticket_,connection_,{{"kind","sdk-connect"}}))end("signaling_unavailable");});
    }
}
bool SdkNetwork::matches() const {
    const auto state=lobby_.state();
    return !ticket_.isEmpty()&&(state["phase"]=="launching"||state["phase"]=="playing")
        &&!lobby_.recoveryUncertain()&&!state["recovering"].toBool()&&state["connection"].toString().isEmpty()
        &&binding(state)==binding_;
}
void SdkNetwork::reconcile() {
    if(ticket_.isEmpty())return;
    if(!matches()){end("membership_changed");return;}
    if(connected_&&!announced_&&lobby_.state()["phase"]=="playing") {
        announced_=true;const auto channel=channel_;const auto pending=std::exchange(pendingData_,{});
        emit event(app_,{{"event","network.ready"},{"session",channelState()}});
        for(const auto& packet:pending)if(announced_&&channel_==channel)emit event(app_,packet);
    }
}
void SdkNetwork::signal(QString peer,QString ticket,QString connection,QJsonObject payload) {
    if(!matches()||peer!=peer_||ticket!=ticket_)return;
    if(host_&&payload==QJsonObject{{"kind","sdk-connect"}}&&connection_.isEmpty()) {
        connection_=connection;
        auto identity=binding_;identity["connection"]=connection_;identity["protocol"]="traineros-sdk-records-v1";
        identity_=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
        QJsonObject config{{"host",true},{"identity",identity_}};
        if(!nearbyAddress_.isEmpty()){config["mode"]="nearby";config["address"]=nearbyAddress_;}
        startHelper(config);return;
    }
    if(!host_&&connection==connection_&&payload["kind"]=="sdk-endpoint"&&process_.state()==QProcess::NotRunning&&identity_.isEmpty()) {
        auto identity=binding_;identity["connection"]=connection_;identity["protocol"]="traineros-sdk-records-v1";
        identity_=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
        QJsonObject config{{"host",false},{"identity",identity_}};
        const auto endpoint=payload["endpoint"].toObject();
        if(endpoint["mode"]=="nearby") {
            // Only host-configured local test/LAN routes can use this mode.
            // The peer cannot cause an arbitrary IP connection on an online host.
            if(nearbyAddress_.isEmpty()){end("route_unavailable");return;}
            config=endpoint;config["address"]=nearbyAddress_;config["host"]=false;config["identity"]=identity_;
        }else {if(!nearbyAddress_.isEmpty()){end("route_unavailable");return;}config["code"]=endpoint["code"];}
        startHelper(config);
    }
}
void SdkNetwork::startHelper(const QJsonObject& config) {
    if(process_.state()!=QProcess::NotRunning){end("transport_busy");return;}
    process_.start(python_,{helper_});
    // QProcess buffers this bounded initial configuration until the child starts.
    if(process_.write(QJsonDocument(config).toJson(QJsonDocument::Compact)+'\n')<0)end("transport_unavailable");
}
bool SdkNetwork::write(const QJsonObject& packet) {
    if(process_.state()==QProcess::NotRunning||process_.bytesToWrite()>262144){end("transport_stalled");return false;}
    const auto bytes=QJsonDocument(packet).toJson(QJsonDocument::Compact)+'\n';
    if(process_.write(bytes)!=bytes.size()){end("transport_stalled");return false;}return true;
}
void SdkNetwork::read() {
    buffer_+=process_.readAllStandardOutput();
    if(buffer_.size()>262144){end("transport_stalled");return;}
    while(buffer_.contains('\n')&&!ticket_.isEmpty()) {
        const auto at=buffer_.indexOf('\n');const auto packet=QJsonDocument::fromJson(buffer_.left(at)).object();buffer_.remove(0,at+1);
        if(!matches()){end("membership_changed");return;}
        const auto type=packet["event"].toString();
        if(type=="ready"&&host_) {
            const auto endpoint=nearbyAddress_.isEmpty()?QJsonObject{{"code",packet["code"]}}:packet["endpoint"].toObject();
            if(!lobby_.sendRuntimeSignal(peer_,ticket_,connection_,{{"kind","sdk-endpoint"},{"endpoint",endpoint}})){end("signaling_unavailable");return;}
        }else if(type=="connected"&&!connected_) {
            connected_=true;channel_=QUuid::createUuid().toString(QUuid::WithoutBraces);deadline_.stop();
            auto done=std::exchange(completion_,{});if(done)done(true,{});reconcile();
        }else if(type=="data"&&connected_&&packet["sequence"].toInteger(-1)==qint64(received_)&&received_-consumed_<64) {
            ++received_;const QJsonObject data{{"event","network.data"},{"channel",channel_},{"peer",peer_},
                {"sequence",QString::number(received_-1)},{"data",packet["data"]}};
            // Direct data may beat the provider's final playing snapshot. Keep
            // bounded credit held until this client receives its ready grant.
            if(announced_)emit event(app_,data);else pendingData_.append(data);
        }else if(type=="ack"&&packet["sequence"].toInteger(-1)==qint64(acked_)&&acked_<sent_) {
            ++acked_;emit event(app_,{{"event","network.ack"},{"channel",channel_},{"sequence",QString::number(acked_-1)}});
        }else if(type!="sent"){end("connection_lost");return;}
    }
}
void SdkNetwork::end(QString reason) {
    if(operation_.isEmpty())return;
    const auto app=app_,channel=channel_,ticket=ticket_;auto done=std::exchange(completion_,{});
    stop();emit event(app,{{"event","network.closed"},{"channel",channel},{"reason",reason}});
    if(done)done(false,"The app connection ended: "+reason);
    if(!ticket.isEmpty())runtime_.finished(ticket);
    else runtime_.cancelPending();
}
void SdkNetwork::cancel(QString operation){if(operation==operation_){const auto app=app_,channel=channel_;stop();emit event(app,{{"event","network.closed"},{"channel",channel},{"reason","session_cancelled"}});}}
void SdkNetwork::stop() {
    stopping_=true;deadline_.stop();operation_.clear();ticket_.clear();completion_={};
    if(process_.state()!=QProcess::NotRunning){process_.kill();process_.waitForFinished(2000);}
    app_.clear();self_.clear();peer_.clear();connection_.clear();identity_.clear();channel_.clear();
    prepared_={};binding_={};endpoint_={};buffer_.clear();pendingData_.clear();connected_=announced_=false;sent_=acked_=received_=consumed_=0;
}
}
