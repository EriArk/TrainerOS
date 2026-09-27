#pragma once
#include <QDateTime>
#include <QList>
#include <QString>
#include <optional>

namespace trainer {
struct JourneyMilestone { QString id, title; std::optional<bool> achieved; };
struct ChampionMember {
    QString speciesId, name, nickname;
    int number = 0, level = 0;
    bool shiny = false;
};
// Historical Hall data only. Current Party/Dex/time must never be copied here.
struct ChampionRecord {
    QString id, lineage, trainerName, adventureId, build, saveRevision;
    int victory = 0;
    QList<ChampionMember> team;
    QDateTime observedAt; // Import time, never victory time.
};
struct JourneySnapshot {
    std::optional<int> playtimeMinutes;
    QList<JourneyMilestone> milestones;
    QList<ChampionRecord> champions;
    QString championError;
};
}
