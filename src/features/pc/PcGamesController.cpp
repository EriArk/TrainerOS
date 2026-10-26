#include "PcGamesController.h"
#include "PcRuntime.h"
#include <QImageReader>
#include <QImage>
#include <QtConcurrent>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStorageInfo>
#include <QSysInfo>
#include <QUrl>
#include <QUuid>
#include <algorithm>

namespace trainer {
namespace {
QVariantMap row(QString title,QString subtitle={}) {return {{"title",title},{"subtitle",subtitle}};}
QString kind(const QFileInfo& file) {
    QFile f(file.filePath());if(!f.open(QIODevice::ReadOnly))return {};
    const auto header=f.read(64);if(header.size()<20)return {};
    if(header.startsWith("MZ") && file.suffix().compare("exe",Qt::CaseInsensitive)==0) {
        if(header.size()<64)return {};
        const auto* b=reinterpret_cast<const unsigned char*>(header.constData()+60);
        const quint32 offset=b[0]|(quint32(b[1])<<8)|(quint32(b[2])<<16)|(quint32(b[3])<<24);
        if(offset>16*1024*1024 || !f.seek(offset) || f.read(4)!=QByteArray("PE\0\0",4))return {};
        return "proton";
    }
    if(header.startsWith(QByteArray("\x7f" "ELF",4)) && header[5]==1 && file.isExecutable()) {
        const int machine=static_cast<unsigned char>(header[18])|(static_cast<unsigned char>(header[19])<<8);
        const auto arch=QSysInfo::currentCpuArchitecture();
        if((arch=="arm64" && machine==183)||(arch=="x86_64" && machine==62))return "native";
    }
    return {};
}
QString installerKind(const QFileInfo& file) {
    const auto runner=kind(file);if(!runner.isEmpty())return runner;
    QFile f(file.filePath());if(!f.open(QIODevice::ReadOnly))return {};
    const auto head=f.read(256);
    if(file.suffix().compare("msi",Qt::CaseInsensitive)==0 && head.startsWith(QByteArray::fromHex("d0cf11e0a1b11ae1")))return "msi";
    if((file.suffix()=="sh"||file.suffix()=="run") &&
       (head.startsWith("#!/bin/sh\n")||head.startsWith("#!/bin/bash\n")||head.startsWith("#!/usr/bin/env bash\n")))return "script";
    return {};
}
bool inside(const QString& path,const QString& root) {
    const auto p=QFileInfo(path).canonicalFilePath(),r=QFileInfo(root).canonicalFilePath();
    return !p.isEmpty()&&!r.isEmpty()&&(p==r||p.startsWith(r+'/'));
}
QString defaultProton() {
    const QDir base(QDir::homePath()+"/.local/share/Steam/compatibilitytools.d");
    for(const auto& dir:base.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name|QDir::Reversed)) {
        QFile manifest(dir.filePath()+"/toolmanifest.vdf");
        if(!manifest.open(QIODevice::ReadOnly)||manifest.readAll().contains("require_tool_appid"))continue;
        const bool arm=QSysInfo::currentCpuArchitecture()=="arm64";
        if(dir.fileName().contains("aarch64")!=arm)continue;
        if(QFileInfo(dir.filePath()+"/proton").isFile()&&kind(QFileInfo(dir.filePath()+"/files/bin/wine"))=="native")return dir.filePath();
    }
    return {};
}
}
QString PcGamesController::fileKind(const QString& path,bool installer) {return installer?installerKind(QFileInfo(path)):kind(QFileInfo(path));}
PcGamesController::PcGamesController(LibraryRepository& library,FilesService& files,QObject* parent)
    :QObject(parent),library_(library),files_(files) {
    setupRuntimeDownloads();
    auto* recovery=new QTimer(this);recovery->setInterval(1000);
    connect(recovery,&QTimer::timeout,this,[this]{
        if(recoveredPid_>1 && !liveInstaller()) {recoveredPid_=0;busy_=false;emit installerFinished();inspectFolder(installRoot_,true);}
    });recovery->start();
    connect(&picker_,&FilePickerController::selected,this,&PcGamesController::selected);
    connect(&picker_,&FilePickerController::changed,this,&PcGamesController::changed);
    connect(&files_,&FilesService::changed,this,[this]{
        if(job_.isEmpty())return;
        for(const auto& item:files_.tasks()) {
            const auto task=item.toMap();if(task["id"]!=job_)continue;
            progress_=task["progress"].toDouble();
            if(!task["terminal"].toBool()){emit changed();break;}
            job_.clear();busy_=false;
            if(stage_=="discarding") {
                if(task["state"]=="done")resetFlow();
                else {stage_="failed";message_="Could not remove the incomplete import. Files remain at "+destination_;}
                persist();emit changed();
            } else if(task["state"]=="done")inspectFolder(installRoot_,true);
            else {stage_="failed";message_=task["detail"].toString()+"\nYour source is unchanged. You can keep or discard the incomplete copy.";persist();emit changed();}
            break;
        }
    });
    connect(&installer_,&ProcessService::started,this,[this]{emit installerStarted(installer_.processId(),title_);emit changed();});
    connect(&installer_,&ProcessService::finished,this,[this](int code,bool crashed,const QString& error){
        busy_=false;emit installerFinished();
        if(editing()) {
            stage_="settings";focus_=0;
            operationState_=installer_.stopRequested()?"cancelled":code==0&&!crashed&&error.isEmpty()?"done":"failed";
            message_=code==0&&!crashed&&error.isEmpty()&&!installer_.stopRequested()?"Operation completed.":
                installer_.stopRequested()?"Operation cancelled. Changes already made by the installer may remain.":QString("Operation failed (%1). %2").arg(code).arg(error);
            emit changed();return;
        }
        if(code==0&&!crashed&&error.isEmpty()&&!installer_.stopRequested())inspectFolder(installRoot_,true);
        else {stage_="failed";message_=installer_.stopRequested()?"Installation cancelled. Keep the incomplete environment or discard it.":
            QString("Installer ended with code %1. %2 You can inspect the files before deciding what to keep.").arg(code).arg(error);persist();emit changed();}
    });
}
PcGamesController::~PcGamesController() {
    disconnect(&runtimeDownload_,nullptr,this,nullptr);
    if(runtimeDownload_.state()!=QProcess::NotRunning){runtimeDownload_.terminate();if(!runtimeDownload_.waitForFinished(3000)){runtimeDownload_.kill();runtimeDownload_.waitForFinished(1000);}}
    disconnect(&installer_,nullptr,this,nullptr);
    if(installer_.active()) {
        QEventLoop wait;
        connect(&installer_,&ProcessService::finished,&wait,&QEventLoop::quit);
        QTimer::singleShot(1400,&wait,&QEventLoop::quit);
        installer_.stop();wait.exec();
    }
}
void PcGamesController::configure(QString directory,QString owner) {
    if(directory_==directory&&owner_==owner)return;
    // SessionState prevents trainer switching while a task is active.
    if(busy_||writing_||installer_.active())return;
    ++generation_;picker_.cancel();open_=false;directory_=std::move(directory);owner_=std::move(owner);
    ownerKey_=QString::fromLatin1(QCryptographicHash::hash(owner_.toUtf8(),QCryptographicHash::Sha256).toHex());
    steam.configure(directory_,owner_.isEmpty()?QString():ownerKey_);
    runtimeState_.clear();runtimeCatalogue_={};runtimeItem_={};
    journal_=owner_.isEmpty()?QString():directory_+"/pc/"+ownerKey_+"/import.json";
    resetFlow();
    QFile file(journal_);
    if(!journal_.isEmpty()&&file.size()<65536&&file.open(QIODevice::ReadOnly)) {
        const auto state=QJsonDocument::fromJson(file.readAll()).object();
        id_=state["id"].toString();destination_=state["destination"].toString();
        if(ownsDestination()&&!library_.registration("pc:"+id_)) {
            mode_=state["mode"].toString();format_=state["format"].toString();source_=state["source"].toString();
            installRoot_=state["installRoot"].toString();title_=state["title"].toString();proton_=state["proton"].toString();
            runtimeConfig_=state["runtime"].toObject();
            if(inside(installRoot_,destination_)) {
                stage_="failed";message_="An unfinished import was kept. Inspect its files or discard only this import.";
                recoveredPid_=liveInstaller();
                if(recoveredPid_>1){busy_=true;stage_="installing";message_="The installer is still running. Return through Apps to finish it; files remain protected.";}
            }
            else resetFlow();
        } else resetFlow();
    }
    emit changed();
}
void PcGamesController::resetFlow() {
    editId_.clear();artwork_.clear();textField_.clear();operation_.clear();operationState_.clear();steamUser_.clear();steamStatus_.clear();addSteam_=false;removeSteam_=false;runtimeConfig_={};runtimeReturn_.clear();
    stage_="source";mode_.clear();format_.clear();picking_.clear();source_.clear();destination_.clear();
    installRoot_.clear();id_.clear();chosen_.clear();candidates_.clear();workingDirectory_.clear();
    message_.clear();title_.clear();proton_.clear();bytes_=0;focus_=0;progress_=0;
}
void PcGamesController::persist() {
    if(editing())return;
    if(journal_.isEmpty())return;
    if(id_.isEmpty()||stage_=="done") {QFile::remove(journal_);return;}
    QDir().mkpath(QFileInfo(journal_).absolutePath());QSaveFile out(journal_);
    const auto data=QJsonDocument(QJsonObject{{"id",id_},{"destination",destination_},{"source",source_},
        {"installRoot",installRoot_},{"title",title_},{"mode",mode_},{"format",format_},{"proton",proton_},{"runtime",runtimeConfig_}}).toJson();
    if(!out.open(QIODevice::WriteOnly)||out.write(data)!=data.size()||!out.commit())message_="Could not save import recovery information. Keep this window open until the import finishes.";
}
bool PcGamesController::ownsDestination() const {
    if(QUuid(id_).isNull()||QFileInfo(destination_).fileName()!=id_||QFileInfo(destination_).isSymLink()||
       !destination_.endsWith("/TrainerOS/PC/"+id_))return false;
    QFile marker(destination_+"/.traineros-import");
    return !QFileInfo(marker).isSymLink()&&marker.open(QIODevice::ReadOnly)&&marker.read(256)==(ownerKey_+":"+id_).toUtf8();
}
QString PcGamesController::prefix() const {
    return mode_=="installer"?destination_+"/prefix":directory_+"/pc/"+ownerKey_+"/"+id_+"/prefix";
}
qint64 PcGamesController::liveInstaller() const {
    if(QSysInfo::kernelType()!="linux")return 0;
    QFile file(destination_+"/.traineros-running");
    if(file.size()>4096||!file.open(QIODevice::ReadOnly))return 0;
    const auto record=QJsonDocument::fromJson(file.readAll()).object();const auto pid=record["pid"].toInteger();
    if(pid<=1)return 0;
    QFile stat(QString("/proc/%1/stat").arg(pid)),command(QString("/proc/%1/cmdline").arg(pid));
    if(!stat.open(QIODevice::ReadOnly)||!command.open(QIODevice::ReadOnly))return 0;
    const auto data=stat.readAll();const auto fields=data.mid(data.lastIndexOf(')')+2).split(' ');
    if(fields.size()>19&&fields[19]==record["start"].toString().toUtf8()&&command.readAll().contains("/var/opt/traineros/pc/launch.py"))return pid;
    return 0;
}
void PcGamesController::begin() {if(!library_.editable()||owner_.isEmpty())return;if(editing()&&!busy_)resetFlow();open_=true;emit changed();}
void PcGamesController::close() {if(writing_)return;picker_.cancel();open_=false;emit changed();}
QVariantList PcGamesController::rows() const {
    if(writing_)return {};
    if(installing()) {
        QVariantList result{row("Return to installer",title_),row("Hide","Installation continues")};
        if(!recoveredPid_)result<<row("Cancel installation","Stops this installer and its child processes");
        return result;
    }
    if(busy_)return {row("Hide","Progress remains in Downloads"),row("Cancel","Keep the source and any incomplete copy")};
    if(stage_=="runtimes") {auto result=runtimeRows_;result<<row("Download a runtime","Official versions · shared cache")<<row("Back");return result;}
    if(stage_=="runtime-catalogue") {QVariantList result;for(const auto& v:runtimeCatalogue_){const auto c=v.toObject();result<<row(c["title"].toString(),QString("%1 MiB · GE-Proton + UMU · first launch needs internet").arg(c["size"].toDouble()/1048576,0,'f',0));}result<<row("Refresh versions")<<row("Back");return result;}
    if(stage_=="steam-users") {auto result=steamUsers_;result<<row("Back");return result;}
    if(stage_=="settings")return {row("Launch",edited_.integrationConfig["runner"]=="native"?"Native Linux":edited_.integrationConfig["runner"]=="wine"?"Wine":QFileInfo(edited_.integrationConfig["proton"].toString()).fileName()),
        row(edited_.integrationConfig["runner"]=="native"?"Updates & patches":"Components",edited_.integrationConfig["runner"]=="native"?"Run an installer for this game":"Dependencies, patches and Windows settings"),
        row("Artwork & Steam",title_),row("Controls",steam.active()?"Steam Input owns this running session":"Controller, keyboard and radial menus"),row("Save changes","Keep this game's stable identity"),row("Performance","Per-game power, clocks and graphics")};
    if(stage_=="launch-settings") {
        QVariantList result{row("Executable",chosen_["path"].toString()),row("Working folder",workingDirectory_),row("Arguments",edited_.integrationConfig["argumentText"].toString()),row("Runtime",edited_.integrationConfig["runner"]=="wine"?edited_.integrationConfig["wine"].toString():QFileInfo(edited_.integrationConfig["proton"].toString()).fileName()),row("Restore previous runtime","Return to its retained environment and earlier saves"),row("Back")};
        if(edited_.integrationConfig["runner"]=="native") {result[3]=QVariantMap{{"title","Native Linux"},{"subtitle","Runs directly on this device"},{"enabled",false}};result.removeAt(4);}return result;
    }
    if(stage_=="components") {
        if(edited_.integrationConfig["runner"]=="native")return {row("Run another installer","A native installer or patch for this game"),row("Back")};
        return {row("Run another EXE / MSI","Patch, mod or DLC in this game's environment"),row("Visual C++ 2015–2022","Winetricks · vcrun2022"),row("DirectX 9 libraries","Winetricks · d3dx9"),row(".NET Framework 4.8","Winetricks · dotnet48"),row("Windows settings","Windows version, libraries and drives"),row("Installed Windows programs","Use each program's own uninstaller"),row("Back")};
    }
    if(stage_=="appearance")return {row("Name",title_),row("Artwork",artwork_.isEmpty()?"Choose an image":"Change image"),row("Steam shortcut",steamStatus_.isEmpty()?"Choose a Steam user":steamStatus_),row("Remove Steam shortcut","Only the owned shortcut is removed"),row("Back")};
    if(stage_=="maintenance-confirm")return {row("Run",operation_=="component"?"Install "+operationValue_+" into this game's environment":"Use this game's current saved environment"),row("Back")};
    if(stage_=="source")return {row("Install a game","Windows EXE / MSI or a Linux installer"),row("Add a ready-to-play game","A folder, archive or executable")};
    if(stage_=="ready-source")return {row("Game folder","Copy the game and all its files"),row("Archive","ZIP, 7Z or RAR"),row("Executable","Copy its containing folder, including adjacent files")};
    if(stage_=="installer-source")return {row("Choose installer","EXE / MSI, native ELF / AppImage, or shell installer"),row("Windows runtime","Select or download Wine / Proton before installing")};
    if(stage_=="storage") {
        QStorageInfo disk(QDir::homePath());
        return {row("Internal storage",QString("%1 GiB available").arg(double(disk.bytesAvailable())/(1024*1024*1024),0,'f',1)),row("Choose storage","SD card or another mounted location")};
    }
    if(stage_=="install-ready") {QVariantList result{row("Start installer",title_),row("Choose different storage"),row("Discard this preparation","Remove the empty environment")};if(format_=="proton"||format_=="msi")result<<row("Compatibility",QFileInfo(proton_).fileName());return result;}
    if(stage_=="candidates") {auto result=candidates_;result<<row("Browse for executable","Choose a file inside this installation")<<row("Import options","Keep or discard the imported files");return result;}
    if(stage_=="confirm") {QVariantList result{row("Name",title_),row("Executable",chosen_["subtitle"].toString()),row("Add to Games",chosen_["runner"]=="native"?"Linux · native":"Windows"),row("Working folder",workingDirectory_),row("Artwork",artwork_.isEmpty()?"Choose an image":"Change image"),row("Add to Steam library",addSteam_?"On · selected Steam user":"Off · optional")};if(chosen_["runner"]!="native")result<<row("Compatibility",runtimeConfig_["runner"].toString("Auto"));return result;}
    if(stage_=="discard")return {row("Keep files","Return without removing anything"),row("Discard incomplete import","Remove only the new folder created by this import")};
    if(stage_=="failed"&&ownsDestination())return {row("Inspect files","Look for the actual game executable"),row("Keep files and start another","The incomplete folder stays on storage"),row("Discard incomplete import")};
    return {row("Add another game")};
}
void PcGamesController::activate(int index) {
    if(!open_||writing_||index<0||index>=rows().size())return;
    focus_=index;
    if(!rows()[index].toMap().value("enabled",true).toBool())return;
    if(installing()) {if(index==0){if(recoveredPid_)emit installerStarted(recoveredPid_,title_);else emit resumeInstaller();}else if(index==1)close();else installer_.stop();return;}
    if(busy_ && runtimeDownload_.state()!=QProcess::NotRunning){if(index==0)close();else runtimeTaskCommand("cancel");return;}
    if(busy_) {
        if(index==0)close();else if(!job_.isEmpty())files_.cancel(job_);
        else {++generation_;busy_=false;stage_="failed";message_="Import cancelled. Source files are unchanged.";persist();emit changed();}
        return;
    }
    if(stage_=="settings") {
        if(index==0)stage_="launch-settings";else if(index==1)stage_="components";else if(index==2)stage_="appearance";
        else if(index==3){close();emit controlsRequested(editId_);return;}else if(index==5){close();emit performanceRequested(editId_);return;}else {saveSettings();return;}focus_=0;emit changed();return;
    }
    if(stage_=="launch-settings") {
        if(index==4&&edited_.integrationConfig["runner"]=="native"){stage_="settings";focus_=0;emit changed();return;}
        if(index==0){picking_="game";picker_.begin(installRoot_);}else if(index==1){picking_="working";picker_.begin(workingDirectory_,true);}
        else if(index==2){textField_="arguments";emit textRequested("Launch arguments",edited_.integrationConfig["argumentText"].toString(),1024);}
        else if(index==3)chooseRuntime();
        else if(index==4){const auto previous=edited_.integrationConfig["previousRuntime"].toObject();const auto issue=pc::runtimeIssue(previous,QSysInfo::currentCpuArchitecture());if(previous.isEmpty()||!issue.isEmpty())message_=previous.isEmpty()?"No previous runtime selection.":issue;else {for(const auto& key:{"runner","proton","wine","umu","prefix","prefixSource","prefixSourceExisted","contentPrefix"})if(previous.contains(key))edited_.integrationConfig[key]=previous[key];else edited_.integrationConfig.remove(key);message_="Previous runtime and retained environment selected. Save to apply.";}}
        else {stage_="settings";focus_=0;}emit changed();return;
    }
    if(stage_=="runtime-catalogue"){if(index<runtimeCatalogue_.size())runtimeDownload("install",runtimeCatalogue_[index].toObject());else if(index==runtimeCatalogue_.size())runtimeDownload("catalogue");else {stage_="runtimes";focus_=0;emit changed();}return;}
    if(stage_=="runtimes") {
        if(index==runtimeRows_.size()){stage_="runtime-catalogue";focus_=0;runtimeDownload("catalogue");return;}
        if(index<runtimeRows_.size()) {
            const auto c=QJsonObject::fromVariantMap(runtimeRows_[index].toMap()["config"].toMap());
            if(editing()){for(const auto& key:{"runner","proton","wine","umu"})edited_.integrationConfig[key]=c[key];}
            else {runtimeConfig_=c;proton_=c["proton"].toString();persist();}
        }
        stage_=runtimeReturn_;
        if(index<runtimeRows_.size()&&runtimeReturn_=="installer-source"&&(format_=="proton"||format_=="msi")&&QFileInfo(source_).isFile()) {
            bytes_=QFileInfo(source_).size();stage_="storage";message_="Choose storage for the selected installer.";
        }
        focus_=0;emit changed();return;
    }
    if(stage_=="components") {
        const bool native=edited_.integrationConfig["runner"]=="native";
        if(index==0){picking_="patch";picker_.begin(installRoot_);}
        else if(index==(native?1:6)){stage_="settings";focus_=1;}
        else {operation_=index<4?"component":"tool";operationValue_=QStringList{"","vcrun2022","d3dx9","dotnet48","winecfg","uninstaller"}.value(index);stage_="maintenance-confirm";focus_=0;message_="Changes apply only to this game's Windows environment. Close the game first. Downloaded components may display their licence terms.";}
        emit changed();return;
    }
    if(stage_=="maintenance-confirm") {if(index==0)startMaintenance(operation_,operationValue_);else {stage_="components";focus_=0;emit changed();}return;}
    if(stage_=="appearance") {
        if(index==0){textField_="name";emit textRequested("Game name",title_,96);}else if(index==1){picking_="artwork";picker_.begin();}
        else if(index==2||index==3){removeSteam_=index==3;loadSteamUsers();}
        else {stage_="settings";focus_=2;}emit changed();return;
    }
    if(stage_=="steam-users") {
        if(index<steamUsers_.size()){steamUser_=steamUsers_[index].toMap()["path"].toString();addSteam_=true;if(editing()){steamAction(removeSteam_);return;}}
        stage_=editing()?"appearance":"confirm";focus_=0;emit changed();return;
    }
    if(stage_=="source") {mode_=index==0?"installer":"ready";stage_=index==0?"installer-source":"ready-source";focus_=0;if(index==0){picking_="installer";picker_.begin();}}
    else if(stage_=="installer-source") {if(index==1)chooseRuntime();else {picking_="installer";picker_.begin();}}
    else if(stage_=="ready-source") {picking_=index==0?"folder":index==1?"archive":"executable";picker_.begin({},index==0);}
    else if(stage_=="storage") {
        picking_="storage";
        if(index==0) {const auto path=QDir::homePath()+"/Games";if(!QDir().mkpath(path)){message_="Cannot create the Games folder.";emit changed();return;}selected(path);}
        else picker_.begin({},true);
    } else if(stage_=="install-ready") {
        if(index==0)startInstaller();
        else if(index==1){stage_="storage";message_="Choose another location. The empty preparation folder will be kept.";}
        else if(index==2){stage_="discard";focus_=0;}else chooseRuntime();
    } else if(stage_=="candidates") {
        if(index==candidates_.size()){picking_="game";picker_.begin(installRoot_);}
        else if(index>candidates_.size()){stage_="failed";focus_=0;message_="Keep the imported files, inspect them again, or discard this import.";}
        else {chosen_=candidates_[index].toMap();workingDirectory_=QFileInfo(chosen_["path"].toString()).absolutePath();stage_="confirm";focus_=2;message_="Confirm the game itself, not its setup or uninstaller.";}
    } else if(stage_=="confirm") {
        if(index==0){textField_="name";emit textRequested("Game name",title_,96);}
        else if(index==1){stage_="candidates";focus_=0;}
        else if(index==2)registerGame();
        else if(index==3){picking_="working";picker_.begin(workingDirectory_,true);}
        else if(index==4){picking_="artwork";picker_.begin();}
        else if(index==5){if(addSteam_)addSteam_=false;else loadSteamUsers();}
        else chooseRuntime();
    } else if(stage_=="discard") {
        if(index==0)stage_="failed";
        else if(ownsDestination()&&!liveInstaller()&&files_.available()) {
            busy_=true;stage_="discarding";message_="Removing this incomplete import…";
            job_=files_.start("delete",{QUrl::fromLocalFile(destination_).toString()});
        } else message_="Cannot safely remove this import on this host. Files were kept.";
    } else if(stage_=="failed"&&ownsDestination()) {
        if(index==0)inspectFolder(installRoot_,true);
        else if(index==2){stage_="discard";focus_=0;}
        else {resetFlow();persist();}
    } else {resetFlow();persist();}
    emit changed();
}
void PcGamesController::applyText(const QString& text) {
    if(busy_||writing_)return;
    if(textField_=="arguments"&&editing()) {
        // QProcess splitting yields an argv, never evaluated shell source.
        edited_.integrationConfig["argumentText"]=text.left(1024);
        edited_.integrationConfig["arguments"]=QJsonArray::fromStringList(QProcess::splitCommand(text.left(1024)));
    }else if(stage_=="confirm"||stage_=="appearance")title_=text.trimmed().left(96);
    textField_.clear();persist();emit changed();
}
void PcGamesController::dispatch(Action action) {
    if(picker_.isOpen()){picker_.dispatch(action);return;}
    if(action==Action::Back){if(busy_){close();return;}if(stage_=="runtime-catalogue"){stage_="runtimes";focus_=0;emit changed();}else if(stage_=="runtimes"){stage_=runtimeReturn_;focus_=0;emit changed();}else if(stage_=="steam-users"){stage_=editing()?"appearance":"confirm";focus_=0;emit changed();}else if(editing()&&stage_!="settings"&&!busy_){stage_="settings";focus_=0;emit changed();}else close();return;}
    if(action==Action::Confirm){activate(focus_);return;}
    if(action==Action::Down)focus_=std::min(focus_+1,std::max(0,int(rows().size())-1));
    if(action==Action::Up)focus_=std::max(0,focus_-1);
    emit changed();
}
void PcGamesController::selected(const QString& path) {
    const QFileInfo file(path);const auto canonical=file.canonicalFilePath();
    if(canonical.isEmpty()||file.isSymLink()){message_="Choose an existing local file or folder.";emit changed();return;}
    if(picking_=="artwork") {
        QImageReader reader(canonical);const auto size=reader.size();
        if(file.size()>24*1024*1024||!size.isValid()||qint64(size.width())*size.height()>32000000) {message_="Choose an image smaller than 32 megapixels and 24 MB.";emit changed();return;}
        reader.setScaledSize(size.scaled(1200,1200,Qt::KeepAspectRatio));const auto image=reader.read();
        const auto folder=directory_+"/pc/"+ownerKey_+"/artwork";QDir().mkpath(folder);
        const auto target=folder+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".png";
        if(image.isNull()||!image.save(target,"PNG"))message_="Could not read or save the image.";else {artwork_=QUrl::fromLocalFile(target).toString();message_.clear();}emit changed();return;
    }
    if(picking_=="patch") {
        const auto format=installerKind(file);const bool native=edited_.integrationConfig["runner"]=="native";
        if(format.isEmpty()||native!=(format=="native"||format=="script")){message_="Choose an installer compatible with this game's runtime.";emit changed();return;}
        operation_="patch";operationValue_=canonical;format_=format;stage_="maintenance-confirm";focus_=0;message_="Run this installer in the existing game environment. Its changes are not automatically undone.";emit changed();return;
    }
    if(picking_=="game"||picking_=="working") {
        if(!inside(canonical,installRoot_)){message_="Choose a location inside this game's installation.";emit changed();return;}
        if(picking_=="working") {if(file.isDir())workingDirectory_=canonical;}
        else {const auto runner=kind(file);if(runner.isEmpty()){message_="This is not a supported game executable.";emit changed();return;}
            if(editing()&&(runner=="native")!=(edited_.integrationConfig["runner"]=="native")){message_="Choose the same executable family as this game.";emit changed();return;}
            chosen_=row(file.fileName(),QDir(installRoot_).relativeFilePath(canonical));chosen_["path"]=canonical;chosen_["runner"]=runner;
            workingDirectory_=file.absolutePath();stage_=editing()?"launch-settings":"confirm";focus_=editing()?0:2;}
        emit changed();return;
    }
    if(stage_=="source"||stage_=="ready-source"||stage_=="installer-source") {
        source_=canonical;title_=file.completeBaseName();
        if(picking_=="installer") {
            format_=installerKind(file);proton_=defaultProton();
            runtimeConfig_={};for(const auto& item:pc::runtimes(directory_,QDir::homePath(),QSysInfo::currentCpuArchitecture()))if(item.toMap()["enabled"].toBool()){runtimeConfig_=QJsonObject::fromVariantMap(item.toMap()["config"].toMap());proton_=runtimeConfig_["proton"].toString();break;}
            if(format_.isEmpty()) {message_="Choose a valid EXE, MSI, native executable, or sh/bash installer.";emit changed();return;}
            if((format_=="proton"||format_=="msi")&&runtimeConfig_.isEmpty()) {message_="A compatible Windows runtime is not installed on this device yet.";emit changed();return;}
            bytes_=file.size();stage_="storage";message_="Choose storage for the game's own installation environment.";
        } else if(picking_=="archive") {
            if(!QStringList{"zip","7z","rar"}.contains(file.suffix().toLower())) {message_="Choose a ZIP, 7Z or RAR archive.";emit changed();return;}
            format_="archive";bytes_=file.size();stage_="storage";message_="The archive will be extracted into its own folder. Your archive stays intact.";
        } else {
            if(picking_=="executable") {if(kind(file).isEmpty()){message_="Choose a supported game executable.";emit changed();return;}source_=file.absolutePath();}
            format_="folder";mode_="ready";title_=QFileInfo(source_).fileName();inspectFolder(source_,false);
        }
        emit changed();return;
    }
    if(stage_!="storage")return;
    if(!file.isDir()||!file.isWritable()){message_="Choose writable storage.";emit changed();return;}
    if(canonical==source_||canonical.startsWith(source_+'/')) {message_="Choose storage outside the source game folder.";emit changed();return;}
    QStorageInfo disk(canonical);
    if(!disk.isReady()||disk.bytesAvailable()<bytes_+64*1024*1024){message_="Not enough free space on this storage.";emit changed();return;}
    if(format_=="folder"&&!files_.available()){message_="File copying is unavailable on this host.";emit changed();return;}
    id_=QUuid::createUuid().toString(QUuid::WithoutBraces);destination_=canonical+"/TrainerOS/PC/"+id_;
    if(!QDir().mkpath(destination_)){message_="Cannot create the game destination.";emit changed();return;}
    QFile marker(destination_+"/.traineros-import");const auto token=(ownerKey_+":"+id_).toUtf8();
    if(!marker.open(QIODevice::WriteOnly|QIODevice::NewOnly)||marker.write(token)!=token.size()){message_="Cannot record ownership of the new folder.";emit changed();return;}marker.close();
    installRoot_=format_=="folder"?destination_+'/'+QFileInfo(source_).fileName():destination_+"/game";
    if(mode_=="installer"&&(format_=="proton"||format_=="msi"))installRoot_=prefix()+"/drive_c";
    if(format_!="folder"&&!QDir().mkpath(installRoot_)){message_="Cannot create the installation folder.";emit changed();return;}
    persist();
    if(mode_=="installer") {stage_="install-ready";message_="Use the installer's normal steps. Keep the game in this location:\n"+installRoot_;emit changed();}
    else copy();
}
QVariantMap PcGamesController::inspect(const QString& root,bool installed) {
    const auto canonical=QFileInfo(root).canonicalFilePath();
    if(canonical.isEmpty()||!QFileInfo(canonical).isDir())return {{"error","Choose an existing game folder."}};
    qint64 bytes=0;int count=0;QVariantList candidates;
    QDirIterator files(canonical,QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System,QDirIterator::Subdirectories);
    while(files.hasNext()) {
        files.next();const auto file=files.fileInfo();
        if(++count>200000)return {{"error","This folder is too large to inspect. Choose only the game folder."}};
        if(file.isSymLink()) {if(installed)continue;return {{"error","This import contains links. Choose a self-contained game folder without linked files."}};}
        if(!file.isFile()&&!file.isDir())return {{"error","The folder contains unsupported special files."}};
        if(!file.isFile())continue;
        bytes+=file.size();
        const auto relative=QDir(canonical).relativeFilePath(file.filePath());
        if(installed&&relative.startsWith("windows/",Qt::CaseInsensitive))continue;
        const auto runner=kind(file);if(runner.isEmpty())continue;
        auto candidate=row(file.fileName(),relative);candidate["path"]=file.filePath();candidate["runner"]=runner;candidates.append(candidate);
        if(candidates.size()>1024)return {{"error","Too many executables. Choose only the game folder."}};
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const QVariant& a,const QVariant& b){
        const auto rank=[](QString name){name=name.toLower();return name.startsWith("setup")||name.startsWith("unins")||name.contains("redist");};
        const auto x=a.toMap()["title"].toString(),y=b.toMap()["title"].toString();
        return rank(x)!=rank(y)?rank(x)<rank(y):x.compare(y,Qt::CaseInsensitive)<0;
    });
    return {{"bytes",bytes},{"candidates",candidates},{"error",candidates.isEmpty()?"No supported game executable found in this folder.":""}};
}
void PcGamesController::inspectFolder(const QString& root,bool copied) {
    busy_=true;message_="Checking game files…";focus_=0;const auto token=++generation_;
    auto* watcher=new QFutureWatcher<QVariantMap>(this);
    connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher,token,copied]{
        const auto result=watcher->result();watcher->deleteLater();if(token!=generation_)return;
        busy_=false;scanComplete(result,copied);
    });
    const bool installed=mode_=="installer";
    watcher->setFuture(QtConcurrent::run([root,installed]{auto result=inspect(root,installed);result["proton"]=defaultProton();return result;}));emit changed();
}
void PcGamesController::scanComplete(const QVariantMap& result,bool copied) {
    message_=result["error"].toString();candidates_=result["candidates"].toList();focus_=0;
    if(!message_.isEmpty()) {stage_=copied?"candidates":"failed";emit changed();return;}
    bytes_=result["bytes"].toLongLong();if(proton_.isEmpty())proton_=result["proton"].toString();
    stage_=copied?"candidates":"storage";
    message_=copied?"Choose the executable that starts your game.":QString("%1 GiB · Choose where to copy this game. The source stays intact.").arg(double(bytes_)/(1024*1024*1024),0,'f',2);
    persist();emit changed();
}
void PcGamesController::copy() {
    busy_=true;stage_="copying";progress_=0;focus_=0;
    message_=format_=="archive"?"Extracting game files… Progress is in Downloads.":"Copying game files… Progress is in Downloads.";
    job_=files_.start(format_=="archive"?"extract":"copy",{QUrl::fromLocalFile(source_).toString()},QUrl::fromLocalFile(format_=="archive"?installRoot_:destination_).toString());emit changed();
}
void PcGamesController::startInstaller() {
    if(QSysInfo::kernelType()!="linux") {
        message_="Interactive installation is available in the Linux session. No installer was started.";emit changed();return;
    }
    if(installationGuard){const auto error=installationGuard();if(!error.isEmpty()){message_=error;emit changed();return;}}
    if(!ownsDestination()||installerKind(QFileInfo(source_))!=format_){message_="Installer or destination changed. Choose it again.";emit changed();return;}
    const QString helper="/var/opt/traineros/pc/launch.py";
    if(!QFileInfo(helper).isFile()){message_="The PC session helper is unavailable.";emit changed();return;}
    QJsonObject config{{"mode","installer"},{"executable",source_},{"installRoot",installRoot_},{"workingDirectory",installRoot_},
        {"runner",(format_=="msi"||format_=="proton")?"proton":"native"},{"installerFormat",format_},
        {"prefix",prefix()},{"proton",proton_},{"umu",directory_+"/runtimes/umu/1.4.4/umu-run"},{"activityFile",destination_+"/.traineros-running"}};
    if(format_=="msi"||format_=="proton") {
        for(auto it=runtimeConfig_.begin();it!=runtimeConfig_.end();++it)config[it.key()]=it.value();
        const auto issue=pc::runtimeIssue(config,QSysInfo::currentCpuArchitecture());if(!issue.isEmpty()){message_=issue;emit changed();return;}
    }
    ProcessCommand command;command.program="/usr/bin/python3";command.arguments={helper,QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact))};command.workingDirectory=installRoot_;
    pc::captureFailure(command);
    busy_=true;stage_="installing";message_="Complete the installer. Home lets you return to TrainerOS while it runs.";persist();
    if(!installer_.start(command)){busy_=false;stage_="failed";message_="The installer could not start.";}
    emit changed();
}
QVariantList PcGamesController::tasks() const {
    if(editing()) {
        if(operationState_.isEmpty())return {};
        QVariantList actions{QVariantMap{{"id","open"},{"label",installing()?"Return to installer":"Review game"},{"enabled",true}}};
        if(installing())actions<<QVariantMap{{"id","cancel"},{"label","Cancel"},{"enabled",true}};
        return {QVariantMap{{"id",editId_},{"kind","pc-maintenance"},{"title",title_},{"detail",message_},{"state",operationState_},{"terminal",!installing()},{"actions",actions}}};
    }
    if(mode_!="installer"||id_.isEmpty()||stage_=="storage"||stage_=="install-ready")return {};
    const bool active=installing();
    QVariantList actions{QVariantMap{{"id","open"},{"label",active?"Return to installer":"Review game"},{"enabled",true}}};
    if(active&&!recoveredPid_)actions<<QVariantMap{{"id","cancel"},{"label","Cancel"},{"enabled",true}};
    return {QVariantMap{{"id",id_},{"kind","pc-install"},{"title",title_},{"detail",message_},{"destination",installRoot_},
        {"state",active?"running":stage_=="done"?"done":"waiting"},{"terminal",stage_=="done"||stage_=="failed"},{"progress",active?0:1},{"actions",actions}}};
}
void PcGamesController::taskCommand(const QString& command) {
    if(command=="cancel"||command=="cancel-all") {if(installer_.active())installer_.stop();}
    else if(command=="open") {if(recoveredPid_)emit installerStarted(recoveredPid_,title_);else if(installer_.active())emit resumeInstaller();else {open_=true;emit changed();}}
}
void PcGamesController::registerGame() {
    if(title_.isEmpty()){message_="Give your game a name.";emit changed();return;}
    const auto exe=chosen_["path"].toString();const auto runner=kind(QFileInfo(exe));
    if(runner.isEmpty()||!inside(exe,installRoot_)||!inside(workingDirectory_,installRoot_)||!QFileInfo(workingDirectory_).isDir()) {message_="The executable or working folder changed. Choose it again.";emit changed();return;}
    const auto umu=directory_+"/runtimes/umu/1.4.4/umu-run";
    if(runner=="proton") {
        if(runtimeConfig_.isEmpty())for(const auto& item:pc::runtimes(directory_,QDir::homePath(),QSysInfo::currentCpuArchitecture()))if(item.toMap()["enabled"].toBool()){runtimeConfig_=QJsonObject::fromVariantMap(item.toMap()["config"].toMap());break;}
        if(runtimeConfig_.isEmpty()||!pc::runtimeIssue(runtimeConfig_,QSysInfo::currentCpuArchitecture()).isEmpty()){message_="No compatible Windows runtime is available yet. Imported files are kept; choose Compatibility after runtime setup.";emit changed();return;}
    }
    AdventureRegistration record;record.adventure.id="pc:"+id_;record.adventure.title=title_;
    record.adventure.platformId="pc";record.adventure.domain="multiverse";record.adventure.adapterId="pc-local";record.contentPath=exe;
    record.integrationConfig={{"runner",runner},{"installRoot",installRoot_},{"workingDirectory",workingDirectory_},
        {"dataRoot",directory_+"/pc/"+ownerKey_+"/"+id_+"/native"},{"prefix",prefix()},{"umu",umu},
        {"proton",runner=="proton"?proton_:QString()},{"ownership",mode_=="installer"?"installed":"copied"}};
    record.integrationConfig["artwork"]=artwork_;
    if(runner=="proton")for(auto it=runtimeConfig_.begin();it!=runtimeConfig_.end();++it)record.integrationConfig[it.key()]=it.value();
    writing_=busy_=true;message_="Adding to your library…";emit changed();
    library_.saveAdventureAsync(record,this,[this,id=record.adventure.id](LibraryWriteResult result){
        writing_=busy_=false;
        if(result.success){stage_="done";message_="Ready to play. Find your game in Games → PC.";focus_=0;persist();emit saved(id);if(addSteam_) {const auto user=steamUser_;edit(id);steamUser_=user;steamAction();}}
        else message_=result.error;
        emit changed();
    });
}
}
