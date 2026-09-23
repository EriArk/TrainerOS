#pragma once
#include "LibraryRepository.h"

namespace trainer {
struct PlatformLabel { QString id, name, badge, shape; std::optional<int> chronology = {}; };
struct EditionChronology {
    QString id, platformId;
    std::optional<int> releaseYear, order;
};
QList<PlatformLabel> collectionPlatforms();
QList<EditionChronology> collectionChronology();
void sortWorldAdventures(QList<Adventure>&, const QList<PlatformLabel>&, const QList<EditionChronology>&);
PlatformLabel platformLabel(const QString& id);
QList<World> collectionWorlds();
QList<Adventure> collectionCatalogue();
// Reference data is bundled, read-only and independent of the user's SQLite
// records. Multiple owned revisions can refer to one catalogue edition.
class CollectionRepository final : public LibraryRepository {
public:
    explicit CollectionRepository(LibraryRepository& personal) : personal_(personal) {}
    QList<World> worlds() const override;
    QList<Adventure> adventures() const override;
    QList<ResumePoint> resumePoints() const override { return personal_.resumePoints(); }
    QList<PlaySession> recentSessions() const override { return personal_.recentSessions(); }
    std::optional<ExitMedia> exitMedia(const QString& id) const override { return personal_.exitMedia(id); }
    std::optional<qint64> recordedSeconds(const QString& id) const override { return personal_.recordedSeconds(id); }
    HomeSnapshot home() const override { return personal_.home(); }
    bool editable() const override { return personal_.editable(); }
    std::optional<AdventureRegistration> registration(const QString& id) const override { return personal_.registration(id); }
    void saveAdventureAsync(const AdventureRegistration&, QObject*, std::function<void(LibraryWriteResult)>) override;
private:
    LibraryRepository& personal_;
};
}
