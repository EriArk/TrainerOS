#pragma once
#include "core/input/Action.h"
#include "core/model/SaveBackup.h"
#include "core/repository/LibraryRepository.h"
#include <QVariantList>

namespace trainer {
class SaveCenterController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canCreate READ canCreate NOTIFY changed)
    Q_PROPERTY(bool confirming READ confirming NOTIFY changed)
    Q_PROPERTY(bool companion READ companion NOTIFY changed)
    Q_PROPERTY(QString restoreLabel READ restoreLabel NOTIFY changed)
public:
    explicit SaveCenterController(LibraryRepository&,QObject* parent=nullptr);
    void configure(SaveBackupService*);
    bool configured() const { return service_ != nullptr; }
    void begin(const QString& preferred={});
    void beginSelected(const QString& adventureId);
    void close();
    void applySearch(const QString&);
    void dispatch(Action);
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void create();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void back();
    Q_INVOKABLE void search();
    QString route() const { return route_; }
    QString title() const;
    QString message() const;
    QString query() const { return query_; }
    QVariantList rows() const;
    int focusIndex() const { return focus_; }
    bool busy() const { return service_ && service_->busy(); }
    bool canCreate() const { return route_=="copies" && snapshot_.hasSave && !snapshot_.token.isEmpty() && !busy(); }
    bool confirming() const { return confirming_; }
    bool companion() const { return companion_; }
    QString restoreLabel() const;
signals:
    void changed();
    void rowsChanged();
    void closeRequested();
    void searchRequested(const QString&);
    void restored(const QString& adventureId);
    void messageRequested(const QString&);
private:
    void rebuild();
    void restore();
    void completed(const AdventureRegistration&,quint64,const SaveBackupResult&);
    LibraryRepository& library_;
    SaveBackupService* service_=nullptr;
    QList<Adventure> adventures_;
    AdventureRegistration selected_;
    SaveBackupSnapshot snapshot_;
    SaveBackup confirmation_;
    QString route_="adventures", query_, message_;
    bool open_=false, confirming_=false;
    bool companion_=false, refreshPending_=false;
    int focus_=0;
    quint64 generation_=0;
};
}
