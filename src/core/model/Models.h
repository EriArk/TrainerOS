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
    QString domain = "pokemon"; // Explicit library context; never inferred from title/path.
};
// Installation metadata belongs to the library/adapter boundary, never primary UI.
struct AdventureRegistration {
    Adventure adventure;
    QString contentPath;
    QJsonObject integrationConfig;
    int revision = 0; // 0 creates; edits compare the previously read revision.
    std::optional<World> newWorld;
    QList<World> additionalNewWorlds = {};
    bool contentAvailable = true; // Worker-observed availability; not persisted identity.
    bool removed = false;
    QString trashPath;
};
struct LibraryWriteResult { bool success; QString error; int revision = 0; };
struct ShellPreferences {
    QString theme = "turquoise";
    bool reducedMotion = false;
    bool worldEditing = false;
};
enum class LibraryEditKind { RenameWorld, RenameGame, MoveGame, RemoveGame, RestoreGame };
struct LibraryEdit {
    LibraryEditKind kind;
    QString id;
    int revision = 0;
    QString text;
    World world;
    QString previousName;
    bool trashFile = true;
};
enum class ResumeAvailability { Exact, LaunchOnly, Stale, Missing, Incompatible };
// Adapter-owned identity of one source revision, never a path interpreted by UI.
struct ResumeSource {
    QString adapterId, sourceId, revision, integrationRevision;
    bool complete() const {
        return !adapterId.isEmpty() && !sourceId.isEmpty() && !revision.isEmpty() && !integrationRevision.isEmpty();
    }
    bool operator==(const ResumeSource&) const = default;
    QJsonObject toJson() const {
        return {{"adapter", adapterId}, {"source", sourceId}, {"revision", revision}, {"integration", integrationRevision}};
    }
    static ResumeSource fromJson(const QJsonObject& value) {
        return {value["adapter"].toString(), value["source"].toString(), value["revision"].toString(), value["integration"].toString()};
    }
};
struct ResumePoint {
    QString id;
    QString adventureId;
    QDateTime savedAt;
    QString location;
    QString summary;
    ResumeSource source = {};
    QDateTime observedAt = {};
    ResumeAvailability availability = ResumeAvailability::Stale;
    QJsonObject adapterPayload = {}; // Never projected into QML or browsing state.
    QString previewKey = {}; // Opaque key for a cached, revision-bound image.
};
enum class PlaySessionOutcome { Running, Returned, Failed, Interrupted };
// Observed child-process time, independent of game-save progress/playtime.
struct PlaySession {
    QString id;
    QString adventureId;
    QDateTime startedAt;
    QDateTime endedAt;
    std::optional<qint64> elapsedSeconds;
    PlaySessionOutcome outcome = PlaySessionOutcome::Running;
};
struct ContinueEntry {
    QString id;
    QString adventureId;
    QDateTime recordedAt;
    std::optional<ResumePoint> resumePoint;
    std::optional<PlaySession> session;
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
    int revision = 0;
};
// Unknown progress stays optional. These samples are not read from a game.
struct HomeSnapshot {
    QString activeAdventureId;
    std::optional<int> badges;
    std::optional<int> caught;
    QString milestone;
};
}
