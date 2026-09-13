#include "SaveCenterController.h"
#include <algorithm>

namespace trainer {
namespace {
QString searchable(const QString& text){QString result;for(const auto c:text.normalized(QString::NormalizationForm_D))if(c.isLetterOrNumber())result+=c.toLower();return result;}
}
SaveCenterController::SaveCenterController(LibraryRepository& library,QObject* parent):QObject(parent),library_(library){}
void SaveCenterController::configure(SaveBackupService* service) {
    if(service_)disconnect(service_,nullptr,this,nullptr);
    service_=service;
    if(service_)connect(service_,&SaveBackupService::busyChanged,this,&SaveCenterController::changed);
}
QString SaveCenterController::title() const { return route_=="adventures"?"Pokémon Center":selected_.adventure.title; }
QString SaveCenterController::message() const {
    if(busy())return "Checking saves and keeping copies… You can leave this page; the operation will finish.";
    if(!message_.isEmpty())return message_;
    if(route_=="adventures")return "In-game save backups · choose an Adventure · unsupported setups stay unavailable";
    if(!snapshot_.error.isEmpty())return snapshot_.error;
    return snapshot_.hasSave?"In-game save found. Copies stay separate from Continue Adventure moments."
        :"No in-game save found. Save inside the Adventure first, or restore an existing copy.";
}
QVariantList SaveCenterController::rows() const {
    QVariantList result;
    if(route_=="adventures") {
        for(const auto& a:adventures_) {
            const auto registration=library_.registration(a.id);
            const bool supported=service_&&registration&&service_->supports(*registration);
            QString world; for(const auto& w:library_.worlds())if(w.id==a.worldId){world=w.name;break;}
            result.append(QVariantMap{{"id",a.id},{"title",a.title},{"detail",world+(supported?" · Save backups":" · Setup not supported yet")},{"available",supported}});
        }
    } else for(const auto& copy:snapshot_.copies) {
        const auto date=copy.createdAt.isValid()?copy.createdAt.toLocalTime().toString("dd MMM yyyy · HH:mm:ss"):QString("Unreadable copy");
        result.append(QVariantMap{{"id",copy.id},{"title",date},{"available",copy.valid&&copy.hasSave&&copy.bytes>0},
            {"detail",!copy.valid?"Damaged or different game content":!copy.hasSave?"Before restore · no previous save existed"
                :copy.bytes==0?"Before restore · previous save was empty":QString(copy.protection?"Before restore · %1 bytes":"Manual copy · %1 bytes").arg(copy.bytes)}});
    }
    return result;
}
QString SaveCenterController::restoreLabel() const {
    return confirmation_.createdAt.toLocalTime().toString("dd MMM yyyy · HH:mm:ss");
}
void SaveCenterController::rebuild() {
    adventures_.clear();
    for(const auto& a:library_.adventures())if(!a.collectionOnly && (query_.isEmpty() || searchable(a.title).contains(searchable(query_))))adventures_.append(a);
    std::sort(adventures_.begin(),adventures_.end(),[](const auto& a,const auto& b){return QString::compare(a.title,b.title,Qt::CaseInsensitive)<0;});
    focus_=std::clamp(focus_,0,std::max(0,int(adventures_.size())-1));emit rowsChanged();emit changed();
}
void SaveCenterController::begin(const QString& preferred) {
    ++generation_;open_=true;confirming_=false;query_.clear();message_.clear();route_="adventures";focus_=0;rebuild();
    for(int i=0;i<adventures_.size();++i)if(adventures_[i].id==preferred)focus_=i;
    emit changed();
}
void SaveCenterController::close(){++generation_;open_=false;confirming_=false;emit changed();}
void SaveCenterController::back() {
    if(confirming_){confirming_=false;emit changed();return;}
    if(route_=="copies"&&!busy()){++generation_;route_="adventures";message_.clear();focus_=0;rebuild();return;}
    close();emit closeRequested();
}
void SaveCenterController::search(){if(open_&&route_=="adventures"&&!busy())emit searchRequested(query_);}
void SaveCenterController::applySearch(const QString& text){if(open_&&route_=="adventures"){query_=text.trimmed();focus_=0;rebuild();}}
void SaveCenterController::refresh() {
    if(!open_ || busy() || !service_ || route_!="copies")return;
    const auto latest=library_.registration(selected_.adventure.id);
    if(!latest){message_="This Adventure is no longer linked. Return to the list.";emit changed();return;}
    selected_=*latest;message_.clear();confirming_=false;const auto generation=++generation_;
    service_->inspect(selected_,this,[this,generation](const SaveBackupSnapshot& result){
        if(!open_||generation!=generation_)return;
        snapshot_=result;focus_=std::clamp(focus_,0,std::max(0,int(snapshot_.copies.size())-1));emit rowsChanged();emit changed();
    });
}
void SaveCenterController::activate(int index) {
    if(!open_||busy())return;
    if(confirming_){restore();return;}
    if(route_=="adventures") {
        if(index<0||index>=adventures_.size()){search();return;}
        const auto record=library_.registration(adventures_[index].id);
        if(!record||!service_){message_="Save services aren't configured in this preview.";emit changed();return;}
        selected_=*record;route_="copies";focus_=0;snapshot_={};emit rowsChanged();refresh();emit changed();
    } else {
        if(index<0||index>=snapshot_.copies.size()){refresh();return;}
        focus_=index;const auto& copy=snapshot_.copies[index];
        if(!copy.valid||!copy.hasSave||copy.bytes==0||snapshot_.token.isEmpty()){message_="This copy can't be restored. Check storage and choose a valid saved copy.";emit changed();return;}
        confirmation_=copy;confirming_=true;message_.clear();emit changed();
    }
}
void SaveCenterController::completed(const AdventureRegistration& record,quint64 generation,const SaveBackupResult& result) {
    if(result.restored)emit restored(record.adventure.id);
    if(!open_||generation!=generation_){if(!result.success)emit messageRequested(result.message);return;}
    message_=result.message;confirming_=false;
    if(result.success){snapshot_=result.snapshot;focus_=0;emit rowsChanged();}
    emit changed();
}
void SaveCenterController::create() {
    if(!canCreate()||!service_)return;
    const auto record=library_.registration(selected_.adventure.id);
    if(!record||record->revision!=selected_.revision){message_="The Adventure changed. Check it again first.";emit changed();return;}
    const auto generation=generation_;message_.clear();
    service_->create(*record,snapshot_.token,this,[this,record=*record,generation](const SaveBackupResult& result){completed(record,generation,result);});
}
void SaveCenterController::restore() {
    if(!confirming_||busy()||!service_)return;
    const auto record=library_.registration(selected_.adventure.id);
    if(!record||record->revision!=selected_.revision){confirming_=false;message_="The Adventure changed. Check it again before restoring.";emit changed();return;}
    confirming_=false;message_.clear();const auto generation=generation_;
    service_->restore(*record,confirmation_,snapshot_.token,this,[this,record=*record,generation](const SaveBackupResult& result){completed(record,generation,result);});
}
void SaveCenterController::dispatch(Action action) {
    if(!open_)return;
    if(action==Action::Back){back();return;}
    if(busy())return;
    if(confirming_){if(action==Action::Confirm)restore();return;}
    if(action==Action::Confirm){activate(focus_);return;}
    if(action==Action::Secondary){if(route_=="adventures")search();else refresh();return;}
    if(action==Action::ToggleContinue){create();return;}
    const int count=route_=="adventures"?adventures_.size():snapshot_.copies.size();
    const int step=action==Action::Up?-1:action==Action::Down?1:action==Action::Left?-8:action==Action::Right?8:0;
    focus_=std::clamp(focus_+step,0,std::max(0,count-1));emit changed();
}
}
