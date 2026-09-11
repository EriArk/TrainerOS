#pragma once
#include "core/input/Action.h"
#include "core/repository/LibraryRepository.h"
#include "integrations/adventure/AdventureAdapter.h"
#include <QObject>
#include <QHash>
#include <QVariantList>
#include <QJsonObject>

namespace trainer {
class WorldsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int regionIndex READ regionIndex NOTIFY changed)
    Q_PROPERTY(int adventureIndex READ adventureIndex NOTIFY changed)
    Q_PROPERTY(QVariantList regions READ regions NOTIFY contentChanged)
    Q_PROPERTY(QVariantList adventures READ adventures NOTIFY contentChanged)
    Q_PROPERTY(QVariantMap region READ region NOTIFY changed)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY changed)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY changed)
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(QString filterLabel READ filterLabel NOTIFY changed)
public:
    WorldsController(LibraryRepository&, AdventureAdapter&, QObject* parent = nullptr);
    QString route() const;
    int focusIndex() const;
    int regionIndex() const;
    int adventureIndex() const;
    QVariantList regions() const;
    QVariantList adventures() const;
    QVariantMap region() const;
    QVariantMap detail() const;
    QVariantList actions() const;
    QString query() const { return queries_.value(worldId_); }
    QString filterLabel() const;
    void applySearch(const QString& text);
    void dispatch(Action);
    void activate(int index);
    void refresh();
    QJsonObject navigationState() const;
    void restoreNavigation(const QJsonObject&);
signals:
    void changed();
    void contentChanged();
    void messageRequested(const QString& message);
    void homeRequested();
    void setupRequested(const QString& adventureId);
    void searchRequested(const QString& initial);
private:
    enum class Route { Regions, Adventures, Detail };
    struct DetailAction { QString id; QString label; bool enabled; };
    QList<Adventure> currentAdventures() const;
    std::optional<Adventure> currentAdventure() const;
    std::optional<ResumePoint> latestResume(const Adventure&) const;
    QList<DetailAction> detailActions() const;
    void back();
    void openRegion();
    void openDetail();
    void executeAction(int index);
    void chooseAdventure(int index);
    void normalizeActionFocus();
    void updateFilter();
    LibraryRepository& repository_;
    AdventureAdapter& adapter_;
    QList<World> worlds_;
    QList<Adventure> adventures_;
    QString worldId_;
    QHash<QString, QString> rememberedAdventures_;
    QHash<QString, QString> queries_, searchText_;
    QHash<QString, int> filters_;
    Route route_ = Route::Regions;
    bool backFocused_ = false;
    int actionFocus_ = 0;
};
}
