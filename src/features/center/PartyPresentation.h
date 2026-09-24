#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantList>
#include "CenterActivities.h"
#include "features/pokedex/ClassicArt.h"
#include "features/pokedex/SpriteArt.h"

namespace trainer {
// Read-only P1 presentation. Never resolves or writes an external save.
class PartyPresentation final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString section READ section NOTIFY changed)
    Q_PROPERTY(bool detailOpen READ detailOpen NOTIFY changed)
    Q_PROPERTY(bool sample READ sample CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int box READ box NOTIFY changed)
    Q_PROPERTY(bool activitiesFocused READ activitiesFocused NOTIFY changed)
    Q_PROPERTY(bool boxFocused READ boxFocused NOTIFY changed)
    Q_PROPERTY(int menuIndex READ menuIndex NOTIFY changed)
    Q_PROPERTY(trainer::CenterActivities* activities READ activities CONSTANT)
public:
    explicit PartyPresentation(bool sample, QObject* parent = nullptr);
    QString section() const { return section_; }
    bool detailOpen() const { return detail_; }
    bool sample() const { return sample_; }
    QString title() const { return title_; }
    QString status() const;
    QVariantList entries() const;
    QVariantMap detail() const;
    int focusIndex() const { return section_ == "activities" ? activities_.focusIndex() : activitiesFocus_ ? -1 : section_ == "party" ? partyFocus_ : storageFocus_[box_]; }
    bool activitiesFocused() const { return activitiesFocus_; }
    bool boxFocused() const { return boxFocus_; }
    int menuIndex() const { return menuIndex_; }
    void configureArtwork(ClassicArt* art, SpriteArt* sprites);
    Q_INVOKABLE void changeBox(int delta);
    CenterActivities* activities() { return &activities_; }
    void openActivities();
    int box() const { return box_; }
    void setAdventure(const QString& id, const QString& title);
    void dispatch(Action);
    void activate(int);
    void openSaves();
    void returnFromSaves();
signals:
    void changed();
private:
    QVariantMap slot(int) const;
    bool sample_, detail_ = false;
    QString section_ = "party", previousSection_ = "party", id_, title_;
    int partyFocus_ = 0, storageFocus_[2] = {0,0}, box_ = 0;
    CenterActivities activities_;
    bool activitiesFocus_ = false;
    bool boxFocus_ = false;
    int menuIndex_ = 0;
    ClassicArt* art_ = nullptr;
    SpriteArt* sprites_ = nullptr;
    QString managementSection_ = "party";
};
}
