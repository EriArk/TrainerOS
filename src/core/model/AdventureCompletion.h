#pragma once
#include "GameProgress.h"

namespace trainer {
struct AdventureCompletion {
    QString identity, policy, message;
    bool completed = false;
};
// Completion is an exact adapter observation, never account ownership or playtime.
inline AdventureCompletion reviewCompletion(const GameProgress& progress) {
    AdventureCompletion result;
    result.identity = progress.contentRevision;
    if(progress.contentRevision != "a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af") {
        result.message = "Completion is not supported for this edition yet."; return result;
    }
    result.policy = "emerald-en/champion-v1";
    result.message = "Become Hoenn Champion to write a review.";
    if(progress.availability != ProgressAvailability::Available || !progress.journey) {
        result.message = "A readable completed save is needed to write a review."; return result;
    }
    for(const auto& milestone : progress.journey->milestones)
        if(milestone.id == "champion") result.completed = milestone.achieved.value_or(false);
    if(result.completed) result.message.clear();
    return result;
}
}
