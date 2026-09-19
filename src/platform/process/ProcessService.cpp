#include "ProcessService.h"
#include <QDir>

namespace trainer {
ProcessService::ProcessService(QObject* parent) : QObject(parent), worker_(new QObject) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    thread_.start();
    killTimer_.setSingleShot(true); killTimer_.setInterval(1500);
    connect(&killTimer_, &QTimer::timeout, this, [this] { if (active_) process_.kill(); });
    connect(&process_, &QProcess::started, this, &ProcessService::started);
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
    if (active_ || !QDir::isAbsolutePath(command.program)) return false;
    active_ = true;
    stopRequested_ = false;
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
                if (!error.isEmpty()) complete(-1, false, error);
                else execute(prepared);
            }, Qt::QueuedConnection);
        }, Qt::QueuedConnection);
    } else execute(command);
    return true;
}
void ProcessService::execute(const ProcessCommand& command) {
    if (!QDir::isAbsolutePath(command.program)) { complete(-1, false, "The Adventure's play setup changed."); return; }
    inspectOutput_ = command.inspectOutput;
    process_.setProcessChannelMode(QProcess::MergedChannels);
    process_.setStandardOutputFile(inspectOutput_ ? QString() : QProcess::nullDevice());
    process_.setProgram(command.program); process_.setArguments(command.arguments);
    process_.setWorkingDirectory(command.workingDirectory);
    process_.start(); // No shell, no interpolated command string, no blocking wait.
}
void ProcessService::drainOutput() {
    while (process_.bytesAvailable() > 0) {
        const auto bytes = process_.read(16384);
        if (!inspectOutput_ || !validationError_.isEmpty()) continue;
        validationError_ = inspectOutput_(bytes);
        if (!validationError_.isEmpty()) { process_.kill(); }
    }
}
void ProcessService::stop() {
    if (!active_) return;
    stopRequested_ = true;
    if (preparing_) {
        *cancelled_ = true; ++request_; preparing_ = false;
        complete(0, false, {}); return;
    }
    process_.terminate(); killTimer_.start(); // Only this owned child can be stopped.
}
void ProcessService::complete(int code, bool crashed, const QString& error) {
    if (!active_) return;
    active_ = false; killTimer_.stop(); emit finished(code, crashed, error);
}
}
