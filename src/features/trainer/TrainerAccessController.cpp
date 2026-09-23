#include "TrainerAccessController.h"
#include <QTimer>
#include <algorithm>

namespace trainer {
TrainerAccessController::TrainerAccessController(LocalStateStore* store,QObject* parent):QObject(parent),store_(store) {}
bool TrainerAccessController::keypad() const { return stage_=="unlock" || stage_=="old" || stage_=="new" || stage_=="repeat" || stage_=="parent" || stage_=="delete-code"; }
QString TrainerAccessController::title() const {
    if(stage_=="delete")return "Remove "+name_+"?";
    if(stage_=="delete-code")return "Ask a parent";
    if(stage_=="unlock")return "Welcome back, "+name_;
    if(stage_=="parent")return "Ask a parent";
    if(stage_=="reset")return "Remove the PIN for "+name_+"?";
    if(stage_=="repeat")return "Once more";
    if(stage_=="new")return family_?"Choose a family code":"Choose your PIN";
    if(stage_=="old")return family_?"Current family code":"Your current PIN";
    if(stage_=="done")return "All set";
    return family_?"Family code":"Your Trainer PIN";
}
QString TrainerAccessController::description() const {
    if(busy_)return "One moment...";
    if(stage_=="delete-code")return "Enter the family code to remove this Trainer.";
    if(stage_=="delete")return "Their journal, history and memories will be removed. Games, game saves and other Trainers stay.";
    if(stage_=="unlock")return "Enter your PIN to open your journal.";
    if(stage_=="parent")return "Enter the family code to reset this Trainer's PIN.";
    if(stage_=="reset")return "Their Trainer, journal and progress will stay safe.";
    if(stage_=="repeat")return "Enter the same digits again.";
    if(stage_=="new")return family_?"Six digits, kept by a parent. This code resets forgotten Trainer PINs.":"Choose 4 to 6 digits you can remember.";
    if(stage_=="old")return "Enter the current code before changing it.";
    if(stage_=="done")return operation_=="remove"?"Your Trainer now opens without a PIN.":family_?"Your family code is ready. Keep it somewhere safe.":"Your PIN has been saved.";
    return family_?"One code for a parent to reset forgotten PINs. Keep it private.":"A little privacy for your Trainer on a shared handheld.";
}
QStringList TrainerAccessController::choices() const {
    if(stage_=="delete")return {"Keep Trainer","Remove Trainer"};
    if(stage_=="menu")return store_->pinProtected(id_)?QStringList{"Change PIN","Remove PIN","Back"}:QStringList{"Set PIN","Back"};
    if(stage_=="reset")return {"Keep PIN","Remove PIN"};
    if(stage_=="done")return {"Done"};
    return {};
}
void TrainerAccessController::move(const QString& stage) { input_=emptyPin();stage_=stage;focus_=0;error_.clear();emit changed(); }
void TrainerAccessController::close() { old_=emptyPin();next_=emptyPin();move({}); }
void TrainerAccessController::beginUnlock(const QString& id) {
    if(!store_ || busy_)return;
    close();family_=false;id_=id;name_.clear();
    for(const auto& p:store_->trainers())if(p.id==id)name_=p.name;
    move("unlock");
}
void TrainerAccessController::beginManage(bool family) {
    if(!store_ || !store_->ready() || busy_)return;
    close();family_=family;id_=store_->ownerId();operation_="change";
    if(family)move(store_->familyProtected()?"old":"new");else move("menu");
}
void TrainerAccessController::beginRemoval() {
    if(!store_ || !store_->ready() || busy_ || !store_->load())return;
    close();id_=store_->ownerId();name_=store_->load()->name;
    family_=store_->familyProtected();operation_="delete";
    move(family_?"delete-code":"delete");
}
void TrainerAccessController::removalFailed(const QString& error) {
    busy_=false;old_=emptyPin();move(family_?"delete-code":"delete");error_=error;emit changed();
}
void TrainerAccessController::recover() {
    if(!canRecover() || busy_)return;
    if(!store_->familyProtected()){error_="A parent can set a family code in Settings on an open Trainer.";emit changed();return;}
    move("parent");
}
void TrainerAccessController::completed(const QString& error) {
    busy_=false;
    if(!error.isEmpty()){input_=emptyPin();error_=error;focus_=0;emit changed();return;}
    old_=emptyPin();next_=emptyPin();move("done");
}
void TrainerAccessController::save() {
    busy_=true;emit changed();
    store_->changePin(old_,next_,family_,this,[this](const QString& e){completed(e);});
}
void TrainerAccessController::submit() {
    if(input_->size()<minimumDigits()) {
        error_=minimumDigits()==6?"Use six digits.":"Use 4 to 6 digits.";emit changed();return;
    }
    if(stage_=="new"){next_=input_;move("repeat");return;}
    if(stage_=="repeat") {
        if(!input_->matches(*next_)){move("repeat");error_="Those digits didn't match. Try again.";emit changed();return;}
        save();return;
    }
    const auto stage=stage_;busy_=true;emit changed();
    auto attempt=input_;input_=emptyPin();
    store_->verifyPin(id_,attempt,family_ || stage=="parent",this,[this,stage,attempt](const QString& e){
        busy_=false;
        if(!e.isEmpty()){error_=e;focus_=0;emit changed();return;}
        if(stage=="unlock") {
            const auto id=id_;close();QTimer::singleShot(0,this,[this,id]{emit unlocked(id);});
        } else if(stage=="delete-code") {old_=attempt;move("delete");}
        else if(stage=="parent") {old_=attempt;move("reset");}
        else {old_=attempt;if(operation_=="remove"){next_=emptyPin();save();}else move("new");}
    });
}
void TrainerAccessController::activate(int index) {
    if(!active() || busy_)return;
    if(keypad()) {
        if(index<0 || index>12)return;
        focus_=index;error_.clear();
        if(index<9)input_->digit(index+1);else if(index==9)input_->erase();else if(index==10)input_->digit(0);
        else if(index==11)input_->clear();else {submit();return;}
    } else if(stage_=="delete") {
        if(index==0)close();
        else if(index==1){busy_=true;emit changed();emit removalRequested(old_);}
    } else if(stage_=="menu") {
        if(index<0 || index>=choices().size())return;
        if(index==choices().size()-1)close();
        else if(!store_->familyProtected()){error_="First ask a parent to set the family code in Settings > Trainer.";}
        else {operation_=index==1?"remove":"change";move(store_->pinProtected(id_)?"old":"new");}
    } else if(stage_=="reset") {
        if(index==0)move("unlock");
        else if(index==1){busy_=true;emit changed();store_->resetPin(id_,old_,this,[this](const QString& e){
            busy_=false;if(!e.isEmpty()){old_=emptyPin();move("parent");error_=e;emit changed();return;}
            const auto id=id_;close();QTimer::singleShot(0,this,[this,id]{emit unlocked(id);});
        });}
    } else if(stage_=="done" && index==0)close();
    emit changed();
}
void TrainerAccessController::dispatch(Action action) {
    if(!active() || busy_)return;
    if(action==Action::Back) {
        if(stage_=="repeat"){next_=emptyPin();move("new");}
        else if(stage_=="parent" || stage_=="reset"){old_=emptyPin();move("unlock");}
        else close();return;
    }
    if(action==Action::Secondary){recover();return;}
    if(action==Action::Confirm){activate(focus_);return;}
    if(keypad()) {
        if(action==Action::Up)focus_=focus_==12?10:std::max(0,focus_-3);
        if(action==Action::Down)focus_=focus_>=9?12:focus_+3;
        if(action==Action::Left && focus_<12 && focus_%3) --focus_;
        if(action==Action::Right && focus_<12 && focus_%3<2) ++focus_;
    } else {
        if(action==Action::Up)focus_=std::max(0,focus_-1);
        if(action==Action::Down)focus_=std::min(int(choices().size())-1,focus_+1);
    }
    emit changed();
}
}
