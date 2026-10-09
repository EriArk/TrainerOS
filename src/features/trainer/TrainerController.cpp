#include "TrainerController.h"
#include "core/input/TextEntryController.h"
#include <QUuid>
#include <algorithm>

namespace trainer {
namespace {
const QStringList emblems{"compass", "leaf", "spark"};
}
TrainerController::TrainerController(TrainerRepository& repository, QObject* parent)
    : QObject(parent), repository_(repository), profile_(repository.load()) {}
QVariantMap TrainerController::profile() const {
    const TrainerProfile current = profile_.value_or(TrainerProfile{});
    auto values=extraProfile(current);values.insert("id",current.id);values.insert("name",current.name);values.insert("emblem",current.emblemId);return values;
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
    cancelExtra();
    if (!editing_) return;
    editing_ = false;
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
            error_.clear();
        }
        emit changed();
    });
}
void TrainerController::reload() {
    if (saving_ || editing_) return;
    profile_ = repository_.load();
    emit changed();
}
void TrainerController::activate(int index) {
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
    if(!editing_)return;
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
}
