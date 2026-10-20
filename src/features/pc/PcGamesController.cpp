#include "PcGamesController.h"
#include <QtConcurrent>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QStorageInfo>
#include <QSysInfo>
#include <QUrl>
#include <QUuid>
#include <algorithm>

namespace trainer {
namespace {
QVariantMap row(QString title,QString subtitle={}) { return {{"title",title},{"subtitle",subtitle}}; }
QString kind(const QFileInfo& file) {
    QFile f(file.filePath());if(!f.open(QIODevice::ReadOnly))return {};
    const auto header=f.read(64);
    if(header.size()<20)return {};
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
QString defaultProton() {
    const QDir base(QDir::homePath()+"/.local/share/Steam/compatibilitytools.d");
    // Only host-native, standalone UMU-compatible tools. SteamRT-managed builds
    // need the separate version manager; don't mutate or guess their runtime.
    for(const auto& dir:base.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name|QDir::Reversed)) {
        QFile manifest(dir.filePath()+"/toolmanifest.vdf");
        if(!manifest.open(QIODevice::ReadOnly)||manifest.readAll().contains("require_tool_appid"))continue;
        const bool arm=QSysInfo::currentCpuArchitecture()=="arm64";
        if(dir.fileName().contains("aarch64")!=arm)continue;
        if(QFileInfo(dir.filePath()+"/proton").isFile() && kind(QFileInfo(dir.filePath()+"/files/bin/wine"))=="native")return dir.filePath();
    }
    return {};
}
}
PcGamesController::PcGamesController(LibraryRepository& library,FilesService& files,QObject* parent)
    :QObject(parent),library_(library),files_(files) {
    connect(&picker_,&FilePickerController::selected,this,&PcGamesController::selected);
    connect(&picker_,&FilePickerController::changed,this,&PcGamesController::changed);
    connect(&files_,&FilesService::changed,this,[this]{
        if(job_.isEmpty())return;
        for(const auto& item:files_.tasks()) {
            const auto task=item.toMap();if(task["id"]!=job_ || !task["terminal"].toBool())continue;
            job_.clear();busy_=false;
            if(task["state"]=="done")inspectFolder(installRoot_,true);
            else {stage_="failed";message_="Import did not finish. Your source is unchanged; partial files remain at "+destination_;emit changed();}
            break;
        }
    });
}
void PcGamesController::configure(QString directory,QString owner) {
    if(directory_==directory && owner_==owner)return;
    ++generation_;if(!job_.isEmpty())files_.cancel(job_);
    job_.clear();picker_.cancel();open_=busy_=false;stage_="source";
    directory_=std::move(directory);owner_=std::move(owner);emit changed();
}
void PcGamesController::begin() {
    if(!library_.editable() || owner_.isEmpty())return;
    open_=true;emit changed();
}
void PcGamesController::close() { if(writing_)return;picker_.cancel();open_=false;emit changed(); }
QVariantList PcGamesController::rows() const {
    if(busy_)return {row("Hide","Import continues in Downloads"),row("Cancel import","Keep the source and any copied files")};
    if(stage_=="source")return {row("Choose game folder","Copy an installed Windows or Linux game, including its files")};
    if(stage_=="storage")return {row("Internal storage",QDir::homePath()+"/Games"),row("Choose storage","SD card or another mounted location")};
    if(stage_=="candidates")return candidates_;
    if(stage_=="confirm")return {row("Name",title_),row("Executable",chosen_["title"].toString()),row("Add to Games",chosen_["runner"]=="native"?"Linux · native":"Windows · Proton")};
    return {row("Add another game")};
}
void PcGamesController::activate(int index) {
    if(!open_||writing_||index<0||index>=rows().size())return;
    focus_=index;
    if(busy_) {
        if(index==0)close();else {++generation_;if(!job_.isEmpty())files_.cancel(job_);else {busy_=false;stage_="failed";message_="Import cancelled. Source files are unchanged.";emit changed();}}
        return;
    }
    if(stage_=="source")picker_.begin({},true);
    else if(stage_=="storage") {
        if(index==0){const auto path=QDir::homePath()+"/Games";if(!QDir().mkpath(path)){message_="Cannot create the Games folder.";emit changed();return;}selected(path);}
        else picker_.begin({},true);
    } else if(stage_=="candidates") {chosen_=candidates_[index].toMap();stage_="confirm";focus_=2;message_="Confirm the game itself, not its setup or uninstaller.";}
    else if(stage_=="confirm") {
        if(index==0)emit textRequested("Game name",title_,96);
        else if(index==1){stage_="candidates";focus_=0;}
        else registerGame();
    } else {stage_="source";message_.clear();focus_=0;}
    emit changed();
}
void PcGamesController::applyText(const QString& text) {if(stage_=="confirm"&&!busy_&&!writing_){title_=text.trimmed().left(96);emit changed();}}
void PcGamesController::dispatch(Action action) {
    if(picker_.isOpen()){picker_.dispatch(action);return;}
    if(action==Action::Back){close();return;}
    if(action==Action::Confirm){activate(focus_);return;}
    if(action==Action::Down)focus_=std::min(focus_+1,int(rows().size())-1);
    if(action==Action::Up)focus_=std::max(0,focus_-1);
    emit changed();
}
void PcGamesController::selected(const QString& path) {
    if(stage_=="source") {source_=QFileInfo(path).canonicalFilePath();title_=QFileInfo(source_).fileName();inspectFolder(source_,false);}
    else if(stage_=="storage") {
        const auto storage=QFileInfo(path).canonicalFilePath();
        if(storage.isEmpty()||!QFileInfo(storage).isWritable()){message_="Choose writable storage.";emit changed();return;}
        id_=QUuid::createUuid().toString(QUuid::WithoutBraces);
        destination_=storage+"/TrainerOS/PC/"+id_;
        installRoot_=destination_+"/"+QFileInfo(source_).fileName();
        if(destination_.startsWith(source_+'/')){message_="Choose storage outside the source game folder.";emit changed();return;}
        QStorageInfo disk(storage);
        if(!disk.isReady()||disk.bytesAvailable()<bytes_+64*1024*1024){message_="Not enough free space on this storage.";emit changed();return;}
        copy();
    }
}
QVariantMap PcGamesController::inspect(const QString& root) {
    const auto canonical=QFileInfo(root).canonicalFilePath();
    if(canonical.isEmpty()||!QFileInfo(canonical).isDir())return {{"error","Choose an existing game folder."}};
    qint64 bytes=0;int count=0;QVariantList candidates;
    QDirIterator files(canonical,QDir::AllEntries|QDir::NoDotAndDotDot|QDir::Hidden|QDir::System,QDirIterator::Subdirectories);
    while(files.hasNext()) {
        files.next();const auto file=files.fileInfo();
        if(++count>200000)return {{"error","This folder is too large to inspect. Choose only the game folder."}};
        if(file.isSymLink())return {{"error","This import contains links. Choose a self-contained game folder without linked files."}};
        if(!file.isFile()&&!file.isDir())return {{"error","The folder contains unsupported special files."}};
        if(!file.isFile())continue;
        bytes+=file.size();
        const auto runner=kind(file);if(runner.isEmpty())continue;
        auto candidate=row(file.fileName(),QDir(canonical).relativeFilePath(file.filePath()));
        candidate["path"]=file.filePath();candidate["runner"]=runner;
        candidates.append(candidate);
        if(candidates.size()>1024)return {{"error","Too many executables. Choose only the game folder."}};
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](const QVariant& a,const QVariant& b){
        const auto rank=[](QString name){name=name.toLower();return name.startsWith("setup")||name.startsWith("unins")||name.contains("redist");};
        const auto x=a.toMap()["title"].toString(),y=b.toMap()["title"].toString();
        return rank(x)!=rank(y)?rank(x)<rank(y):x.compare(y,Qt::CaseInsensitive)<0;
    });
    return {{"bytes",bytes},{"candidates",candidates},{"error",candidates.isEmpty()?"No supported executable found. Choose a Windows EXE or a native Linux game folder for this device.":""}};
}
void PcGamesController::inspectFolder(const QString& root,bool copied) {
    busy_=true;message_="Checking game files…";focus_=0;
    const auto token=++generation_;
    auto* watcher=new QFutureWatcher<QVariantMap>(this);
    connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher,token,copied]{
        const auto result=watcher->result();watcher->deleteLater();if(token!=generation_)return;
        busy_=false;scanComplete(result,copied);
    });
    watcher->setFuture(QtConcurrent::run([root]{auto result=inspect(root);result["proton"]=defaultProton();return result;}));emit changed();
}
void PcGamesController::scanComplete(const QVariantMap& result,bool copied) {
    message_=result["error"].toString();
    if(!message_.isEmpty()){stage_="failed";emit changed();return;}
    bytes_=result["bytes"].toLongLong();candidates_=result["candidates"].toList();focus_=0;
    proton_=result["proton"].toString();
    const bool windowsOnly=std::all_of(candidates_.cbegin(),candidates_.cend(),[](const auto& row){return row.toMap()["runner"]=="proton";});
    if(!copied && windowsOnly && (proton_.isEmpty() || !QFileInfo(directory_+"/runtimes/umu/1.4.4/umu-run").isFile())) {
        stage_="failed";message_="A compatible Windows runtime is not installed on this device yet. No game files were copied.";emit changed();return;
    }
    stage_=copied?"candidates":"storage";
    message_=copied?"Choose the executable that starts your game.":QString("%1 GiB · Choose where to copy this game. The source stays intact.").arg(double(bytes_)/(1024*1024*1024),0,'f',2);
    emit changed();
}
void PcGamesController::copy() {
    if(!files_.available()){message_="File copying is unavailable on this host.";emit changed();return;}
    if(!QDir().mkpath(destination_)){message_="Cannot create the game destination.";emit changed();return;}
    busy_=true;stage_="copying";message_="Copying game files… Progress is available in Downloads.";focus_=0;
    job_=files_.start("copy",{QUrl::fromLocalFile(source_).toString()},QUrl::fromLocalFile(destination_).toString());
    emit changed();
}
void PcGamesController::registerGame() {
    if(title_.isEmpty()){message_="Give your game a name.";emit changed();return;}
    const auto exe=chosen_["path"].toString();const auto runner=kind(QFileInfo(exe));
    if(runner.isEmpty()||!QFileInfo(exe).canonicalFilePath().startsWith(QFileInfo(installRoot_).canonicalFilePath()+'/')){message_="The executable changed. Choose it again.";emit changed();return;}
    const auto umu=directory_+"/runtimes/umu/1.4.4/umu-run";
    const auto proton=runner=="proton"?proton_:QString();
    if(runner=="proton"&&(proton.isEmpty()||!QFileInfo(umu).isFile())) {message_="No compatible Windows runtime is available yet. Your imported files are kept; select a Linux executable or return after runtime setup.";emit changed();return;}
    const auto owner=QString::fromLatin1(QCryptographicHash::hash(owner_.toUtf8(),QCryptographicHash::Sha256).toHex());
    AdventureRegistration record;record.adventure.id="pc:"+id_;record.adventure.title=title_;
    record.adventure.platformId="pc";record.adventure.domain="multiverse";record.adventure.adapterId="pc-local";
    record.contentPath=exe;
    record.integrationConfig={{"runner",runner},{"installRoot",installRoot_},{"workingDirectory",QFileInfo(exe).absolutePath()},
        {"dataRoot",directory_+"/pc/"+owner+"/"+id_+"/native"},
        {"prefix",directory_+"/pc/"+owner+"/"+id_+"/prefix"},{"umu",umu},{"proton",proton},{"ownership","copied"}};
    writing_=busy_=true;message_="Adding to your library…";emit changed();
    library_.saveAdventureAsync(record,this,[this,id=record.adventure.id](LibraryWriteResult result){
        writing_=busy_=false;
        if(result.success){stage_="done";message_="Ready to play. Find your game in Games → PC.";focus_=0;emit saved(id);}
        else message_=result.error;
        emit changed();
    });
}
}
