#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantList>
#include <QRect>
#include <QMap>

namespace trainer {
// Host-owned layout/rendering. External widgets supply plain data, never QML.
class ControlsDashboard final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList widgets READ widgets NOTIFY layoutChanged)
    Q_PROPERTY(QVariantList available READ available NOTIFY changed)
    Q_PROPERTY(QString selected READ selected NOTIFY changed)
    Q_PROPERTY(bool editing READ editing NOTIFY changed)
    Q_PROPERTY(bool moving READ moving NOTIFY changed)
    Q_PROPERTY(bool switching READ switching NOTIFY changed)
    Q_PROPERTY(int switchIndex READ switchIndex NOTIFY changed)
    Q_PROPERTY(bool picking READ picking NOTIFY changed)
    Q_PROPERTY(bool adjusting READ adjusting NOTIFY changed)
    Q_PROPERTY(int page READ page NOTIFY changed)
    Q_PROPERTY(int pages READ pages NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString selectedTitle READ selectedTitle NOTIFY changed)
    Q_PROPERTY(bool widgetSelected READ widgetSelected NOTIFY changed)
public:
    explicit ControlsDashboard(QObject* parent=nullptr);
    QVariantList widgets() const;
    QVariantList available() const;
    QString selected() const{return selected_;}
    bool editing() const{return editing_;}
    bool moving() const{return moving_;}
    bool switching() const{return switching_;}
    int switchIndex() const{return switchIndex_;}
    bool picking() const{return picking_;}
    bool adjusting() const{return adjusting_;}
    int page() const{return page_;}
    int pages() const;
    QString status() const{return status_;}
    QString selectedTitle() const;
    bool widgetSelected() const;
    void setOwner(const QString& owner);
    void setExternalWidgets(const QVariantList& widgets);
    void begin();
    void leave();
    void dispatch(Action);
    Q_INVOKABLE void back() { dispatch(Action::Back); }
    Q_INVOKABLE void select(QString id);
    Q_INVOKABLE void activate(QString id);
    Q_INVOKABLE void edit();
    Q_INVOKABLE void audioOutputs();
    Q_INVOKABLE void pick();
    Q_INVOKABLE void restore(QString id);
    Q_INVOKABLE void hide();
    Q_INVOKABLE void resize();
    Q_INVOKABLE void changePage(int delta);
    Q_INVOKABLE void drop(QString id,int x,int y);
signals:
    void changed();
    void layoutChanged();
    void actionRequested(QString id);
    void contributionActivated(QString id);
    void adjustRequested(QString id,int delta);
    void closeRequested();
private:
    struct Widget {QString id;QRect rect;int page=0;bool hidden=false;};
    QList<Widget> items_;
    QVariantList publishedLayout_;
    QMap<QString,QVariantMap> external_;
    QVariantMap externalLayout_;
    QString key_,selected_="volume",status_;
    int page_=0,pickIndex_=0,switchIndex_=0;
    bool switching_=false;
    bool editing_=false,moving_=false,picking_=false,adjusting_=false;
    Widget original_;
    QList<Widget> moveOriginal_;
    static QList<Widget> defaults();
    QList<QSize> sizes(const QString& id) const;
    bool place(const Widget& candidate);
    bool fits(const Widget&,const QList<Widget>&) const;
    Widget* find(const QString&);
    void save();
    void normalize();
    void move(int dx,int dy);
    void navigate(int dx,int dy);
};
}
