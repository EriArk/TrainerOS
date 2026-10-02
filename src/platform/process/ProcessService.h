#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QThread>
#include <QVariantMap>
#include <functional>
#include <atomic>
#include <memory>

namespace trainer {
struct ProcessOutcome {
    bool started = false;
    int exitCode = -1;
    bool crashed = false, stopped = false;
};
struct ProcessCommand {
    QString program; QStringList arguments; QString workingDirectory;
    // Runs off the GUI thread, after the navigation checkpoint. Return a
    // user-facing error to prevent execution. Captures must own their data.
    std::function<QString(ProcessCommand&, const std::atomic_bool&)> prepare{};
    // Optional adapter-owned, bounded output validation. No output is logged.
    std::function<QString(const QByteArray&)> inspectOutput{};
    // Worker-only observation after the owned child is gone, before return.
    // Also called for failed/cancelled preparation if preparation installed it.
    std::function<void(const ProcessOutcome&)> settled{};
    QVariantMap runtimeControls;
};
class ProcessService final : public QObject {
    Q_OBJECT
public:
    explicit ProcessService(QObject* parent = nullptr);
    ~ProcessService() override;
    bool active() const { return active_; }
    bool stopRequested() const { return stopRequested_; }
    qint64 processId() const { return process_.processId(); }
    bool start(const ProcessCommand&);
    void stop();
    QVariantMap runtimeControls() const{return active_?runtimeControls_:QVariantMap{};}
    bool runtimeCommand(const QString& action,const QString& text={});
signals:
    void started();
    void finished(int exitCode, bool crashed, const QString& error);
private:
    void complete(int code, bool crashed, const QString& error);
    void execute(const ProcessCommand&);
    void drainOutput();
    QProcess process_;
    QVariantMap runtimeControls_;
    QTimer killTimer_;
    QThread thread_;
    QObject* worker_;
    std::shared_ptr<std::atomic_bool> cancelled_;
    std::function<QString(const QByteArray&)> inspectOutput_;
    std::function<void(const ProcessOutcome&)> settled_;
    QString validationError_;
    quint64 request_ = 0;
    bool preparing_ = false;
    bool active_ = false;
    bool stopRequested_ = false;
    bool childStarted_ = false, finalizing_ = false;
};
}
