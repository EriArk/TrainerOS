#include "CratesController.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <algorithm>

namespace trainer {
CratesController::CratesController(QObject* parent):QObject(parent) {
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
    return catalogue_.projects.value(catalogue_.contents.value(current()["id"].toString()).value(projectIndex()));
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
void CratesController::closeDetail(){detailOpen_=versionsOpen_=false;emit changed();}
void CratesController::turnPage(int delta) {
    if(!opened_ || detailOpen_ || projectCount()==0)return;
    const int next=std::clamp(page()+delta,0,pageCount()-1);
    selectProject(std::min(next*6+projectIndex()%6,projectCount()-1));
}
void CratesController::showVersions(){if(detailOpen_&&!project()["releases"].toList().isEmpty()){versionsOpen_=!versionsOpen_;emit changed();}}
void CratesController::selectRelease(int i){if(detailOpen_&&i>=0&&i<project()["releases"].toList().size()){releaseIndex_=i;emit changed();}}
void CratesController::selectScreenshot(int i){if(detailOpen_&&i>=0&&i<project()["screenshots"].toList().size()){screenshotIndex_=i;emit changed();}}
void CratesController::back(){
    if(versionsOpen_){versionsOpen_=false;emit changed();return;}
    if(detailOpen_){closeDetail();return;}
    opening_.stop();opened_=false;emit changed();
}
void CratesController::leave(){if(opening()||detailOpen_){opening_.stop();closeDetail();}}
bool CratesController::dispatch(Action action) {
    if(action==Action::Back){if(!opened_&&!opening())return false;back();return true;}
    if(detailOpen_) {
        if(action==Action::ContextMenu || action==Action::LocalAction)showVersions();
        else if(versionsOpen_) {
            if(action==Action::Up)selectRelease(releaseIndex_-1);
            if(action==Action::Down)selectRelease(releaseIndex_+1);
            if(action==Action::ScrollUp)emit detailScrollRequested(-70);
            if(action==Action::ScrollDown)emit detailScrollRequested(70);
            if(action==Action::Confirm)showVersions();
        } else {
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
