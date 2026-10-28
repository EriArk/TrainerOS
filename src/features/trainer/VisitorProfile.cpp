#include "VisitorProfile.h"
#include "AvatarAssets.h"
#include "features/accounts/BackendAccount.h"
#include <QtConcurrent>
#include <QUrl>
#include <QPointer>
#include <QDir>
#include <QFile>
namespace trainer {
VisitorProfile::VisitorProfile(BackendAccount& account,QObject* parent):QObject(parent),account_(account) {
    connect(&account_,&BackendAccount::changed,this,[this]{
        const auto owner=account_.snapshot()["player_id"].toString();
        if(owner_!=owner){
            const bool reopen=open_;const auto targetSubject=subject_,targetPlayer=player_;
            close();owner_=owner;open_=reopen;subject_=targetSubject;player_=preview_?owner:targetPlayer;
            if(reopen){status_="Account changed. Refresh to view this profile.";emit changed();}
        }
    });
    connect(&job_,&QFutureWatcher<QJsonObject>::finished,this,[this]{
        if(renderGeneration_!=generation_){if(open_)render();return;}
        const auto result=job_.result();
        if(result["error"].toString().isEmpty())previewUrl_=QUrl::fromLocalFile(result["preview"].toString()).toString();
        const auto files=QDir(cache_.path()).entryInfoList({"*.glb"},QDir::Files,QDir::Time);
        for(int i=2;i<files.size();++i)QFile::remove(files[i].absoluteFilePath());
        emit changed();
    });
}
VisitorProfile::~VisitorProfile(){job_.waitForFinished();}
void VisitorProfile::close(){++generation_;open_=false;busy_=false;data_={};sharing_={};previewUrl_.clear();status_.clear();subject_.clear();player_.clear();emit changed();}
void VisitorProfile::open(QString subject,QString player,bool preview){
    close();owner_=account_.snapshot()["player_id"].toString();subject_=subject;player_=player;preview_=preview;open_=true;refresh();
}
void VisitorProfile::refresh(bool more){
    if(busy_||!open_)return;
    const auto cursor=more?data_["next"].toString():QString();if(more&&cursor.isEmpty())return;
    const auto generation=++generation_;busy_=true;status_="Loading profile…";data_={};previewUrl_.clear();emit changed();
    account_.viewProfile(player_,subject_,cursor,[this,guard=QPointer<VisitorProfile>(this),generation](int code,QJsonObject value){
        if(!guard||generation!=generation_)return;busy_=false;
        if(code==200 && !value["player_id"].toString().isEmpty()) {data_=value;status_.clear();render();}
        else status_=code==404?"This player has not connected a TrainerOS profile yet.":code==401?"Sign in to TrainerOS to view player profiles.":"Profile unavailable. Try again.";
        emit changed();
    });
}
void VisitorProfile::loadSharing(){
    if(busy_)return;open_=true;owner_=account_.snapshot()["player_id"].toString();const auto generation=++generation_;busy_=true;sharing_={};emit changed();
    account_.profileSharing({},[this,guard=QPointer<VisitorProfile>(this),generation](int code,QJsonObject value){
        if(!guard||generation!=generation_)return;busy_=false;
        if(code==200){sharing_=value;status_.clear();}else status_="Sign in and refresh to change profile visibility.";emit changed();
    });
}
void VisitorProfile::toggleSharing(int index){
    static const QStringList fields{"level","shelf","history","activity","favorites"};
    if(busy_||sharing_.isEmpty()||index<0||index>=fields.size())return;
    auto update=sharing_;update[fields[index]]=!update[fields[index]].toBool();busy_=true;const auto generation=++generation_;emit changed();
    account_.profileSharing(update,[this,guard=QPointer<VisitorProfile>(this),generation](int code,QJsonObject value){
        if(!guard||generation!=generation_)return;busy_=false;
        if(code==200){sharing_=value;status_="Visibility saved.";}
        else if(code==409){status_="Changed on another device. Review the latest choices.";loadSharing();}
        else status_="Could not save visibility. Your previous choices still apply.";emit changed();
    });
}
void VisitorProfile::render(){
    if(job_.isRunning()||data_["avatar"].isNull()||!data_["avatar"].isObject())return;
    const auto recipe=data_["avatar"].toObject();const auto cache=cache_.path();renderGeneration_=generation_;
    job_.setFuture(QtConcurrent::run([recipe,cache]{
        const auto appearance=recipe["appearance"].toObject();
        if(recipe.size()!=3||recipe["schema"]!=1||!AvatarAssets::valid(appearance))return QJsonObject{{"error","invalid_avatar_recipe"}};
        const auto result=AvatarAssets::resolve(appearance,{},cache);
        if(result["recipe"].toObject()["packRevision"]!=recipe["packRevision"])return QJsonObject{{"error","avatar_pack_mismatch"}};
        return result;
    }));
}
void VisitorProfile::publishFavorites(QJsonArray games){
    if(busy_||sharing_.isEmpty())return;
    const auto generation=++generation_;busy_=true;emit changed();
    account_.publishProfileFavorites(games,sharing_["revision"].toString(),[this,guard=QPointer<VisitorProfile>(this),generation](int code,QJsonObject value){
        if(!guard||generation!=generation_)return;busy_=false;
        if(code==200){sharing_=value;status_="Favorite games published. Refresh this selection after changing your library favorites.";}
        else status_="Could not publish favorites. Refresh the sharing choices and try again.";emit changed();
    });
}
}
