#include "pokemon/PokemonExperience.h"
#include "core/navigation/ShellController.h"
#include "generic/GenericExperience.h"
namespace trainer {
QVariantMap builtinExperienceIdentity(const QString& platform,const QString& path) {return observePokemonIdentity(platform,path);}
PokemonExperience& pokemonModule(ShellController& shell) {return *static_cast<PokemonExperience*>(shell.module("pokemon"));}

ExperienceFactory builtinExperiences(PokedexReferenceProvider& reference,PokedexProgressRepository& progress) {
    return [&reference,&progress](ExperienceServices services){
        ExperienceModules modules;
        modules.push_back(std::make_unique<PokemonExperience>(services,reference,progress));
        modules.push_back(std::make_unique<GenericExperience>(services));
        return modules;
    };
}
}
