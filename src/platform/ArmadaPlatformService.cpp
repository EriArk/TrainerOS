#include "ArmadaPlatformService.h"
#include <QFileInfo>

namespace trainer {
static const QString helper = "/var/opt/traineros/session/control.py";
ArmadaPlatformService::ArmadaPlatformService(bool enabled, QObject* parent)
    : QObject(parent), enabled_(enabled) {
    timeout_.setSingleShot(true);
    timeout_.setInterval(15000);
    process_.setStandardOutputFile(QProcess::nullDevice());
    process_.setStandardErrorFile(QProcess::nullDevice());
    connect(&timeout_, &QTimer::timeout, this, [this] { process_.kill(); });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) { timeout_.stop(); emit transitionFinished(false); }
    });
    connect(&process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        timeout_.stop(); emit transitionFinished(code == 0 && status == QProcess::NormalExit);
    });
}
bool ArmadaPlatformService::canSwitchSession() const {
#ifdef Q_OS_LINUX
    const QFileInfo file(helper), directory(file.absolutePath());
    const auto unsafe = QFileDevice::WriteGroup | QFileDevice::WriteOther;
    return enabled_ && file.isFile() && !file.isSymLink() && file.isExecutable()
        && file.ownerId() == 0 && !(file.permissions() & unsafe)
        && directory.ownerId() == 0 && !(directory.permissions() & unsafe);
#else
    return false;
#endif
}
bool ArmadaPlatformService::dedicatedSession() const {
    return canSwitchSession() && qEnvironmentVariable("TRAINEROS_SESSION") == "1";
}
QString ArmadaPlatformService::sessionStatus() const {
    return canSwitchSession() ? "TrainerOS, Steam Gaming Mode and Desktop Mode are available."
        : "Session controls are unavailable. Install and validate the ArmadaOS session package in Desktop Mode.";
}
void ArmadaPlatformService::switchMode(const QString& mode) {
    if (!canSwitchSession() || !QStringList{"traineros", "desktop", "steam", "reboot", "poweroff"}.contains(mode)
            || process_.state() != QProcess::NotRunning) {
        emit transitionFinished(false); return;
    }
    timeout_.start();
    process_.start("/usr/bin/sudo", {"-n", helper, mode});
}
}
