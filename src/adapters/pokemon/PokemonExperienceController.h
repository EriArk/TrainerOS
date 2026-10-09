#pragma once
#include "core/input/Action.h"
#include "core/experience/ExperienceNavigation.h"
#include <QString>
#include <functional>
#include <QJsonObject>
#include "core/model/Models.h"
#include <QVariantMap>

namespace trainer {
const ExperienceDescriptor& pokemonExperienceDescriptor();
class PokedexController;
class SaveCenterController;
class PartyPresentation;
class GameProgressProvider;
// Built-in presenter glue only. The referenced controllers retain exact-build,
// owner, read-only, active-game and protected transaction authorization.
class PokemonExperienceController {
public:
    PokemonExperienceController(PokedexController&,SaveCenterController&,PartyPresentation&);
    QString face() const {return face_;}
    QString centerRoute() const {return centerRoute_;}
    void setCenterRoute(const QString&);
    void restoreFace(const QString&);
    void show(const QString& face,const QString& adventure);
    void dispatch(Action,const QString& adventure);
    void activate(int index,const QString& area,const QString& adventure);
    int focusIndex() const;
    QJsonObject navigation() const;
    void restoreNavigation(const QJsonObject&);
    QVariantMap homeProgress(const std::optional<Adventure>&,const HomeSnapshot&,GameProgressProvider*) const;
private:
    PokedexController& dex_;
    SaveCenterController& center_;
    PartyPresentation& party_;
    QString face_="dex",centerRoute_="clinic",playroomRoute_="playroom";
};
}
