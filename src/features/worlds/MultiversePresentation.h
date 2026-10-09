#pragma once
#include "core/input/Action.h"
#include "GameCollections.h"
#include <QObject>
#include <QVariantList>
#include <QHash>
#include <QJsonObject>
#include "core/repository/LibraryRepository.h"
#include "integrations/adventure/AdventureAdapter.h"

namespace trainer {
// View model over shared registrations/history; launch stays with the shell adapter.
class MultiversePresentation final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString collection READ collection NOTIFY changed)
    Q_PROPERTY(QString collectionName READ collectionName NOTIFY changed)
    Q_PROPERTY(QString collectionArt READ collectionArt NOTIFY changed)
    Q_PROPERTY(QVariantList collections READ collections NOTIFY libraryChanged)
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(QVariantList systems READ systems NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY changed)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(QString systemName READ systemName NOTIFY changed)
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(QString filterLabel READ filterLabel NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool sample READ sample CONSTANT)
public:
    explicit MultiversePresentation(bool sample, QObject* parent = nullptr);
    MultiversePresentation(LibraryRepository&, AdventureAdapter&, QObject* parent = nullptr);
    void refresh();
    QString collection() const { return collection_; }
    QString collectionName() const;
    QString collectionArt() const;
    GameCollections* collectionManager() const { return collections_; }
    QVariantMap game(const QString& id) const;
    QVariantList collections() const;
    void setCollection(const QString&);

    void showSystems();
    QJsonObject navigationState() const;
    void restoreNavigation(const QJsonObject&);
    bool sample() const { return sample_; }
    QString route() const { return route_; }
    QVariantList systems() const;
    QVariantList games() const;
    QVariantMap detail() const;
    QVariantMap selected() const;
    QVariantList choices() const;
    QString systemName() const;
    QString query() const { return queries_.value(system_); }
    QString filterLabel() const;
    int focusIndex() const;
    void applySearch(const QString&);
    void select(const QString&);
    void dispatch(Action);
    void activate(int);
signals:
    void changed();
    void gamesChanged();
    void libraryChanged();
    void searchRequested(const QString&);
    void homeRequested();
    void setupRequested(const QString& id);
    void messageRequested(const QString& message);
private:
    QJsonObject localNavigation() const;
    void restoreLocal(const QJsonObject&);
    GameCollections* collections_ = nullptr;
    QString collection_ = "multiverse";
    QJsonObject collectionStates_;
    struct Game { QString id, system, title; bool linked; QString series; };
    LibraryRepository* repository_ = nullptr;
    AdventureAdapter* adapter_ = nullptr;
    QVariantMap present(const Game&) const;
    QList<Game> filtered() const;
    bool belongs(const Game&) const;
    bool sample_;
    QString route_ = "systems", system_ = "gb", selected_;
    int systemFocus_ = 0;
    QHash<QString, QString> queries_;
    QHash<QString, int> filters_, positions_;
    QList<Game> entries_;
    mutable QHash<QString,QVariantMap> presentations_;
    mutable QVariantList systemsCache_, collectionsCache_;
    mutable bool systemsCached_ = false;
};
}
