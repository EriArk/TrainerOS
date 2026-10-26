#pragma once
#include "core/input/Action.h"
#include "core/repository/LibraryRepository.h"
#include "platform/process/ProcessService.h"
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

namespace trainer {
// Trainer + stable game identity; host tuning is a lease of the actual session.
class GamePerformance final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool live READ live NOTIFY changed)
    Q_PROPERTY(bool applying READ applying NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY changed)
    Q_PROPERTY(QVariantMap capabilities READ capabilities NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int page READ page NOTIFY changed)
    Q_PROPERTY(bool proton READ proton NOTIFY changed)
public:
    explicit GamePerformance(LibraryRepository&,QObject* parent=nullptr);
    void configure(const QString& directory,const QString& owner);
    Q_INVOKABLE void begin(const QString& game);
    Q_INVOKABLE void close();
    Q_INVOKABLE void selectPage(int);
    Q_INVOKABLE void adjust(int row,int direction);
    Q_INVOKABLE void setGpuClock(int value);
    Q_INVOKABLE void activate(int row);
    Q_INVOKABLE void save();
    Q_INVOKABLE void reset();
    void dispatch(Action);
    bool isOpen() const{return !game_.isEmpty();}
    bool busy() const{return applying_ || probe_.state()!=QProcess::NotRunning;}
    bool applying() const{return applying_;}
    bool live() const{return liveAvailable_ && !game_.isEmpty() && game_==sessionGame_ && owner_==sessionOwner_;}
    void setLiveAvailable(bool);
    std::function<bool(const QString&,const QJsonObject&,std::function<void(QJsonObject)>)> applyLive;
    QString title() const{return title_;}
    QString status() const{return status_;}
    QVariantMap draft() const{return draft_.toVariantMap();}
    QVariantMap capabilities() const{return caps_.toVariantMap();}
    QVariantList rows() const;
    int focusIndex() const{return focus_;}
    int page() const{return page_;}
    bool proton() const{return proton_;}
    QJsonObject snapshot(const QString& game) const;
    ProcessCommand prepare(ProcessCommand command,const QString& game);
    static bool valid(const QJsonObject&);
    // Pure capability injection for host-independent UI/contract tests.
    void setCapabilities(const QJsonObject& caps){caps_=caps;emit changed();}
signals:
    void changed();
    void compatibilityRequested(QString game);
private:
    QString path(const QString&) const;
    LibraryRepository& library_;
    QString directory_,owner_,game_,title_,status_;
    QString sessionGame_,sessionOwner_;
    quint64 editRevision_=0;
    QJsonObject draft_,caps_;
    int page_=0,focus_=0;
    bool proton_=false,pc_=false;
    bool applying_=false,liveAvailable_=false;
    QProcess probe_;
    QTimer timeout_;
};
}
