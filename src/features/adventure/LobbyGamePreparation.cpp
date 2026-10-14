#include "LobbyGamePreparation.h"
#include <utility>

namespace trainer {
LobbyGamePreparation::LobbyGamePreparation(ProcessService& process,AdventureLaunchController& lifecycle,
        AdventureExitPresentation& overlay,std::function<quint64()> account,QObject* parent)
    :QObject(parent),process_(process),lifecycle_(lifecycle),overlay_(overlay),account_(std::move(account)) {
    connect(&overlay_,&AdventureExitPresentation::changed,this,&LobbyGamePreparation::show);
    connect(&lifecycle_.exitController(),&AdventureExitController::confirmationRequested,this,[this]{
        if(!exiting_)return;
        if(completion_&&processId_==process_.processId()&&accountId_==account_())lifecycle_.exitController().confirm();
        else lifecycle_.exitController().cancel();
    });
    connect(&lifecycle_.exitController(),&AdventureExitController::returnToGameRequested,this,[this]{complete(false,"Keep playing. Prepare again after saving.");});
    connect(&lifecycle_.exitController(),&AdventureExitController::failed,this,[this](QString error){complete(false,error);});
    connect(&lifecycle_,&AdventureLaunchController::adventureFinished,this,[this](bool failed){
        if(!completion_)return;
        const auto operation=operation_;const auto account=accountId_;const auto error=lifecycle_.error();
        QTimer::singleShot(0,this,[this,operation,account,failed,error]{
            if(operation_!=operation||!completion_)return;
            complete(!failed&&account==account_()&&!lifecycle_.active(),failed?error:account!=account_()?QString("The account changed. Prepare again."):QString());
        });
    });
}
void LobbyGamePreparation::prepare(QString operation,LobbyRuntimeService::Completion done) {
    if(completion_){done(false,"Another game is already being prepared.");return;}
    if(!lifecycle_.active()){done(true,{});return;}
    if(operation.isEmpty()||lifecycle_.state()!="running"||!lifecycle_.exitController().available()) {
        done(false,"Game controls are temporarily unavailable. Save first and try again.");return;
    }
    operation_=operation;completion_=std::move(done);processId_=process_.processId();accountId_=account_();
    if(overlay_.menuOpen()&&!lifecycle_.minimized())show();else emit optionsRequested();
}
void LobbyGamePreparation::show() {
    if(!completion_||exiting_||!overlay_.menuOpen()||!overlay_.ready()||lifecycle_.minimized()||overlay_.panel()=="lobby-prepare")return;
    overlay_.setPanel("lobby-prepare","Save in your current game before preparing the lobby. Closing the game keeps the lobby open.",
        {QVariantMap{{"id","multiplayer-lobby-saved:"+operation_},{"label","Saved — prepare game"}},
         QVariantMap{{"id","multiplayer-lobby-later:"+operation_},{"label","Keep playing"}}},"multiplayer-lobby-later:"+operation_);
}
bool LobbyGamePreparation::action(const QString& id) {
    if(!id.startsWith("multiplayer-lobby-"))return false;
    if(id.section(':',1)!=operation_||!completion_)return true;
    if(id.startsWith("multiplayer-lobby-later:")){complete(false,"Keep playing. Prepare again after saving.");overlay_.dismissMenu();}
    else if(id.startsWith("multiplayer-lobby-saved:")&&overlay_.ready()&&processId_==process_.processId()&&accountId_==account_()) {
        exiting_=true;if(!overlay_.exitFromMenu())complete(false,"Your game couldn't close safely. Try again.");
    }
    return true;
}
void LobbyGamePreparation::complete(bool ok,QString error) {
    if(!completion_)return;
    auto done=std::exchange(completion_,{});operation_.clear();exiting_=false;done(ok,error);
}
void LobbyGamePreparation::cancel(const QString& operation) {
    if(operation!=operation_)return;
    const bool wasExiting=exiting_;completion_={};operation_.clear();exiting_=false;
    if(wasExiting)lifecycle_.exitController().cancel();
    if(overlay_.panel()=="lobby-prepare")overlay_.dismissMenu();
}
}
