#include "OutfitController.h"
#include "TrainerController.h"
#include "AvatarAssets.h"
#include "features/accounts/BackendAccount.h"
#include <QtConcurrent>
#include <QUrl>
#include <QDir>
#include <QFile>

namespace trainer {
OutfitController::OutfitController(BackendAccount& account,TrainerController& trainer,QObject* parent)
    :QObject(parent),account_(account),trainer_(trainer) {
    connect(&account_,&BackendAccount::changed,this,[this]{if(open_){index_=qBound(0,index_,int(rows().size())-1);preview();emit changed();}});
    connect(&trainer_,&TrainerController::changed,this,[this]{
        if(open_&&trainer_.profile()["id"].toString()!=owner_){close();emit backRequested();return;}
        if(open_){preview();emit changed();}
    });
    connect(&job_,&QFutureWatcher<QJsonObject>::finished,this,[this]{
        if(jobGeneration_!=generation_){if(open_)startPreview();return;}
        const auto result=job_.result();previewError_=result["error"].toString();
        previewUrl_=previewError_.isEmpty()?QUrl::fromLocalFile(result["preview"].toString()).toString():QString();
        if(previewError_.isEmpty())for(const auto& file:QDir(cache_.path()).entryInfoList({"*.glb"},QDir::Files))
            if(file.absoluteFilePath()!=result["glb"].toString()&&file.absoluteFilePath()!=result["preview"].toString())QFile::remove(file.absoluteFilePath());
        emit changed();
    });
}
OutfitController::~OutfitController(){job_.waitForFinished();}
QVariantList OutfitController::rows() const {
    QVariantList result{QVariantMap{{"id","current"},{"name","Current look"},{"current",true}}};
    for(const auto item:account_.outfits()["items"].toList()) {
        auto row=item.toMap();const auto saved=row["saved"].toMap();
        row["name"]=saved.isEmpty()?"Empty slot "+row["slot_number"].toString():saved["name"];
        result.append(row);
    }
    const auto next=account_.outfits()["next"].toMap();
    if(!next.isEmpty()){auto row=next;row["name"]="Add a look";row["buy"]=true;result.append(row);}
    return result;
}
QVariantMap OutfitController::selected() const {return rows().value(index_).toMap();}
QVariantList OutfitController::actions() const {
    const auto row=selected();
    if(row["current"].toBool())return {"Edit appearance","Refresh slots"};
    if(row["buy"].toBool())return {account_.outfits()["connected"].toBool()?"Review purchase":"Sign in to TrainerOS","Refresh slots"};
    if(row["saved"].toMap().isEmpty())return {"Save current look","Refresh slots"};
    return {"Wear this look","Replace with current","Rename","Refresh slots"};
}
QVariantMap OutfitController::view() const {
    const auto shop=account_.outfits();const auto row=selected();const auto saved=row["saved"].toMap();
    const auto appearance=QJsonObject::fromVariantMap(saved["recipe"].toMap())["appearance"].toObject();
    const auto status=status_=="Applying look…"&&!trainer_.saving()&&!trainer_.avatarBusy()
        ?(trainer_.error().isEmpty()?QString("Look applied. Supported games receive the change automatically."):trainer_.error()):status_;
    return {{"rows",rows()},{"selected",row},{"index",index_},{"focus",focus_},{"actions",actions()},
        {"preview",previewUrl_},{"previewBusy",job_.isRunning()},{"previewError",!previewError_.isEmpty()},
        {"busy",trainer_.saving()},{"replace",replace_},{"review",shop["review"]},
        {"status",!status.isEmpty()?status:shop["status"].toString()},{"stale",shop["stale"]},
        {"wearing",!appearance.isEmpty()&&appearance==trainer_.avatarDescriptor()["appearance"].toObject()}};
}
void OutfitController::open(){account_.cancelPurchaseReview();open_=true;owner_=trainer_.profile()["id"].toString();index_=focus_=0;status_.clear();replace_=false;pending_={};preview();account_.refreshShop();emit changed();}
void OutfitController::close(){if(!open_)return;open_=false;++generation_;previewUrl_.clear();previewAppearance_={};pending_={};replace_=false;account_.cancelPurchaseReview();}
void OutfitController::select(int index){if(!open_||view()["busy"].toBool()||replace_||!account_.outfits()["review"].toMap().isEmpty())return;index_=qBound(0,index,int(rows().size())-1);focus_=0;status_.clear();preview();emit changed();}
void OutfitController::refresh(){status_.clear();account_.refreshShop();}
void OutfitController::preview(){
    const auto row=selected();auto a=QJsonObject::fromVariantMap(row["saved"].toMap()["recipe"].toMap())["appearance"].toObject();
    if(a.isEmpty())a=trainer_.avatarDescriptor()["appearance"].toObject();
    if(a==previewAppearance_)return;
    previewAppearance_=a;previewUrl_.clear();previewError_.clear();++generation_;if(!job_.isRunning())startPreview();
}
void OutfitController::startPreview(){
    jobGeneration_=generation_;if(!open_||!AvatarAssets::valid(previewAppearance_))return;
    const auto appearance=previewAppearance_;const auto cache=cache_.path();
    job_.setFuture(QtConcurrent::run([appearance,cache]{return AvatarAssets::resolve(appearance,{},cache);}));
}
void OutfitController::askName(bool rename){
    const auto inventory=account_.outfits();
    if(!inventory["connected"].toBool()){status_="Connect your TrainerOS account to save or rename looks. Saved looks can still be worn offline.";emit changed();return;}
    if(inventory["stale"].toBool()||inventory["pending"].toBool()){status_="Refresh your slots and resolve any pending purchase before saving.";emit changed();return;}
    const auto row=selected();const auto saved=row["saved"].toMap();
    const auto recipe=rename?QJsonObject::fromVariantMap(saved["recipe"].toMap()):trainer_.avatarDescriptor()["recipe"].toObject();
    if(trainer_.avatarBusy()||recipe.isEmpty()){status_="Wait for your current look to finish preparing.";emit changed();return;}
    pending_={{"id",row["id"].toString()},{"revision",saved.value("revision","0").toString()},{"recipe",recipe},
        {"player",account_.snapshot()["player_id"]},{"owner",owner_}};
    emit nameRequested(saved.value("name","Look "+row["slot_number"].toString()).toString());
}
void OutfitController::acceptName(const QString& name){
    const auto pending=pending_;pending_={};
    if(!open_||pending.isEmpty()||pending["owner"].toString()!=owner_||pending["player"]!=account_.snapshot()["player_id"])return;
    account_.saveOutfit(pending["id"].toString(),name,pending["recipe"].toObject(),pending["revision"].toString());
}
void OutfitController::activate(int action){
    if(!open_||view()["busy"].toBool()||action<0||action>=actions().size())return;
    focus_=action+1;status_.clear();const auto row=selected();
    // Cached looks and the free editor remain usable while a catalogue request
    // waits for the network. Server mutations retain the account's busy guard.
    if(account_.outfits()["busy"].toBool()
        && !(action==0&&(row["current"].toBool()||!row["saved"].toMap().isEmpty())))return;
    if(action==actions().size()-1){refresh();return;}
    if(row["current"].toBool()){emit editRequested();return;}
    if(row["buy"].toBool()){account_.reviewOutfitSlot();focus_=1;emit changed();return;}
    const auto saved=row["saved"].toMap();
    if(saved.isEmpty())askName(false);
    else if(action==0){
        const auto appearance=QJsonObject::fromVariantMap(saved["recipe"].toMap())["appearance"].toObject();
        if(!previewError_.isEmpty()||job_.isRunning()||previewUrl_.isEmpty()){status_="Wait for the preview before wearing this look.";}
        else if(trainer_.applyAppearance(appearance))status_="Applying look…";
        else status_="Could not apply this look. Your current appearance is unchanged.";
    }else if(action==1){replace_=true;focus_=1;}else askName(true);
    emit changed();
}
void OutfitController::confirm(bool yes){
    if(!open_||view()["busy"].toBool()||account_.outfits()["busy"].toBool())return;
    if(replace_){replace_=false;if(yes)askName(false);}
    else if(yes)account_.confirmPurchase();else account_.cancelPurchaseReview();
    focus_=0;emit changed();
}
void OutfitController::dispatch(Action action){
    if(!open_)return;
    if(replace_||!account_.outfits()["review"].toMap().isEmpty()){
        if(action==Action::Back)confirm(false);
        else if(action==Action::Left)focus_=1;else if(action==Action::Right)focus_=2;
        else if(action==Action::Confirm)confirm(focus_==2);
    }else if(action==Action::Back){close();emit backRequested();}
    else if(action==Action::Up||action==Action::Down){
        if(focus_==0)select(index_+(action==Action::Down?1:-1));
        else if(action==Action::Up)focus_=focus_>2?focus_-2:0;
        else if(focus_+2<=actions().size())focus_+=2;
    }
    else if(action==Action::Left)focus_=focus_>0&&focus_%2==0?focus_-1:0;
    else if(action==Action::Right)focus_=focus_==0?1:focus_%2==1?qMin(focus_+1,int(actions().size())):focus_;
    else if(action==Action::Confirm)activate(focus_==0?0:focus_-1);
    emit changed();
}
}
