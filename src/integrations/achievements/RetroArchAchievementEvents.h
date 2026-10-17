#pragma once
#include <QByteArray>
#include <QString>
#include <QList>

namespace trainer {
struct RuntimeAchievementEvent {
    enum Kind { Identified, Loaded, Awarded, Captured, CaptureFailed, ModeChanged, Invalidated } kind;
    QString game, hash, id, title, mode;
};
// RetroArch 1.22 rc_client diagnostics. Only the owned child's pipe is accepted;
// no arbitrary logs, historical records, title matching, or filesystem polling.
class RetroArchAchievementEvents {
public:
    QList<RuntimeAchievementEvent> feed(const QByteArray&);
    void clear() { pending_.clear(); discarding_=false; }
private:
    QByteArray pending_;
    bool discarding_=false;
};
}
