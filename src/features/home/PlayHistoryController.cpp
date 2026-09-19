#include "PlayHistoryController.h"
#include <QUuid>

namespace trainer {
PlayHistoryController::PlayHistoryController(AdventureLaunchController& launch, PlayHistoryRepository& repository, QObject* parent)
    : QObject(parent), repository_(repository) {
    connect(&launch, &AdventureLaunchController::adventureStarted, this, [this](const QString& id) {
        error_.clear();
        active_ = PlaySession{QUuid::createUuid().toString(QUuid::WithoutBraces), id, QDateTime::currentDateTimeUtc(), {}, {}, PlaySessionOutcome::Running};
        source_ = mediaSource_ ? mediaSource_(id) : std::nullopt;
        capture_.reset(); captureRequestedAt_ = {};
        clock_.start(); save(*active_);
    });
    connect(&launch.exitController(), &AdventureExitController::captureRequested, this, [this](quint64) {
        capture_.reset(); captureRequestedAt_ = QDateTime::currentDateTimeUtc();
    });
    connect(&launch.exitController(), &AdventureExitController::completed, this, [this](quint64, const QImage& frame, bool confirmed) {
        if (active_ && source_ && confirmed && !frame.isNull() && captureRequestedAt_.isValid())
            capture_ = ExitCapture{frame, captureRequestedAt_};
    });
    connect(&launch, &AdventureLaunchController::adventureFinished, this, [this](bool failed) {
        if (!active_) return;
        auto session = *active_; active_.reset();
        session.endedAt = QDateTime::currentDateTimeUtc();
        session.elapsedSeconds = clock_.elapsed() / 1000;
        session.outcome = failed ? PlaySessionOutcome::Failed : PlaySessionOutcome::Returned;
        save(session);
        source_.reset(); capture_.reset();
    });
}
void PlayHistoryController::save(const PlaySession& session) {
    repository_.saveSessionMediaAsync(session, source_, capture_, this, [this](const QString& error) {
        if (!error.isEmpty()) { error_ = error; emit writeFailed(error); }
    });
}
QString recordedDuration(qint64 seconds) {
    if (seconds < 60) return "< 1 min";
    if (seconds < 3600) return QString::number(seconds / 60) + " min";
    return QString("%1 h %2 min").arg(seconds / 3600).arg((seconds % 3600) / 60);
}
}
