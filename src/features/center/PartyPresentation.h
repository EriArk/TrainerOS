#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QVariantList>

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
public:
    explicit PartyPresentation(bool sample, QObject* parent = nullptr) : QObject(parent), sample_(sample) {}
    QString section() const { return section_; }
    bool detailOpen() const { return detail_; }
    bool sample() const { return sample_; }
    QString title() const { return title_; }
    QString status() const;
    QVariantList entries() const;
    QVariantMap detail() const;
    int focusIndex() const { return section_ == "party" ? partyFocus_ : storageFocus_[box_]; }
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
};
}
