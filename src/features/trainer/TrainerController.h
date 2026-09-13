#pragma once
#include "core/input/Action.h"
#include "core/repository/TrainerRepository.h"
#include "features/pokedex/SpeciesPicker.h"
#include "TrainerOverview.h"
#include <QObject>
#include <QVariantMap>
#include <QStringList>

namespace trainer {
class TrainerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exists READ exists NOTIFY changed)
    Q_PROPERTY(bool editing READ editing NOTIFY changed)
    Q_PROPERTY(bool saving READ saving NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(QVariantMap profile READ profile NOTIFY changed)
    Q_PROPERTY(QString draftName READ draftName NOTIFY changed)
    Q_PROPERTY(QString draftEmblem READ draftEmblem NOTIFY changed)
    Q_PROPERTY(QString draftFavorite READ draftFavorite NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(trainer::SpeciesPicker* picker READ picker CONSTANT)
    Q_PROPERTY(QVariantList overview READ overview NOTIFY changed)
public:
    static constexpr int NameLimit = 24;
    explicit TrainerController(TrainerRepository&, QObject* parent = nullptr);
    bool exists() const { return profile_.has_value(); }
    bool editing() const { return editing_; }
    bool saving() const { return saving_; }
    void reload();
    void configure(LibraryRepository*, PokedexReferenceProvider*, PokedexProgressRepository*, HallOfFameRepository*);
    void refreshOverview();
    QVariantList overview() const;
    SpeciesPicker* picker() { return &picker_; }
    int focusIndex() const { return focus_; }
    QVariantMap profile() const;
    QString draftName() const { return draft_.name; }
    QString draftEmblem() const { return draft_.emblemId; }
    QString draftFavorite() const { return favoriteLabel(draft_.favoritePokemonId); }
    QString error() const { return error_; }
    void beginEdit();
    void cancel();
    void setDraftName(const QString& name);
    void dispatch(Action);
    Q_INVOKABLE void activate(int index);
signals:
    void changed();
    void nameRequested(const QString& initial);
    void messageRequested(const QString& message);
private:
    void save();
    QString favoriteLabel(const QString& id) const;
    TrainerRepository& repository_;
    SpeciesPicker picker_;
    LibraryRepository* library_ = nullptr;
    PokedexReferenceProvider* reference_ = nullptr;
    PokedexProgressRepository* journal_ = nullptr;
    HallOfFameRepository* archive_ = nullptr;
    TrainerOverview overview_;
    QHash<QString, QString> names_;
    std::optional<TrainerProfile> profile_;
    TrainerProfile draft_;
    bool editing_ = false;
    bool saving_ = false;
    int focus_ = 0;
    QString error_;
};
}
