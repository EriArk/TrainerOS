#pragma once
#include "core/input/Action.h"
#include "core/repository/PokedexRepository.h"
#include <QVariantList>

namespace trainer {
// A draft-only selector shared with profile editing. It never writes journal marks.
class SpeciesPicker final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY rowsChanged)
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit SpeciesPicker(QObject* parent = nullptr) : QObject(parent) {}
    void setReference(PokedexReferenceProvider* reference) { reference_ = reference; }
    bool isOpen() const { return open_; }
    int focusIndex() const { return focus_; }
    QVariantList entries() const;
    QString query() const { return query_; }
    QString error() const { return error_; }
    void begin(const QString& selected);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void requestSearch() { if (open_) emit searchRequested(query_); }
    Q_INVOKABLE void clearChoice() { if (open_) { cancel(); emit selected({}); } }
    void applySearch(const QString&);
    void dispatch(Action);
    Q_INVOKABLE void activate(int index);
signals:
    void changed();
    void rowsChanged();
    void selected(const QString& id);
    void searchRequested(const QString& initial);
private:
    void rebuild();
    PokedexReferenceProvider* reference_ = nullptr;
    QList<PokedexEntry> catalog_, filtered_;
    bool open_ = false;
    int focus_ = 0;
    QString query_, error_;
};
}
