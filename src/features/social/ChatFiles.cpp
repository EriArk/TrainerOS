#include "ChatFiles.h"
#include <QDateTime>
#include <QFileInfo>
#include <QUuid>
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
    if(changedAccount){selections_.clear();jobs_.clear();busy_=uncertain_=false;nonce_.clear();status_.clear();}
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
    if(event["kind"]=="downloads"){jobs_=event["rows"].toList();emit changed();return;}
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
}
