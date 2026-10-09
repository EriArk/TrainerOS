#pragma once
#include "core/input/Action.h"
#include "core/repository/TrainerRepository.h"
#include <QObject>
#include <QVariantMap>
#include <QStringList>

namespace trainer {
class TrainerController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exists READ exists NOTIFY changed)
    Q_PROPERTY(bool editing READ editing NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY changed)
    Q_PROPERTY(QString draftName READ draftName NOTIFY changed)
    Q_PROPERTY(QString draftEmblem READ draftEmblem NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QVariantList editRows READ editRows NOTIFY changed)
public:
    static constexpr int NameLimit = 24;
    explicit TrainerController(TrainerRepository&, QObject* parent = nullptr);
    bool exists() const { return profile_.has_value(); }
    bool editing() const { return editing_; }
    bool saving() const { return saving_; }
    void reload();
    QVariantList editRows() const;
    int focusIndex() const { return focus_; }
    QVariantMap profile() const;
    QString draftName() const { return draft_.name; }
    QString draftEmblem() const { return draft_.emblemId; }
    QString error() const { return error_; }
    void beginEdit();
    void cancel();
    void setDraftName(const QString& name);
    void dispatch(Action, bool vertical = false);
    Q_INVOKABLE void activate(int index);
signals:
    void changed();
    void nameRequested(const QString& initial);
    void messageRequested(const QString& message);
private:
    void save();
protected:
    virtual int extraRows() const { return 0; }
    virtual QVariantList extraEditRows() const { return {}; }
    virtual QVariantMap extraProfile(const TrainerProfile&) const { return {}; }
    virtual void activateExtra(int) {}
    virtual bool dispatchExtra(Action) { return false; }
    virtual bool activateOverlay(int) { return false; }
    virtual void cancelExtra() {}
    TrainerRepository& repository_;
    std::optional<TrainerProfile> profile_;
    TrainerProfile draft_;
    bool editing_ = false;
    bool saving_ = false;
    int focus_ = 0;
    QString error_;
};
}
