#pragma once
#include "core/input/Action.h"
#include "core/repository/PreferencesRepository.h"
#include <QVariantList>

namespace trainer {
class SettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme NOTIFY changed)
    Q_PROPERTY(bool reducedMotion READ reducedMotion NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(bool creditsOpen READ creditsOpen NOTIFY changed)
public:
    using QObject::QObject;
    void setRepository(PreferencesRepository* repository) { repository_ = repository; reload(); }
    void reload();
    void begin() { focus_ = 0; credits_ = false; emit changed(); }
    bool creditsOpen() const { return credits_; }
    QString theme() const { return value_.theme; }
    bool reducedMotion() const { return value_.reducedMotion; }
    bool saving() const { return saving_; }
    QString error() const { return error_; }
    int focusIndex() const { return focus_; }
    QVariantList rows() const;
    void dispatch(Action);
    void activate(int index);
signals:
    void changed();
    void closeRequested();
    void deviceRequested();
    void trainerRequested();
    void messageRequested(const QString& message);
private:
    PreferencesRepository* repository_ = nullptr;
    ShellPreferences value_;
    int focus_ = 0;
    bool saving_ = false;
    bool credits_ = false;
    QString error_;
};
}
