#pragma once
#include "LibraryRepository.h"

namespace trainer {
// External providers publish snapshots; feature code still consumes the same
// library contract and never needs an emulator or a filesystem scanner.
class ResumeLibraryRepository final : public QObject, public LibraryRepository {
    Q_OBJECT
public:
    explicit ResumeLibraryRepository(LibraryRepository& base) : base_(base) {}
    QList<World> worlds() const override { return base_.worlds(); }
    QList<Adventure> adventures() const override { return base_.adventures(); }
    QList<ResumePoint> resumePoints() const override { return points_; }
    QList<PlaySession> recentSessions() const override { return base_.recentSessions(); }
    std::optional<qint64> recordedSeconds(const QString& id) const override { return base_.recordedSeconds(id); }
    HomeSnapshot home() const override { return base_.home(); }
    bool editable() const override { return base_.editable(); }
    std::optional<AdventureRegistration> registration(const QString& id) const override { return base_.registration(id); }
    void saveAdventureAsync(const AdventureRegistration& r, QObject* c, std::function<void(LibraryWriteResult)> f) override {
        base_.saveAdventureAsync(r, c, std::move(f));
    }
    void refreshResumePoints(const QString& id) override { emit scanRequested(id); }
    void publish(QList<ResumePoint> points) { points_ = std::move(points); emit changed(); }
signals:
    void scanRequested(const QString& adventureId);
    void changed();
private:
    LibraryRepository& base_;
    QList<ResumePoint> points_;
};
}
