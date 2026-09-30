#include "PracticeController.h"
#include "integrations/progress/EmeraldPractice.h"
#include <QFileInfo>
#include <QJsonArray>
#include <QRandomGenerator>
#include <algorithm>

namespace trainer {
PracticeController::PracticeController(QObject* parent):QObject(parent),session_(this) {
    sourceTimer_.setInterval(3000);
    connect(&sourceTimer_,&QTimer::timeout,this,[this]{if(!checking_ && running())check([]{});});
    checkDeadline_.setSingleShot(true);checkDeadline_.setInterval(6000);
    connect(&checkDeadline_,&QTimer::timeout,this,[this]{fail("Your Party could not be checked. Try again.");});
    connect(&session_,&PracticeSession::changed,this,&PracticeController::consumeState);
    connect(&session_,&PracticeSession::stopped,this,[this](const QString& reason){
        if(!discarding_ && open_ && !reason.isEmpty())fail(reason);
        emit changed();
    });
}
void PracticeController::configureRuntime(const QString& root) {
#ifdef Q_OS_WIN
    const auto node=root+"/bin/node.exe";
#else
    const auto node=root+"/bin/node";
#endif
    configureRuntime(node,root+"/src/integrations/practice/emerald-worker.cjs",root+"/node_modules/pokemon-showdown");
}
void PracticeController::configureRuntime(const QString& node,const QString& worker,const QString& engine) {
    node_=node;worker_=worker;engine_=engine;emit changed();
}
bool PracticeController::ready() const {
    return bool(verify_) && !source_.trainerId.isEmpty() && progress_.party
        && progress_.availability==ProgressAvailability::Available && progress_.party->error.isEmpty()
        && QFileInfo::exists(node_) && QFileInfo::exists(worker_) && QFileInfo::exists(engine_+"/package.json")
        && candidates().size()>=2;
}
QVariantList PracticeController::candidates() const {
    QVariantList result;
    if(!progress_.party)return result;
    // Use the same exact-game gate as launch, including individual battle facts.
    for(int i=0;i<progress_.party->party.size() && i<6;++i) {
        bool usable=false;
        for(int j=0;j<progress_.party->party.size() && j<6;++j)
            if(i!=j && emeraldPracticePair(progress_,i,j).error.isEmpty()){usable=true;break;}
        if(usable){auto row=presentation(i);row["chosen"]=i==first_;result.append(row);}
    }
    return result;
}
QVariantMap PracticeController::presentation(int slot) const {
    if(slot<0)return {};
    for(const auto& actor:actors_)if(actor.toMap().value("index",-1).toInt()==slot)return actor.toMap();
    return {};
}
void PracticeController::setObservation(const PracticeSource& source,const GameProgress& progress,const QVariantList& actors) {
    const bool different=source_!=source || progress.availability!=ProgressAvailability::Available;
    if(different && open_ && (first_>=0 || running()))fail("Your Party changed. Choose your partners again.");
    source_=source;progress_=progress;actors_=actors;
    session_.updateSource(source,progress.availability==ProgressAvailability::Available);
    if(different && stage_=="first")focus_=0;
    emit changed();
}
void PracticeController::enter() {
    leave();open_=true;stage_="first";focus_=0;error_.clear();emit changed();
}
void PracticeController::leave() {
    if (!open_) return;
    ++generation_;open_=false;discarding_=true;checking_=false;
    sourceTimer_.stop();checkDeadline_.stop();session_.cancel();
    first_=second_=-1;frozen_.clear();playback_.reset();observed_={};stage_="first";error_.clear();emit changed();
}
void PracticeController::fail(const QString& message) {
    ++generation_;checking_=false;checkDeadline_.stop();sourceTimer_.stop();discarding_=true;session_.cancel();
    playback_.reset();frozen_.clear();observed_={};first_=second_=-1;
    stage_="error";error_=message;focus_=0;emit changed();
}
void PracticeController::check(std::function<void()> success) {
    if(checking_)return;
    if(!verify_){fail("Practice is not ready on this device.");return;}
    checking_=true;checkDeadline_.start();const auto generation=generation_;
    verify_(source_,progress_,this,[this,generation,success=std::move(success)](bool valid){
        if(generation!=generation_ || !open_)return;
        checking_=false;checkDeadline_.stop();
        if(!valid){fail("Your Party changed. Choose your partners again.");return;}
        success();
    });
}
void PracticeController::begin() {
    if(!ready() || running())return;
    stage_="starting";emit changed();
    check([this]{
        frozen_={presentation(first_),presentation(second_)};observed_={};discarding_=false;
        std::array<int,4> seed;for(auto& value:seed)value=int(QRandomGenerator::global()->bounded(65536u));
        if(!session_.begin(node_,worker_,engine_,source_,progress_,first_,second_,seed)) {fail("These partners cannot start practice.");return;}
        sourceTimer_.start();
    });
}
QVariantList PracticeController::fighters() const {
    auto result=frozen_.isEmpty()?QVariantList{presentation(first_),presentation(second_)}:frozen_;
    const auto states=playback_.active()?playback_.sides():observed_["sides"].toArray();
    for(int i=0;i<result.size();++i) {
        auto row=result[i].toMap();if(row.isEmpty())continue;
        const int slot=i==0?first_:second_;
        const int maxHp=states.size()==2?states[i].toObject()["maxHp"].toInt():progress_.party->party[slot].stats[0];
        const int hp=states.size()==2?states[i].toObject()["hp"].toInt():maxHp;
        row["battleHp"]=hp;row["maxHp"]=maxHp;row["hpRatio"]=maxHp?double(hp)/maxHp:0;
        row["battleStatus"]=states.size()==2?states[i].toObject()["status"].toString():QString();
        const auto e=event();
        if(playback_.active() && e["actor"].toInt()==i){row["effectBeat"]=e["serial"];row["effectKind"]=e["effect"];row["effectElement"]=e["element"];if(e["effect"]=="move")row["attackBeat"]=e["serial"];}
        result[i]=row;
    }
    return result;
}
QVariantList PracticeController::moves() const {
    const auto sides=observed_["sides"].toArray();
    return sides.size()==2?sides[side_].toObject()["moves"].toArray().toVariantList():QVariantList{};
}
QString PracticeController::message() const {
    if(stage_=="error")return error_;
    if(!ready() && stage_=="first")return candidates().size()<2
        ? "Choose an Emerald Adventure with two supported Party members."
        : "Practice is not installed on this device yet.";
    if(stage_=="first")return "Choose your first partner";
    if(stage_=="second")return "Choose a sparring partner";
    if(stage_=="ready")return "Partners ready!";
    if(stage_=="starting")return "Getting ready...";
    if(stage_=="waiting")return "Here we go!";
    if(stage_=="moves")return frozen_.value(side_).toMap()["name"].toString()+" - choose a move";
    if(stage_=="events")return event()["text"].toString();
    if(stage_=="finished") {
        const auto winner=observed_["winner"].toString();
        return winner.isEmpty()?QStringLiteral("A draw! Well played."):
            frozen_.value(winner=="Partner 1"?0:1).toMap()["name"].toString()+" wins!";
    }
    return {};
}
QVariantMap PracticeController::event() const {return playback_.event();}
void PracticeController::consumeState() {
    if(!open_ || discarding_)return;
    const auto state=session_.state();if(state.isEmpty() || state==observed_)return;
    const QJsonArray teams{QJsonArray{QJsonObject::fromVariantMap(frozen_.value(0).toMap())},QJsonArray{QJsonObject::fromVariantMap(frozen_.value(1).toMap())}};
    playback_.load(observed_,state,teams);observed_=state;side_=0;focus_=0;
    if(!playback_.active())finishEvents();else stage_="events";
    emit changed();
}
void PracticeController::finishEvents() {
    stage_=observed_["ended"].toBool()?"finished":"moves";side_=0;focus_=0;playback_.reset();
    if(stage_=="finished")sourceTimer_.stop();
}
void PracticeController::activate(int index) {
    if(!open_)return;
    if(stage_=="error" || stage_=="finished"){enter();return;}
    if(stage_=="first" || stage_=="second") {
        if(!ready()){leave();emit closeRequested();return;}
        if(running())return;
        const auto list=candidates();if(index<0 || index>=list.size())return;
        const int slot=list[index].toMap()["index"].toInt();focus_=index;
        if(stage_=="first"){first_=slot;stage_="second";focus_=(index+1)%list.size();}
        else if(slot!=first_){second_=slot;stage_="ready";focus_=0;}
    } else if(stage_=="ready") {begin();return;}
    else if(stage_=="events"){if(!playback_.advance())finishEvents();}
    else if(stage_=="moves") {
        if(checking_)return;
        const auto list=moves();if(index<0 || index>=list.size())return;
        choices_[side_]=list[index].toMap()["slot"].toInt();
        if(side_==0){side_=1;focus_=0;}
        else {stage_="waiting";emit changed();check([this]{if(!session_.choose(choices_[0],choices_[1]))fail("That move is no longer available. Try again.");});return;}
    }
    emit changed();
}
void PracticeController::dispatch(Action action) {
    if(action==Action::Back) {
        if(stage_=="second"){first_=-1;stage_="first";focus_=0;}
        else if(stage_=="ready"){second_=-1;stage_="second";focus_=0;}
        else if(stage_=="moves" && side_==1){side_=0;focus_=0;}
        else {leave();emit closeRequested();return;}
    } else if(action==Action::Confirm){activate(focus_);return;}
    else if(stage_=="first" || stage_=="second" || stage_=="moves") {
        const int count=stage_=="moves"?moves().size():candidates().size();
        if(count>0) {
            const int delta=action==Action::Right?1:action==Action::Left?-1:action==Action::Down?2:action==Action::Up?-2:0;
            focus_=(focus_+delta%count+count)%count;
        }
    }
    emit changed();
}
}
