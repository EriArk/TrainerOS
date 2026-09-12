#pragma once
#include "core/model/Models.h"

namespace trainer {
inline QString resumeLabel(ResumeAvailability status) {
    switch (status) {
    case ResumeAvailability::Exact: return "Saved moment";
    case ResumeAvailability::LaunchOnly: return "Choose a save in Adventure";
    case ResumeAvailability::Stale: return "Saved moment needs checking";
    case ResumeAvailability::Missing: return "Saved moment unavailable";
    case ResumeAvailability::Incompatible: return "Resume setup changed";
    }
    return "Saved moment needs checking";
}
}
