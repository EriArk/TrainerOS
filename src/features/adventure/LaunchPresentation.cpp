#include "LaunchPresentation.h"
#include "core/navigation/LaunchPreparation.h"
#include "core/navigation/AdventureLaunchController.h"
#include "RuntimeMultiplayer.h"
#include "AdventureExitPresentation.h"
#include "features/social/SocialController.h"
namespace trainer {
LaunchPresentation::LaunchPresentation(LibraryRepository& library,LaunchPreparation& preparation,
 AdventureLaunchController& lifecycle,RuntimeMultiplayer& multiplayer,SocialController& social,AdventureExitPresentation& exit)
 :library_(library),preparation_(preparation),lifecycle_(lifecycle),multiplayer_(multiplayer),social_(social),exit_(exit) {
    connect(&preparation_,&LaunchPreparation::changed,this,&LaunchPresentation::changed);
    connect(&lifecycle_,&AdventureLaunchController::changed,this,&LaunchPresentation::changed);
    connect(&multiplayer_,&RuntimeMultiplayer::changed,this,&LaunchPresentation::changed);
    connect(social_.lobbyRuntime(),&LobbyRuntime::changed,this,&LaunchPresentation::changed);
    connect(&exit_,&AdventureExitPresentation::changed,this,&LaunchPresentation::changed);
}
bool LaunchPresentation::visible() const {
    if(exit_.visible()||exit_.menuOpen()||exit_.capturing()||lifecycle_.state()=="running")return false;
    return preparation_.busy()||lifecycle_.opening()||social_.lobbyRuntime()->opening()||multiplayer_.opening();
}
QVariantMap LaunchPresentation::state() const {
    QString id,stage,title,art;
    if(lifecycle_.opening()){id=lifecycle_.adventureId();stage=lifecycle_.launchStage();}
    else if(preparation_.busy()){id=preparation_.adventureId();stage=preparation_.stage();}
    else if(multiplayer_.opening()){id=multiplayer_.openingGame();stage=multiplayer_.openingStage();}
    else stage=social_.lobbyRuntime()->status();
    if(const auto record=library_.registration(id)) {
        title=record->adventure.title;
        const auto media=library_.artwork(id);art=media.value("screenshot").toString();
        if(art.isEmpty())art=media.value("fanart").toString();
        if(art.isEmpty())art=media.value("cover").toString();
    }
    if(title.isEmpty())title=social_.lobbyCoordinator()->state()["game"].toObject()["title"].toString();
    return {{"visible",visible()},{"title",title.isEmpty()?QString("Your game"):title},{"art",art},
        {"stage",stage},{"cancellable",!preparation_.cancelling()&&lifecycle_.state()!="stopping"}};
}
void LaunchPresentation::cancel() {
    if(!visible())return;
    preparation_.cancel();lifecycle_.cancelOpening();
    if(social_.lobbyRuntime()->opening())social_.lobbyRuntime()->cancelPending();
    multiplayer_.cancelOpening();emit changed();
}
}
