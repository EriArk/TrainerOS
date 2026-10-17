#include "RetroArchAchievementEvents.h"
#include <QRegularExpression>

namespace trainer {
QList<RuntimeAchievementEvent> RetroArchAchievementEvents::feed(const QByteArray& bytes) {
    QList<RuntimeAchievementEvent> result;
    static const QRegularExpression identified(R"(^Identified game: ([1-9][0-9]{0,9})(?: ".*")? \(([0-9a-f]{32})\)$)"),
        loaded(R"(^Game ([1-9][0-9]{0,9}) loaded, hardcore (enabled|disabled)$)"),
        awarded(R"(^Awarding achievement ([1-9][0-9]{0,9}): (.{1,256})$)"),
        captured(R"(^Captured screenshot for achievement ([1-9][0-9]{0,9})$)"),
        failed(R"(^Failed to capture screenshot for achievement ([1-9][0-9]{0,9})$)");
    for (char ch:bytes) {
        if (ch!='\n') {
            if(!discarding_) {pending_.append(ch);if(pending_.size()>4096){pending_.clear();discarding_=true;}}
            continue;
        }
        if(discarding_){discarding_=false;continue;}
        auto line=QString::fromUtf8(pending_).trimmed();pending_.clear();
        // Native RARCH_LOG adds [INFO]; RCHEEVOS_TAG is fixed in this contract.
        if(!line.startsWith("[INFO] [RCHEEVOS] "))continue;
        line.remove(0,18);
        auto m=identified.match(line);
        if(m.hasMatch()){result.append({RuntimeAchievementEvent::Identified,m.captured(1),m.captured(2)});continue;}
        m=loaded.match(line);
        if(m.hasMatch()){RuntimeAchievementEvent e{RuntimeAchievementEvent::Loaded};e.game=m.captured(1);e.mode=m.captured(2)=="enabled"?"Hardcore":"Standard";result.append(e);continue;}
        m=awarded.match(line);
        if(m.hasMatch()){RuntimeAchievementEvent e{RuntimeAchievementEvent::Awarded};e.id=m.captured(1);e.title=m.captured(2);result.append(e);continue;}
        m=captured.match(line);
        if(m.hasMatch()){RuntimeAchievementEvent e{RuntimeAchievementEvent::Captured};e.id=m.captured(1);result.append(e);continue;}
        m=failed.match(line);
        if(m.hasMatch()){RuntimeAchievementEvent e{RuntimeAchievementEvent::CaptureFailed};e.id=m.captured(1);result.append(e);continue;}
        if(line.startsWith("Hardcore ")){result.append(RuntimeAchievementEvent{RuntimeAchievementEvent::ModeChanged});continue;}
        // Completion is a normal notification, not a changed game identity.
        if(QRegularExpression(R"(^Game [1-9][0-9]{0,9} (completed|mastered)$)").match(line).hasMatch())continue;
        if(line.startsWith("Unloading game ") || line.startsWith("Game ") || line.contains("spectating")
            || line.startsWith("Switching game "))result.append(RuntimeAchievementEvent{RuntimeAchievementEvent::Invalidated});
    }
    return result;
}
}
