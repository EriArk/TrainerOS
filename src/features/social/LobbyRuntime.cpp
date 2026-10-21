#include "LobbyRuntime.h"
#include <QJsonArray>
#include <QUuid>

namespace trainer {
LobbyRuntime::LobbyRuntime(LobbyCoordinator& lobby,QObject* parent):QObject(parent),lobby_(lobby),input_(lobby,this) {
    connect(&lobby_,&LobbyCoordinator::changed,this,&LobbyRuntime::reconcile);
    connect(&lobby_,&LobbyCoordinator::launchRequested,this,&LobbyRuntime::launch);
    connect(&lobby_,&LobbyCoordinator::liveJoinRequested,this,[this](QString peer,QString operation,int seat){
        const auto s=lobby_.state();
        if(playing_&&activeService_&&ticket_==s["ticket"].toString()&&liveSupported()&&s["liveJoining"].toBool()
           &&activeService_->admitLive(ticket_,peer,operation,seat)) {
            if(lobby_.admitLive(peer,operation,seat))return;
            activeService_->releaseLive(peer,operation);
        }
        lobby_.rejectLive(peer,operation);
    });
    connect(&lobby_,&LobbyCoordinator::liveReleased,this,[this](QString peer,QString operation){
        if(activeService_)activeService_->releaseLive(peer,operation);
    });
    connect(&lobby_,&LobbyCoordinator::liveLaunchRequested,this,[this](QString operation,QString ticket,QJsonObject party){
        if(!live_||operation_!=operation||!prepared_)return;
        busy_=false;preparedContext_=context();launch(ticket,party);
    });
    connect(&lobby_,&LobbyCoordinator::liveJoinFailed,this,[this](QString operation){
        if(operation_!=operation)return;
        cancel();status_="Joining is unavailable or the controller was taken. You can keep watching.";emit changed();
    });
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
    // Provider presence is not the emulator connection. Once all launches are
    // acknowledged, a messenger reconnect must not tear down that connection.
    const auto s=lobby_.state();
    if(self_!=self||(!playing_&&(ticket_.isEmpty()||s["phase"]!="playing"||s["ticket"]!=ticket_)))cancel();
    self_=self;online_=online;emit changed();
}
void LobbyRuntime::registerService(QString mode,LobbyRuntimeService* service) {
    if(services_.value(mode)==service)return;
    if(activeService_&&activeService_==services_.value(mode)) {
        if(playing_)return; // Replace after the active game's safe return, even without its room.
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
        const auto m=v.toObject();
        if(live_&&m["id"]!=self_&&m["id"]!=s["owner"])continue;
        members.append(QJsonObject{{"id",m["id"]},{"seat",m["seat"]},
            {"spectator",m["spectator"]},{"connected",m["connected"]}});
    }
    QJsonObject result{{"lobby",s["id"]},{"owner",s["owner"]},{"generation",s["generation"]},
        {"game",s["game"]},{"vote",s["vote"].toObject()["id"]},{"members",members},{"self",self_}};
    if(live_)result["liveAdmission"]=operation_;
    return result;
}
bool LobbyRuntime::liveSupported() const {
    const auto game=lobby_.state()["game"].toObject();const auto service=services_.value(game["mode"].toString());
    return service&&service->supportsLive(game["runtime"].toObject());
}
bool LobbyRuntime::setLiveJoining(bool enabled) {
    return (!enabled||liveSupported())&&lobby_.setLiveJoining(enabled);
}
bool LobbyRuntime::liveAvailable() const {
    const auto s=lobby_.state();if(playing_||busy_||!online_||lobby_.hosting()||s["phase"]!="playing"||!s["liveJoining"].toBool()
        ||s["recovering"].toBool()||!s["connection"].toString().isEmpty()||!liveSupported())return false;
    int players=0;bool spectator=false;
    for(const auto& v:s["members"].toArray()) {const auto m=v.toObject();if(m["seat"].toInt())++players;
        if(m["id"]==self_)spectator=m["spectator"].toBool()&&m["connected"].toBool();}
    return spectator&&players<s["game"].toObject()["seats"].toInt();
}
bool LobbyRuntime::joinLive() {
    if(!liveAvailable())return false;
    cancel();live_=true;operation_=QUuid::createUuid().toString(QUuid::WithoutBraces);
    preparedContext_=context();activeService_=services_.value(preparedContext_["game"].toObject()["mode"].toString());
    const auto operation=operation_;busy_=true;status_="Preparing to join…";deadline_.start();emit changed();
    const QPointer<LobbyRuntime> guard(this);
    activeService_->prepare(operation,preparedContext_,[guard,operation](bool ok,QString error){
        if(!guard||guard->operation_!=operation||!guard->live_||guard->context()!=guard->preparedContext_)return;
        guard->busy_=ok;guard->prepared_=ok;
        guard->status_="Waiting for a controller…";emit guard->changed();
        if(ok&&guard->lobby_.requestLive(operation))return;
        guard->cancel();guard->status_=error.isEmpty()?QString("Joining is no longer available. You can keep watching."):error.left(180);emit guard->changed();
    });return true;
}
bool LobbyRuntime::available() const {
    if(!lobby_.state()["vote"].toObject().isEmpty())return false;
    const auto s=lobby_.state();if(playing_||!online_||s["recovering"].toBool()||!s["connection"].toString().isEmpty()||s["phase"]!="gathering"||!services_.value(s["game"].toObject()["mode"].toString()))return false;
    for(const auto& v:s["members"].toArray()) {
        const auto m=v.toObject();if(m["id"]==self_)return m["connected"].toBool()&&m["seat"].toInt()>0&&!m["spectator"].toBool();
    }
    return false;
}
void LobbyRuntime::cancel() {
    problem_.clear();problemContext_={};
    deadline_.stop();
    const auto operation=operation_;const auto service=activeService_;
    const bool abandoned=live_&&!playing_;
    operation_.clear();ticket_.clear();preparedContext_={};activeService_.clear();busy_=prepared_=playing_=live_=false;status_.clear();
    if(service&&!operation.isEmpty())service->cancel(operation);
    if(abandoned)lobby_.cancelLive(operation);
}
void LobbyRuntime::cancelPending() {
    if(playing_)return;
    const auto ticket=ticket_;const auto generation=quint64(lobby_.state()["generation"].toInteger());
    cancel();
    if(!ticket.isEmpty())lobby_.acknowledgeLaunch(ticket,false);
    else lobby_.prepared(generation,false);
    status_="Opening cancelled. You can prepare again.";emit changed();
}
void LobbyRuntime::reconcile() {
    if(!problemContext_.isEmpty()&&problemContext_!=context()){problem_.clear();problemContext_={};status_.clear();}
    const auto s=lobby_.state();
    if(live_&&ticket_.isEmpty()&&!s["liveJoining"].toBool()) {
        cancel();status_="The host paused new joins. You can keep watching.";emit changed();return;
    }
    if(!ticket_.isEmpty()&&s["phase"]=="playing"&&s["ticket"]==ticket_)playing_=true;
    // Membership and the game process have separate lifetimes. Only the ordinary
    // Home/save return ends an acknowledged game, even if its room disappears.
    if(playing_&&activeService_) {deadline_.stop();status_=s["ticket"]==ticket_?"Playing":"Game continues · close it from Game Options";emit changed();return;}
    auto current=context(),prepared=preparedContext_;
    if(!operation_.isEmpty()&&(current!=prepared||!activeService_||(!ticket_.isEmpty()&&s["ticket"]!=ticket_))) {
        cancel();
        if(lobby_.state()["phase"]=="gathering")lobby_.prepared(quint64(lobby_.state()["generation"].toInteger()),false);
    }
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
        if(!ok) {const auto problem=guard->activeService_?guard->activeService_->preparationProblem():QString();const auto context=guard->context();guard->cancel();guard->problem_=problem;guard->problemContext_=context;guard->status_=error.isEmpty()?QString("Couldn't prepare the game. Try again."):error.left(180);}
        emit guard->changed();
    });return true;
}
void LobbyRuntime::launch(QString ticket,QJsonObject party) {
    if(ticket==ticket_)return;
    if(!prepared_||busy_||!activeService_||preparedContext_!=context()) {lobby_.acknowledgeLaunch(ticket,false);return;}
    ticket_=ticket;busy_=true;status_="Opening game…";deadline_.start();emit changed();
    const auto operation=operation_;const QPointer<LobbyRuntime> guard(this);
    activeService_->launch(operation,ticket,party,[guard,operation,ticket](bool ok,QString error){
        if(!guard||guard->operation_!=operation||!guard->busy_||guard->ticket_!=ticket||(!guard->live_&&guard->context()!=guard->preparedContext_))return;
        guard->busy_=false;guard->status_=ok?QString("Waiting for players…"):error.left(180);
        guard->lobby_.acknowledgeLaunch(ticket,ok);
        if(!ok) {guard->cancel();guard->status_=error.isEmpty()?QString("Couldn't open the game. Prepare again."):error.left(180);}
        emit guard->changed();
    });
}
bool LobbyRuntime::finished(QString ticket) {
    if(ticket.isEmpty()||ticket!=ticket_)return false;
    const bool launching=lobby_.state()["phase"]=="launching";
    const bool played=playing_;
    cancel();const bool result=launching?lobby_.acknowledgeLaunch(ticket,false):lobby_.finishGame(ticket);emit changed();return played||result;
}
}
