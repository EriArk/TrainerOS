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
signals:
    void writeFailed(const QString& error);
private:
    void save(const PlaySession&);
    PlayHistoryRepository& repository_;
    std::optional<PlaySession> active_;
    QElapsedTimer clock_;
    QString error_;
};
QString recordedDuration(qint64 seconds);
}
