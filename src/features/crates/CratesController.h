#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantList>
#include <QTimer>
#include <QJsonObject>
#include "CratesCatalogue.h"
#include "CratesInstaller.h"

namespace trainer {
// The authored showcase is independent of the future remote project catalogue.
class CratesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList crates READ crates NOTIFY catalogueChanged)
    Q_PROPERTY(QString syncStatus READ syncStatus NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(bool opened READ opened NOTIFY changed)
    Q_PROPERTY(bool opening READ opening NOTIFY changed)
    Q_PROPERTY(bool reducedMotion MEMBER reducedMotion_)
    Q_PROPERTY(QVariantList projects READ projects NOTIFY catalogueChanged)
    Q_PROPERTY(int projectIndex READ projectIndex NOTIFY changed)
    Q_PROPERTY(int projectCount READ projectCount NOTIFY catalogueChanged)
    Q_PROPERTY(int page READ page NOTIFY changed)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY catalogueChanged)
    Q_PROPERTY(bool detailOpen READ detailOpen NOTIFY changed)
    Q_PROPERTY(QVariantMap project READ project NOTIFY projectChanged)
    Q_PROPERTY(bool versionsOpen READ versionsOpen NOTIFY changed)
    Q_PROPERTY(int releaseIndex READ releaseIndex NOTIFY changed)
    Q_PROPERTY(int screenshotIndex READ screenshotIndex NOTIFY changed)
    Q_PROPERTY(QString primaryLabel READ primaryLabel NOTIFY projectChanged)
    Q_PROPERTY(bool primaryEnabled READ primaryEnabled NOTIFY projectChanged)
    Q_PROPERTY(bool actionsOpen READ actionsOpen NOTIFY changed)
    Q_PROPERTY(int actionIndex READ actionIndex NOTIFY changed)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY projectChanged)
    Q_PROPERTY(int updateCount READ updateCount NOTIFY projectChanged)
    Q_PROPERTY(bool updatePrompt READ updatePrompt NOTIFY changed)
    Q_PROPERTY(int updateChoice READ updateChoice NOTIFY changed)
public:
    explicit CratesController(QObject* parent=nullptr);
    ~CratesController() override;
    void configureCatalogue(const QString& helper,const QString& cache);
    void enter();
    QString syncStatus() const {return syncStatus_;}
    QVariantList crates() const { return crates_; }
    int focusIndex() const { return focus_; }
    QVariantMap current() const { return crates_.value(focus_).toMap(); }
    bool opened() const { return opened_; }
    bool opening() const { return opening_.isActive(); }
    QVariantList projects() const;
    QVariantList downloadTasks() const;
    int projectIndex() const;
    int projectCount() const;
    int page() const {return projectIndex()/6;}
    int pageCount() const {return (projectCount()+5)/6;}
    bool detailOpen() const {return detailOpen_;}
    QVariantMap project() const;
    CratesInstaller* installer(){return &installer_;}
    QString primaryLabel() const;
    bool primaryEnabled() const;
    bool actionsOpen() const{return actionsOpen_;}
    int actionIndex() const{return actionIndex_;}
    QVariantList actions() const;
    Q_INVOKABLE void primary();
    Q_INVOKABLE void installSelectedVersion();
    int updateCount() const;
    Q_INVOKABLE void updateAll();
    Q_INVOKABLE void confirmUpdates(bool accept);
    bool updatePrompt() const {return updatePrompt_;}
    int updateChoice() const {return updateChoice_;}
    Q_INVOKABLE void showActions();
    Q_INVOKABLE void activateAction(int index);
    bool versionsOpen() const {return versionsOpen_;}
    int releaseIndex() const {return releaseIndex_;}
    int screenshotIndex() const {return screenshotIndex_;}
    // Called only at a reviewed catalogue boundary, never from QML metadata.
    bool loadCatalogue(const QByteArray&);
    bool dispatch(Action action);
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void open(int index, bool reducedMotion=false);
    Q_INVOKABLE void back();
    Q_INVOKABLE void leave();
    Q_INVOKABLE void selectProject(int index);
    Q_INVOKABLE void openProject(int index);
    Q_INVOKABLE void closeDetail();
    Q_INVOKABLE void turnPage(int delta);
    Q_INVOKABLE void showVersions();
    Q_INVOKABLE void selectRelease(int index);
    Q_INVOKABLE void selectScreenshot(int index);
signals:
    void changed();
    void catalogueChanged();
    void projectChanged();
    void detailScrollRequested(int delta);
    void launchRequested(QString id,QString kind);
    void downloadsRequested();
private:
    void refreshCatalogue(bool network);
    bool applyCatalogue(const QJsonObject&);
    QProcess catalogueWorker_;
    QTimer catalogueDeadline_;
    QString catalogueHelper_,catalogueCache_,syncStatus_;
    QByteArray catalogueBuffer_;
    bool catalogueActive_=false,pendingSync_=false,networkSync_=false;
    CratesInstaller installer_{this};
    bool actionsOpen_=false,confirmRemove_=false;
    QString pendingVersion_;
    bool updatePrompt_=false;
    int updateChoice_=0;
    void requestPackage(const QString& version={});
    int actionIndex_=0;
    QVariantList crates_;
    int focus_=0;
    bool opened_=false;
    bool reducedMotion_=false;
    QTimer opening_;
    CratesCatalogue catalogue_;
    QHash<QString,QString> selectedProjects_;
    bool detailOpen_=false,versionsOpen_=false;
    int releaseIndex_=0,screenshotIndex_=0;
};
}
