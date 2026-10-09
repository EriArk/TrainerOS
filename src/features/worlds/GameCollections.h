#pragma once
#include "core/repository/LibraryRepository.h"
#include "core/input/Action.h"
#include <QJsonArray>
#include <QObject>
#include <QSet>
#include <QVariantList>

namespace trainer {
// Personal views over stable game IDs. Collection edits never mutate a ROM/save.
class GameCollections final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
public:
    GameCollections(LibraryRepository& library, QObject* parent=nullptr):QObject(parent),library_(library){}
    void configure(const QString& directory,const QString& owner);
    void refresh();
    QJsonArray definitions() const { return definitions_; }
    bool contains(const QString& collection,const Adventure& game) const;
    bool known(const QString& id) const;
    bool played(const QString& id) const { return played_.contains(id); }
    QVariantList automatic() const;
    bool isOpen() const { return !route_.isEmpty(); }
    QString title() const;
    QString detail() const;
    QString error() const { return error_; }
    QVariantList rows() const;
    int focusIndex() const { return focus_; }
    void begin(const QString& id={});
    void beginMembership(const QString& game);
    void close();
    void dispatch(Action);
    void activate(int);
    void applyText(const QString&);
signals:
    void changed();
    void definitionsChanged();
    void textRequested(QString title,QString initial,int limit);
private:
    bool save(const QJsonArray&);
    void edit(const QJsonObject&);
    QStringList options(const QString& field) const;
    QList<Adventure> candidates() const;
    LibraryRepository& library_;
    QJsonArray definitions_;
    QSet<QString> played_;
    QString file_,route_,error_,field_,query_,game_;
    bool writable_=true;
    QJsonObject draft_;
    int focus_=0;
};
}
