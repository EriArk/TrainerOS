#pragma once
#include "platform/process/ProcessService.h"
#include <QJsonObject>

namespace trainer {
// Device-independent lifecycle. A validated adapter supplies the command; the
// application owns checkpoint persistence and platform-specific window handoff.
class AdventureLaunchController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool preparing READ preparing NOTIFY changed)
public:
    explicit AdventureLaunchController(ProcessService&, QObject* parent = nullptr);
    QString state() const { return state_; }
    QString error() const { return error_; }
    bool active() const { return state_ == "preparing" || state_ == "starting" || state_ == "running" || state_ == "stopping"; }
    bool preparing() const { return state_ == "preparing" || state_ == "starting"; }
    bool launch(const ProcessCommand&, const QJsonObject& returnContext, const QString& adventureId = {});
    void checkpointCompleted(quint64 request, const QString& error);
    Q_INVOKABLE void cancel();
signals:
    void changed();
    void checkpointRequested(quint64 request, const QJsonObject& context);
    void suspendRequested();
    void restoreRequested(const QJsonObject& context);
    void adventureStarted(const QString& id);
    void adventureFinished(bool failed);
private:
    void restore(const QString& error);
    ProcessService& process_;
    ProcessCommand command_;
    QJsonObject context_;
    QString state_ = "idle", error_;
    QString adventureId_;
    bool started_ = false;
    quint64 request_ = 0;
};
}
