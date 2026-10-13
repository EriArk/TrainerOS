#include "SocialController.h"

namespace trainer {
void SocialController::setGameProcess(qint64 pid) {
    if(streamGamePid_==pid)return;
    if(streamGamePid_)emit commandRequested("voice-share-stop",{});
    streamGamePid_=pid;emit changed();
}
void SocialController::streamAction(QString action) {
    if(action=="stop-watch") {
        stream_.close();emit commandRequested("voice-watch-stop",{});return;
    }
    const auto voice=snapshot_["voice"].toMap();
    if(action=="stop-share") {emit commandRequested("voice-share-stop",{});return;}
    if(voice["state"]!="connected")return;
    if(action=="share"&&streamGamePid_>1) {
        emit commandRequested("voice-share-start",{{"pid",streamGamePid_}});return;
    }
    if(action.startsWith("watch:"))for(const auto& row:voice["streams"].toList()) {
        const auto identity=row.toMap()["identity"].toString();if(identity!=action.mid(6))continue;
        const auto socket=stream_.open();if(socket.isEmpty())return;
        emit commandRequested("voice-watch",{{"identity",identity},{"socket",socket}});return;
    }
}
void SocialController::streamsMenu() {
    closeMenu();
    const auto voice=snapshot_["voice"].toMap();
    menuMode_="streams";menuTitle_="Game streams";menuFocus_=0;
    menuDetail_=voice["channel"].toString().isEmpty()?"Join the conversation's call to watch or share.":
        "Audience: "+voice["name"].toString()+" · "+voice["streamStatus"].toString();
    const auto add=[this](QString label,QString action){menu_.append(label);menuCommands_.append("stream:"+action);};
    if(voice["sharing"].toBool()||voice["sharePending"].toBool())add("Stop sharing game","stop-share");
    else if(voice["state"]=="connected"&&voice["captureAvailable"].toBool()&&streamGamePid_>1)
        add("Share game with this call","share");
    for(const auto& row:voice["streams"].toList()) {
        const auto r=row.toMap();add("Watch · "+r["name"].toString(),"watch:"+r["identity"].toString());
    }
    if(stream_.active())add("Stop watching","stop-watch");
    menu_.append("Back");menuCommands_.append("cancel");emit changed();
}
}
