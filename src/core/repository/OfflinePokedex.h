#pragma once
#include "PokedexRepository.h"

namespace trainer {
class OfflinePokedex final : public PokedexReferenceProvider {
public:
    PokedexCatalog load() override;
    static PokedexCatalog decode(const QByteArray&);
private:
    std::optional<PokedexCatalog> catalog_;
};
}
