#pragma once
#include "core/model/Models.h"
#include <QObject>
#include <functional>

namespace trainer {
class LibraryRepository {
public:
    virtual ~LibraryRepository() = default;
    virtual QList<World> worlds() const = 0;
    virtual QList<Adventure> adventures() const = 0;
    // Cached snapshot only: external scanning/validation must publish updates
    // asynchronously. IDs are unique/stable; source revisions track replacement.
    virtual QList<ResumePoint> resumePoints() const = 0;
    virtual QList<PlaySession> recentSessions() const { return {}; }
    virtual std::optional<qint64> recordedSeconds(const QString&) const { return {}; }
    virtual HomeSnapshot home() const = 0;
    virtual bool editable() const { return false; }
    virtual std::optional<AdventureRegistration> registration(const QString&) const { return {}; }
    virtual void saveAdventureAsync(const AdventureRegistration&, QObject*, std::function<void(LibraryWriteResult)> completed) {
        completed({false, "Library editing isn't available in this sample preview."});
    }
};

class MockLibraryRepository final : public LibraryRepository {
public:
    QList<World> worlds() const override;
    QList<Adventure> adventures() const override;
    QList<ResumePoint> resumePoints() const override;
    HomeSnapshot home() const override;
};
}
