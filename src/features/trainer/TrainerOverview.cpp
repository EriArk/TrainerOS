#include "TrainerOverview.h"
#include <QSet>
#include <algorithm>
#include <limits>

namespace trainer {
TrainerOverview trainerOverview(const LibraryRepository& library, const PokedexCatalog& reference,
        const PokedexProgressRepository& journal, const HallOfFameRepository& archive) {
    TrainerOverview result;
    QSet<QString> adventures, worlds;
    for (const auto& adventure : library.adventures()) {
        if (adventure.collectionOnly || adventures.contains(adventure.id)) continue;
        adventures.insert(adventure.id); worlds.insert(adventure.worldId);
        for (const auto& world : adventure.additionalWorldIds) worlds.insert(world);
        const auto seconds = library.recordedSeconds(adventure.id);
        if (seconds && *seconds >= 0) {
            const auto old = result.recordedSeconds.value_or(0);
            result.recordedSeconds = old + std::min(*seconds, std::numeric_limits<qint64>::max() - old);
        }
    }
    worlds.remove({});
    result.adventures = adventures.size(); result.worlds = worlds.size();
    if (reference.success) {
        result.seen = 0; result.caught = 0; result.favorites = 0;
        QSet<QString> counted;
        for (const auto& entry : reference.entries) {
            if (counted.contains(entry.id)) continue;
            counted.insert(entry.id);
            const auto record = journal.progress(entry.id);
            if (record.seen.value_or(false)) ++*result.seen;
            if (record.caught.value_or(false)) ++*result.caught;
            if (record.favorite) ++*result.favorites;
        }
    }
    const auto memories = archive.loadArchive();
    if (memories.success) result.memories = memories.entries.size();
    return result;
}
}
