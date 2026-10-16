#pragma once
#include "core/input/Action.h"
#include "core/repository/LibraryRepository.h"
#include <QObject>
#include <QVariantList>
#include <QSet>

namespace trainer {
// Album of validated local media. Never captures, launches or modifies game data.
class ScreensController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QVariantList filters READ filters NOTIFY itemsChanged)
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int index READ index NOTIFY changed)
    Q_PROPERTY(bool filtering READ filtering NOTIFY changed)
    Q_PROPERTY(int filterFocus READ filterFocus NOTIFY changed)
    Q_PROPERTY(QString filterLabel READ filterLabel NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool favoritesView READ favoritesView NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit ScreensController(QObject* parent=nullptr):QObject(parent){}
    void setRepository(LibraryRepository* repository) {repository_=repository;}
    void configure(const QString& directory,const QString& owner);
    void setOwner(const QString& owner) {configure(directory_,owner);}
    void setMedia(const QVariantList& media);
    void dispatch(Action action);
    void forGame(const QString& id);
    QVariantList items() const {return items_;}
    QVariantList filters() const;
    QVariantMap current() const;
    int count() const {return items_.size();}
    int index() const {return index_;}
    bool filtering() const {return filtering_;}
    int filterFocus() const {return filterFocus_;}
    QString filterLabel() const;
    QString error() const {return error_;}
    bool favoritesView() const {return favoritesView_;}
    bool busy() const {return busy_;}
    Q_INVOKABLE void switchLibrary();
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void close();
    Q_INVOKABLE void toggleFavorite();
    Q_INVOKABLE void showFilters();
    Q_INVOKABLE void chooseFilter(int index);
signals:
    void changed();
    void itemsChanged();
private:
    void rebuild();
    QVariantList source_,items_;
    QSet<QString> favorites_;
    QString directory_,path_,owner_,filter_,selected_,error_;
    QString bufferSelection_,favoriteSelection_;
    LibraryRepository* repository_=nullptr;
    bool favoritesView_=false,busy_=false;
    int index_=0,filterFocus_=0;
    bool filtering_=false;
};
}
