#include "PokedexRepository.h"
#include <algorithm>

namespace trainer {
MockPokedexRepository::MockPokedexRepository() {
    catalog_.collections = {{"kanto", "Kanto"}, {"johto", "Johto"}, {"hoenn", "Hoenn"},
        {"sinnoh", "Sinnoh"}, {"unova", "Unova"}, {"kalos", "Kalos"}, {"alola", "Alola"},
        {"galar", "Galar"}, {"paldea", "Paldea"}};
    // Small text-only fixture, not a complete regional availability database.
    catalog_.entries = {
        {"bulbasaur", 1, "Bulbasaur", {"Grass", "Poison"}, {"kanto"}},
        {"charmander", 4, "Charmander", {"Fire"}, {"kanto"}},
        {"squirtle", 7, "Squirtle", {"Water"}, {"kanto"}},
        {"pikachu", 25, "Pikachu", {"Electric"}, {"kanto", "hoenn"}},
        {"jigglypuff", 39, "Jigglypuff", {"Normal", "Fairy"}, {"kanto", "hoenn"}},
        {"meowth", 52, "Meowth", {"Normal"}, {"kanto"}},
        {"eevee", 133, "Eevee", {"Normal"}, {"kanto", "johto"}},
        {"chikorita", 152, "Chikorita", {"Grass"}, {"johto"}},
        {"cyndaquil", 155, "Cyndaquil", {"Fire"}, {"johto"}},
        {"totodile", 158, "Totodile", {"Water"}, {"johto"}},
        {"treecko", 252, "Treecko", {"Grass"}, {"hoenn"}},
        {"torchic", 255, "Torchic", {"Fire"}, {"hoenn"}},
        {"mudkip", 258, "Mudkip", {"Water"}, {"hoenn"}},
        {"ralts", 280, "Ralts", {"Psychic", "Fairy"}, {"hoenn"}}
    };
    progress_.insert("bulbasaur", {true, true, false});
    progress_.insert("charmander", {true, false, false});
    progress_.insert("squirtle", {false, false, false});
    progress_.insert("pikachu", {true, true, true});
    progress_.insert("eevee", {true, false, true});
    progress_.insert("treecko", {true, true, false});
    progress_.insert("mudkip", {true, true, true});
    progress_.insert("ralts", {true, false, false});
}
PokedexCatalog MockPokedexRepository::load() {
    if (failLoad_) { failLoad_ = false; return {false, {}, {}, "The sample field guide couldn't be loaded. Try again."}; }
    return catalog_;
}
QString MockPokedexRepository::setFavorite(const QString& id, bool favorite) {
    if (std::none_of(catalog_.entries.begin(), catalog_.entries.end(), [&](const auto& e) { return e.id == id; }))
        return "This entry is no longer available.";
    if (failWrite_) { failWrite_ = false; return "Couldn't keep this favorite. Try again; the previous mark has been kept."; }
    progress_[id].favorite = favorite;
    return {};
}
}
