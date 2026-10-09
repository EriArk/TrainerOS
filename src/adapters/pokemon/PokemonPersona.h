#pragma once
#include "features/trainer/TrainerController.h"
#include "adapters/pokemon/TrainerOverview.h"
#include "adapters/pokemon/pokedex/SpeciesPicker.h"
namespace trainer {
class PokemonPersona final : public TrainerController {
    Q_OBJECT
    Q_PROPERTY(trainer::SpeciesPicker* picker READ picker CONSTANT)
    Q_PROPERTY(QString draftFavorite READ draftFavorite NOTIFY changed)
    Q_PROPERTY(QVariantList overview READ overview NOTIFY changed)
public:
    explicit PokemonPersona(TrainerRepository&);
    void configure(LibraryRepository*,PokedexReferenceProvider*,PokedexProgressRepository*,HallOfFameRepository*);
    void refreshOverview();
    SpeciesPicker* picker() {return &picker_;}
    QString draftFavorite() const {return favoriteLabel(draft_.favoritePokemonId);}
    QVariantList overview() const;
protected:
    int extraRows() const override {return 1;}
    QVariantList extraEditRows() const override {return {QVariantMap{{"title","Favorite"},{"detail",draftFavorite()},{"kind","action"}}};}
    QVariantMap extraProfile(const TrainerProfile& profile) const override {return {{"favorite",favoriteLabel(profile.favoritePokemonId)}};}
    void activateExtra(int) override {picker_.begin(draft_.favoritePokemonId);}
    bool activateOverlay(int index) override {if(!picker_.isOpen())return false;picker_.activate(index);return true;}
    bool dispatchExtra(Action action) override {if(!picker_.isOpen())return false;picker_.dispatch(action);return true;}
    void cancelExtra() override {picker_.cancel();}
private:
    QString favoriteLabel(const QString&) const;
    SpeciesPicker picker_;
    LibraryRepository* library_=nullptr;
    PokedexReferenceProvider* reference_=nullptr;
    PokedexProgressRepository* journal_=nullptr;
    HallOfFameRepository* archive_=nullptr;
    TrainerOverview overview_;
    QHash<QString,QString> names_;
};
}
