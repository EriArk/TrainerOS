#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantList>
#include <QMap>
#include <QTimer>
#include <QElapsedTimer>
#include "features/library/FilePickerController.h"

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
    Q_PROPERTY(trainer::FilePickerController* picker READ picker CONSTANT)
public:
    explicit DownloadsController(QObject* parent=nullptr);
    FilePickerController* picker(){return &picker_;}
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
    Q_INVOKABLE void showActions(int index);
    Q_INVOKABLE void dismissActions();
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void controlAll(const QString& command);
    Q_INVOKABLE void command(const QString& key,const QString& command);
    Q_INVOKABLE void moveBefore(const QString& key,const QString& before);
signals:
    void changed();
    void commandRequested(QString provider,QString task,QString command);
    void reorderRequested(QString provider,QString task,QString before);
    void destinationRequested(QString provider,QString task,QString folder);
private:
    FilePickerController picker_{nullptr};
    QString pickerKey_;
    struct Sample {qint64 bytes=0,time=0; double speed=0; QVariantList history;};
    QMap<QString,Sample> samples_;
    QElapsedTimer clock_;
    QTimer sampleTimer_;
    QMap<QString,QVariantList> sources_;
    QString selected_;
    int action_=0;
    bool open_=false,pane_=false;
};
}
