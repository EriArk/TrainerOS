#include "ChatFiles.h"
#include <QDateTime>
#include <QFileInfo>
#include <QUuid>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QTimer>
#include "platform/files/FilesService.h"
namespace trainer {
ChatFiles::ChatFiles(QObject* parent):QObject(parent){
    connect(&picker_,&FilePickerController::selected,this,[this](const QString& path){
        if(channel_.isEmpty()||busy_)return;
        const QFileInfo f(path);if(!f.isFile()||!f.isReadable()){status_="This file is no longer available.";emit changed();return;}
        auto& rows=selections_[channel_];
        for(const auto& v:rows)if(v.toMap()["path"]==f.canonicalFilePath())return;
        rows.append(QVariantMap{{"path",f.canonicalFilePath()},{"name",f.fileName()},{"size",f.size()},{"modified",f.lastModified().toMSecsSinceEpoch()}});
        nonce_.clear();uncertain_=false;status_.clear();emit changed();
    });
    connect(&picker_,&FilePickerController::changed,this,&ChatFiles::changed);
}
QVariantList ChatFiles::selected()const{return selections_.value(channel_);}
void ChatFiles::context(QString account,QString channel){
    const bool changedAccount=account_!=account;
    if(!changedAccount&&channel_==channel)return;
    account_=account;channel_=channel;
    if(changedAccount){selections_.clear();jobs_.clear();moving_.clear();preparing_.clear();busy_=uncertain_=false;nonce_.clear();status_.clear();}
    picker_.cancel();emit changed();
}
void ChatFiles::choose(){if(!channel_.isEmpty()&&!busy_&&!uncertain_)picker_.begin();}
void ChatFiles::remove(int index){if(busy_||uncertain_)return;auto& rows=selections_[channel_];if(index>=0&&index<rows.size())rows.removeAt(index);nonce_.clear();status_.clear();emit changed();}
bool ChatFiles::send(QString text){
    if(busy_||channel_.isEmpty()||selected().isEmpty())return false;
    if(uncertain_&&(uploadChannel_!=channel_||text_!=text||QDateTime::currentMSecsSinceEpoch()-nonceTime_>240000)){
        status_="Delivery is unknown. Check the room before discarding this send.";emit changed();return false;
    }
    if(nonce_.isEmpty()||!uncertain_){nonce_=QUuid::createUuid().toString(QUuid::Id128);nonceTime_=QDateTime::currentMSecsSinceEpoch();}
    uploadChannel_=channel_;text_=text;busy_=true;progress_=0;status_="Sending…";emit changed();
    emit request("files-send",{{"channel",channel_},{"files",selected()},{"text",text},{"nonce",nonce_}});return true;
}
void ChatFiles::cancel(){if(busy_)emit request("files-cancel",{});else if(uncertain_){uncertain_=false;nonce_.clear();status_.clear();emit changed();}}
void ChatFiles::receive(QVariantMap event){
    if(event["kind"]=="downloads"){
        jobs_=event["rows"].toList();
        for(const auto& value:jobs_){const auto row=value.toMap();const auto id=row["id"].toString();if(row["state"]!="moving"&&moving_.value(id)=="acknowledging")moving_.remove(id);}
        emit changed();QTimer::singleShot(0,this,&ChatFiles::relocateDownloads);return;
    }
    if(event["nonce"]!=nonce_)return;
    progress_=event["progress"].toDouble();status_=event["status"].toString();
    if(event["finished"].toBool()){
        busy_=false;uncertain_=event["uncertain"].toBool();
        if(event["ok"].toBool()){selections_.remove(uploadChannel_);emit sent(uploadChannel_,text_);nonce_.clear();status_.clear();uncertain_=false;}
    }
    emit changed();
}
void ChatFiles::control(QString id,QString action){
    if(action=="open")for(const auto& v:jobs_){const auto row=v.toMap();if(row["id"]==id&&row["state"]=="done"){emit folderRequested(row["folder"].toString());return;}}
    emit request("files-control",{{"id",id},{"action",action}});
}
void ChatFiles::setDownloadDestination(QString id,QString folder){
    preparing_.remove(id);
    emit request("files-control",{{"id",id},{"action","destination"},{"folder",folder}});
}
QVariantList ChatFiles::downloads() const {
    auto rows=jobs_;
    if(!fileService_||!fileService_->available())for(auto& value:rows){auto row=value.toMap();row["canChooseDestination"]=false;value=row;}
    return rows;
}
void ChatFiles::setFilesService(FilesService* service){
    fileService_=service;
    connect(service,&FilesService::changed,this,[this]{
        const auto active=moving_;
        for(auto it=active.cbegin();it!=active.cend();++it)for(const auto& value:fileService_->tasks()){
            const auto task=value.toMap();if(task["id"]!=it.value()||!task["terminal"].toBool())continue;
            moving_[it.key()]="acknowledging";
            const auto raw=task["destination"].toString();const auto path=QUrl(raw).isLocalFile()?QUrl(raw).toLocalFile():raw;
            emit request("files-control",{{"id",it.key()},{"action",task["state"]=="done"?"moved":"move-failed"},{"path",path}});
            break;
        }
    });
}
void ChatFiles::relocateDownloads(){
    if(!fileService_||!fileService_->available())return;
    const auto snapshot=jobs_;
    for(const auto& value:snapshot){const auto job=value.toMap();const auto id=job["id"].toString();
        if(moving_.contains(id))continue;
        if(job["state"]=="moving"){
            preparing_.remove(id);moving_[id]="starting";
            moving_[id]=fileService_->start("rename",{job["path"].toString()},job["moveTarget"].toString());
        }else if(job["state"]=="done"&&!job["moveFailed"].toBool()&&!preparing_.contains(id)){
            const auto folder=job["destination"].toString(),path=job["path"].toString();
            if(folder.isEmpty()||path.isEmpty()||QFileInfo(path).canonicalPath()==QFileInfo(folder).canonicalFilePath())continue;
            if(!QFileInfo(folder).isDir()||!QFileInfo(path).isFile()){emit request("files-control",{{"id",id},{"action","move-failed"}});continue;}
            const QFileInfo name(path);auto target=QDir(folder).filePath(name.fileName());int suffix=1;
            while(QFile::exists(target))target=QDir(folder).filePath(name.completeBaseName()+" ("+QString::number(suffix++)+")"+(name.suffix().isEmpty()?QString():"."+name.suffix()));
            preparing_.insert(id);
            // The provider persists the target before acknowledging the move.
            emit request("files-control",{{"id",id},{"action","prepare-move"},{"path",target}});
        }
    }
}
}
