#include "CratesInstaller.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QSysInfo>
#include <QRegularExpression>
namespace trainer {
namespace {
bool canRun(const QVariantMap& record) {
    const auto cpu=QSysInfo::currentCpuArchitecture();
    const auto arch=cpu=="arm64"?QString("aarch64"):cpu;
    return record["architecture"]==arch;
}
}
CratesInstaller::CratesInstaller(QObject* p):QObject(p) {
    inventoryRefresh_.setSingleShot(true);inventoryRefresh_.setInterval(100);
    connect(&inventoryWatch_,&QFileSystemWatcher::directoryChanged,this,[this]{inventoryRefresh_.start();});
    connect(&inventoryRefresh_,&QTimer::timeout,this,[this]{watchInventory();readInventory();});
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
    inventoryRefresh_.stop();
    if(!inventoryWatch_.directories().isEmpty())inventoryWatch_.removePaths(inventoryWatch_.directories());
    helper_=helper;registry_=registry;root_=root;
    QFile f(registry);if(f.size()<1024*1024&&f.open(QIODevice::ReadOnly))packages_=QJsonDocument::fromJson(f.readAll()).object()["packages"].toArray().toVariantList();
    QFile jobs(root_+"/jobs.json");if(jobs.size()<1024*1024&&jobs.open(QIODevice::ReadOnly)){
        jobs_=QJsonDocument::fromJson(jobs.readAll()).array().toVariantList();
        for(auto& v:jobs_){auto r=v.toMap();if(!r["terminal"].toBool()){r["terminal"]=true;r["state"]="failed";r["detail"]="Interrupted · Retry to continue";v=r;}}
    }
    watchInventory();readInventory();emit changed();
}
void CratesInstaller::watchInventory(){
    if(root_.isEmpty())return;
    // Watch directories: the external installer atomically replaces receipts.
    // The store parent also covers first installation / recreation of active/.
    for(const auto& path:QStringList{QFileInfo(root_).absolutePath(),root_,root_+"/active"})
        if(QFileInfo(path).isDir()&&!inventoryWatch_.directories().contains(path))inventoryWatch_.addPath(path);
}
void CratesInstaller::readInventory(){
    QMap<QString,QVariantMap> next;QDir dir(root_+"/active");
    if(root_.isEmpty()){inventory_.clear();return;}
    for(const auto& name:dir.entryList({"*.json"},QDir::Files)){QFile f(dir.filePath(name));if(f.size()>1024*1024||!f.open(QIODevice::ReadOnly))continue;
        const auto r=QJsonDocument::fromJson(f.readAll()).object().toVariantMap();next[r["id"].toString()]=r;}
    if(next==inventory_)return;
    inventory_=next;emit inventoryChanged();emit changed();
}
void CratesInstaller::useCatalogue(const QVariantList& packages,const QString& registry){packages_=packages;registry_=registry;emit changed();}
void CratesInstaller::setOnline(bool online){if(online_==online)return;online_=online;emit changed();}
QVariantList CratesInstaller::installed() const {QVariantList out;for(const auto& r:inventory_)out<<r;return out;}
QVariantList CratesInstaller::activities() const {
    QVariantList out;QSet<QString> seen,ambiguous;
    const QRegularExpression id("^[a-z0-9][a-z0-9._-]{0,79}$"),appId("^[a-zA-Z0-9_.-]{1,128}$"),protocol("^[a-zA-Z0-9_.-]{1,64}$"),hash("^[0-9a-f]{64}$");
    for(const auto& r:inventory_) {
        const auto project=r["id"].toString(),sha=r["sha256"].toString(),object=r["object"].toString();
        if(r["kind"]!="game"||!id.match(project).hasMatch()||!hash.match(sha).hasMatch()||object!=project+"-"+sha)continue;
        const QDir base(root_+"/objects/"+object);
        QFile f(base.filePath("traineros-activity.json"));
        if(QFileInfo(base.path()).isSymLink()||QFileInfo(f).isSymLink()||f.size()>4096||!f.open(QIODevice::ReadOnly))continue;
        const auto m=QJsonDocument::fromJson(f.readAll()).object();const auto app=m["app"].toString();
        if(m["schema"]!=1||m["seats"]!=2||!appId.match(app).hasMatch()||!protocol.match(m["protocol"].toString()).hasMatch())continue;
        if(seen.contains(app)){ambiguous.insert(app);continue;}seen.insert(app);
        out<<QVariantMap{{"app",app},{"protocol",m["protocol"].toString()},{"title",r["title"]},{"local","crates:"+project}};
    }
    for(qsizetype i=out.size();i-->0;)if(ambiguous.contains(out[i].toMap()["app"].toString()))out.removeAt(i);
    return out;
}
QVariantMap CratesInstaller::state(const QString& id) const {
    QVariantList releases;
    for(const auto& v:packages_){
        auto r=v.toMap();
        if(r["id"]!=id || r["withdrawn"].toBool() || !canRun(r))continue;
        r["sizeBytes"]=r["size"];r["canRun"]=true;releases<<r;
    }
    const auto installed=inventory_.value(id);const auto latest=releases.value(0).toMap();
    QVariantMap out{{"installedVersion",installed["version"]},{"availableVersion",latest["version"]},{"packageReleases",releases},
        {"canInstall",online_&&!releases.isEmpty()&&!helper_.isEmpty()&&latest["canRun"].toBool()},
        {"offline",!online_},{"updateAvailable",online_&&!installed.isEmpty()&&!latest.isEmpty()&&latest["canRun"].toBool()&&installed["version"]!=latest["version"]}};
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
    if(operation!="remove"&&!online_)return;
    QVariantMap record;for(const auto& p:packages_){auto r=p.toMap();if(r["id"]==id&&!r["withdrawn"].toBool()&&canRun(r)&&(version.isEmpty()||r["version"]==version)){record=r;break;}}
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
        if(e.contains("error")){r["state"]=r["cancelling"].toBool()?"cancelled":"failed";r["detail"]=r["cancelling"].toBool()?QString("Cancelled"):e["error"].toString();}
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
