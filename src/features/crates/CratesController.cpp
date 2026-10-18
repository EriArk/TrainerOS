#include "CratesController.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <algorithm>

namespace trainer {
CratesController::CratesController(QObject* parent):QObject(parent) {
    connect(&installer_,&CratesInstaller::changed,this,[this]{emit projectChanged();emit changed();});
    QFile data(":/crates/showcase.json");
    if(data.open(QIODevice::ReadOnly))crates_=QJsonDocument::fromJson(data.readAll()).array().toVariantList();
    QFile catalogue(":/crates/catalogue.json");
    if(catalogue.open(QIODevice::ReadOnly))loadCatalogue(catalogue.readAll());
    opening_.setSingleShot(true);
    connect(&opening_,&QTimer::timeout,this,[this]{opened_=true;emit changed();});
}
void CratesController::select(int index) {
    if(opened_ || opening() || index<0 || index>=crates_.size() || focus_==index)return;
    focus_=index;emit catalogueChanged();emit projectChanged();emit changed();
}
void CratesController::open(int index,bool reducedMotion) {
    if(opened_ || index<0 || index>=crates_.size())return;
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
    if(!p["installedVersion"].toString().isEmpty())return p["kind"]=="game"?"Play":"Open";
    return p["canInstall"].toBool()?"Download":"Package coming soon";
}
int CratesController::updateCount() const {int count=0;for(const auto& p:installer_.installed())if(installer_.state(p.toMap()["id"].toString())["updateAvailable"].toBool())++count;return count;}
void CratesController::updateAll(){for(const auto& p:installer_.installed()){const auto id=p.toMap()["id"].toString();if(installer_.state(id)["updateAvailable"].toBool())installer_.request(id);}emit downloadsRequested();}
bool CratesController::primaryEnabled() const {const auto p=project();return p["busy"].toBool()||p["canInstall"].toBool()||!p["installedVersion"].toString().isEmpty();}
void CratesController::primary(){
    if(!detailOpen_||!primaryEnabled())return;const auto p=project();
    if(p["busy"].toBool()){emit downloadsRequested();return;}
    if(!p["installedVersion"].toString().isEmpty()){emit launchRequested(p["id"].toString(),p["kind"].toString());return;}
    installer_.request(p["id"].toString());emit downloadsRequested();
}
void CratesController::installSelectedVersion(){
    const auto p=project();const auto r=p["releases"].toList().value(releaseIndex_).toMap();
    if(!versionsOpen_||!p["canInstall"].toBool()||p["busy"].toBool()||r.isEmpty())return;
    installer_.request(p["id"].toString(),r["version"].toString());showVersions();emit downloadsRequested();
}
QVariantList CratesController::actions() const {
    if(confirmRemove_)return {QVariantMap{{"id","confirm-remove"},{"label","Remove · keep my data"}},QVariantMap{{"id","back"},{"label","Cancel"}}};
    const auto p=project();QVariantList a;
    if(p["updateAvailable"].toBool())a<<QVariantMap{{"id","update"},{"label","Update"}};
    if(!p["releases"].toList().isEmpty())a<<QVariantMap{{"id","versions"},{"label","Versions"}};
    if(!p["installedVersion"].toString().isEmpty())a<<QVariantMap{{"id","remove"},{"label","Remove…"}};
    return a;
}
void CratesController::showActions(){if(!detailOpen_||actions().isEmpty())return;actionsOpen_=!actionsOpen_;confirmRemove_=false;actionIndex_=0;emit projectChanged();emit changed();}
void CratesController::activateAction(int i){
    const auto action=actions().value(i).toMap()["id"].toString();const auto id=project()["id"].toString();
    if(action=="remove"){confirmRemove_=true;actionIndex_=0;emit projectChanged();emit changed();return;}
    if(action=="update")installer_.request(id);
    if(action=="confirm-remove")installer_.request(id,{},"remove");
    actionsOpen_=confirmRemove_=false;
    if(action=="versions")showVersions();
    if(action=="update"||action=="confirm-remove")emit downloadsRequested();
    emit projectChanged();emit changed();
}
QVariantList CratesController::projects() const {
    QVariantList rows;const auto ids=catalogue_.contents.value(current()["id"].toString());
    for(int i=page()*6;i<std::min(page()*6+6,int(ids.size()));++i) {
        auto row=catalogue_.projects.value(ids[i]);row["index"]=i;rows<<row;
    }
    return rows;
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
    selectProject(index);detailOpen_=true;versionsOpen_=false;screenshotIndex_=releaseIndex_=0;emit changed();
}
void CratesController::closeDetail(){detailOpen_=versionsOpen_=actionsOpen_=confirmRemove_=false;emit changed();}
void CratesController::turnPage(int delta) {
    if(!opened_ || detailOpen_ || projectCount()==0)return;
    const int next=std::clamp(page()+delta,0,pageCount()-1);
    selectProject(std::min(next*6+projectIndex()%6,projectCount()-1));
}
void CratesController::showVersions(){if(detailOpen_&&!project()["releases"].toList().isEmpty()){versionsOpen_=!versionsOpen_;emit changed();}}
void CratesController::selectRelease(int i){if(detailOpen_&&i>=0&&i<project()["releases"].toList().size()){releaseIndex_=i;emit changed();}}
void CratesController::selectScreenshot(int i){if(detailOpen_&&i>=0&&i<project()["screenshots"].toList().size()){screenshotIndex_=i;emit changed();}}
void CratesController::back(){
    if(actionsOpen_){actionsOpen_=confirmRemove_=false;emit projectChanged();emit changed();return;}
    if(versionsOpen_){versionsOpen_=false;emit changed();return;}
    if(detailOpen_){closeDetail();return;}
    opening_.stop();opened_=false;emit changed();
}
void CratesController::leave(){if(opening()||detailOpen_){opening_.stop();closeDetail();}}
bool CratesController::dispatch(Action action) {
    if(!opened_&&action==Action::ContextMenu&&updateCount()>0){updateAll();return true;}
    if(action==Action::Back){if(!opened_&&!opening())return false;back();return true;}
    if(detailOpen_) {
        if(action==Action::ContextMenu)showActions();
        else if(action==Action::LocalAction)showVersions();
        else if(actionsOpen_){
            if(action==Action::Up)actionIndex_=std::max(0,actionIndex_-1);
            if(action==Action::Down)actionIndex_=std::min(int(actions().size())-1,actionIndex_+1);
            if(action==Action::Confirm)activateAction(actionIndex_);
            emit changed();
        }
        else if(versionsOpen_) {
            if(action==Action::Up)selectRelease(releaseIndex_-1);
            if(action==Action::Down)selectRelease(releaseIndex_+1);
            if(action==Action::ScrollUp)emit detailScrollRequested(-70);
            if(action==Action::ScrollDown)emit detailScrollRequested(70);
            if(action==Action::Confirm){
                if(project()["canInstall"].toBool())installSelectedVersion();else showVersions();
            }
        } else {
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
