#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>

namespace trainer {
struct ProcessCommand { QString program; QStringList arguments; QString workingDirectory; };
class ProcessService final : public QObject {
    Q_OBJECT
public:
    explicit ProcessService(QObject* parent = nullptr);
    bool active() const { return active_; }
    bool start(const ProcessCommand&);
    void stop();
signals:
    void started();
    void finished(int exitCode, bool crashed, const QString& error);
private:
    void complete(int code, bool crashed, const QString& error);
    QProcess process_;
    QTimer killTimer_;
    bool active_ = false;
};
}
