#pragma once
#include "RetroArchAdapter.h"
#include "core/model/SaveBackup.h"
#include <QThread>
#include <QImage>

namespace trainer {
struct RetroArchResumeSnapshot {
    QList<ResumePoint> points;
    QHash<QString, QImage> previews;
};
// These functions perform bounded I/O and belong on a worker, never in getters.
RetroArchResumeSnapshot scanRetroArchMoments(const QList<AdventureRegistration>&,
    const RetroArchInstallation&, const QList<ResumePoint>& previous, const std::atomic_bool& cancelled);
QString prepareRetroArchResume(ProcessCommand&, const AdventureRegistration&,
    const ResumePoint&, const RetroArchInstallation&, const std::atomic_bool& cancelled);
// Verified GBA in-game SRAM layout; does not read/interpret progression.
SaveTarget resolveRetroArchSave(const AdventureRegistration&, const RetroArchInstallation&);

class RetroArchResumeProvider final : public QObject {
    Q_OBJECT
public:
    RetroArchResumeProvider(LibraryRepository&, RetroArchInstallation);
    ~RetroArchResumeProvider() override;
    void refresh(const QString& adventureId = {});
    QImage preview(const QString& key) const { return previews_.value(key); }
signals:
    void updated(const QList<ResumePoint>& points);
private:
    void startScan();
    LibraryRepository& repository_;
    RetroArchInstallation installation_;
    QThread thread_;
    QObject* worker_;
    std::shared_ptr<std::atomic_bool> cancelled_;
    QList<ResumePoint> points_;
    QHash<QString, QImage> previews_;
    QStringList requested_;
    bool scanning_ = false;
    bool pending_ = false;
    quint64 generation_ = 0;
};
}
