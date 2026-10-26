#include "CratesInstaller.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QSysInfo>
#include <QRegularExpression>
#include <QVersionNumber>
#ifdef Q_OS_UNIX
#include <signal.h>
#endif
namespace trainer {
namespace {
bool canRun(const QVariantMap& record) {
    const auto cpu=QSysInfo::currentCpuArchitecture();
    const auto arch=cpu=="arm64"?QString("aarch64"):cpu;
    return record["architecture"]==arch;
}
bool newerRelease(const QString& installed,const QVariantList& releases) {
    if(installed.isEmpty()||releases.isEmpty())return false;
    const auto latest=releases.first().toMap()["version"].toString();
    if(latest==installed)return false;
    qsizetype oldEnd=0,newEnd=0;
    const auto oldVersion=QVersionNumber::fromString(installed,&oldEnd);
    const auto newVersion=QVersionNumber::fromString(latest,&newEnd);
    if(!oldVersion.isNull()&&!newVersion.isNull()&&oldEnd==installed.size()&&newEnd==latest.size())
        return QVersionNumber::compare(newVersion,oldVersion)>0;
    // Non-numeric versions use the reviewed catalogue order only when the
    // installed release is known. An unpublished build is never downgraded.
    for(qsizetype i=1;i<releases.size();++i)
        if(releases[i].toMap()["version"]==installed)return true;
    return false;
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
            if(code==75&&!r["cancelling"].toBool()) {r["state"]=r["yield"].toBool()?"queued":"paused";r["detail"]=r["state"]=="queued"?"Queued":"Paused";r.remove("pauseRequested");r.remove("yield");v=r;continue;}
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
        for(auto& v:jobs_){auto r=v.toMap();if(!r["terminal"].toBool()&&r["state"]!="paused"){r["terminal"]=true;r["state"]="failed";r["detail"]="Interrupted · Retry to continue";v=r;}}
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
void CratesInstaller::setOnline(bool online){if(online_==online)return;online_=online;emit changed();if(online_)pump();}
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
        const int seats=m["seats"].toInt(-1);
        if(m["schema"]!=1||!m["seats"].isDouble()||m["seats"].toDouble()!=seats||seats<2||seats>16||!appId.match(app).hasMatch()||!protocol.match(m["protocol"].toString()).hasMatch())continue;
        if(seen.contains(app)){ambiguous.insert(app);continue;}seen.insert(app);
        out<<QVariantMap{{"app",app},{"protocol",m["protocol"].toString()},{"seats",seats},{"title",r["title"]},{"local","crates:"+project}};
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
        {"offline",!online_},{"updateAvailable",online_&&!helper_.isEmpty()&&newerRelease(installed["version"].toString(),releases)}};
    for(const auto& v:jobs_){auto r=v.toMap();if(r["id"]==id){out["job"]=r;out["busy"]=!r["terminal"].toBool();}}
    return out;
}
QVariantList CratesInstaller::tasks() const {
    auto out=jobs_;for(auto& v:out){auto r=v.toMap();QVariantList a;
        const bool pending=r["state"]=="queued"||r["state"]=="paused";
        if(!r["terminal"].toBool()){
            if(pending){a<<QVariantMap{{"id","first"},{"label","Download first"}};
                a<<QVariantMap{{"id",r["state"]=="paused"?"resume":"pause"},{"label",r["state"]=="paused"?"Resume":"Pause"}};
                a<<QVariantMap{{"id","earlier"},{"label","Move up"}}<<QVariantMap{{"id","later"},{"label","Move down"}};
            } else if(r["stage"]=="Downloading"&&!r["pauseRequested"].toBool())a<<QVariantMap{{"id","pause"},{"label","Pause"}};
            a<<QVariantMap{{"id","cancel"},{"label","Cancel"}};
        }
        else if(r["state"]!="done")a<<QVariantMap{{"id","retry"},{"label","Retry"}};
        r["actions"]=a;r["reorderable"]=pending;r["source"]="Crates";r["kind"]="package";r["destination"]=root_;v=r;}return out;
}
void CratesInstaller::request(const QString& id,const QString& version,const QString& operation){
    if(operation!="remove"&&!online_)return;
    QVariantMap record;for(const auto& p:packages_){auto r=p.toMap();if(r["id"]==id&&!r["withdrawn"].toBool()&&canRun(r)&&(version.isEmpty()||r["version"]==version)){record=r;break;}}
    if(operation=="remove")record=inventory_.value(id);
    if(record.isEmpty()||state(id)["busy"].toBool())return;
    for(int i=jobs_.size()-1;i>=0;--i)if(jobs_[i].toMap()["id"]==id)jobs_.removeAt(i);
    jobs_<<QVariantMap{{"id",id},{"title",record["title"]},{"version",record["version"]},{"operation",operation},
        {"state","queued"},{"detail","Queued"},{"terminal",false},{"picture",record["picture"]},{"platform","Crates"},
        {"description",record["notes"]},{"total",record["size"]},{"received",0},{"sha256",record["sha256"]}};
    emit changed();pump();
}
void CratesInstaller::pump(){
    if(!active_.isEmpty())return;
    retry_.stop();
    for(auto& v:jobs_){auto r=v.toMap();if(r["state"]!="queued")continue;
        if(r["operation"]!="remove"&&!online_)continue;
        const auto guard=replacementGuard?replacementGuard():QString();
        if(!guard.isEmpty()){r["detail"]=guard;v=r;retry_.start();emit changed();return;}
        active_=r["id"].toString();r["state"]="running";r["stage"]="Preparing";r["detail"]="Preparing installation";v=r;buffer_.clear();
        worker_.start("/usr/bin/python3",{helper_,registry_,root_,r["operation"].toString(),active_,r["version"].toString()});emit changed();return;
    }
}
void CratesInstaller::queueAutomaticUpdates(){
    for(const auto& receipt:installed()){
        const auto id=receipt.toMap()["id"].toString();const auto current=state(id);
        const auto release=current["packageReleases"].toList().value(0).toMap();
        if(!current["updateAvailable"].toBool()||current["busy"].toBool()||release["compatibility"]!="supported")continue;
        const auto previous=current["job"].toMap();
        // A user's pause/cancel, or an installation failure, must not become a
        // retry loop on every catalogue refresh (also after shell restart).
        if(previous["version"]==release["version"]&&previous["sha256"]==release["sha256"]&&!previous["automaticDisabled"].toBool())continue;
        request(id,release["version"].toString());
        for(auto& value:jobs_){auto job=value.toMap();if(job["id"]==id){job["automatic"]=true;value=job;}}
    }
    emit changed();
}
void CratesInstaller::stopAutomaticUpdates(){
    for(auto& value:jobs_){auto job=value.toMap();
        if(job["automatic"].toBool()&&(job["state"]=="queued"||job["state"]=="paused")){
            job["state"]="cancelled";job["detail"]="Automatic updates disabled";job["terminal"]=true;job["automaticDisabled"]=true;value=job;
        }
    }
    emit changed();
}
void CratesInstaller::event(const QByteArray& bytes){
    auto e=QJsonDocument::fromJson(bytes).object();
    for(auto& v:jobs_){auto r=v.toMap();if(r["id"]!=active_)continue;
        if(e.contains("error")){r["state"]=r["cancelling"].toBool()?"cancelled":"failed";r["detail"]=r["cancelling"].toBool()?QString("Cancelled"):e["error"].toString();}
        else {r["stage"]=e["stage"].toString();r["detail"]=r["pauseRequested"].toBool()?QString("Pausing…"):e["stage"].toString();}
        if(e.contains("received"))r["received"]=e["received"].toVariant();
        if(e.contains("total"))r["total"]=e["total"].toVariant();
        if(e.contains("progress"))r["progress"]=e["progress"].toDouble();else r.remove("progress");v=r;}
    emit changed();
}
void CratesInstaller::command(const QString& id,const QString& command){
    if(command=="earlier"||command=="later"||command=="first") {
        QStringList ids;for(const auto& v:jobs_){const auto r=v.toMap();if(r["state"]=="queued"||r["state"]=="paused")ids<<r["id"].toString();}
        const int i=ids.indexOf(id);if(i<0)return;
        if(command=="first"){
            for(int n=0;n<jobs_.size();++n)if(jobs_[n].toMap()["id"]==id){jobs_.prepend(jobs_.takeAt(n));break;}
            for(auto& v:jobs_){auto r=v.toMap();if(r["id"]==id){r["state"]="queued";r["detail"]="Queued first";v=r;}}
            if(!active_.isEmpty()&&active_!=id){const auto active=active_;this->command(active,"pause");for(auto& v:jobs_){auto r=v.toMap();if(r["id"]==active&&r["pauseRequested"].toBool()){r["yield"]=true;v=r;}}}
            persistJobs();emit changed();pump();
        }else if(command=="earlier"&&i>0)moveBefore(id,ids[i-1]);
        else if(command=="later"&&i+1<ids.size())moveBefore(id,ids.value(i+2));
        return;
    }
    if(command=="pause"||command=="pause-all"||command=="resume"||command=="resume-all"){
        for(auto& v:jobs_){auto r=v.toMap();if((!command.endsWith("-all")&&r["id"]!=id)||r["terminal"].toBool())continue;
            if(command.startsWith("pause")){
                r.remove("yield"); // A later explicit pause wins over priority handoff.
                if(r["state"]=="queued"){r["state"]="paused";r["detail"]="Paused";}
#ifdef Q_OS_UNIX
                else if(r["id"]==active_&&r["stage"]=="Downloading"&&!r["pauseRequested"].toBool()&&worker_.processId()>0){
                    if(::kill(pid_t(worker_.processId()),SIGUSR1)==0){r["pauseRequested"]=true;r["detail"]="Pausing…";}
                }
#endif
            }else if(r["pauseRequested"].toBool())r["yield"]=true;
            else if(r["state"]=="paused"){r["state"]="queued";r["detail"]="Queued";}
            v=r;
        }emit changed();persistJobs();pump();return;
    }
    if(command=="retry"){for(const auto& v:jobs_){const auto r=v.toMap();if(r["id"]==id){request(id,r["version"].toString(),r["operation"].toString());return;}}}
    if(command!="cancel"&&command!="cancel-all")return;
    for(auto& v:jobs_){auto r=v.toMap();if((command!="cancel-all"&&r["id"]!=id)||r["terminal"].toBool())continue;
        if(r["id"]==active_){r["cancelling"]=true;r["detail"]="Cancelling…";worker_.terminate();}
        else {r["state"]="cancelled";r["detail"]="Cancelled";r["terminal"]=true;}
        v=r;}emit changed();
}
void CratesInstaller::moveBefore(const QString& id,const QString& before){
    if(id==before)return;int from=-1,to=jobs_.size();
    for(int i=0;i<jobs_.size();++i){const auto r=jobs_[i].toMap();const bool pending=r["state"]=="queued"||r["state"]=="paused";
        if(r["id"]==id&&pending)from=i;if(!before.isEmpty()&&r["id"]==before){if(!pending)return;to=i;}}
    if(from<0||(!before.isEmpty()&&to==jobs_.size()))return;
    auto row=jobs_.takeAt(from);if(to>from)--to;jobs_.insert(to,row);persistJobs();emit changed();
}
}
