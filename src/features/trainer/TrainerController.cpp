#include "TrainerController.h"
#include "AvatarAssets.h"
#include <QtConcurrent>
#include <QTimer>
#include <QUrl>
#include <QDir>
#include <QFile>
#include "core/input/TextEntryController.h"
#include <QUuid>
#include <algorithm>

namespace trainer {
namespace {
const QStringList emblems{"compass", "leaf", "spark"};
}
TrainerController::TrainerController(TrainerRepository& repository, QObject* parent)
    : QObject(parent), repository_(repository), profile_(repository.load()) {
    connect(&avatarJob_, &QFutureWatcher<QJsonObject>::finished, this, [this] {
        if(avatarJobGeneration_!=avatarGeneration_){startAvatarJob();return;}
        const auto result=avatarJob_.result();avatarError_=result["error"].toString();
        if(avatarError_.isEmpty()){
            if(avatarJobDraft_){
                previewAvatar_=result;
                const QDir dir(avatarDraftCache_.path());
                for(const auto& file:dir.entryInfoList({"*.glb"},QDir::Files))
                    if(file.absoluteFilePath()!=result["glb"].toString()&&file.absoluteFilePath()!=result["preview"].toString())QFile::remove(file.absoluteFilePath());
            }else liveAvatar_=result;
        }
        emit changed();
    });
    QTimer::singleShot(0,this,[this]{refreshAvatar();});
}
TrainerController::~TrainerController(){avatarJob_.waitForFinished();}
QVariantMap TrainerController::profile() const {
    const TrainerProfile current = profile_.value_or(TrainerProfile{});
    auto values=extraProfile(current);values.insert("id",current.id);values.insert("name",current.name);values.insert("emblem",current.emblemId);
    values.insert("joined",current.createdAt.isValid()?current.createdAt.toString("MMM yyyy"):QString());return values;
}
QVariantList TrainerController::editRows() const {
    QVariantList rows{QVariantMap{{"title","Name"},{"detail",draftName()},{"kind","action"}},
        QVariantMap{{"title","Emblem"},{"detail",draftEmblem()},{"kind","action"}}};
    rows.append(extraEditRows());
    rows.append(QVariantMap{{"title",saving_?"Saving…":"Save profile"},{"kind","action"}});
    rows.append(QVariantMap{{"title","Cancel"},{"kind","action"}});
    return rows;
}
void TrainerController::beginEdit() {
    if (editing_ || saving_) return;
    draft_ = profile_.value_or(TrainerProfile{});
    if (!profile_) {
        draft_.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        draft_.createdAt = QDateTime::currentDateTimeUtc();
    }
    error_.clear();
    focus_ = 0;
    editing_ = true;
    emit changed();
}
void TrainerController::cancel() {
    if(avatarEditing_){avatarEditing_=false;previewAvatar_={};refreshAvatar();}
    cancelExtra();
    if (!editing_) return;
    editing_ = false;
    focus_=0;
    draft_ = {};
    error_.clear();
    emit changed();
}
void TrainerController::setDraftName(const QString& name) {
    if (!editing_ || saving_) return;
    draft_.name = name;
    error_.clear();
    emit changed();
}
void TrainerController::save() {
    TrainerProfile candidate = draft_;
    candidate.name = candidate.name.trimmed();
    if (candidate.name.isEmpty()) error_ = "Give your Trainer a name before saving.";
    else if (TextEntryController::characterCount(candidate.name) > NameLimit)
        error_ = QString("Your Trainer name can have up to %1 characters.").arg(NameLimit);
    else if (std::any_of(candidate.name.begin(), candidate.name.end(), [](QChar c) {
        return c.category() == QChar::Other_Control || c.category() == QChar::Separator_Line
            || c.category() == QChar::Separator_Paragraph;
    })) error_ = "Use a single line for your Trainer name.";
    else error_.clear();
    if (!error_.isEmpty()) { focus_ = 0; return; }
    saving_ = true;
    repository_.saveAsync(candidate, this, [this, candidate](const ProfileWriteResult& result) {
        saving_ = false;
        if (!result.success) {
            error_ = result.error.isEmpty() ? "Couldn't save your Trainer. Try again." : result.error;
            if (!editing_) emit messageRequested(error_);
        } else {
            profile_ = candidate;
            draft_ = {};
            editing_ = false;
            focus_=0;
            error_.clear();
            avatarEditing_=false;previewAvatar_={};refreshAvatar();
        }
        emit changed();
    });
}
void TrainerController::reload() {
    if (saving_ || editing_) return;
    const auto previousOwner=profile_?profile_->id:QString();
    profile_ = repository_.load();
    focus_=0;
    if(!profile_||profile_->id!=previousOwner){liveAvatar_={};previewAvatar_={};}
    refreshAvatar();
    emit changed();
}
void TrainerController::activate(int index) {
    if(!editing_){if(index==1)beginAvatarEdit();else beginEdit();return;}
    if(avatarEditing_){if(index<AvatarAssets::keys().size())cycleAvatar(index);else if(index==AvatarAssets::keys().size())finishAvatar(true);else finishAvatar(false);return;}
    if(activateOverlay(index))return;
    const int saveIndex=2+extraRows();
    if(editing_ && index==saveIndex+1){cancel();return;}
    if(!editing_ || saving_ || index<0 || index>saveIndex+1)return;
    focus_=index;
    if(index==0)emit nameRequested(draft_.name);
    else if(index==1)draft_.emblemId=emblems[(emblems.indexOf(draft_.emblemId)+1)%emblems.size()];
    else if(index<saveIndex)activateExtra(index-2);
    else if(index==saveIndex)save();
    emit changed();
}
void TrainerController::dispatch(Action action,bool vertical) {
    if(!editing_){
        if(action==Action::Up)focus_=0;
        if(action==Action::Down)focus_=avatarAvailable()&&exists()?1:0;
        if(action==Action::Confirm)activate(focus_);
        emit changed();return;
    }
    if(avatarEditing_){
        if(action==Action::Back){finishAvatar(false);return;}
        if(action==Action::Up)focus_=std::max(0,focus_-1);
        if(action==Action::Down)focus_=std::min(int(AvatarAssets::keys().size())+1,focus_+1);
        if(action==Action::Left||action==Action::Right)cycleAvatar(focus_,action==Action::Right?1:-1);
        if(action==Action::Confirm)activate(focus_);
        emit changed();return;
    }
    if(dispatchExtra(action))return;
    if(action==Action::Back){cancel();return;}
    if(action==Action::Confirm){activate(focus_);return;}
    const int saveIndex=2+extraRows();
    if(vertical && (action==Action::Up || action==Action::Down))focus_=std::clamp(focus_+(action==Action::Down?1:-1),0,saveIndex+1);
    else {
        if(action==Action::Up)focus_=focus_>=saveIndex?saveIndex-1:std::max(0,focus_-1);
        if(action==Action::Down)focus_=focus_<saveIndex?focus_+1:focus_;
        if(action==Action::Right && focus_==saveIndex)focus_=saveIndex+1;
        if(action==Action::Left && focus_==saveIndex+1)focus_=saveIndex;
    }
    emit changed();
}
bool TrainerController::avatarAvailable() const {return AvatarAssets::available();}
QJsonObject TrainerController::appearance() const {return profile_ && !profile_->avatar.isEmpty()?AvatarAssets::normalized(profile_->avatar):AvatarAssets::defaults();}
bool TrainerController::avatarRendererAvailable() const {
#ifdef TRAINER_AVATAR_QUICK3D
    return true;
#else
    return false;
#endif
}
QString TrainerController::avatarPreview() const {
    const auto descriptor=avatarEditing_&&!previewAvatar_.isEmpty()?previewAvatar_:liveAvatar_;
    return QUrl::fromLocalFile(descriptor["preview"].toString()).toString();
}
QVariantList TrainerController::avatarRows() const {
    const auto a=draft_.avatar.isEmpty()?AvatarAssets::defaults():draft_.avatar;
    const QStringList labels{"Hairstyle","Eyes","Top","Bottom","Socks","Shoes","Headwear","Hair colour","Eye colour","Skin colour","Top colour","Bottom colour","Socks colour","Shoes colour","Headwear colour"};
    QVariantList rows;int i=0;for(const auto& key:AvatarAssets::keys())rows.append(QVariantMap{{"title",labels[i++]},{"detail",a[key].toString()},{"choices",AvatarAssets::choices(key)},{"colour",key.endsWith("Color")?a[key].toString():QString()}});
    rows.append(QVariantMap{{"title",saving_?"Saving…":"Save appearance"}});rows.append(QVariantMap{{"title","Cancel"}});return rows;
}
void TrainerController::beginAvatarEdit() {
    if(!exists()||editing_||saving_||!avatarAvailable())return;
    draft_=*profile_;draft_.avatar=draft_.avatar.isEmpty()?AvatarAssets::defaults():AvatarAssets::normalized(draft_.avatar);
    avatarEditing_=true;editing_=true;focus_=0;error_.clear();previewAvatar_=liveAvatar_;refreshAvatar();emit changed();
}
bool TrainerController::applyAppearance(const QJsonObject& appearance) {
    if(!exists()||editing_||saving_||!AvatarAssets::valid(appearance)||!avatarAvailable())return false;
    draft_=*profile_;draft_.avatar=AvatarAssets::normalized(appearance);error_.clear();save();return true;
}
void TrainerController::cycleAvatar(int index,int delta) {
    if(!avatarEditing_||saving_||index<0||index>=AvatarAssets::keys().size())return;
    const auto key=AvatarAssets::keys()[index];const auto values=AvatarAssets::choices(key);
    const int old=values.indexOf(draft_.avatar[key].toString());chooseAvatar(index,values[(std::max(0,old)+delta+values.size())%values.size()]);
}
void TrainerController::chooseAvatar(int index,const QString& value) {
    if(!avatarEditing_||saving_||index<0||index>=AvatarAssets::keys().size())return;
    const auto key=AvatarAssets::keys()[index];
    if(!AvatarAssets::choices(key).contains(value))return;
    focus_=index;
    if(draft_.avatar[key]!=value){
        draft_.avatar=AvatarAssets::equipped(draft_.avatar,key,value);
        refreshAvatar();
    }
    emit changed();
}
void TrainerController::finishAvatar(bool saveChanges) {
    if(!avatarEditing_||saving_)return;
    if(saveChanges)save();else cancel();emit changed();
}
void TrainerController::refreshAvatar() {
    ++avatarGeneration_;if(!avatarJob_.isRunning())startAvatarJob();
}
void TrainerController::startAvatarJob() {
    avatarJobGeneration_=avatarGeneration_;avatarJobDraft_=avatarEditing_;
    if(!profile_||!avatarAvailable()){liveAvatar_={};previewAvatar_={};return;}
    auto a=avatarEditing_?draft_.avatar:profile_->avatar;if(a.isEmpty())a=AvatarAssets::defaults();
    const auto cache=avatarEditing_?avatarDraftCache_.path():QString();
    avatarJob_.setFuture(QtConcurrent::run([a,cache]{return AvatarAssets::resolve(a,{},cache);}));
}
}
