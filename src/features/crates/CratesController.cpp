#include "CratesController.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <algorithm>

namespace trainer {
CratesController::CratesController(QObject* parent):QObject(parent) {
    QFile data(":/crates/showcase.json");
    if(data.open(QIODevice::ReadOnly))crates_=QJsonDocument::fromJson(data.readAll()).array().toVariantList();
    opening_.setSingleShot(true);
    connect(&opening_,&QTimer::timeout,this,[this]{opened_=true;emit changed();});
}
void CratesController::select(int index) {
    if(opened_ || opening() || index<0 || index>=crates_.size() || focus_==index)return;
    focus_=index;emit changed();
}
void CratesController::open(int index,bool reducedMotion) {
    if(opened_ || index<0 || index>=crates_.size())return;
    if(opening()){opening_.stop();opened_=true;}
    else {focus_=index;if(reducedMotion||reducedMotion_)opened_=true;else opening_.start(210);}
    emit changed();
}
void CratesController::back(){opening_.stop();opened_=false;emit changed();}
void CratesController::leave(){if(opening()){opening_.stop();emit changed();}}
bool CratesController::dispatch(Action action) {
    if(action==Action::Back){if(!opened_&&!opening())return false;back();return true;}
    if(action==Action::Confirm){open(focus_);return true;}
    if(opened_||opening()||crates_.isEmpty())return true;
    int next=focus_;
    // Two featured crates above four thematic crates; vertical moves retain side.
    if(action==Action::Left)next=std::max(focus_<2?0:2,focus_-1);
    if(action==Action::Right)next=std::min(focus_<2?1:int(crates_.size())-1,focus_+1);
    if(action==Action::Down&&focus_<2)next=2+focus_*2;
    if(action==Action::Up&&focus_>=2)next=(focus_-2)/2;
    select(next);return true;
}
}
