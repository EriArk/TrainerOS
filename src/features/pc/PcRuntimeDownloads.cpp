#include "PcGamesController.h"
#include "PcRuntime.h"
#include <QJsonDocument>
#include <QSaveFile>
#include <QSysInfo>
#include <QDir>

namespace trainer {
void PcGamesController::setupRuntimeDownloads(){
    connect(&runtimeDownload_,&QProcess::readyReadStandardOutput,this,&PcGamesController::readRuntimeDownload);
    connect(&runtimeDownload_,&QProcess::finished,this,[this](int code,QProcess::ExitStatus){
        readRuntimeDownload();busy_=false;stage_="runtimes";focus_=0;
        runtimeRows_=pc::runtimes(directory_,QDir::homePath(),QSysInfo::currentCpuArchitecture());
        if(runtimeOperation_=="install") {
            runtimeState_=runtimeCancelled_?"cancelled":code==0?"done":"failed";
            if(code==0)runtimeDetail_="Cached. Select this version explicitly; existing games keep their runtime.";
            else if(runtimeCancelled_)runtimeDetail_="Cancelled. Completed cache is retained; retry is available.";
            else if(runtimeDetail_.isEmpty())runtimeDetail_="Runtime download failed.";
            message_=runtimeDetail_;
        }else {stage_="runtime-catalogue";if(code!=0)message_="Could not refresh the catalogue. Previously loaded versions can be retried.";}
        emit changed();
    });
    connect(&runtimeDownload_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e!=QProcess::FailedToStart)return;busy_=false;runtimeState_="failed";runtimeDetail_=message_="Runtime download support could not start.";emit changed();});
}
void PcGamesController::readRuntimeDownload(){
    runtimeBuffer_+=runtimeDownload_.readAllStandardOutput();
    if(runtimeBuffer_.size()>2*1024*1024){runtimeDownload_.terminate();runtimeBuffer_.clear();runtimeDetail_="Runtime helper response exceeded its limit.";return;}
    while(runtimeBuffer_.contains('\n')) {
        const int split=runtimeBuffer_.indexOf('\n');const auto o=QJsonDocument::fromJson(runtimeBuffer_.left(split)).object();runtimeBuffer_.remove(0,split+1);
        if(o.contains("catalogue")) {
            runtimeCatalogue_=o["catalogue"].toArray();QDir().mkpath(directory_+"/runtimes");QSaveFile f(directory_+"/runtimes/catalogue.json");
            if(f.open(QIODevice::WriteOnly)){f.write(QJsonDocument(runtimeCatalogue_).toJson());f.commit();}
            message_="Official GE-Proton builds for this architecture. UMU prepares its required Steam runtime on first launch; internet is needed once.";
        }
        if(o.contains("received")){runtimeReceived_=o["received"].toInteger();runtimeTotal_=o["total"].toInteger();progress_=runtimeTotal_>0?double(runtimeReceived_)/runtimeTotal_:0;}
        if(o.contains("detail"))runtimeDetail_=message_=o["detail"].toString();
        if(o.contains("error"))runtimeDetail_=message_=o["error"].toString();
    }
    emit changed();
}
void PcGamesController::runtimeDownload(const QString& operation,const QJsonObject& item){
    if(busy_)return;
    const QString helper="/var/opt/traineros/pc/runtimes.py";
    if(QSysInfo::kernelType()!="linux"||!QFileInfo::exists(helper)){message_="Runtime downloads are available with the Linux PC support package.";emit changed();return;}
    QFile cached(directory_+"/runtimes/catalogue.json");if(runtimeCatalogue_.isEmpty()&&cached.open(QIODevice::ReadOnly))runtimeCatalogue_=QJsonDocument::fromJson(cached.read(2*1024*1024)).array();
    runtimeOperation_=operation;runtimeCancelled_=false;runtimeBuffer_.clear();runtimeDetail_.clear();busy_=true;progress_=0;runtimeReceived_=runtimeTotal_=0;
    QStringList args{helper,operation};
    if(operation=="catalogue")args<<QSysInfo::currentCpuArchitecture();
    else {runtimeItem_=item;runtimeState_="running";args<<directory_<<QString::fromUtf8(QJsonDocument(item).toJson(QJsonDocument::Compact));}
    message_=operation=="catalogue"?"Checking official runtime versions…":"Downloading into the shared runtime cache…";
    runtimeDownload_.start("/usr/bin/python3",args);emit changed();
}
QVariantList PcGamesController::runtimeTasks() const {
    if(runtimeState_.isEmpty())return {};
    const bool running=runtimeDownload_.state()!=QProcess::NotRunning&&runtimeOperation_=="install";
    QVariantList actions{QVariantMap{{"id","open"},{"label","Runtime versions"},{"enabled",true}}};
    if(running)actions<<QVariantMap{{"id","cancel"},{"label","Cancel"},{"enabled",true}};
    else if(runtimeState_!="done")actions<<QVariantMap{{"id","retry"},{"label","Retry"},{"enabled",!busy_}};
    return {QVariantMap{{"id","runtime"},{"title",runtimeItem_["title"].toString()},{"detail",runtimeDetail_},{"state",runtimeState_},
        {"terminal",!running},{"received",runtimeReceived_},{"total",runtimeTotal_},{"progress",runtimeTotal_>0?double(runtimeReceived_)/runtimeTotal_:0},{"actions",actions}}};
}
void PcGamesController::runtimeTaskCommand(const QString& command){
    if(command=="cancel"||command=="cancel-all") {runtimeCancelled_=true;runtimeDownload_.terminate();}
    else if(command=="retry")runtimeDownload("install",runtimeItem_);
    else if(command=="open"){open_=true;if(!busy_){runtimeReturn_=editing()?"launch-settings":"confirm";stage_="runtimes";runtimeRows_=pc::runtimes(directory_,QDir::homePath(),QSysInfo::currentCpuArchitecture());focus_=0;}emit changed();}
}
}
