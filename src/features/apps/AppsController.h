#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QSet>
#include <functional>

namespace trainer {
class AppsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantList rail READ rail NOTIFY changed)
    Q_PROPERTY(QVariantList menu READ menu NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int menuIndex READ menuIndex NOTIFY changed)
    Q_PROPERTY(bool railFocused READ railFocused NOTIFY changed)
    Q_PROPERTY(bool hiddenView READ hiddenView NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString current READ current NOTIFY changed)
    Q_PROPERTY(QString currentName READ currentName NOTIFY changed)
    Q_PROPERTY(bool controlsOpen READ controlsOpen NOTIFY changed)
    Q_PROPERTY(int controlIndex READ controlIndex NOTIFY changed)
public:
    explicit AppsController(QObject* parent=nullptr);
    ~AppsController() override;
    void configure(const QString& directory,const QString& owner);
    void start(const QString& python,const QString& helper);
    void accept(const QJsonObject&); // Platform events, also usable by isolated test providers.
    void dispatch(Action,bool quick=false);
    std::function<QString()> launchGuard;
    QVariantList rows() const;
    QVariantList rail() const;
    QVariantList menu() const;
    int focusIndex() const{return railFocused_?railFocus_:focus_;}
    int menuIndex() const{return menuFocus_;}
    bool railFocused() const{return railFocused_;}
    bool hiddenView() const{return hidden_;}
    bool ready() const{return ready_;}
    QString status() const{return status_;}
    QString current() const{return current_;}
    qint64 processId() const{return processId_;}
    QString currentName() const;
    bool controlsOpen() const{return controls_;}
    int controlIndex() const{return controlFocus_;}
    Q_INVOKABLE void refresh();
    void focusRail() { railFocused_=true;normalize();emit changed(); }
    Q_INVOKABLE void select(int index,bool rail=false);
    Q_INVOKABLE void open(int index,bool rail=false);
    Q_INVOKABLE void options(int index,bool rail=false);
    Q_INVOKABLE void menuAction(int index);
    Q_INVOKABLE void showHidden();
    Q_INVOKABLE void showControls();
    Q_INVOKABLE void control(int index);
    void leave();
signals:
    void changed();
    void commandRequested(QJsonObject);
    void foregroundRequested();
    void shellRequested();
    void homeRequested();
private:
    void send(const QJsonObject&);
    void normalize();
    bool persist(const QSet<QString>& favorites,const QSet<QString>& hidden);
    QVariantMap decorated(const QVariantMap&) const;
    QString selected() const;
    QProcess process_;
    QTimer heartbeat_;
    QByteArray buffer_;
    QVariantList catalogue_;
    QSet<QString> favorites_,hiddenIds_,running_;
    QString preferences_,menuId_,current_,status_="Applications are available on Armada.",launching_,python_,helper_;
    int focus_=0,railFocus_=0,menuFocus_=0,controlFocus_=0;
    qint64 processId_=0;
    bool railFocused_=false,hidden_=false,ready_=false,controls_=false;
};
}
