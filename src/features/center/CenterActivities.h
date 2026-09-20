#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantMap>

namespace trainer {
// Presentation rehearsal only: no save, transport, battle or asset provider.
class CenterActivities final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool sample READ sample CONSTANT)
    Q_PROPERTY(QString reaction READ reaction NOTIFY changed)
    Q_PROPERTY(QVariantMap page READ page NOTIFY changed)
public:
    explicit CenterActivities(bool sample, QObject* parent = nullptr) : QObject(parent), sample_(sample) {}
    QString route() const { return route_; }
    QString stage() const { return stage_; }
    int focusIndex() const { return route_ == "menu" ? menu_ : actor_; }
    bool sample() const { return sample_; }
    QString reaction() const { return reaction_; }
    QVariantMap page() const;
    void reset();
    void dispatch(Action);
    void activate(int);
signals:
    void changed();
    void closeRequested();
private:
    bool sample_;
    QString route_ = "menu", stage_ = "setup", reaction_;
    int menu_ = 0, actor_ = 0;
};
}
