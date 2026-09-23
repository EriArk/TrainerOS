#pragma once
#include "core/input/Action.h"
#include <QObject>
#include "core/storage/TrainerPin.h"
#include <QVariantList>
#include "core/model/Models.h"

namespace trainer {
// Shared registration/chooser presentation. SessionState binds live profile
// requests; the optional PIN rehearsal remains confined to development samples.
class TrainerSetupPresentation final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString description READ description NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString emblem READ emblem NOTIFY changed)
    Q_PROPERTY(QString favorite READ favorite NOTIFY changed)
    Q_PROPERTY(QString pinMask READ pinMask NOTIFY changed)
    Q_PROPERTY(QString pinChoice READ pinChoice NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(bool keypad READ keypad NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(bool live READ live NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    using QObject::QObject;
    bool live() const {return live_;}
    bool busy() const {return busy_;}
    void configure(const QList<TrainerProfile>& profiles, const QString& active);
    void setBusy(bool value) {busy_=value;emit changed();}
    void failed(const QString& error) {busy_=false;error_=error;emit changed();}
    QString stage() const { return stage_; }
    QString title() const;
    QString description() const;
    QString name() const { const auto p=selectedProfile(); return p ? p->name : name_; }
    QString emblem() const { const auto p=selectedProfile(); return p ? p->emblemId : emblem_; }
    QString favorite() const {
        const auto p=selectedProfile();
        if (!p) return favorite_;
        const auto id=p->favoritePokemonId;
        return id.isEmpty() ? QString("Not chosen") : id.left(1).toUpper()+id.mid(1);
    }
    QString pinMask() const { return QString(pin_->size(), QChar(0x2022)); }
    QString pinChoice() const { return pinChosen_ ? (live_ ? "PIN enabled" : "PIN chosen for preview") : "No PIN chosen"; }
    QString error() const { return error_; }
    bool keypad() const { return stage_ == "pin" || stage_ == "repeat" || stage_ == "unlock"; }
    int focusIndex() const { return focus_; }
    QVariantList rows() const;
    void begin();
    void beginStartup();
    void setFamilyReady(bool ready) { familyReady_=ready; }
    SecretPin registrationPin() const { return firstPin_; }
    void close();
    void applyName(const QString&);
    void dispatch(Action);
    Q_INVOKABLE void activate(int);
signals:
    void changed();
    void closeRequested();
    void nameRequested(const QString&);
    void createRequested(const trainer::TrainerProfile&);
    void selectRequested(const QString&);
private:
    const TrainerProfile* selectedProfile() const {
        return live_ && stage_=="chooser" && focus_>=0 && focus_<profiles_.size() ? &profiles_[focus_] : nullptr;
    }
    void moveTo(const QString&, int focus = 0);
    void back();
    QString stage_ = "menu", name_, emblem_ = "compass", favorite_ = "Not chosen";
    QString error_;
    SecretPin pin_=emptyPin(), firstPin_=emptyPin();
    bool startup_=false;
    bool familyReady_=false;
    bool pinChosen_ = false;
    int focus_ = 0;
    bool live_ = false, busy_ = false;
    QList<TrainerProfile> profiles_;
    QString active_;
};
}
