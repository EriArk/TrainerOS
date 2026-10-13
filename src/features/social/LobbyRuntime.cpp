#include "LobbyRuntime.h"
#include <QJsonArray>
#include <QUuid>

namespace trainer {
LobbyRuntime::LobbyRuntime(LobbyCoordinator& lobby,QObject* parent):QObject(parent),lobby_(lobby) {
    connect(&lobby_,&LobbyCoordinator::changed,this,&LobbyRuntime::reconcile);
    connect(&lobby_,&LobbyCoordinator::launchRequested,this,&LobbyRuntime::launch);
    deadline_.setSingleShot(true);deadline_.setInterval(180000);
    connect(&deadline_,&QTimer::timeout,this,[this]{
        const auto ticket=ticket_;cancel();
        if(!ticket.isEmpty())lobby_.acknowledgeLaunch(ticket,false);
        else lobby_.prepared(quint64(lobby_.state()["generation"].toInteger()),false);
        status_="Preparation timed out. Try again.";emit changed();
    });
}
void LobbyRuntime::configure(QString self,bool online) {
    if(self_==self&&online_==online)return;
    cancel();self_=self;online_=online;emit changed();
}
void LobbyRuntime::registerService(QString mode,LobbyRuntimeService* service) {
    if(services_.value(mode)==service)return;
    if(activeService_&&activeService_==services_.value(mode)) {
        if(lobby_.state()["phase"]=="playing")return; // Replace after the active game's safe return.
        const auto ticket=ticket_;cancel();
        if(!ticket.isEmpty())lobby_.acknowledgeLaunch(ticket,false);
        else lobby_.prepared(quint64(lobby_.state()["generation"].toInteger()),false);
    }
    services_[mode]=service;
    if(service)connect(service,&QObject::destroyed,this,[this]{reconcile();});
    emit changed();
}
QJsonObject LobbyRuntime::context() const {
    const auto s=lobby_.state();QJsonArray members;
    for(const auto& v:s["members"].toArray()) {
        const auto m=v.toObject();members.append(QJsonObject{{"id",m["id"]},{"seat",m["seat"]},
            {"spectator",m["spectator"]},{"connected",m["connected"]}});
    }
    return {{"lobby",s["id"]},{"owner",s["owner"]},{"generation",s["generation"]},
        {"game",s["game"]},{"members",members},{"self",self_}};
}
bool LobbyRuntime::available() const {
    const auto s=lobby_.state();if(!online_||s["phase"]!="gathering"||!services_.value(s["game"].toObject()["mode"].toString()))return false;
    for(const auto& v:s["members"].toArray()) {
        const auto m=v.toObject();if(m["id"]==self_)return m["connected"].toBool()&&m["seat"].toInt()>0&&!m["spectator"].toBool();
    }
    return false;
}
void LobbyRuntime::cancel() {
    deadline_.stop();
    const auto operation=operation_;const auto service=activeService_;
    operation_.clear();ticket_.clear();preparedContext_={};activeService_.clear();busy_=prepared_=false;status_.clear();
    if(service&&!operation.isEmpty())service->cancel(operation);
}
void LobbyRuntime::reconcile() {
    const auto s=lobby_.state();
    if(!operation_.isEmpty()&&(context()!=preparedContext_||!activeService_||(!ticket_.isEmpty()&&s["ticket"]!=ticket_))) {
        cancel();
        if(lobby_.state()["phase"]=="gathering")lobby_.prepared(quint64(lobby_.state()["generation"].toInteger()),false);
    }
    if(!ticket_.isEmpty()&&s["phase"]=="playing"&&s["ticket"]==ticket_) {deadline_.stop();status_="Playing";}
    emit changed();
}
bool LobbyRuntime::prepare() {
    if(!available()||busy_||prepared_)return false;
    cancel();preparedContext_=context();activeService_=services_.value(preparedContext_["game"].toObject()["mode"].toString());
    operation_=QUuid::createUuid().toString(QUuid::WithoutBraces);const auto operation=operation_;
    busy_=true;status_="Preparing game…";deadline_.start();emit changed();
    const QPointer<LobbyRuntime> guard(this);
    activeService_->prepare(operation,preparedContext_,[guard,operation](bool ok,QString error){
        if(!guard||guard->operation_!=operation||!guard->busy_||guard->prepared_||guard->context()!=guard->preparedContext_)return;
        guard->deadline_.stop();
        guard->busy_=false;guard->prepared_=ok;guard->status_=ok?QString("Ready when you are"):error.left(180);
        guard->lobby_.prepared(quint64(guard->preparedContext_["generation"].toInteger()),ok);
        if(!ok) {guard->cancel();guard->status_=error.isEmpty()?QString("Couldn't prepare the game. Try again."):error.left(180);}
        emit guard->changed();
    });return true;
}
void LobbyRuntime::launch(QString ticket,QJsonObject party) {
    if(ticket==ticket_)return;
    if(!prepared_||busy_||!activeService_||preparedContext_!=context()) {lobby_.acknowledgeLaunch(ticket,false);return;}
    ticket_=ticket;busy_=true;status_="Opening game…";deadline_.start();emit changed();
    const auto operation=operation_;const QPointer<LobbyRuntime> guard(this);
    activeService_->launch(operation,ticket,party,[guard,operation,ticket](bool ok,QString error){
        if(!guard||guard->operation_!=operation||!guard->busy_||guard->ticket_!=ticket||guard->context()!=guard->preparedContext_)return;
        guard->busy_=false;guard->status_=ok?QString("Waiting for players…"):error.left(180);
        guard->lobby_.acknowledgeLaunch(ticket,ok);
        if(!ok) {guard->cancel();guard->status_=error.isEmpty()?QString("Couldn't open the game. Prepare again."):error.left(180);}
        emit guard->changed();
    });
}
bool LobbyRuntime::finished(QString ticket) {
    if(ticket.isEmpty()||ticket!=ticket_)return false;
    const bool launching=lobby_.state()["phase"]=="launching";
    cancel();const bool result=launching?lobby_.acknowledgeLaunch(ticket,false):lobby_.finishGame(ticket);emit changed();return result;
}
}
