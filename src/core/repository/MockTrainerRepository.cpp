#include "TrainerRepository.h"

namespace trainer {
ProfileWriteResult MockTrainerRepository::save(const TrainerProfile& profile) {
    if (failNextSave_) {
        failNextSave_ = false;
        return {false, "Couldn't save your Trainer. Try Save again, or cancel to keep the previous profile."};
    }
    if (profile.id.isEmpty() || (profile_ && profile_->id != profile.id))
        return {false, "Your Trainer identity could not be verified. The previous profile has been kept."};
    profile_ = profile;
    return {true, {}};
}
}
