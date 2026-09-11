#pragma once
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <optional>

namespace trainer {
enum class JourneyStatus { NotStarted, InProgress, Completed };
enum class AdventureKind { Original, Remake, RomHack };
struct World {
    QString id;
    QString name;
    std::optional<JourneyStatus> status;
};
struct Adventure {
    QString id;
    QString worldId;
    QString title;
    QString adapterId;
    AdventureKind kind = AdventureKind::Original;
    QString description;
    std::optional<JourneyStatus> status;
    std::optional<int> badges;
    std::optional<int> caught;
    QStringList additionalWorldIds = {};
    QString platformId = {};
    QString catalogueId = {};
    QString variant = {};
    bool collectionOnly = false; // Reference entry, never an installed Adventure.
    QString limitation = {};
};
// Installation metadata belongs to the library/adapter boundary, never primary UI.
struct AdventureRegistration {
    Adventure adventure;
    QString contentPath;
    QJsonObject integrationConfig;
    int revision = 0; // 0 creates; edits compare the previously read revision.
    std::optional<World> newWorld;
    QList<World> additionalNewWorlds = {};
};
struct LibraryWriteResult { bool success; QString error; int revision = 0; };
struct ShellPreferences {
    QString theme = "turquoise";
    bool reducedMotion = false;
};
struct ResumePoint {
    QString id;
    QString adventureId;
    QDateTime savedAt;
    QString location;
    QString summary;
};
struct TrainerProfile {
    QString id;
    QString name;
    QString emblemId = "compass";
    QString favoritePokemonId;
    QDateTime createdAt;
};
enum class ArchiveSource { Manual, Imported };
struct HallOfFameMember {
    QString name;
    std::optional<int> level;
};
struct HallOfFameEntry {
    QString id;
    QString adventureId;
    QString adventureTitle;
    QString world;
    QDateTime completedAt;
    std::optional<int> playtimeMinutes;
    QList<HallOfFameMember> team;
    QString notes;
    ArchiveSource source = ArchiveSource::Manual;
};
// Unknown progress stays optional. These samples are not read from a game.
struct HomeSnapshot {
    QString activeAdventureId;
    std::optional<int> badges;
    std::optional<int> caught;
    QString milestone;
};
}
