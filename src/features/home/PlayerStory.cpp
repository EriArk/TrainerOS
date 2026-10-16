#include "PlayerStory.h"
#include "PlayHistoryController.h"
#include <QSet>
#include <algorithm>

namespace trainer {
QVariantMap PlayerStory::summary() const {
    const auto history=library_.playerHistory();
    const auto context=achievements_.context();
    QSet<QString> games,unlocks;
    for(const auto& game:library_.adventures())games.insert(game.id);
    bool known=false;
    if(!context.accountId.isEmpty())for(const auto& set:achievements_.sets()) {
        if(!set.supported || !games.contains(set.adventureId))continue;
        const auto snapshot=achievements_.snapshot(set.id);
        if(snapshot.context!=context || snapshot.setId!=set.id
           || snapshot.state==AchievementState::Disconnected || snapshot.state==AchievementState::Unsupported)continue;
        if(snapshot.definitions.isEmpty())continue;
        known=true;
        QSet<QString> definitions;
        for(const auto& definition:snapshot.definitions)definitions.insert(definition.id);
        for(const auto& unlock:snapshot.unlocks)if(unlock.unlocked==true && definitions.contains(unlock.achievementId)) {
            // Several local editions / Standard + Hardcore can describe one award.
            unlocks.insert((set.gameId.isEmpty()?set.id:set.gameId)+QChar(0x1f)+unlock.achievementId);
        }
    }
    return {{"recordedTime",history.recordedSeconds?recordedDuration(*history.recordedSeconds):QString::fromUtf8("—")},
        {"gamesPlayed",history.gamesPlayed},{"sessionCount",history.sessionCount},
        {"achievementCount",known?QString::number(unlocks.size()):QString::fromUtf8("—")},
        {"achievementNote",context.accountId.isEmpty()?"Connect RetroAchievements":known?"Confirmed in your library":"No confirmed records yet"}};
}
QVariantList PlayerStory::history() const {
    QHash<QString,QString> titles;
    for(const auto& game:library_.adventures())titles.insert(game.id,game.title);
    QVariantList rows;
    for(const auto& session:library_.playerHistory().sessions) {
        const auto local=session.startedAt.toLocalTime();
        auto title=titles.value(session.adventureId);
        if(title.isEmpty())if(const auto record=library_.registration(session.adventureId))title=record->adventure.title;
        if(title.isEmpty())title="Unavailable game";
        rows.append(QVariantMap{{"id",session.id},{"adventure",session.adventureId},
            {"game",title},
            {"time",local.toString("dd MMM yyyy · HH:mm")},{"date",local.toString("dd MMM yyyy")},{"clock",local.toString("HH:mm")},
            {"ended",session.endedAt.isValid()?session.endedAt.toLocalTime().toString("dd MMM yyyy · HH:mm"):QString()},
            {"outcome",session.outcome==PlaySessionOutcome::Returned?"returned":session.outcome==PlaySessionOutcome::Running?"running":session.outcome==PlaySessionOutcome::Failed?"failed":"interrupted"},
            {"duration",session.elapsedSeconds?recordedDuration(*session.elapsedSeconds):session.outcome==PlaySessionOutcome::Running?QString("Session in progress"):QString("Duration unavailable")},
            {"status",session.outcome==PlaySessionOutcome::Running?"Playing":session.outcome==PlaySessionOutcome::Failed?"Ended with an error":session.outcome==PlaySessionOutcome::Interrupted?"Interrupted":"Finished"}});
    }
    return rows;
}
QVariantList PlayerStory::media() const {
    QVariantList rows;
    for(const auto& game:library_.adventures()) {
        // The repository validates owner, domain and current registration revision;
        // the image provider revalidates the same opaque session on every read.
        const auto media=library_.exitMedia(game.id);
        if(!media || media->sessionId.isEmpty() || media->adventureId!=game.id)continue;
        rows.append(QVariantMap{{"id",media->sessionId},{"adventure",game.id},{"game",game.title},
            {"source","image://exit-media/"+media->sessionId},{"capturedAt",media->capturedAt},
            {"caption",game.title+" · Last exit · "+media->capturedAt.toLocalTime().toString("dd MMM yyyy · HH:mm")}});
    }
    std::stable_sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.toMap()["capturedAt"].toDateTime()>b.toMap()["capturedAt"].toDateTime();});
    return rows;
}
}
