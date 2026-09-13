#pragma once
#include <QJsonObject>
#include "core/input/Action.h"
#include "core/repository/HallOfFameRepository.h"
#include "integrations/achievements/AchievementProvider.h"
#include "ArchiveEditor.h"
#include <QVariantList>

namespace trainer {
class HallOfFameController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(QString zone READ zone NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int rowIndex READ rowIndex NOTIFY changed)
    Q_PROPERTY(bool archive READ isArchive NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY changed)
    Q_PROPERTY(QVariantList team READ team NOTIFY changed)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString emptyMessage READ emptyMessage NOTIFY changed)
    Q_PROPERTY(trainer::ArchiveEditor* editor READ editor CONSTANT)
    Q_PROPERTY(bool editable READ editable CONSTANT)
public:
    HallOfFameController(HallOfFameRepository&, AchievementProvider&, QObject* parent = nullptr);
    QString route() const { return route_; }
    QString zone() const { return zone_; }
    bool isArchive() const { return route_.startsWith("archive"); }
    int focusIndex() const;
    int rowIndex() const;
    QVariantList rows() const;
    QVariantMap detail() const;
    QVariantList team() const;
    QVariantList actions() const;
    QString status() const;
    QString emptyMessage() const;
    ArchiveEditor* editor() { return &editor_; }
    bool editable() const { return repository_.archiveEditable(); }
    void beginMemory(bool edit);
    void dispatch(Action);
    void activate(int index);
    void activateControl(const QString& zone, int index);
    void refreshArchive();
    QJsonObject navigationState() const;
    void restoreNavigation(const QJsonObject&);
signals:
    void changed();
    void rowsChanged();
    void messageRequested(const QString& message);
private:
    struct Row { QString id; QString title; QString subtitle; };
    QList<Row> currentRows() const;
    AchievementSet selectedSet() const;
    AchievementSnapshot currentSnapshot() const;
    AchievementSnapshot checkedSnapshot(const AchievementSet&) const;
    QString selectedRowId() const;
    void selectRow(const QString& id);
    void reconcile();
    void normalizeActions();
    void back();
    bool isDetail() const { return route_.endsWith("detail"); }
    HallOfFameRepository& repository_;
    AchievementProvider& provider_;
    ArchiveEditor editor_;
    QList<HallOfFameEntry> archive_;
    QString archiveError_;
    QString archiveId_, setId_;
    QHash<QString, QString> achievementIds_;
    QString route_ = "archive-list", zone_ = "list";
    int railFocus_ = 0, actionFocus_ = 0;
};
}
