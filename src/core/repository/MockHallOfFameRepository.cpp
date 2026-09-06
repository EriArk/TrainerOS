#include "HallOfFameRepository.h"

namespace trainer {
ArchiveResult MockHallOfFameRepository::load() {
    if (fail_) { fail_ = false; return {false, {}, "The sample archive couldn't be refreshed. Your saved memories have been kept."}; }
    if (empty_) return {};
    const auto date = [](const char* value) { return QDateTime::fromString(QString::fromLatin1(value), Qt::ISODate); };
    return {true, {
        {"crystal-champion", "crystal-demo", "Pokémon Crystal", "Johto", date("2026-09-01T18:30:00Z"), 2545,
            {{"Typhlosion", 54}, {"Ampharos", 51}, {"Espeon", 52}, {"Lapras", 50}, {"Crobat", 53}, {"Heracross", 51}},
            "A sample memory of the team that reached the summit together.", ArchiveSource::Manual},
        {"emerald-old-run", "emerald-demo", "Pokémon Emerald", "Hoenn", date("2026-08-12T12:00:00Z"), 1870,
            {{"Swampert", 56}, {"Gardevoir", 52}, {"Breloom", 51}, {"Manectric", 50}, {"Flygon", 53}, {"Absol", {}}},
            "An earlier sample journey. This archive does not describe the current save.", ArchiveSource::Imported},
        {"firered-old-run", "firered-demo", "Pokémon FireRed", "Kanto", date("2026-07-18T09:00:00Z"), {},
            {{"Charizard", 55}}, "Only one team member was recorded in this sample.", ArchiveSource::Manual},
        {"crystal-undated", "crystal-demo", "Pokémon Crystal", "Johto", {}, {}, {},
            "A sample record with an unknown date, playtime and team.", ArchiveSource::Imported}
    }, {}};
}
}
