#pragma once
#include "core/repository/LibraryRepository.h"
#include "features/library/FilePickerController.h"
#include "platform/files/FilesService.h"
#include "platform/process/ProcessService.h"
#include <QFutureWatcher>
#include "PcSteamBridge.h"

namespace trainer {
// User-added PC games use the shared library and shared Files task owner.
class PcGamesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString artwork READ artwork NOTIFY changed)
    Q_PROPERTY(bool editing READ editing NOTIFY changed)
    Q_PROPERTY(trainer::FilePickerController* picker READ picker CONSTANT)
public:
    PcGamesController(LibraryRepository&, FilesService&, QObject* parent=nullptr);
    ~PcGamesController() override;
    void configure(QString directory, QString owner);
    void setCatalog(FileCatalog* catalog) { picker_.setCatalog(catalog); }
    FilePickerController* picker() { return &picker_; }
    bool isOpen() const { return open_; }
    bool busy() const { return busy_; }
    QString stage() const { return stage_; }
    QString message() const { return message_; }
    QString title() const { return title_; }
    QVariantList rows() const;
    int focusIndex() const { return focus_; }
    double progress() const { return progress_; }
    QString artwork() const { return artwork_; }
    bool editing() const { return !editId_.isEmpty(); }
    bool installing() const { return installer_.active() || recoveredPid_>1; }
    std::function<QString()> installationGuard;
    std::function<QString()> settingsGuard;
    PcSteamBridge steam;
    QVariantList tasks() const;
    void taskCommand(const QString&);
    Q_INVOKABLE void begin();
    Q_INVOKABLE void edit(const QString& id);
    Q_INVOKABLE void close();
    Q_INVOKABLE void activate(int);
    void dispatch(Action);
    void applyText(const QString&);
    static QVariantMap inspect(const QString& root,bool installed=false);
    static QString fileKind(const QString& path,bool installer=false);
signals:
    void changed();
    void saved(QString id);
    void textRequested(QString title,QString initial,int limit);
    void installerStarted(qint64 pid,QString title);
    void installerFinished();
    void resumeInstaller();
    void controlsRequested(QString id);
private:
    void selected(const QString&);
    void inspectFolder(const QString&,bool copied);
    void copy();
    void scanComplete(const QVariantMap&,bool copied);
    void registerGame();
    void startInstaller();
    void saveSettings();
    void startMaintenance(const QString& operation,const QString& value={});
    void chooseRuntime();
    void steamAction(bool remove=false);
    void loadSteamUsers();
    void resetFlow();
    void persist();
    bool ownsDestination() const;
    QString prefix() const;
    qint64 liveInstaller() const;
    LibraryRepository& library_;
    FilesService& files_;
    FilePickerController picker_{nullptr};
    ProcessService installer_;
    QString directory_, owner_, stage_="source", message_, source_, destination_, installRoot_, job_, id_, title_, proton_;
    QVariantList candidates_;
    QVariantMap chosen_;
    QString mode_, format_, picking_, workingDirectory_, journal_, ownerKey_;
    QString artwork_, editId_, textField_, operation_, operationValue_, operationState_, steamUser_, steamStatus_, runtimeReturn_;
    QJsonObject runtimeConfig_;
    AdventureRegistration edited_;
    QVariantList runtimeRows_, steamUsers_;
    bool addSteam_=false, removeSteam_=false;
    double progress_=0;
    qint64 bytes_=0;
    qint64 recoveredPid_=0;
    int focus_=0, generation_=0;
    bool open_=false,busy_=false,writing_=false;
};
}
