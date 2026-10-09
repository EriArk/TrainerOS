#pragma once
#include "core/experience/ExperienceModule.h"
#include "adapters/pokemon/PokemonExperienceController.h"
#include "adapters/pokemon/pokedex/PokedexController.h"
#include "adapters/pokemon/center/SaveCenterController.h"
#include "adapters/pokemon/center/PartyPresentation.h"
#include "features/trainer/TrainerController.h"
#include "PokemonPersona.h"
#include "features/halloffame/HallOfFameController.h"
namespace trainer {
class ShellController;
class PokemonExperience final : public ExperienceModule {
    Q_OBJECT
    Q_PROPERTY(trainer::PokedexController* pokedex READ pokedex CONSTANT)
    Q_PROPERTY(trainer::SaveCenterController* center READ center CONSTANT)
    Q_PROPERTY(trainer::PartyPresentation* party READ party CONSTANT)
    Q_PROPERTY(QString face READ face NOTIFY changed)
    Q_PROPERTY(bool centerFace READ centerFace NOTIFY changed)
    Q_PROPERTY(bool historyFace READ historyFace NOTIFY changed)
    Q_PROPERTY(trainer::PokemonPersona* persona READ persona CONSTANT)
public:
    PokemonExperience(ExperienceServices,PokedexReferenceProvider&,PokedexProgressRepository&);
    ExperienceDescriptor descriptor() const override { return pokemonExperienceDescriptor(); }
    ExperienceManifest manifest() const override;
    QUrl presenter() const override { return QUrl("qrc:/TrainerOS/src/adapters/pokemon/PokemonExperienceViews.qml"); }
    PokedexController* pokedex() {return &dex_;}
    SaveCenterController* center() {return &center_;}
    PartyPresentation* party() {return &party_;}
    PokemonPersona* persona() {return &persona_;}
    QString face() const {return controller_.face();}
    bool centerFace() const {return slot_==0 && face()!="dex";}
    bool historyFace() const {return slot_==1 && view_!="profile";}
    void refresh(const ExperienceContext&) override;
    QVariantMap homeProgress() const override;
    QStringList capabilityNotes() const override {return center_.readOnly()?QStringList{"Save changes: Read-only"}:QStringList{};}
    void show(int,const QString&) override;
    void leave(bool) override;
    bool dispatch(Action) override;
    bool activate(int,const QString&) override;
    int focusIndex() const override;
    bool modalOpen() const override;
    bool navigationBlocked(bool) const override;
    bool activityBusy(bool recovery=false) const override {const auto* link=party_.activities()->link();return party_.activities()->practice()->running() || (link->active() && !(recovery && link->canBrowseForRecovery()));}
    bool writing() const override {return center_.writing() || persona_.saving();}
    bool recentsAllowed() const override {return !(slot_==0 && face()=="shops");}
    bool intercept(Action) override;
    QVariantMap invitation() const override;
    QJsonObject navigation() const override {return controller_.navigation();}
    void restoreNavigation(const QJsonObject& state) override {controller_.restoreNavigation(state);}
    QJsonObject legacyState() const override;
    QStringList restoreLegacy(const QJsonObject&) override;
    void reload() override {dex_.refresh();persona_.reload();persona_.refreshOverview();}
    void cancelText() override;
private:
    ExperienceServices services_;
    PokemonPersona persona_;
    PokedexController dex_;
    SaveCenterController center_;
    PartyPresentation party_;
    PokemonExperienceController controller_{dex_,center_,party_};
    int slot_=-1;
    QString view_;
    bool boxName_=false,progressConfigured_=false;
};
PokemonExperience& pokemonModule(ShellController&);
ExperienceFactory builtinExperiences(PokedexReferenceProvider&,PokedexProgressRepository&);
QVariantMap observePokemonIdentity(const QString& platform,const QString& path);
QVariantMap builtinExperienceIdentity(const QString& platform,const QString& path);
}
