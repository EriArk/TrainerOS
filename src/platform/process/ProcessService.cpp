#include "ProcessService.h"
#include <QDir>

namespace trainer {
ProcessService::ProcessService(QObject* parent) : QObject(parent) {
    killTimer_.setSingleShot(true); killTimer_.setInterval(1500);
    connect(&killTimer_, &QTimer::timeout, this, [this] { if (active_) process_.kill(); });
    connect(&process_, &QProcess::started, this, &ProcessService::started);
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) complete(-1, false, "The Adventure couldn't start. Check its setup and try again.");
    });
    connect(&process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        complete(code, status == QProcess::CrashExit, status == QProcess::CrashExit ? "The Adventure stopped unexpectedly. You're back in TrainerOS." : QString());
    });
    // Child output is drained and bounded by omission here; logs need a separate
    // redacted adapter-owned policy before real emulator integration.
    process_.setStandardOutputFile(QProcess::nullDevice());
    process_.setStandardErrorFile(QProcess::nullDevice());
}
bool ProcessService::start(const ProcessCommand& command) {
    if (active_ || !QDir::isAbsolutePath(command.program)) return false;
    active_ = true;
    process_.setProgram(command.program); process_.setArguments(command.arguments);
    process_.setWorkingDirectory(command.workingDirectory);
    process_.start(); // No shell, no interpolated command string, no blocking wait.
    return true;
}
void ProcessService::stop() {
    if (!active_) return;
    process_.terminate(); killTimer_.start(); // Only this owned child can be stopped.
}
void ProcessService::complete(int code, bool crashed, const QString& error) {
    if (!active_) return;
    active_ = false; killTimer_.stop(); emit finished(code, crashed, error);
}
}
