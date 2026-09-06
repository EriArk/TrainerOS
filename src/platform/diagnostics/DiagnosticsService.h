#pragma once
#include <QObject>
#include <QJsonObject>
#include <QPointer>
#include <QThread>
#include <QWindow>
#include <QScreen>

namespace trainer {
// Runtime observations only. No session commands, environment dumps or library access.
class DiagnosticsService final : public QObject {
    Q_OBJECT
public:
    explicit DiagnosticsService(QString directory, QObject* parent = nullptr);
    ~DiagnosticsService() override;
    void setWindow(QWindow* window);
    QJsonObject runtime() const;
    void save(const QJsonObject& observations);
    bool saving() const { return saving_; }
    QString lastReportPath() const { return lastReportPath_; }
signals:
    void completed(const QString& error, const QString& filename);
    void runtimeChanged();
private:
    void watchScreen(QScreen* screen);
    QPointer<QWindow> window_;
    QPointer<QScreen> screen_;
    QString directory_, lastReportPath_;
    QThread thread_;
    QObject* worker_;
    bool saving_ = false;
};
}
