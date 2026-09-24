#pragma once
#include "core/input/Action.h"
#include "core/repository/LibraryRepository.h"
#include <QObject>
#include <QVariantList>

namespace trainer {
// Small, contextual library operations. Installation and save routing remain
// repository concerns; this controller never renames a ROM for a display edit.
class LibraryToolsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
public:
    explicit LibraryToolsController(LibraryRepository& repository, QObject* parent=nullptr):QObject(parent),repository_(repository){}
    bool isOpen() const {return !route_.isEmpty();}
    bool busy() const {return busy_;}
    QString route() const {return route_;}
    QString title() const;
    QString detail() const;
    QString error() const {return error_;}
    QVariantList rows() const;
    int focusIndex() const {return focus_;}
    void beginGame(const QString& id);
    void beginWorld(const QString& id, bool enabled);
    void beginTrash();
    void close();
    void dispatch(Action);
    void activate(int);
    void applyText(const QString&);
signals:
    void changed();
    void saved();
    void textRequested(const QString& title,const QString& initial,int limit);
private:
    void submit(LibraryEdit);
    LibraryRepository& repository_;
    QString route_,error_;
    AdventureRegistration game_;
    World world_;
    QList<World> destinations_;
    QList<AdventureRegistration> trash_;
    int focus_=0;
    bool busy_=false;
};
}
