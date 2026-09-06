#pragma once
#include "platform/process/ProcessService.h"
#include <QJsonObject>

namespace trainer {
// Device-independent lifecycle. A validated adapter supplies the command; the
// application owns checkpoint persistence and platform-specific window handoff.
class AdventureLaunchController final : public QObject {
    Q_OBJECT
public:
    explicit AdventureLaunchController(ProcessService&, QObject* parent = nullptr);
    QString state() const { return state_; }
    QString error() const { return error_; }
    bool active() const { return state_ == "preparing" || state_ == "starting" || state_ == "running" || state_ == "stopping"; }
    bool launch(const ProcessCommand&, const QJsonObject& returnContext);
    void checkpointCompleted(quint64 request, const QString& error);
    void cancel();
signals:
    void changed();
    void checkpointRequested(quint64 request, const QJsonObject& context);
    void suspendRequested();
    void restoreRequested(const QJsonObject& context);
private:
    void restore(const QString& error);
    ProcessService& process_;
    ProcessCommand command_;
    QJsonObject context_;
    QString state_ = "idle", error_;
    quint64 request_ = 0;
};
}
