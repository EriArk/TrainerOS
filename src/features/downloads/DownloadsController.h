#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantList>
#include <QMap>

namespace trainer {
// Provider-neutral presentation and command routing. Providers own their safe
// scheduling, cancellation and storage boundaries; no network logic lives here.
class DownloadsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(QVariantList tasks READ tasks NOTIFY changed)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int actionIndex READ actionIndex NOTIFY changed)
    Q_PROPERTY(bool actionsFocused READ actionsFocused NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
public:
    explicit DownloadsController(QObject* parent=nullptr):QObject(parent){}
    void publish(const QString& provider,const QVariantList& tasks);
    bool isOpen() const{return open_;}
    QVariantList tasks() const;
    QVariantList actions() const;
    QString summary() const;
    int focusIndex() const;
    int actionIndex() const{return action_;}
    bool actionsFocused() const{return pane_;}
    void dispatch(Action);
    Q_INVOKABLE void begin();
    Q_INVOKABLE void close();
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void controlAll(const QString& command);
signals:
    void changed();
    void commandRequested(QString provider,QString task,QString command);
private:
    QMap<QString,QVariantList> sources_;
    QString selected_;
    int action_=0;
    bool open_=false,pane_=false;
};
}
