#include "LobbyImportController.h"
#include "RuntimeMultiplayer.h"
#include "features/social/SocialController.h"
#include <QFileInfo>
#include <QDir>
#include <QUuid>
namespace trainer {
LobbyImportController::LobbyImportController(LibraryRepository& library,AdventureAdapter& adapter,FileCatalog* catalog,SocialController& social,RuntimeMultiplayer& runtime)
 :library_(library),adapter_(adapter),social_(social),runtime_(runtime),files_(catalog,this) {
    connect(&files_,&FilePickerController::selected,this,&LobbyImportController::select);
    connect(&files_,&FilePickerController::changed,this,[this]{
        // File selection closes the picker before emitting selected. Defer the
        // cancellation check so that validation can take ownership first.
        QTimer::singleShot(0,this,[this]{if(open_&&!busy_&&error_.isEmpty()&&!files_.isOpen())cancel();});emit changed();
    });
    connect(&social_,&SocialController::lobbyImportRequested,this,&LobbyImportController::begin);
    connect(&social_,&SocialController::changed,this,[this]{if(open_&&!saving_&&context()!=context_)cancel();});
}
QJsonObject LobbyImportController::context() const {
    const auto s=social_.lobbyCoordinator()->state();QJsonArray members;
    for(const auto& v:s["members"].toArray()){const auto m=v.toObject();members.append(QJsonObject{{"id",m["id"]},{"seat",m["seat"]},{"connected",m["connected"]},{"spectator",m["spectator"]}});}
    return {{"account",qint64(social_.accountGeneration())},{"room",s["id"]},{"owner",s["owner"]},
        {"generation",s["generation"]},{"game",s["game"]},{"vote",s["vote"].toObject()["id"]},{"members",members},{"phase",s["phase"]}};
}
void LobbyImportController::begin() {
    if(open_||!library_.editable()||social_.lobbyRuntime()->problem()!="local-content"||!social_.lobbyRuntime()->available())return;
    context_=context();open_=true;error_.clear();files_.begin();emit changed();
}
void LobbyImportController::cancel() {
    if(saving_)return; // The accepted library write must settle before dismissing.
    ++generation_;if(cancelled_)*cancelled_=true;open_=busy_=false;files_.cancel();context_={};error_.clear();emit changed();
}
void LobbyImportController::chooseAgain() {if(open_&&!busy_){error_.clear();files_.begin();emit changed();}}
void LobbyImportController::dispatch(Action action) {
    if(!open_)return;
    if(busy_){if(action==Action::Back)cancel();return;}
    if(files_.isOpen()){files_.dispatch(action);return;}
    if(action==Action::Back)cancel();else if(action==Action::Confirm)chooseAgain();
}
void LobbyImportController::select(QString path) {
    if(!open_||busy_||context()!=context_)return;
    AdventureRegistration candidate;const auto game=context_["game"].toObject();
    candidate.adventure.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    candidate.adventure.title=game["title"].toString();candidate.adventure.platformId=game["platform"].toString();
    candidate.adventure.domain="multiverse";candidate.adventure.adapterId="unconfigured";
    candidate.contentPath=QDir::cleanPath(path);
    for(const auto& r:library_.registrations())if(!r.removed&&QDir::cleanPath(r.contentPath)==candidate.contentPath){candidate=r;break;}
    adapter_.prepareInstallation(candidate);
    busy_=true;error_.clear();const auto token=++generation_;cancelled_=std::make_shared<std::atomic_bool>(false);emit changed();
    QPointer<LobbyImportController> guard(this);
    runtime_.verifyImport(candidate,game,cancelled_,[guard,token,candidate](QJsonObject,QString error){
        if(!guard||!guard->open_||guard->generation_!=token||guard->context()!=guard->context_)return;
        if(!error.isEmpty()){guard->busy_=false;guard->error_=error;emit guard->changed();return;}
        guard->saving_=true;emit guard->changed();
        guard->library_.saveAdventureAsync(candidate,guard,[guard,token](LibraryWriteResult result){
            if(!guard||token!=guard->generation_)return;
            guard->saving_=guard->busy_=false;
            if(!result.success){guard->error_=result.error;emit guard->changed();return;}
            guard->cancel();emit guard->imported();
        });
    });
}
}
