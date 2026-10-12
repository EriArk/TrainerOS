#include "ProcessService.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

namespace trainer {
ProcessService::ProcessService(QObject* parent) : QObject(parent), worker_(new QObject) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    thread_.start();
    killTimer_.setSingleShot(true); killTimer_.setInterval(1500);
    connect(&killTimer_, &QTimer::timeout, this, [this] { if (active_) process_.kill(); });
    connect(&process_, &QProcess::started, this, [this]{ childStarted_=true; emit started(); });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) complete(-1, false, "The Adventure couldn't start. Check its setup and try again.");
    });
    connect(&process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        drainOutput();
        complete(code, status == QProcess::CrashExit, !validationError_.isEmpty() ? validationError_
            : status == QProcess::CrashExit ? "The Adventure stopped unexpectedly. You're back in TrainerOS." : QString());
    });
    connect(&process_, &QProcess::readyReadStandardOutput, this, &ProcessService::drainOutput);
}
ProcessService::~ProcessService() {
    if (cancelled_) *cancelled_ = true;
    thread_.quit(); thread_.wait();
}
bool ProcessService::start(const ProcessCommand& command) {
    // Runtime bundles choose the exact executable during cancellable validation.
    // The prepared command is checked again by execute before any child starts.
    if (active_ || (!command.prepare && !QDir::isAbsolutePath(command.program))) return false;
    active_ = true;
    runtimeControls_.clear();
    stopRequested_ = false;
    childStarted_ = false; finalizing_ = false; settled_=command.settled;finalize_=command.finalize;
    validationError_.clear(); inspectOutput_ = {};
    const auto token = ++request_;
    if (command.prepare) {
        preparing_ = true;
        cancelled_ = std::make_shared<std::atomic_bool>(false);
        QMetaObject::invokeMethod(worker_, [this, token, command, cancel = cancelled_]() mutable {
            auto prepared = command;
            const auto error = prepared.prepare(prepared, *cancel);
            prepared.prepare = {};
            QMetaObject::invokeMethod(this, [this, token, prepared, error] {
                if (token != request_ || !active_) return;
                preparing_ = false;
                settled_=prepared.settled;
                finalize_=prepared.finalize;
                if (stopRequested_) complete(0, false, {});
                else if (!error.isEmpty()) complete(-1, false, error);
                else execute(prepared);
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    } else execute(command);
    return true;
}
void ProcessService::execute(const ProcessCommand& command) {
    if (!QDir::isAbsolutePath(command.program)) { complete(-1, false, "The Adventure's play setup changed."); return; }
    inspectOutput_ = command.inspectOutput;
    runtimeControls_=command.runtimeControls;
    process_.setProcessChannelMode(QProcess::MergedChannels);
    process_.setStandardOutputFile(inspectOutput_ || runtimeControls_.contains("netplay") ? QString() : QProcess::nullDevice());
    process_.setProgram(command.program); process_.setArguments(command.arguments);
    process_.setWorkingDirectory(command.workingDirectory);
    process_.start(); // No shell, no interpolated command string, no blocking wait.
}
void ProcessService::drainOutput() {
    while (process_.bytesAvailable() > 0) {
        const auto bytes = process_.read(16384);
        if(runtimeControls_.contains("netplay"))emit runtimeOutput(bytes);
        if (!inspectOutput_ || !validationError_.isEmpty()) continue;
        validationError_ = inspectOutput_(bytes);
        if (!validationError_.isEmpty()) { process_.kill(); }
    }
}
bool ProcessService::runtimeCommand(const QString& action,const QString& text) {
    if(!active_||stopRequested_||process_.state()!=QProcess::Running||process_.bytesToWrite()>4096)return false;
    if(runtimeControls_["kind"]=="dolphin"&&runtimeControls_["hotseat"]=="dolphin-golf-p1-v1") {
        QJsonObject packet;
        if(action=="hotseat-end")packet={{"action","quit"}};
        else if(action=="hotseat-pass"&&runtimeControls_["netplayHost"].toBool()&&text.size()<=128) {
            const auto input=QJsonDocument::fromJson(text.toUtf8()).object();
            const auto sequence=input["sequence"].toInteger(-1);const int target=input["target"].toInt();
            if(sequence<1||(target!=1&&target!=2))return false;
            packet={{"action","pass"},{"sequence",sequence},{"target",target}};
        } else return false;
        const auto command=QJsonDocument(packet).toJson(QJsonDocument::Compact)+'\n';
        return process_.write(command)==command.size();
    }
    if(runtimeControls_["kind"]!="retroarch")return false;
    QByteArray command;
    if(action=="shader")command="SHADER_TOGGLE";
    else if(action=="next-shader")command="SHADER_NEXT";
    else if(action=="previous-shader")command="SHADER_PREV";
    else if((action=="hotseat-play"||action=="hotseat-spectate")&&runtimeControls_["hotseat"]=="retroarch-p1-v1")command="NETPLAY_GAME_WATCH";
    else if(action=="hotseat-end"&&runtimeControls_["hotseat"]=="retroarch-p1-v1")command="QUIT";
    else if(action=="notify") {auto message=text.left(160);message.replace('\n',' ');message.replace('\r',' ');command="SHOW_MSG "+message.toUtf8();}
    else return false;
    return process_.write(command+'\n')==command.size()+1;
}
void ProcessService::stop() {
    if (!active_ || finalizing_) return;
    stopRequested_ = true;
    if (preparing_) {
        *cancelled_ = true; return; // Let worker preparation settle before return/relaunch.
    }
    process_.terminate(); killTimer_.start(); // Only this owned child can be stopped.
}
void ProcessService::complete(int code, bool crashed, const QString& error) {
    if (!active_ || finalizing_) return;
    killTimer_.stop();
    if(settled_||finalize_) {
        finalizing_=true;
        auto settled=std::move(settled_);
        auto finalize=std::move(finalize_);
        const ProcessOutcome outcome{childStarted_,code,crashed,stopRequested_};
        QMetaObject::invokeMethod(worker_,[this,settled=std::move(settled),finalize=std::move(finalize),outcome,error]{
            const auto saved=finalize?finalize(outcome):QString();
            if(settled)settled(outcome);
            QMetaObject::invokeMethod(this,[this,outcome,error=saved.isEmpty()?error:saved]{
                finalizing_=false; active_=false;
                emit finished(outcome.exitCode,outcome.crashed,error);
            },Qt::QueuedConnection);
        },Qt::QueuedConnection);
    } else { active_=false; emit finished(code,crashed,error); }
}
}
