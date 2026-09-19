#pragma once
#include "core/navigation/AdventureLaunchController.h"
#include "core/repository/PlayHistoryRepository.h"
#include <QElapsedTimer>

namespace trainer {
class PlayHistoryController final : public QObject {
    Q_OBJECT
public:
    PlayHistoryController(AdventureLaunchController&, PlayHistoryRepository&, QObject* parent = nullptr);
    QString error() const { return error_; }
    void setMediaSource(std::function<std::optional<ExitMediaSource>(const QString&)> source) { mediaSource_ = std::move(source); }
signals:
    void writeFailed(const QString& error);
private:
    void save(const PlaySession&);
    PlayHistoryRepository& repository_;
    std::optional<PlaySession> active_;
    QElapsedTimer clock_;
    QString error_;
    std::function<std::optional<ExitMediaSource>(const QString&)> mediaSource_;
    std::optional<ExitMediaSource> source_;
    std::optional<ExitCapture> capture_;
    QDateTime captureRequestedAt_;
};
QString recordedDuration(qint64 seconds);
}
