#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantMap>
#include <QJsonObject>
#include <QFutureWatcher>
#include <QTemporaryDir>

namespace trainer {
class BackendAccount;
class TrainerController;
class OutfitController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap view READ view NOTIFY changed)
public:
    OutfitController(BackendAccount&,TrainerController&,QObject* parent=nullptr);
    ~OutfitController() override;
    QVariantMap view() const;
    void open();
    void close();
    void dispatch(Action);
    void acceptName(const QString&);
    Q_INVOKABLE void select(int);
    Q_INVOKABLE void activate(int);
    Q_INVOKABLE void confirm(bool);
    Q_INVOKABLE void refresh();
signals:
    void changed();
    void backRequested();
    void nameRequested(const QString&);
    void editRequested();
private:
    QVariantList rows() const;
    QVariantMap selected() const;
    QVariantList actions() const;
    void preview();
    void startPreview();
    void askName(bool rename);
    BackendAccount& account_;
    TrainerController& trainer_;
    bool open_=false,replace_=false;
    int index_=0,focus_=0;
    QString owner_,status_,previewUrl_,previewError_;
    QJsonObject previewAppearance_,pending_;
    QFutureWatcher<QJsonObject> job_;
    QTemporaryDir cache_;
    quint64 generation_=0,jobGeneration_=0;
};
}
