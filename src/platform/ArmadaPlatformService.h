#pragma once
#include "PlatformService.h"
#include <QObject>
#include <QProcess>
#include <QTimer>

namespace trainer {
class ArmadaPlatformService final : public QObject, public PlatformService {
    Q_OBJECT
public:
    explicit ArmadaPlatformService(bool enabled, QObject* parent = nullptr);
    bool canSwitchSession() const override;
    QString sessionStatus() const override;
    bool dedicatedSession() const override;
    void switchMode(const QString& mode);
signals:
    void transitionFinished(bool success);
private:
    bool enabled_ = false;
    QProcess process_;
    QTimer timeout_;
};
}
