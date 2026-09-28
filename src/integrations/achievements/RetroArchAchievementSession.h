#pragma once
#include "RetroAchievementsApi.h"
#include "platform/process/ProcessService.h"

namespace trainer {
// Call on the owning thread with the selected Trainer's account snapshot.
// Preparation/cleanup happen with the existing process lifecycle, not in QML.
void useRetroArchAchievementAccount(ProcessCommand&, AchievementAccount);
}
