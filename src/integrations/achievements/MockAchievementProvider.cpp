#include "AchievementProvider.h"
#include <QTimer>

namespace trainer {
MockAchievementProvider::MockAchievementProvider(QObject* parent) : AchievementProvider(parent) {
    for (const auto& set : sets()) snapshots_.insert(set.id, sample(set.id));
}
QList<AchievementSet> MockAchievementProvider::sets() const {
    return {{"emerald-sample", "sample-game-emerald", "emerald-demo", "Pokémon Emerald", "Hoenn", true},
            {"crystal-sample", "sample-game-crystal", "crystal-demo", "Pokémon Crystal", "Johto", true},
            {"hack-sample", "sample-game-hack", "emerald-trails-demo", "Emerald: New Trails", "Hoenn", false}};
}
AchievementSnapshot MockAchievementProvider::sample(const QString& setId) const {
    AchievementSnapshot result{context_, setId, AchievementState::Unsupported, {}, {}, {}};
    if (setId != "emerald-sample" && setId != "crystal-sample") return result;
    result.state = context_.accountId.isEmpty() ? AchievementState::Disconnected : AchievementState::Ready;
    if (result.state == AchievementState::Disconnected) return result;
    // Original, fictional achievement examples. No real RA set or unlock is claimed.
    result.definitions = {{"first-trail", "The first trail", "Sample goal: begin a new journey with a trusted partner."},
        {"steady-team", "A team to remember", "Sample goal: reach a milestone together with your team."},
        {"long-way", "The scenic route", "Sample goal: explore beyond the familiar path."},
        {"new-chapter", "A new chapter", "Sample goal with an unavailable account record."},
        {"lasting-memory", "A lasting memory", "Sample goal with a recorded unlock but no known unlock date."}};
    if (setId == "crystal-sample") result.definitions = result.definitions.mid(0, 3);
    if (context_.accountId == "sample-trainer") {
        result.unlocks = {{"first-trail", true, AchievementMode::Standard, QDateTime::fromString("2026-08-20T10:00:00Z", Qt::ISODate)},
            {"steady-team", true, AchievementMode::Hardcore, QDateTime::fromString("2026-08-22T14:00:00Z", Qt::ISODate)},
            {"long-way", false, {}, {}}, {"new-chapter", {}, {}, {}},
            {"lasting-memory", true, AchievementMode::Standard, {}}};
    }
    result.fetchedAt = QDateTime::fromString("2026-09-01T20:00:00Z", Qt::ISODate);
    return result;
}
AchievementSnapshot MockAchievementProvider::snapshot(const QString& setId) const {
    return snapshots_.value(setId, {context_, setId, AchievementState::Disconnected, {}, {}, {}});
}
void MockAchievementProvider::setState(const QString& setId, AchievementState state) {
    auto value = snapshot(setId);
    value.state = state;
    if (state == AchievementState::Disconnected || state == AchievementState::Unsupported) {
        value.definitions.clear(); value.unlocks.clear(); value.fetchedAt = {};
    }
    snapshots_.insert(setId, value);
    emit snapshotChanged(setId);
}
void MockAchievementProvider::setAccount(const QString& accountId) {
    ++generation_;
    pending_.clear(); snapshots_.clear(); context_.accountId = accountId;
    emit snapshotChanged({});
}
void MockAchievementProvider::refresh(const QString& setId) {
    bool supported = false;
    for (const auto& set : sets()) if (set.id == setId) supported = set.supported;
    if (!supported) { setState(setId, AchievementState::Unsupported); return; }
    if (context_.accountId.isEmpty()) { setState(setId, AchievementState::Disconnected); return; }
    if (pending_.contains(setId)) return;
    pending_.insert(setId, nextResult_); nextResult_ = AchievementState::Ready;
    setState(setId, AchievementState::Loading);
    if (!hold_) {
        const int generation = generation_;
        QTimer::singleShot(0, this, [this, setId, generation] {
            if (generation == generation_) finishRefresh(setId);
        });
    }
}
void MockAchievementProvider::finishRefresh(const QString& setId) {
    if (!pending_.contains(setId)) return;
    const auto state = pending_.take(setId);
    if (state == AchievementState::Ready) {
        auto value = sample(setId); value.fetchedAt = QDateTime::currentDateTimeUtc();
        snapshots_.insert(setId, value);
        emit snapshotChanged(setId);
    } else setState(setId, state);
}
}
