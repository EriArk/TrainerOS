#include "LibraryRepository.h"

namespace trainer {
QList<World> MockLibraryRepository::worlds() const {
    return {{"kanto", "Kanto", JourneyStatus::InProgress}, {"johto", "Johto", JourneyStatus::Completed},
            {"hoenn", "Hoenn", JourneyStatus::InProgress}, {"sinnoh", "Sinnoh", {}},
            {"unova", "Unova", {}}, {"kalos", "Kalos", {}}, {"alola", "Alola", {}},
            {"galar", "Galar", {}}, {"paldea", "Paldea", {}}};
}
QList<Adventure> MockLibraryRepository::adventures() const {
    return {{"emerald-demo", "hoenn", "Pokémon Emerald", "mock", AdventureKind::Original,
             "Your current trail through Hoenn. All progress shown here is sample data.", JourneyStatus::InProgress, 4, 42},
            {"crystal-demo", "johto", "Pokémon Crystal", "mock", AdventureKind::Original,
             "A completed journey in this sample library.", JourneyStatus::Completed, 16, {}},
            {"firered-demo", "kanto", "Pokémon FireRed", "mock", AdventureKind::Remake,
             "A familiar beginning, revisited. Sample Adventure record.", JourneyStatus::InProgress, 0, 1},
            {"ruby-demo", "hoenn", "Pokémon Ruby", "mock", AdventureKind::Original,
             "A fresh route through the same World. This sample supports opening the Adventure without direct resume.", JourneyStatus::NotStarted, {}, {}},
            {"sapphire-demo", "hoenn", "Pokémon Sapphire", "mock", AdventureKind::Original,
             "A sample Adventure that still needs setup. Its library record remains available.", {}, {}, {}},
            {"alpha-sapphire-demo", "hoenn", "Pokémon Alpha Sapphire", "mock", AdventureKind::Remake,
             "A different edition of the same World. Progress has not been recorded in this sample.", {}, {}, {}},
            {"emerald-trails-demo", "hoenn", "Emerald: New Trails", "mock", AdventureKind::RomHack,
             "A fictional ROM hack created only as a prototype record. It is not a bundled or downloadable game.", {}, {}, {}}};
}
QList<ResumePoint> MockLibraryRepository::resumePoints() const {
    // Deliberately unsorted: the use-case owns recency ordering.
    QList<ResumePoint> points{{"crystal-1", "crystal-demo", QDateTime::fromString("2026-09-04T19:10:00Z", Qt::ISODate),
             "Goldenrod City", "A new road ahead"},
            {"emerald-1", "emerald-demo", QDateTime::fromString("2026-09-06T18:42:00Z", Qt::ISODate),
             "Route 119", "4 badges · 42 caught"},
            {"firered-1", "firered-demo", QDateTime::fromString("2026-09-03T16:20:00Z", Qt::ISODate),
             "Pallet Town", "First steps"}};
    for (auto& point : points) {
        point.source = {"mock", point.id, "sample-1", "mock-v1"};
        point.observedAt = point.savedAt;
        point.availability = ResumeAvailability::Exact;
    }
    return points;
}
HomeSnapshot MockLibraryRepository::home() const {
    return {"emerald-demo", 4, 42, "The next trail is waiting."};
}
}
