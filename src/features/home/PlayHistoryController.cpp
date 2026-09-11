#include "PlayHistoryController.h"
#include <QUuid>

namespace trainer {
PlayHistoryController::PlayHistoryController(AdventureLaunchController& launch, PlayHistoryRepository& repository, QObject* parent)
    : QObject(parent), repository_(repository) {
    connect(&launch, &AdventureLaunchController::adventureStarted, this, [this](const QString& id) {
        error_.clear();
        active_ = PlaySession{QUuid::createUuid().toString(QUuid::WithoutBraces), id, QDateTime::currentDateTimeUtc(), {}, {}, PlaySessionOutcome::Running};
        clock_.start(); save(*active_);
    });
    connect(&launch, &AdventureLaunchController::adventureFinished, this, [this](bool failed) {
        if (!active_) return;
        auto session = *active_; active_.reset();
        session.endedAt = QDateTime::currentDateTimeUtc();
        session.elapsedSeconds = clock_.elapsed() / 1000;
        session.outcome = failed ? PlaySessionOutcome::Failed : PlaySessionOutcome::Returned;
        save(session);
    });
}
void PlayHistoryController::save(const PlaySession& session) {
    repository_.saveSessionAsync(session, this, [this](const QString& error) {
        if (!error.isEmpty()) { error_ = error; emit writeFailed(error); }
    });
}
QString recordedDuration(qint64 seconds) {
    if (seconds < 60) return "< 1 min";
    if (seconds < 3600) return QString::number(seconds / 60) + " min";
    return QString("%1 h %2 min").arg(seconds / 3600).arg((seconds % 3600) / 60);
}
}
