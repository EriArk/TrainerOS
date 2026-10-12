#include "HotseatHandoff.h"
#include <QUuid>

namespace trainer {
HotseatHandoff::HotseatHandoff(QObject* parent):QObject(parent) {
    timer_.setSingleShot(true);timer_.setInterval(12000);
    connect(&timer_,&QTimer::timeout,this,&HotseatHandoff::fail);
}
void HotseatHandoff::begin(QString session,int slot) {
    stop();
    if(session.size()!=36||QUuid(session).isNull()||(slot!=1&&slot!=2))return;
    session_=std::move(session);slot_=slot;timer_.start(75000);emit changed();
}
void HotseatHandoff::stop() {
    timer_.stop();session_.clear();slot_=owner_=next_=nativeOwner_=0;
    epoch_=observedSequence_=commandEpoch_=0;phase_=Waiting;commanded_=false;
}
void HotseatHandoff::send(QString action,QJsonObject extra) {
    if(!active())return;
    extra["channel"]="hotseat-v1";extra["session"]=session_;extra["action"]=action;
    extra["epoch"]=qint64(epoch_);emit outgoing(slot_==1?2:1,extra);
}
void HotseatHandoff::fail() {
    if(!active())return;
    // Mark inactive before external callbacks can re-enter this coordinator.
    const auto session=session_;const auto epoch=epoch_;const int target=slot_==1?2:1;
    stop();
    emit outgoing(target,{{"channel","hotseat-v1"},{"session",session},{"action","abort"},{"epoch",qint64(epoch)}});
    emit changed();emit failed("Controller ownership couldn't be confirmed. End this session and invite again.");
}
void HotseatHandoff::publish() {
    send("phase",{{"phase",int(phase_)},{"owner",owner_},{"next",next_}});
    emit changed();
}
void HotseatHandoff::command(bool playing) {
    if(!active()||(commanded_&&commandEpoch_==epoch_))return;
    commanded_=true;commandEpoch_=epoch_;
    if(!requestNativeRole||!requestNativeRole(playing))fail();
}
void HotseatHandoff::pass() {
    if(!canPass())return;
    if(slot_==1)startPass();else send("pass");
}
void HotseatHandoff::startPass() {
    if(slot_!=1||!ready())return;
    ++epoch_;next_=3-owner_;phase_=Releasing;timer_.start(12000);
    publish();
    if(active()&&phase_==Releasing&&owner_==slot_)command(false);
}
void HotseatHandoff::observed(quint64 sequence,int nativeOwner,bool initial) {
    if(!active()||slot_!=2)return;
    if(nativeOwner< -1||nativeOwner>1||sequence!=observedSequence_+1) {fail();return;}
    observedSequence_=sequence;nativeOwner_=nativeOwner+1;
    send("observed",{{"sequence",qint64(sequence)},{"nativeOwner",nativeOwner_},{"initial",initial}});
}
void HotseatHandoff::acknowledge(quint64 sequence,int nativeOwner,bool initial) {
    if(sequence<=observedSequence_)return; // duplicate/delayed provider delivery
    if(sequence!=observedSequence_+1||nativeOwner<0||nativeOwner>2) {fail();return;}
    observedSequence_=sequence;nativeOwner_=nativeOwner;
    if(phase_==Waiting&&initial&&sequence==1&&nativeOwner==1) {
        owner_=1;phase_=Idle;timer_.stop();publish();return;
    }
    if(initial) {fail();return;}
    if(phase_==Releasing&&nativeOwner==0) {
        phase_=Acquiring;timer_.start(12000);publish();
        if(active()&&phase_==Acquiring&&next_==slot_)command(true);
    } else if(phase_==Acquiring&&nativeOwner==next_) {
        owner_=next_;next_=0;phase_=Idle;timer_.stop();publish();
    } else fail(); // native hotkeys/unexpected ownership cannot silently change turns
}
void HotseatHandoff::applyPhase(const QJsonObject& p) {
    const auto epoch=p["epoch"].toInteger(-1);
    const int phase=p["phase"].toInt(-1),owner=p["owner"].toInt(-1),next=p["next"].toInt(-1);
    if(epoch<0||quint64(epoch)<epoch_)return;
    if(owner<1||owner>2||phase<Releasing||phase>Idle) {fail();return;}
    if(phase_==Waiting) {
        if(epoch!=0||phase!=Idle||owner!=1||next!=0||nativeOwner_!=1) {fail();return;}
    } else if(quint64(epoch)==epoch_) {
        if(phase<=phase_)return; // duplicate or reordered older phase
        if(owner!=owner_&&phase!=Idle) {fail();return;}
        if(phase==Idle&&(owner!=next_||nativeOwner_!=owner||next!=0)) {fail();return;}
        // The host's native MODE may beat its provider phase to the guest.
        // Release-before-acquire is still checked by the native observer and
        // host coordinator. This delayed phase never sends a guest command.
        const bool remoteAlreadyAcquired=next==1&&nativeOwner_==1;
        if(phase==Acquiring&&(next!=next_||(nativeOwner_!=0&&!remoteAlreadyAcquired))) {fail();return;}
    } else {
        if(quint64(epoch)!=epoch_+1||phase_!=Idle||owner!=owner_||
           (phase!=Releasing&&phase!=Acquiring)) {fail();return;}
    }
    if(phase!=Idle&&next!=3-owner) {fail();return;}
    epoch_=quint64(epoch);phase_=Phase(phase);owner_=owner;next_=next;
    if(phase_==Idle)timer_.stop();else timer_.start(12000);
    emit changed();
    if(phase_==Releasing&&owner_==slot_) {
        if(nativeOwner_!=slot_) {fail();return;}
        command(false);
    } else if(phase_==Acquiring&&next_==slot_) {
        if(nativeOwner_!=0) {fail();return;}
        command(true);
    }
}
void HotseatHandoff::receive(int sender,const QJsonObject& p) {
    if(!active()||sender!=(slot_==1?2:1)||p["channel"]!="hotseat-v1"||p["session"]!=session_)return;
    const auto action=p["action"].toString();const auto epoch=p["epoch"].toInteger(-1);
    if(epoch<0)return;
    if(action=="abort") {if(quint64(epoch)>=epoch_)fail();return;}
    if(slot_==2) {if(action=="phase")applyPhase(p);return;}
    if(action=="pass") {
        if(quint64(epoch)==epoch_&&ready()&&owner_==sender)startPass();
    } else if(action=="observed") {
        if(quint64(epoch)>epoch_) {fail();return;}
        const auto sequence=p["sequence"].toInteger(-1);
        if(sequence<=0||!p["initial"].isBool()) {fail();return;}
        acknowledge(quint64(sequence),p["nativeOwner"].toInt(-1),p["initial"].toBool());
    }
}
}
