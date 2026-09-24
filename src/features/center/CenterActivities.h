#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantMap>

namespace trainer {
// Read-only Party scene. Practice and Link retain their separate capability gates.
class CenterActivities final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool sample READ sample CONSTANT)
    Q_PROPERTY(QString reaction READ reaction NOTIFY changed)
    Q_PROPERTY(QVariantMap page READ page NOTIFY changed)
    Q_PROPERTY(QVariantList actors READ actors NOTIFY actorsChanged)
    Q_PROPERTY(bool hasParty READ hasParty NOTIFY changed)
    Q_PROPERTY(int reactionSerial READ reactionSerial NOTIFY changed)
    Q_PROPERTY(QString gesture READ gesture NOTIFY changed)
public:
    explicit CenterActivities(bool sample, QObject* parent = nullptr) : QObject(parent), sample_(sample) {}
    QString route() const { return route_; }
    QString stage() const { return stage_; }
    int focusIndex() const { return route_ == "menu" ? menu_ : actor_; }
    bool sample() const { return sample_; }
    QString reaction() const { return reaction_; }
    QVariantMap page() const;
    QVariantList actors() const { return actors_; }
    bool hasParty() const { return !actors_.isEmpty(); }
    int reactionSerial() const { return reactionSerial_; }
    QString gesture() const { return gesture_; }
    void setParty(const QVariantList&, const QString& source, const QString& unavailable);
    void reset();
    void dispatch(Action);
    Q_INVOKABLE void activate(int);
signals:
    void changed();
    void actorsChanged();
    void reactionRequested(int actor, int partner, const QString& gesture);
    void closeRequested();
    void shopsRequested();
private:
    void react(const QString& gesture);
    bool sample_;
    QString route_ = "menu", stage_ = "setup", reaction_;
    int menu_ = 0, actor_ = 0;
    QVariantList actors_;
    QString source_, unavailable_, gesture_;
    int reactionSerial_ = 0;
};
}
