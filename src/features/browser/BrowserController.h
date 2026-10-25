#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantList>
#include <QPointer>
#include <QTimer>
#include <QPointF>

namespace trainer {
class FilesService;
class BrowserController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QObject* profile READ profile NOTIFY profileChanged)
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY changed)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY changed)
    Q_PROPERTY(QString selected READ selected NOTIFY changed)
    Q_PROPERTY(QString storagePath READ storagePath NOTIFY profileChanged)
    Q_PROPERTY(QVariantList downloads READ downloads NOTIFY downloadsChanged)
public:
    explicit BrowserController(QObject* parent=nullptr);
    ~BrowserController() override;
    bool available() const;
    QObject* profile() const{return profile_;}
    QVariantList tabs() const{return tabs_;}
    QVariantList bookmarks() const{return bookmarks_;}
    QVariantList history() const{return history_;}
    QString selected() const{return selected_;}
    QString storagePath() const{return path_;}
    QVariantList downloads() const;
    void configure(const QString& directory,const QString& owner);
    void enter();
    void ensureEngine();
    Q_INVOKABLE void openUrl(const QString& input);
    void dispatch(Action);
    void applyText(const QString&);
    static QString navigationUrl(QString input);
    Q_INVOKABLE QString newTab(QString url=QString());
    Q_INVOKABLE void select(const QString& id);
    Q_INVOKABLE void closeTab(const QString& id);
    Q_INVOKABLE void updateTab(const QString& id,const QString& url,const QString& title,const QString& icon,bool visited=false);
    Q_INVOKABLE void toggleBookmark(const QString& url,const QString& title);
    Q_INVOKABLE void removeHistory();
    Q_INVOKABLE void requestAddress(const QString& url);
    Q_INVOKABLE void copyText(const QString& text);
    Q_INVOKABLE void pointerEvent(QObject* item,qreal x,qreal y,int button=0);
    Q_INVOKABLE void wheel(QObject* item,qreal x,qreal y,qreal dx,qreal dy);
    Q_INVOKABLE void editPageField();
    Q_INVOKABLE void controlDownload(const QString& id,const QString& command);
    void moveDownloadBefore(const QString& id,const QString& before);
    void setDownloadDestination(const QString& id,const QString& folder);
    void setFilesService(FilesService* service);
signals:
    void changed();
    void profileChanging();
    void profileChanged();
    void downloadsChanged();
    void commandRequested(QString command);
    void textRequested(QString title,QString text,int limit,bool secret);
    void navigateRequested(QString url);
private:
    void persist();
    void persistDownloads();
    void acceptDownload(QObject* request);
    void pumpDownloads();
    void finishDownload(const QString& id);
    struct Download {QString id,name,path,state,error,destination,moveTask,moveTarget; qint64 received=0,total=-1; bool paused=false,queued=false; QPointer<QObject> request;};
    QPointer<FilesService> files_;
    QList<Download> jobs_;
    QVariantList tabs_,bookmarks_,history_;
    QString selected_,path_,owner_,textTarget_;
    QPointer<QObject> profile_,field_;
    QTimer saveTimer_,downloadSaveTimer_;
};
}
