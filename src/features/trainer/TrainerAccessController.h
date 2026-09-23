#pragma once
#include "core/input/Action.h"
#include "core/storage/LocalStateStore.h"
#include <QObject>
#include <QStringList>

namespace trainer {
class TrainerAccessController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool keypad READ keypad NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString description READ description NOTIFY changed)
    Q_PROPERTY(QString mask READ mask NOTIFY changed)
    Q_PROPERTY(int minimumDigits READ minimumDigits NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QStringList choices READ choices NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool canRecover READ canRecover NOTIFY changed)
public:
    explicit TrainerAccessController(LocalStateStore*, QObject* parent=nullptr);
    bool active() const { return !stage_.isEmpty(); }
    bool busy() const { return busy_; }
    bool keypad() const;
    QString title() const;
    QString description() const;
    QString mask() const { return QString(input_->size(),QChar(0x2022)); }
    int minimumDigits() const { return family_ || stage_=="parent" ? 6 : 4; }
    QString error() const { return error_; }
    QStringList choices() const;
    int focusIndex() const { return focus_; }
    bool canRecover() const { return stage_=="unlock"; }
    void beginUnlock(const QString& id);
    void beginManage(bool family=false);
    void beginRemoval();
    void removalFailed(const QString&);
    void cancel() { if(!busy_)close(); }
    void dispatch(Action);
    Q_INVOKABLE void activate(int);
    Q_INVOKABLE void recover();
signals:
    void changed();
    void unlocked(const QString&);
    void removalRequested(trainer::SecretPin familyCode);
private:
    void move(const QString&);
    void close();
    void submit();
    void save();
    void completed(const QString&);
    LocalStateStore* store_;
    QString stage_, id_, name_, error_, operation_;
    bool busy_=false, family_=false;
    int focus_=0;
    SecretPin input_=emptyPin(), old_=emptyPin(), next_=emptyPin();
};
}
