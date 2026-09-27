#include "PracticeSession.h"
#include "integrations/progress/EmeraldPractice.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QProcessEnvironment>

namespace trainer {
PracticeSession::PracticeSession(QObject* parent):QObject(parent) {
    deadline_.setSingleShot(true);deadline_.setInterval(5000);
    connect(&deadline_,&QTimer::timeout,this,[this]{stop("The practice engine did not respond.");});
}
PracticeSession::~PracticeSession() {
    if(process_) {process_->disconnect(this);process_->kill();process_->waitForFinished(1000);}
}
bool PracticeSession::begin(const QString& node,const QString& worker,const QString& engineRoot,
    const PracticeSource& source,const GameProgress& progress,int first,int second,
    const std::array<int,4>& seed) {
    if(process_)return false;
    const auto pair=emeraldPracticePair(progress,first,second);
    if(!pair.error.isEmpty() || source.trainerId.isEmpty() || source.adventureId.isEmpty()
        || source.contextRevision!=progress.contextRevision || source.contentRevision!=progress.contentRevision
        || source.saveRevision!=progress.saveRevision) return false;
    QJsonArray seeds;for(int value:seed){if(value<0 || value>65535)return false;seeds.append(value);}
    source_=source;start_=pair.input;start_["command"]="start";start_["seed"]=seeds;
    ready_=false;pending_=true;stopping_=false;reason_.clear();buffer_.clear();state_={};
    process_=new QProcess(this);
    // Node options/preloads from the desktop environment cannot alter the worker.
    auto env=QProcessEnvironment::systemEnvironment();env.remove("NODE_OPTIONS");env.remove("NODE_PATH");
    process_->setProcessEnvironment(env);
    process_->setStandardErrorFile(QProcess::nullDevice());
    connect(process_,&QProcess::readyReadStandardOutput,this,&PracticeSession::receive);
    connect(process_,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart) {
            auto* old=process_;process_=nullptr;deadline_.stop();state_={};
            old->deleteLater();emit changed();emit stopped("The practice engine could not start.");
        } else if(!stopping_)stop("The practice engine stopped unexpectedly.");
    });
    connect(process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int,QProcess::ExitStatus){
        auto* old=process_;process_=nullptr;deadline_.stop();
        if(!stopping_){reason_="The practice engine stopped unexpectedly.";state_={};}
        old->deleteLater();emit changed();emit stopped(reason_);
    });
    deadline_.start();process_->start(node,{"--max-old-space-size=192",worker,engineRoot});
    emit changed();return true;
}
void PracticeSession::send(const QJsonObject& input) {
    if(!process_ || stopping_)return;
    const auto bytes=QJsonDocument(input).toJson(QJsonDocument::Compact)+'\n';
    if(bytes.size()>16384 || process_->write(bytes)!=bytes.size()) {stop("The practice request could not be sent.");return;}
    pending_=true;deadline_.start();
}
void PracticeSession::receive() {
    if(!process_)return;
    const auto bytes=process_->readAllStandardOutput();
    if(stopping_)return;
    if(buffer_.size()+bytes.size()>65536){stop("Invalid practice response.");return;}
    buffer_+=bytes;
    int at;
    while((at=buffer_.indexOf('\n'))>=0 && !stopping_) {
        const auto line=buffer_.left(at);buffer_.remove(0,at+1);
        QJsonParseError error;const auto doc=QJsonDocument::fromJson(line,&error);
        if(error.error!=QJsonParseError::NoError || !doc.isObject()){stop("Invalid practice response.");return;}
        const auto input=doc.object();const auto type=input["type"].toString();
        if(!ready_ && type=="ready" && input["protocol"].toInt()==1) {ready_=true;send(start_);start_={};}
        else if(ready_ && pending_ && type=="state" && input["sides"].toArray().size()==2
            && input["turn"].toInt()>0 && input["turn"].toInt()<=201) {
            deadline_.stop();pending_=false;state_=input;emit changed();
            if(input["ended"].toBool())stop({});
        } else {stop(type=="error"?QStringLiteral("These partners cannot start or continue practice."):QStringLiteral("Invalid practice response."));}
    }
}
bool PracticeSession::choose(int firstSlot,int secondSlot) {
    if(!process_ || stopping_ || pending_ || state_.isEmpty() || state_["ended"].toBool())return false;
    const auto sides=state_["sides"].toArray();const std::array<int,2> selections{firstSlot,secondSlot};
    for(int i=0;i<2;++i) {
        bool allowed=false;for(const auto& value:sides[i].toObject()["moves"].toArray())
            if(value.toObject()["slot"].toInt(-1)==selections[i])allowed=true;
        if(!allowed)return false;
    }
    send({{"command","turn"},{"turn",state_["turn"]},{"moves",QJsonArray{firstSlot,secondSlot}}});return true;
}
void PracticeSession::updateSource(const PracticeSource& source,bool available) {
    if(available && source==source_)return;
    if(process_)stop("The selected Party changed.");
    else if(!state_.isEmpty()){state_={};emit changed();}
}
void PracticeSession::cancel() {
    if(process_)stop("Practice cancelled.");
    else if(!state_.isEmpty()){state_={};emit changed();}
}
void PracticeSession::stop(const QString& reason) {
    if(!process_ || stopping_)return;
    stopping_=true;reason_=reason;deadline_.stop();start_={};
    if(!reason.isEmpty())state_={};
    // This process owns only disposable semantic copies, never the emulator.
    process_->kill();emit changed();
}
}
