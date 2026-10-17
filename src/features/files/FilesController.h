#pragma once
#include "core/input/Action.h"
#include "platform/files/FilesService.h"
#include <QSet>
namespace trainer {
class FilesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(QVariantList places READ places NOTIFY changed)
    Q_PROPERTY(QVariantList breadcrumbs READ breadcrumbs NOTIFY changed)
    Q_PROPERTY(QVariantList controls READ controls NOTIFY changed)
    Q_PROPERTY(QVariantList menu READ menu NOTIFY changed)
    Q_PROPERTY(QVariantMap view READ view NOTIFY changed)
    Q_PROPERTY(QVariantMap dialog READ dialog NOTIFY changed)
public:
    explicit FilesController(QObject* parent=nullptr);
    QVariantList rows() const;
    QVariantList places() const{return places_;}
    QVariantList breadcrumbs() const;
    QVariantList controls() const;
    QVariantList menu() const;
    QVariantMap view() const;
    QVariantMap dialog() const{return dialog_;}
    FilesService* service(){return &service_;}
    bool modal() const{return !dialog_.isEmpty();}
    bool menuOpen() const{return menuOpen_;}
    void configure(const QString& directory,const QString& owner);
    void enter();
    void leave();
    void dispatch(Action);
    void applyText(const QString&);
    Q_INVOKABLE void navigate(QString url);
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void context(int index=-1);
    Q_INVOKABLE void command(const QString& action);
    Q_INVOKABLE void answer(int index);
    Q_INVOKABLE void hideControls(bool hide);
    Q_INVOKABLE void focus(const QString& area,int index);
    Q_INVOKABLE void setColumns(int columns);
Q_SIGNALS:
    void changed();
    void rowsChanged();
    void textRequested(QString title,QString initial,int limit);
    void openRequested(QString url);
    void scrollRequested(int direction);
private:
    void load(const QString& url);
    void rebuild();
    void persist();
    QStringList selectedUrls() const;
    void operation(QString op,QStringList sources,QString destination={});
    FilesService service_;
    QVariantList entries_,rows_,places_;
    QString owner_,preferences_,url_,query_,error_,area_="files",textAction_,textBase_,menuUrl_;
    QStringList history_,clipboard_,pendingSources_;
    QSet<QString> selected_;
    QVariantMap dialog_;
    QString pendingOperation_,pendingDestination_;
    int historyIndex_=-1,index_=0,placeIndex_=0,controlIndex_=0,menuIndex_=0,dialogIndex_=0,columns_=5;
    bool busy_=false,grid_=true,hidden_=false,controls_=true,menuOpen_=false,cut_=false;
    QSet<QString> finished_;
};
}
