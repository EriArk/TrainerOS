#pragma once
#include "core/repository/PokedexRepository.h"
#include "core/input/Action.h"
#include <QVariantList>

namespace trainer {
class PokedexJournalEditor final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantList fields READ fields NOTIFY changed)
public:
    PokedexJournalEditor(PokedexProgressRepository& repository,QObject* parent=nullptr) : QObject(parent),repository_(repository) {}
    bool isOpen() const { return open_; }
    bool saving() const { return saving_; }
    QString name() const { return name_; }
    QString error() const { return error_; }
    int focusIndex() const { return focus_; }
    QVariantList fields() const;
    void begin(const QString& id,const QString& name);
    void cancel();
    void applyNote(const QString&);
    void dispatch(Action);
    Q_INVOKABLE void activate(int);
    Q_INVOKABLE void submit();
    Q_INVOKABLE void back() { if(!saving_)cancel(); }
signals:
    void changed();
    void noteRequested(const QString&);
    void saved();
    void messageRequested(const QString&);
private:
    PokedexProgressRepository& repository_;
    PokedexProgress draft_;
    QString id_,name_,error_;
    int focus_=0;
    bool open_=false,saving_=false;
};
}
