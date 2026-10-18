#include "CratesInstaller.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
namespace trainer {
CratesInstaller::CratesInstaller(QObject* p):QObject(p) {
    persist_.setSingleShot(true);persist_.setInterval(500);
    connect(&persist_,&QTimer::timeout,this,&CratesInstaller::persistJobs);
    connect(this,&CratesInstaller::changed,this,[this]{if(!persist_.isActive())persist_.start();});
    retry_.setInterval(1000);connect(&retry_,&QTimer::timeout,this,&CratesInstaller::pump);
    connect(&worker_,&QProcess::readyReadStandardOutput,this,[this]{
        buffer_+=worker_.readAllStandardOutput();
        if(buffer_.size()>1024*1024){worker_.terminate();return;}
        while(buffer_.contains('\n')){const auto n=buffer_.indexOf('\n');event(buffer_.left(n));buffer_.remove(0,n+1);}
    });
    connect(&worker_,&QProcess::readyReadStandardError,this,[this]{worker_.readAllStandardError();});
    connect(&worker_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){
        if(e!=QProcess::FailedToStart)return;
        event("{\"error\":\"Package worker could not start\"}");
        for(auto& v:jobs_){auto r=v.toMap();if(r["id"]==active_){r["terminal"]=true;v=r;}}
        active_.clear();emit changed();retry_.start();
    });
    connect(&worker_,&QProcess::finished,this,[this](int code,QProcess::ExitStatus status){
        for(auto& v:jobs_){auto r=v.toMap();if(r["id"]!=active_)continue;
            if(r["state"]!="failed") {r["state"]=code==0&&status==QProcess::NormalExit?"done":r["cancelling"].toBool()?"cancelled":"failed";
                r["detail"]=r["state"]=="done"?(r["operation"]=="remove"?"Removed · user data kept":"Installed"):r["state"]=="cancelled"?"Cancelled":"Installation interrupted; previous version kept";}
            r["terminal"]=true;r.remove("progress");v=r;
        }
        active_.clear();readInventory();emit changed();QTimer::singleShot(0,this,&CratesInstaller::pump);
    });
}
CratesInstaller::~CratesInstaller(){if(worker_.state()!=QProcess::NotRunning){worker_.terminate();if(!worker_.waitForFinished(3000)){worker_.kill();worker_.waitForFinished();}}persistJobs();}
void CratesInstaller::persistJobs(){
    if(root_.isEmpty())return;QDir().mkpath(root_);QSaveFile file(root_+"/jobs.json");
    if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(QJsonArray::fromVariantList(jobs_)).toJson(QJsonDocument::Compact));file.commit();}
}
void CratesInstaller::configure(const QString& helper,const QString& registry,const QString& root) {
    helper_=helper;registry_=registry;root_=root;
    QFile f(registry);if(f.size()<1024*1024&&f.open(QIODevice::ReadOnly))packages_=QJsonDocument::fromJson(f.readAll()).object()["packages"].toArray().toVariantList();
    QFile jobs(root_+"/jobs.json");if(jobs.size()<1024*1024&&jobs.open(QIODevice::ReadOnly)){
        jobs_=QJsonDocument::fromJson(jobs.readAll()).array().toVariantList();
        for(auto& v:jobs_){auto r=v.toMap();if(!r["terminal"].toBool()){r["terminal"]=true;r["state"]="failed";r["detail"]="Interrupted · Retry to continue";v=r;}}
    }
    readInventory();emit changed();
}
void CratesInstaller::readInventory(){
    inventory_.clear();QDir dir(root_+"/active");
    for(const auto& name:dir.entryList({"*.json"},QDir::Files)){QFile f(dir.filePath(name));if(f.size()>1024*1024||!f.open(QIODevice::ReadOnly))continue;
        const auto r=QJsonDocument::fromJson(f.readAll()).object().toVariantMap();inventory_[r["id"].toString()]=r;}
    emit inventoryChanged();
}
QVariantList CratesInstaller::installed() const {QVariantList out;for(const auto& r:inventory_)out<<r;return out;}
QVariantMap CratesInstaller::state(const QString& id) const {
    QVariantList releases;for(const auto& v:packages_)if(v.toMap()["id"]==id){auto r=v.toMap();r["sizeBytes"]=r["size"];releases<<r;}
    const auto installed=inventory_.value(id);const auto latest=releases.value(0).toMap();
    QVariantMap out{{"installedVersion",installed["version"]},{"availableVersion",latest["version"]},{"packageReleases",releases},
        {"canInstall",!releases.isEmpty()&&!helper_.isEmpty()},{"updateAvailable",!installed.isEmpty()&&!latest.isEmpty()&&installed["version"]!=latest["version"]}};
    for(const auto& v:jobs_){auto r=v.toMap();if(r["id"]==id){out["job"]=r;out["busy"]=!r["terminal"].toBool();}}
    return out;
}
QVariantList CratesInstaller::tasks() const {
    auto out=jobs_;for(auto& v:out){auto r=v.toMap();QVariantList a;
        if(!r["terminal"].toBool())a<<QVariantMap{{"id","cancel"},{"label","Cancel"}};
        else if(r["state"]!="done")a<<QVariantMap{{"id","retry"},{"label","Retry"}};
        r["actions"]=a;r["kind"]="package";v=r;}return out;
}
void CratesInstaller::request(const QString& id,const QString& version,const QString& operation){
    QVariantMap record;for(const auto& p:packages_){auto r=p.toMap();if(r["id"]==id&&(version.isEmpty()||r["version"]==version)){record=r;break;}}
    if(operation=="remove")record=inventory_.value(id);
    if(record.isEmpty()||state(id)["busy"].toBool())return;
    for(int i=jobs_.size()-1;i>=0;--i)if(jobs_[i].toMap()["id"]==id)jobs_.removeAt(i);
    jobs_<<QVariantMap{{"id",id},{"title",record["title"]},{"version",record["version"]},{"operation",operation},
        {"state","queued"},{"detail","Queued"},{"terminal",false},{"picture",record["picture"]},{"platform","Crates"}};
    emit changed();pump();
}
void CratesInstaller::pump(){
    if(!active_.isEmpty())return;
    retry_.stop();
    for(auto& v:jobs_){auto r=v.toMap();if(r["state"]!="queued")continue;
        const auto guard=replacementGuard?replacementGuard():QString();
        if(!guard.isEmpty()){r["detail"]=guard;v=r;retry_.start();emit changed();return;}
        active_=r["id"].toString();r["state"]="running";r["detail"]="Preparing installation";v=r;buffer_.clear();
        worker_.start("/usr/bin/python3",{helper_,registry_,root_,r["operation"].toString(),active_,r["version"].toString()});emit changed();return;
    }
}
void CratesInstaller::event(const QByteArray& bytes){
    auto e=QJsonDocument::fromJson(bytes).object();
    for(auto& v:jobs_){auto r=v.toMap();if(r["id"]!=active_)continue;
        if(e.contains("error")){r["state"]="failed";r["detail"]=e["error"].toString();}
        else r["detail"]=e["stage"].toString();
        if(e.contains("progress"))r["progress"]=e["progress"].toDouble();else r.remove("progress");v=r;}
    emit changed();
}
void CratesInstaller::command(const QString& id,const QString& command){
    if(command=="retry"){for(const auto& v:jobs_){const auto r=v.toMap();if(r["id"]==id){request(id,r["version"].toString(),r["operation"].toString());return;}}}
    if(command!="cancel"&&command!="cancel-all")return;
    for(auto& v:jobs_){auto r=v.toMap();if((command!="cancel-all"&&r["id"]!=id)||r["terminal"].toBool())continue;
        if(r["id"]==active_){r["cancelling"]=true;r["detail"]="Cancelling…";worker_.terminate();}
        else {r["state"]="cancelled";r["detail"]="Cancelled";r["terminal"]=true;}v=r;}emit changed();
}
}
