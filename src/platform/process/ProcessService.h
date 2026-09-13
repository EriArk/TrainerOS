#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QThread>
#include <functional>
#include <atomic>
#include <memory>

namespace trainer {
struct ProcessCommand {
    QString program; QStringList arguments; QString workingDirectory;
    // Runs off the GUI thread, after the navigation checkpoint. Return a
    // user-facing error to prevent execution. Captures must own their data.
    std::function<QString(ProcessCommand&, const std::atomic_bool&)> prepare{};
    // Optional adapter-owned, bounded output validation. No output is logged.
    std::function<QString(const QByteArray&)> inspectOutput{};
};
class ProcessService final : public QObject {
    Q_OBJECT
public:
    explicit ProcessService(QObject* parent = nullptr);
    ~ProcessService() override;
    bool active() const { return active_; }
    bool start(const ProcessCommand&);
    void stop();
signals:
    void started();
    void finished(int exitCode, bool crashed, const QString& error);
private:
    void complete(int code, bool crashed, const QString& error);
    void execute(const ProcessCommand&);
    void drainOutput();
    QProcess process_;
    QTimer killTimer_;
    QThread thread_;
    QObject* worker_;
    std::shared_ptr<std::atomic_bool> cancelled_;
    std::function<QString(const QByteArray&)> inspectOutput_;
    QString validationError_;
    quint64 request_ = 0;
    bool preparing_ = false;
    bool active_ = false;
};
}
