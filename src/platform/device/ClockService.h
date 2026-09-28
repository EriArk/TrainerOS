#pragma once
#include <QDateTime>
#include <QStringList>
namespace trainer {
struct ClockSnapshot {
    QString zone, error;
    QStringList zones;
    bool available = false, automatic = false, synchronized = false, canSync = false;
};
// System operations run on the caller's worker, never the render thread.
ClockSnapshot readClock();
QString changeClock(const QString& operation, const QString& value);
}
