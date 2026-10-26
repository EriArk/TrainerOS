#include "core/PerformanceTrace.h"
#include "PlayerStory.h"
#include "ExitImage.h"
#include "PlayHistoryController.h"
#include <QSet>
#include <QBuffer>
#include <QImageReader>
#include <QRegularExpression>
#include <algorithm>

namespace trainer {
QVariantMap PlayerStory::profile() const {
    auto result=summary();
    const auto sessions=history();
    result["latestSession"]=sessions.isEmpty()?QVariantMap{}:sessions.first().toMap();
    if(!sessions.isEmpty()) {
        auto latest=sessions.first().toMap();
        const auto media=library_.exitMedia(latest["adventure"].toString());
        if(media && media->sessionId==latest["id"].toString())latest["image"]="image://exit-media/"+media->sessionId;
        result["latestSession"]=latest;
    }
    const auto context=achievements_.context();
    QSet<QString> games;
    for(const auto& game:library_.adventures())games.insert(game.id);
    QMap<QString,QVariantMap> awards;
    static const QRegularExpression badgeIdentity("^[0-9]+$");
    if(!context.accountId.isEmpty())for(const auto& set:achievements_.sets()) {
        if(!set.supported || !games.contains(set.adventureId))continue;
        const auto snapshot=achievements_.snapshot(set.id);
        if(snapshot.context!=context || snapshot.setId!=set.id
           || snapshot.state==AchievementState::Disconnected || snapshot.state==AchievementState::Unsupported)continue;
        for(const auto& unlock:snapshot.unlocks) {
            if(unlock.unlocked!=true)continue;
            const auto definition=std::find_if(snapshot.definitions.begin(),snapshot.definitions.end(),[&](const auto& d){return d.id==unlock.achievementId;});
            if(definition==snapshot.definitions.end())continue;
            const auto key=(set.gameId.isEmpty()?set.id:set.gameId)+QChar(0x1f)+definition->id;
            if(awards.contains(key) && (!unlock.earnedAt.isValid() || awards[key]["earnedAt"].toDateTime()>=unlock.earnedAt))continue;
            awards[key]={{"id",key},{"set",set.id},{"achievement",definition->id},{"title",definition->title},{"game",set.title},{"earnedAt",unlock.earnedAt},
                {"date",unlock.earnedAt.isValid()?unlock.earnedAt.toLocalTime().toString("dd MMM · HH:mm"):QString("Date unavailable")},
                {"icon",badgeIdentity.match(definition->badgeName).hasMatch()?"https://media.retroachievements.org/Badge/"+definition->badgeName+".png":QString()}};
        }
    }
    QVariantList badges;
    for(const auto& award:awards)badges.append(award);
    std::stable_sort(badges.begin(),badges.end(),[](const auto& a,const auto& b){return a.toMap()["earnedAt"].toDateTime()>b.toMap()["earnedAt"].toDateTime();});
    while(badges.size()>2)badges.removeLast();
    result["badges"]=badges;
    return result;
}
QVariantMap PlayerStory::summary() const {
    PerformanceTrace::Scope perf("PlayerStory::summary");
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
    PerformanceTrace::Scope perf("PlayerStory::history");
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
    if(const auto album=library_.screenMedia())return *album;
    QVariantList rows;
    QHash<QString,QSize> currentSizes;
    for(const auto& game:library_.adventures()) {
        // The repository validates owner, domain and current registration revision;
        // the image provider revalidates the same opaque session on every read.
        const auto media=library_.exitMedia(game.id);
        if(!media || media->sessionId.isEmpty() || media->adventureId!=game.id)continue;
        // Match the provider's removal of paired emulator bars. Immutable
        // capture IDs let us decode once; retain only current, validated media.
        // Geometry never depends on the Image element it is resizing.
        auto size=framedSizes_.value(media->sessionId);
        if(!framedSizes_.contains(media->sessionId)) {
            QBuffer buffer;buffer.setData(media->jpeg);buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer,"JPEG");const auto header=reader.size();
            if(header.width()>0 && header.width()<=960 && header.height()>0 && header.height()<=540)
                size=frameExitImage(reader.read()).size();
        }
        currentSizes.insert(media->sessionId,size);
        const double ratio=size.width()>0 && size.height()>0 ?double(size.width())/size.height():16.0/9.0;
        rows.append(QVariantMap{{"id",media->sessionId},{"adventure",game.id},{"game",game.title},
            {"ratio",ratio},
            {"day",media->capturedAt.toLocalTime().toString("dd MMM yyyy")},{"clock",media->capturedAt.toLocalTime().toString("HH:mm")},
            {"source","image://exit-media/"+media->sessionId},{"capturedAt",media->capturedAt},
            {"caption",game.title+" · Last exit · "+media->capturedAt.toLocalTime().toString("dd MMM yyyy · HH:mm")}});
    }
    framedSizes_=std::move(currentSizes);
    std::stable_sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.toMap()["capturedAt"].toDateTime()>b.toMap()["capturedAt"].toDateTime();});
    return rows;
}
}
