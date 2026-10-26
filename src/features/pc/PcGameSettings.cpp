#include "PcGamesController.h"
#include "PcRuntime.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSysInfo>
#include <QTimer>
#include <QUuid>

namespace trainer {
void PcGamesController::edit(const QString& id) {
    if(busy_||writing_||owner_.isEmpty()||!library_.editable())return;
    const auto record=library_.registration(id);
    if(!record||record->removed||record->adventure.adapterId!="pc-local")return;
    // Do not abandon a recoverable import to edit another game.
    if(!id_.isEmpty()&&stage_!="done"&&!editing()) {open_=true;message_="Finish or keep the current import first.";emit changed();return;}
    resetFlow();editId_=id;edited_=*record;title_=record->adventure.title;
    installRoot_=record->integrationConfig["installRoot"].toString();
    workingDirectory_=record->integrationConfig["workingDirectory"].toString();
    artwork_=record->integrationConfig["artwork"].toString();
    chosen_={{"path",record->contentPath},{"runner",record->integrationConfig["runner"].toString()}};
    stage_="settings";open_=true;emit changed();
}
void PcGamesController::chooseRuntime() {
    runtimeReturn_=stage_;
    runtimeRows_=pc::runtimes(directory_,QDir::homePath(),QSysInfo::currentCpuArchitecture());
    stage_="runtimes";focus_=0;message_=runtimeRows_.isEmpty()?"No compatible Windows runtime is installed.":"Select an installed version. Host installations stay unchanged; other games keep their selection.";emit changed();
}
void PcGamesController::saveSettings() {
    if(!editing()||busy_||writing_)return;
    if(settingsGuard){const auto error=settingsGuard();if(!error.isEmpty()){message_=error;emit changed();return;}}
    if(title_.isEmpty()){message_="Give your game a name.";emit changed();return;}
    const auto exe=chosen_["path"].toString();
    if((fileKind(exe)=="native")!=(edited_.integrationConfig["runner"]=="native")){message_="The executable does not match this game's runtime.";emit changed();return;}
    const auto root=QFileInfo(installRoot_).canonicalFilePath(),file=QFileInfo(exe).canonicalFilePath(),work=QFileInfo(workingDirectory_).canonicalFilePath();
    if(root.isEmpty()||fileKind(exe).isEmpty()||!file.startsWith(root+'/')||
       !QFileInfo(work).isDir()||(work!=root&&!work.startsWith(root+'/'))) {message_="Reconnect the game's storage and choose a valid executable and working folder.";emit changed();return;}
    const auto current=library_.registration(editId_);
    if(!current||current->removed||current->revision!=edited_.revision){message_="The game changed. Close and reopen its settings.";emit changed();return;}
    bool runtimeChanged=false;for(const auto& key:{"runner","proton","wine","umu"})runtimeChanged|=current->integrationConfig.value(key)!=edited_.integrationConfig.value(key);
    if(runtimeChanged) {
        const auto issue=pc::runtimeIssue(edited_.integrationConfig,QSysInfo::currentCpuArchitecture());
        if(!issue.isEmpty()){message_=issue;emit changed();return;}
        QJsonObject previous;for(const auto& key:{"runner","proton","wine","umu","prefix","prefixSource","prefixSourceExisted","contentPrefix"})if(current->integrationConfig.contains(key))previous[key]=current->integrationConfig[key];
        if(edited_.integrationConfig["prefix"]==current->integrationConfig["prefix"]) {
            auto source=current->integrationConfig["prefix"].toString();
            if(!QFileInfo(source).exists()&&!current->integrationConfig["prefixSource"].toString().isEmpty())source=current->integrationConfig["prefixSource"].toString();
            edited_.integrationConfig["prefixSource"]=source;
            edited_.integrationConfig["prefixSourceExisted"]=QFileInfo(source).isDir();
            edited_.integrationConfig["contentPrefix"]=current->integrationConfig["contentPrefix"].toString(current->integrationConfig["prefix"].toString());
            edited_.integrationConfig["prefix"]=directory_+"/pc/"+ownerKey_+"/environments/"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        edited_.integrationConfig["previousRuntime"]=previous;
    }
    edited_.adventure.title=title_;edited_.contentPath=exe;
    edited_.integrationConfig["workingDirectory"]=workingDirectory_;edited_.integrationConfig["artwork"]=artwork_;
    writing_=busy_=true;message_="Saving game settings…";emit changed();
    library_.saveAdventureAsync(edited_,this,[this](LibraryWriteResult r){
        writing_=busy_=false;
        if(r.success){if(const auto saved=library_.registration(editId_))edited_=*saved;stage_="settings";message_="Saved. Changes apply on the next launch.";emit saved(editId_);}
        else message_=r.error;
        emit changed();
    });
}
void PcGamesController::startMaintenance(const QString& operation,const QString& value) {
    if(busy_||writing_||!editing())return;
    if(QSysInfo::kernelType()!="linux"){message_="Game environment tools are available in the Linux session.";emit changed();return;}
    if(installationGuard){const auto error=installationGuard();if(!error.isEmpty()){message_=error;emit changed();return;}}
    const auto saved=library_.registration(editId_);
    if(!saved||saved->removed){message_="This game is no longer available.";emit changed();return;}
    // Never silently run against an unsaved runner, executable or prefix.
    if(saved->integrationConfig!=edited_.integrationConfig){message_="Save your launch settings before changing this environment.";emit changed();return;}
    auto c=saved->integrationConfig;
    const auto issue=pc::runtimeIssue(c,QSysInfo::currentCpuArchitecture());
    if(!issue.isEmpty()){message_=issue;emit changed();return;}
    const auto helper=QString("/var/opt/traineros/pc/launch.py");
    if(!QFileInfo(helper).isFile()){message_="PC session support is not installed.";emit changed();return;}
    c["executable"]=operation=="patch"?value:saved->contentPath;c["mode"]="maintenance";
    c["operation"]=operation;c["operationValue"]=value;c["installerFormat"]=format_;
    c["arguments"]=QJsonArray();
    ProcessCommand cmd;cmd.program="/usr/bin/python3";cmd.arguments={helper,QString::fromUtf8(QJsonDocument(c).toJson(QJsonDocument::Compact))};
    pc::captureFailure(cmd);
    cmd.workingDirectory=installRoot_;busy_=true;stage_="installing";operationState_="running";message_="Running in this game's environment. Home returns to TrainerOS; Cancel stops this operation.";
    if(!installer_.start(cmd)){busy_=false;stage_="settings";operationState_="failed";message_="The environment tool could not start.";}emit changed();
}
void PcGamesController::loadSteamUsers() {
    steamUsers_.clear();
    QStringList seen;
    for(const auto& base:{QDir::homePath()+"/.local/share/Steam",QDir::homePath()+"/.steam/steam"}) {
        for(const auto& user:QDir(base+"/userdata").entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot)) {
            bool numeric=false;user.fileName().toULongLong(&numeric);if(!numeric)continue;
            const auto path=QFileInfo(user.filePath()+"/config").canonicalFilePath();if(path.isEmpty()||seen.contains(path))continue;seen<<path;
            steamUsers_<<QVariantMap{{"title","Steam user "+user.fileName()},{"subtitle",path},{"path",path}};
        }
    }
    if(steamUsers_.isEmpty()){message_="No Steam user found. Open Steam and sign in once if you want to add this game there.";emit changed();return;}
    stage_="steam-users";focus_=0;message_="Choose the Steam library to change. Close Steam before adding or removing shortcuts.";emit changed();
}
void PcGamesController::steamAction(bool remove) {
    if(busy_||steamUser_.isEmpty())return;
    if(QSysInfo::kernelType()!="linux"){message_="Steam integration is available in the Linux session.";emit changed();return;}
    const auto record=library_.registration(editId_);if(!record){message_="Save the game first.";emit changed();return;}
    const auto helper=QString("/var/opt/traineros/pc/steam.py");
    if(!QFileInfo(helper).isFile()){message_="Steam shortcut support is not installed.";emit changed();return;}
    QJsonObject request{{"operation",remove?"remove":"add"},{"config",steamUser_},{"id",editId_},{"owner",ownerKey_},
        {"state",directory_},{"title",record->adventure.title},{"artwork",record->integrationConfig["artwork"]}};
    auto* process=new QProcess(this);busy_=writing_=true;message_="Updating the Steam shortcut…";emit changed();
    auto finish=[this,process,remove](int code){
        const auto error=QString::fromUtf8(process->readAllStandardError()).trimmed().right(800);
        busy_=writing_=false;stage_="appearance";focus_=2;
        steamStatus_=code==0?(remove?"Removed from Steam user ":"Added to Steam user ")+QFileInfo(QFileInfo(steamUser_).absolutePath()).fileName():"Not updated";
        message_=code==0?"Steam shortcut updated. Game files and saves were kept.":error.isEmpty()?"Could not update the Steam shortcut.":error;
        process->deleteLater();emit changed();
    };
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[finish](int code,QProcess::ExitStatus){finish(code);});
    connect(process,&QProcess::errorOccurred,this,[finish](QProcess::ProcessError error){if(error==QProcess::FailedToStart)finish(1);});
    QTimer::singleShot(15000,process,[process]{if(process->state()!=QProcess::NotRunning)process->kill();});
    process->start("/usr/bin/python3",{helper,QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact))});
}
}
