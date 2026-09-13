#pragma once
#include "core/model/Models.h"

namespace trainer {
struct AdventureCapabilities {
    bool launch = false;
    bool directResume = false;
    bool screenshots = false;
};
struct AdventureResult {
    bool success;
    QString message;
    bool inProgress = false;
};
class AdventureAdapter {
public:
    virtual ~AdventureAdapter() = default;
    virtual QString id() const = 0;
    virtual AdventureCapabilities capabilities(const Adventure&) const = 0;
    // Cached, nonblocking assessment only. resume() must revalidate the exact
    // source before starting a process; an unavailable state must never launch.
    virtual ResumeAvailability resumeAvailability(const Adventure& adventure, const ResumePoint& point) const {
        if (adventure.collectionOnly || point.adventureId != adventure.id || point.source.adapterId != adventure.adapterId)
            return ResumeAvailability::Incompatible;
        if (!point.source.complete() || point.id.isEmpty() || !point.observedAt.isValid()) return ResumeAvailability::Stale;
        if (point.availability != ResumeAvailability::Exact) return point.availability;
        return capabilities(adventure).directResume ? ResumeAvailability::Exact : ResumeAvailability::LaunchOnly;
    }
    virtual AdventureResult launch(const Adventure&) = 0;
    virtual AdventureResult resume(const Adventure&, const ResumePoint&) = 0;
    virtual void prepareInstallation(AdventureRegistration&) const {}
};
class UnconfiguredAdventureAdapter final : public AdventureAdapter {
public:
    QString id() const override { return "unconfigured"; }
    AdventureCapabilities capabilities(const Adventure&) const override { return {}; }
    AdventureResult launch(const Adventure&) override { return {false, "Play setup will be available after device validation."}; }
    AdventureResult resume(const Adventure&, const ResumePoint&) override { return {false, "There is no supported resume point for this Adventure yet."}; }
};
}
