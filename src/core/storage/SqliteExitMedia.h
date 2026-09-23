#pragma once
#include "core/model/ExitMedia.h"
#include <QSqlDatabase>

namespace trainer {
struct PreparedExitMedia {
    ExitMediaSource source;
    QString buildSha256;
    qint64 contentSize = 0, contentModified = 0;
};
QString migrateExitMedia(QSqlDatabase&);
std::optional<PreparedExitMedia> prepareExitMedia(QSqlDatabase&, const ExitMediaSource&);
QString writeExitMedia(QSqlDatabase&, const PlaySession&, const PreparedExitMedia&, const ExitCapture&);
QList<ExitMedia> readExitMedia(QSqlDatabase&, const QString& owner);
}
