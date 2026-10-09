#pragma once
#include "core/repository/LibraryRepository.h"
#include "core/repository/PokedexRepository.h"
#include "core/repository/HallOfFameRepository.h"

namespace trainer {
struct TrainerOverview {
    int adventures = 0, worlds = 0;
    std::optional<int> seen, caught, favorites, memories;
    std::optional<qint64> recordedSeconds;
};
// Cached local observations only; no external account or game-save interpretation.
TrainerOverview trainerOverview(const LibraryRepository&, const PokedexCatalog&,
                                const PokedexProgressRepository&, const HallOfFameRepository&);
}
