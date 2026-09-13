#pragma once
#include "core/input/Action.h"
#include "integrations/achievements/AchievementProvider.h"
#include <QVariantList>

namespace trainer {
class AchievementAccountController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY changed)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
public:
    explicit AchievementAccountController(AchievementProvider&, QObject* parent = nullptr);
    bool isOpen() const { return open_; }
    bool available() const { return provider_.canManageAccount(); }
    bool busy() const { return provider_.accountBusy(); }
    int focusIndex() const { return focus_; }
    QString status() const;
    QVariantList rows() const;
    void begin();
    void close();
    void dispatch(Action);
    void activate(int index);
    void applyText(const QString&);
signals:
    void changed();
    void textRequested(const QString& title, const QString& initial, int limit, bool secret);
private:
    bool connected() const { return !provider_.context().accountId.isEmpty(); }
    AchievementProvider& provider_;
    QString username_, password_;
    int focus_ = 0;
    bool open_ = false, passwordField_ = false, confirmSignOut_ = false;
};
}
