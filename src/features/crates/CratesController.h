#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantList>
#include <QTimer>

namespace trainer {
// The authored showcase is independent of the future remote project catalogue.
class CratesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList crates READ crates CONSTANT)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(bool opened READ opened NOTIFY changed)
    Q_PROPERTY(bool opening READ opening NOTIFY changed)
    Q_PROPERTY(bool reducedMotion MEMBER reducedMotion_)
public:
    explicit CratesController(QObject* parent=nullptr);
    QVariantList crates() const { return crates_; }
    int focusIndex() const { return focus_; }
    QVariantMap current() const { return crates_.value(focus_).toMap(); }
    bool opened() const { return opened_; }
    bool opening() const { return opening_.isActive(); }
    bool dispatch(Action action);
    Q_INVOKABLE void select(int index);
    Q_INVOKABLE void open(int index, bool reducedMotion=false);
    Q_INVOKABLE void back();
    Q_INVOKABLE void leave();
signals:
    void changed();
private:
    QVariantList crates_;
    int focus_=0;
    bool opened_=false;
    bool reducedMotion_=false;
    QTimer opening_;
};
}
