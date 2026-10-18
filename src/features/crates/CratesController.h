#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantList>
#include <QTimer>
#include "CratesCatalogue.h"

namespace trainer {
// The authored showcase is independent of the future remote project catalogue.
class CratesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList crates READ crates CONSTANT)
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
public:
    explicit CratesController(QObject* parent=nullptr);
    QVariantList crates() const { return crates_; }
    int focusIndex() const { return focus_; }
    QVariantMap current() const { return crates_.value(focus_).toMap(); }
    bool opened() const { return opened_; }
    bool opening() const { return opening_.isActive(); }
    QVariantList projects() const;
    int projectIndex() const;
    int projectCount() const;
    int page() const {return projectIndex()/6;}
    int pageCount() const {return (projectCount()+5)/6;}
    bool detailOpen() const {return detailOpen_;}
    QVariantMap project() const;
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
private:
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
