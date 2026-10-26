#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QJsonObject>
#include <functional>

namespace trainer {
// Supplied by trusted installation/bootstrap composition, never by a feed or QML.
// Source/development builds intentionally have no release identity or trust key.
struct SoftwareUpdateSource {
    QString python, helper, publicKey, betaUrl, previewUrl, state, cache;
    QString installedVersion, target;
    qint64 installedSequence = -1;
    int sqliteSchema = -1, firstRunSchema = -1;
    qint64 minimumFeedSequence = 0, minimumReleaseSequence = 0;
    QString installationRoot, manager;
    bool rollbackAvailable = false;
};

class SoftwareUpdateController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString installedVersion READ installedVersion NOTIFY changed)
    Q_PROPERTY(QString channel READ channel NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString version READ version NOTIFY changed)
    Q_PROPERTY(QString notes READ notes NOTIFY changed)
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY changed)
    Q_PROPERTY(QString lastChecked READ lastChecked NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool notesOpen READ notesOpen NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY changed)
    Q_PROPERTY(bool transitionPending READ transitionPending NOTIFY changed)
    Q_PROPERTY(bool confirming READ confirming NOTIFY changed)
public:
    explicit SoftwareUpdateController(QObject* parent = nullptr);
    ~SoftwareUpdateController() override;
    void configure(const SoftwareUpdateSource& source);
    void cancelRestart();
    void attachInstallation(const QString& root, const QString& dataDirectory, const QString& executable);
    bool transitionPending() const { return operation_=="request" || status_=="restart-required"; }
    bool confirming() const { return !confirmation_.isEmpty(); }
    std::function<QString()> maintenanceGuard;
    bool configured() const { return configured_; }
    QString installedVersion() const;
    QString channel() const { return channel_; }
    QString status() const { return status_; }
    QString message() const;
    QString version() const { return candidate_["version"].toString(); }
    QString notes() const { return candidate_["notes"].toString(); }
    QString sizeText() const;
    QString lastChecked() const { return lastChecked_; }
    double progress() const;
    bool busy() const { return busy_; }
    bool notesOpen() const { return notesOpen_; }
    int focusIndex() const { return focus_; }
    QVariantList actions() const;
    QVariantList tasks() const;
    QVariantMap notification() const;
    void dismissNotification();
    void command(const QString& command);
    void dispatch(Action action);
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void closeNotes();
    void check(bool background = false);
signals:
    void changed();
    void backRequested();
    void downloadsRequested();
    void cratesRequested();
    void notesScrollRequested(int direction);
    void updateAvailable(const QString& version);
    void restartRequested();
private:
    void start(bool prepare, bool background = false);
    void stop(const QString& reason);
    void readOutput();
    void finish(int code, QProcess::ExitStatus status);
    void fail(const QString& message);
    void publish();
    void saveState();
    void restoreState();
    void maintenance(const QString& command);
    SoftwareUpdateSource source_;
    QProcess worker_;
    QTimer periodic_, killTimer_, deadline_, persistTimer_;
    QByteArray output_;
    QJsonObject result_, candidate_;
    QString channel_, status_ = "unconfigured", phase_, error_, lastChecked_, stopReason_, notifiedVersion_, notificationVersion_;
    QString operation_, confirmation_, expectedData_, expectedExecutable_, persistenceError_;
    qint64 done_ = 0, total_ = 0;
    int focus_ = 0;
    bool configured_ = false, busy_ = false, preparing_ = false, background_ = false;
    bool backgroundChecks_ = true, notesOpen_ = false, hasTask_ = false, protocolError_ = false;
};
}
