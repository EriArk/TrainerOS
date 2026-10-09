#include "PokemonExperienceController.h"
#include "core/experience/ExperienceNavigation.h"
#include "adapters/pokemon/pokedex/PokedexController.h"
#include "adapters/pokemon/center/SaveCenterController.h"
#include "adapters/pokemon/center/PartyPresentation.h"
#include "core/model/GameProgressProvider.h"
#include "features/home/BadgeAssets.h"
#include <bit>

namespace trainer {
const ExperienceDescriptor& pokemonExperienceDescriptor() {
    static const ExperienceDescriptor value{"pokemon",1,"pokemon-home",
        {"Companions",{{"dex","Guide","pokemon-guide"},{"party","Party","pokemon-party"},
            {"boxes","Boxes","pokemon-boxes"},{"center","Center","pokemon-center"},
            {"playroom","Playroom","pokemon-playroom"},{"shops","Shops","pokemon-shops"}}},
        {"Trainer",{{"profile","Profile","pokemon-persona"},{"journey","Journey","pokemon-journey"},
            {"hall","Hall","pokemon-hall"},{"ra","RA","achievements"}}}};
    return value;
}

PokemonExperienceController::PokemonExperienceController(PokedexController& dex,
        SaveCenterController& center,PartyPresentation& party):dex_(dex),center_(center),party_(party) {}
QVariantMap PokemonExperienceController::homeProgress(const std::optional<Adventure>& game,const HomeSnapshot& home,GameProgressProvider* provider) const {
    std::optional<int> badges,caught;QVariantList badgeEntries;QString note,set;
    if(game) {
        badges=game->badges;caught=game->caught;
        if(game->id==home.activeAdventureId){if(!badges)badges=home.badges;if(!caught)caught=home.caught;}
        if(provider) {
            badges.reset();caught.reset();
            if(provider->adventureId()==game->id) {
                const auto progress=provider->snapshot();note=progress.message;
                if(progress.availability==ProgressAvailability::Available) {
                    set=progress.badgeSet;caught=progress.caught;badgeEntries=BadgeAssets::entries(set,progress.badgeMask);
                    if(progress.badgeMask)badges=std::popcount(static_cast<unsigned>(*progress.badgeMask)&255u);
                }
            }
        }
    }
    return {{"badges",badges?QString::number(*badges):QString::fromUtf8("—")},{"caught",caught?QString::number(*caught):QString::fromUtf8("—")},
        {"badgeSlots",badgeEntries},{"progressNote",note},{"badgeSet",set}};
}
void PokemonExperienceController::setCenterRoute(const QString& route) {
    centerRoute_=QStringList{"clinic","backups","link"}.contains(route)?route:"clinic";
}
void PokemonExperienceController::restoreFace(const QString& face) {
    face_="dex";
    for(const auto& candidate:pokemonExperienceDescriptor().first.faces)if(candidate.id==face){face_=face;break;}
}
void PokemonExperienceController::show(const QString& face,const QString& adventure) {
    if(face_=="playroom")playroomRoute_=party_.activities()->route()=="practice"?"practice":"playroom";
    if(face!=face_){party_.activities()->practice()->leave();party_.activities()->link()->leave();}
    restoreFace(face);
    center_.leaveClinic();center_.leaveShops();
    if(face_=="dex")return;
    center_.beginSelected(adventure);
    if(face_=="party" || face_=="boxes")party_.showSection(face_=="boxes"?"storage":"party");
    else if(face_=="center") {
        if(centerRoute_=="backups"){party_.openSaves();center_.refresh();}
        else if(centerRoute_=="link"){party_.showSection("activities");party_.activities()->showPlace("link");}
        else {party_.showSection("party");center_.visitClinic();}
    } else if(face_=="shops")center_.visitShops();
    else if(face_=="playroom"){party_.showSection("activities");party_.activities()->showPlace(playroomRoute_);}
}
int PokemonExperienceController::focusIndex() const {
    return face_=="dex"?dex_.focusIndex():(party_.section()=="saves" || center_.shopsOpen() || center_.clinicOpen()?center_.focusIndex():party_.focusIndex());
}
QJsonObject PokemonExperienceController::navigation() const {
    return {{"pokedex",dex_.navigationState()},{"party",party_.navigationState()},
        {"centerRoute",centerRoute_},{"playroomRoute",playroomRoute_}};
}
void PokemonExperienceController::restoreNavigation(const QJsonObject& state) {
    dex_.restoreNavigation(state["pokedex"].toObject());
    party_.restoreNavigation(state["party"].toObject());
    setCenterRoute(state["centerRoute"].toString());
    playroomRoute_=state["playroomRoute"].toString()=="practice"?"practice":"playroom";
}
void PokemonExperienceController::dispatch(Action action,const QString& adventure) {
    if(face_=="dex"){dex_.dispatch(action);return;}
    if(face_=="center" && center_.clinicOpen() && action==Action::LocalAction) {centerRoute_="backups";center_.leaveClinic();party_.openSaves();}
    else if(face_=="center" && center_.clinicOpen() && action==Action::Secondary) {centerRoute_="link";center_.leaveClinic();party_.showSection("activities");party_.activities()->showPlace("link");}
    else if(center_.clinicOpen()){if(action!=Action::Back)center_.dispatch(action);if(!center_.clinicOpen())center_.visitClinic();}
    else if(center_.shopsOpen()){center_.dispatch(action);if(!center_.shopsOpen())center_.visitShops();}
    else if(face_=="center" && centerRoute_=="link")party_.dispatch(action);
    else if(face_=="center" && action==Action::Back && !center_.confirming()){centerRoute_="clinic";show("center",adventure);}
    else if(party_.section()!="saves")party_.dispatch(action);
    else if(action==Action::Back && !center_.confirming() && !center_.busy()){centerRoute_="clinic";show("center",adventure);}
    else center_.dispatch(action);
}
void PokemonExperienceController::activate(int index,const QString& area,const QString& adventure) {
    if(face_=="dex") {
        if(area.isEmpty())dex_.activate(index);else dex_.activateControl(area,index);
    } else if(area=="party-activities")show("playroom",adventure);
    else if(center_.shopsOpen())center_.shopActivate(index);
    else if(center_.clinicOpen())center_.dispatch(Action::Confirm);
    else if(party_.section()=="saves")center_.activate(index);
    else party_.activate(index);
}
}
