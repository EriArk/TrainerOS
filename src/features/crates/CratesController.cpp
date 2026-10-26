#include "CratesController.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>
#include <QDebug>
#include <QSettings>
#include <algorithm>

namespace trainer {
CratesController::CratesController(QObject* parent):QObject(parent) {
    connect(&installer_,&CratesInstaller::changed,this,[this]{emit projectChanged();emit changed();});
    QFile data(":/crates/showcase.json");
    if(data.open(QIODevice::ReadOnly))crates_=QJsonDocument::fromJson(data.readAll()).array().toVariantList();
    QFile catalogue(":/crates/catalogue.json");
    if(catalogue.open(QIODevice::ReadOnly))loadCatalogue(catalogue.readAll());
    automaticCheck_.setInterval(15*60*1000);
    connect(&automaticCheck_,&QTimer::timeout,this,&CratesController::checkAutomaticUpdates);
    opening_.setSingleShot(true);
    connect(&opening_,&QTimer::timeout,this,[this]{opened_=true;emit changed();});
    catalogueDeadline_.setSingleShot(true);catalogueDeadline_.setInterval(20000);
    connect(&catalogueDeadline_,&QTimer::timeout,this,[this]{catalogueWorker_.kill();});
    connect(&catalogueWorker_,&QProcess::readyReadStandardOutput,this,[this]{
        catalogueBuffer_+=catalogueWorker_.readAllStandardOutput();
        if(catalogueBuffer_.size()>12*1024*1024)catalogueWorker_.kill();
    });
    connect(&catalogueWorker_,&QProcess::readyReadStandardError,this,[this]{catalogueWorker_.readAllStandardError();});
    connect(&catalogueWorker_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){catalogueDeadline_.stop();syncStatus_="Catalogue unavailable";installer_.setOnline(false);emit changed();}
    });
    connect(&catalogueWorker_,&QProcess::finished,this,[this](int code,QProcess::ExitStatus status){
        catalogueDeadline_.stop();
        const auto response=QJsonDocument::fromJson(catalogueBuffer_).object();
        const bool applied=code==0&&status==QProcess::NormalExit&&!response.contains("error")&&applyCatalogue(response);
        if(networkSync_){
            installer_.setOnline(applied);
            if(applied&&automaticUpdates_)installer_.queueAutomaticUpdates();
            syncStatus_=applied?QString():QString("Offline · saved catalogue");
            qInfo()<<"Crates catalogue"<<(applied?response["result"].toString():QString("unavailable"))<<"revision"<<response["revision"].toInteger();
        }
        emit changed();
        if(pendingSync_&&(catalogueActive_||automaticUpdates_)){pendingSync_=false;refreshCatalogue(true);}
    });
}
CratesController::~CratesController(){catalogueWorker_.disconnect(this);if(catalogueWorker_.state()!=QProcess::NotRunning){catalogueWorker_.kill();catalogueWorker_.waitForFinished(1000);}}
void CratesController::configureCatalogue(const QString& helper,const QString& cache){
    catalogueHelper_=helper;catalogueCache_=cache;installer_.setOnline(false);
    // Loading the last authenticated snapshot never performs a network request.
    if(QFileInfo::exists(cache+"/current.json"))refreshCatalogue(false);
    automaticUpdates_=QSettings().value("crates/automaticUpdates",false).toBool();
    emit automaticUpdatesChanged();
    if(automaticUpdates_){automaticCheck_.start();QTimer::singleShot(30000,this,&CratesController::checkAutomaticUpdates);}
}
void CratesController::setAutomaticUpdates(bool enabled){
    if(automaticUpdates_==enabled)return;
    QSettings settings;settings.setValue("crates/automaticUpdates",enabled);settings.sync();
    if(settings.status()!=QSettings::NoError){emit preferenceError("Could not save automatic update preference.");return;}
    automaticUpdates_=enabled;
    if(enabled){automaticCheck_.start();checkAutomaticUpdates();}
    else {automaticCheck_.stop();installer_.stopAutomaticUpdates();}
    emit automaticUpdatesChanged();emit changed();
}
void CratesController::checkAutomaticUpdates(){
    if(!automaticUpdates_||catalogueHelper_.isEmpty())return;
    if(catalogueWorker_.state()!=QProcess::NotRunning){if(!networkSync_)pendingSync_=true;return;}
    refreshCatalogue(true);
}
void CratesController::enter(){
    if(catalogueActive_)return;catalogueActive_=true;
    if(catalogueHelper_.isEmpty())return;
    if(catalogueWorker_.state()!=QProcess::NotRunning){if(!networkSync_)pendingSync_=true;return;}
    refreshCatalogue(true);
}
void CratesController::refreshCatalogue(bool network){
    networkSync_=network;catalogueBuffer_.clear();
    if(network){syncStatus_="Refreshing…";emit changed();}
    catalogueWorker_.start("/usr/bin/python3",{catalogueHelper_,network?"sync":"load",catalogueCache_});
    catalogueDeadline_.start();
}
bool CratesController::applyCatalogue(const QJsonObject& response){
    const auto nextCrates=response["crates"].toArray().toVariantList();
    if(nextCrates.size()!=6)return false;
    QStringList ids;for(const auto& c:nextCrates)ids<<c.toMap()["id"].toString();
    CratesCatalogue next;
    if(!CratesCatalogue::read(QJsonDocument(response["catalogue"].toObject()).toJson(),ids,next,catalogueCache_+"/assets"))return false;
    const auto crateId=current()["id"].toString(),projectId=project()["id"].toString();
    crates_=nextCrates;focus_=std::max(0,int(ids.indexOf(crateId)));catalogue_=std::move(next);
    if(detailOpen_&&!catalogue_.contents.value(crateId).contains(projectId))closeDetail();
    screenshotIndex_=0;
    installer_.useCatalogue(response["packages"].toArray().toVariantList(),catalogueCache_+"/current.json");
    emit catalogueChanged();emit projectChanged();emit changed();return true;
}
void CratesController::select(int index) {
    if(opened_ || opening() || index<0 || index>=crates_.size() || focus_==index)return;
    focus_=index;emit catalogueChanged();emit projectChanged();emit changed();
}
void CratesController::open(int index,bool reducedMotion) {
    if(opened_ || index<0 || index>=crates_.size())return;
    // A restored Crates root stays offline at boot. Opening its first crate is
    // also an explicit user entry, without requiring a detour to another tab.
    enter();
    if(opening()){opening_.stop();opened_=true;}
    else {focus_=index;if(reducedMotion||reducedMotion_)opened_=true;else opening_.start(210);}
    emit catalogueChanged();emit projectChanged();emit changed();
}
bool CratesController::loadCatalogue(const QByteArray& data) {
    QStringList ids;for(const auto& crate:crates_)ids<<crate.toMap()["id"].toString();
    CratesCatalogue next;
    if(!CratesCatalogue::read(data,ids,next))return false;
    catalogue_=std::move(next);closeDetail();emit catalogueChanged();emit projectChanged();emit changed();return true;
}
int CratesController::projectCount() const {return catalogue_.contents.value(current()["id"].toString()).size();}
int CratesController::projectIndex() const {
    const auto id=current()["id"].toString();
    return std::max(0,int(catalogue_.contents.value(id).indexOf(selectedProjects_.value(id))));
}
QVariantMap CratesController::project() const {
    auto out=catalogue_.projects.value(catalogue_.contents.value(current()["id"].toString()).value(projectIndex()));
    const auto state=installer_.state(out["id"].toString());
    for(auto i=state.begin();i!=state.end();++i)out[i.key()]=i.value();
    if(!state["packageReleases"].toList().isEmpty())out["releases"]=state["packageReleases"];
    return out;
}
QString CratesController::primaryLabel() const {
    const auto p=project();if(p["busy"].toBool())return "View progress";
    if(p["updateAvailable"].toBool())return "Update";
    if(!p["installedVersion"].toString().isEmpty())return p["kind"]=="game"?"Play":"Open";
    if(p["offline"].toBool())return "Connect to download";
    return p["canInstall"].toBool()?"Download":"Package coming soon";
}
int CratesController::updateCount() const {int count=0;for(const auto& p:installer_.installed())if(installer_.state(p.toMap()["id"].toString())["updateAvailable"].toBool())++count;return count;}
void CratesController::updateAll(){
    if(!updateCount())return;
    for(const auto& p:installer_.installed()){
        const auto state=installer_.state(p.toMap()["id"].toString());
        if(state["updateAvailable"].toBool()&&state["packageReleases"].toList().value(0).toMap()["compatibility"]!="supported"){
            updatePrompt_=true;updateChoice_=0;emit changed();return;
        }
    }
    confirmUpdates(true);
}
void CratesController::confirmUpdates(bool accept){
    updatePrompt_=false;emit changed();if(!accept)return;
    for(const auto& p:installer_.installed()){const auto id=p.toMap()["id"].toString();if(installer_.state(id)["updateAvailable"].toBool())installer_.request(id);}
    emit downloadsRequested();
}
bool CratesController::primaryEnabled() const {const auto p=project();return p["busy"].toBool()||p["canInstall"].toBool()||!p["installedVersion"].toString().isEmpty();}
void CratesController::primary(){
    if(!detailOpen_||!primaryEnabled())return;const auto p=project();
    if(p["busy"].toBool()){emit downloadsRequested();return;}
    if(p["updateAvailable"].toBool()){requestPackage();return;}
    if(!p["installedVersion"].toString().isEmpty()){emit launchRequested(p["id"].toString(),p["kind"].toString());return;}
    requestPackage();
}
void CratesController::requestPackage(const QString& version){
    const auto p=project();const auto rows=p["releases"].toList();QVariantMap release;
    for(const auto& row:rows){const auto r=row.toMap();if(version.isEmpty()||r["version"]==version){release=r;break;}}
    if(release.isEmpty()||!p["canInstall"].toBool()||(release.contains("canRun")&&!release["canRun"].toBool()))return;
    if(release["compatibility"]=="untested"||release["compatibility"]=="incompatible"){
        pendingVersion_=release["version"].toString();actionsOpen_=true;actionIndex_=0;
        emit projectChanged();emit changed();return;
    }
    installer_.request(p["id"].toString(),release["version"].toString());emit downloadsRequested();
}
QVariantList CratesController::actions() const {
    if(!pendingVersion_.isEmpty())return {QVariantMap{{"id","confirm-install"},{"label","Install anyway · untested"}},QVariantMap{{"id","back"},{"label","Cancel"}}};
    if(confirmRemove_)return {QVariantMap{{"id","confirm-remove"},{"label","Remove · keep my data"}},QVariantMap{{"id","back"},{"label","Cancel"}}};
    const auto p=project();QVariantList a;
    if(p["updateAvailable"].toBool())a<<QVariantMap{{"id","update"},{"label","Update"}};
    if(!p["installedVersion"].toString().isEmpty())a<<QVariantMap{{"id","remove"},{"label","Remove…"}};
    return a;
}
void CratesController::showActions(){if(!detailOpen_||actions().isEmpty())return;actionsOpen_=!actionsOpen_;confirmRemove_=false;pendingVersion_.clear();actionIndex_=0;emit projectChanged();emit changed();}
void CratesController::activateAction(int i){
    const auto action=actions().value(i).toMap()["id"].toString();const auto id=project()["id"].toString();
    if(action=="remove"){confirmRemove_=true;actionIndex_=0;emit projectChanged();emit changed();return;}
    if(action=="update"){actionsOpen_=false;requestPackage();return;}
    if(action=="confirm-install")installer_.request(id,pendingVersion_);
    if(action=="confirm-remove")installer_.request(id,{},"remove");
    actionsOpen_=confirmRemove_=false;
    pendingVersion_.clear();
    if(action=="confirm-install"||action=="confirm-remove")emit downloadsRequested();
    emit projectChanged();emit changed();
}
QVariantList CratesController::projects() const {
    QVariantList rows;const auto ids=catalogue_.contents.value(current()["id"].toString());
    for(int i=page()*6;i<std::min(page()*6+6,int(ids.size()));++i) {
        auto row=catalogue_.projects.value(ids[i]);row["index"]=i;rows<<row;
    }
    return rows;
}
QVariantList CratesController::downloadTasks() const {
    auto rows=installer_.tasks();
    for(auto& value:rows){auto row=value.toMap();const auto project=catalogue_.projects.value(row["id"].toString());
        if(!project.isEmpty()){
            row["description"]=project["summary"].toString().isEmpty()?project["description"]:project["summary"];
            if(row["picture"].toString().isEmpty())row["picture"]=project["hero"];
        }value=row;
    }return rows;
}
void CratesController::selectProject(int index) {
    const auto id=current()["id"].toString();const auto ids=catalogue_.contents.value(id);
    if(!opened_ || detailOpen_ || index<0 || index>=ids.size())return;
    if(selectedProjects_.value(id)==ids[index])return;
    const int oldPage=page();selectedProjects_[id]=ids[index];
    if(page()!=oldPage)emit catalogueChanged();
    emit projectChanged();emit changed();
}
void CratesController::openProject(int index) {
    if(!opened_ || detailOpen_ || index<0 || index>=projectCount())return;
    selectProject(index);detailOpen_=true;screenshotIndex_=0;emit changed();
}
void CratesController::closeDetail(){detailOpen_=actionsOpen_=confirmRemove_=false;pendingVersion_.clear();emit changed();}
void CratesController::turnPage(int delta) {
    if(!opened_ || detailOpen_ || projectCount()==0)return;
    const int next=std::clamp(page()+delta,0,pageCount()-1);
    selectProject(std::min(next*6+projectIndex()%6,projectCount()-1));
}
void CratesController::selectScreenshot(int i){if(detailOpen_&&i>=0&&i<project()["screenshots"].toList().size()){screenshotIndex_=i;emit changed();}}
void CratesController::back(){
    if(updatePrompt_){confirmUpdates(false);return;}
    if(actionsOpen_){actionsOpen_=confirmRemove_=false;pendingVersion_.clear();emit projectChanged();emit changed();return;}
    if(detailOpen_){closeDetail();return;}
    opening_.stop();opened_=false;emit changed();
}
void CratesController::leave(){catalogueActive_=pendingSync_=false;if(updatePrompt_)confirmUpdates(false);if(opening()||detailOpen_){opening_.stop();closeDetail();}}
bool CratesController::dispatch(Action action) {
    if(updatePrompt_){
        if(action==Action::Back)confirmUpdates(false);
        else if(action==Action::Left||action==Action::Right){updateChoice_=action==Action::Right?1:0;emit changed();}
        else if(action==Action::Confirm)confirmUpdates(updateChoice_==1);
        return true;
    }
    if(!opened_&&action==Action::ContextMenu&&updateCount()>0){updateAll();return true;}
    if(action==Action::Back){if(!opened_&&!opening())return false;back();return true;}
    if(detailOpen_) {
        if(action==Action::ContextMenu)showActions();
        else if(actionsOpen_){
            if(action==Action::Up)actionIndex_=std::max(0,actionIndex_-1);
            if(action==Action::Down)actionIndex_=std::min(int(actions().size())-1,actionIndex_+1);
            if(action==Action::Confirm)activateAction(actionIndex_);
            emit changed();
        }
        else {
            if(action==Action::Confirm)primary();
            if(action==Action::Left)selectScreenshot(screenshotIndex_-1);
            if(action==Action::Right)selectScreenshot(screenshotIndex_+1);
            if(action==Action::Up||action==Action::ScrollUp)emit detailScrollRequested(-70);
            if(action==Action::Down||action==Action::ScrollDown)emit detailScrollRequested(70);
        }
        return true;
    }
    if(opened_) {
        if(action==Action::Confirm)openProject(projectIndex());
        else if(action==Action::ScrollUp)turnPage(-1);
        else if(action==Action::ScrollDown)turnPage(1);
        else if(projectCount()>0) {
            int i=projectIndex();
            if(action==Action::Left)--i;
            if(action==Action::Right)++i;
            if(action==Action::Up)i-=3;
            if(action==Action::Down)i=std::min(i+3,projectCount()-1);
            selectProject(i);
        }
        return true;
    }
    if(action==Action::Confirm){open(focus_);return true;}
    if(opening()||crates_.isEmpty())return true;
    int next=focus_;
    // Two featured crates above four thematic crates; vertical moves retain side.
    if(action==Action::Left)next=std::max(focus_<2?0:2,focus_-1);
    if(action==Action::Right)next=std::min(focus_<2?1:int(crates_.size())-1,focus_+1);
    if(action==Action::Down&&focus_<2)next=2+focus_*2;
    if(action==Action::Up&&focus_>=2)next=(focus_-2)/2;
    select(next);return true;
}
}
