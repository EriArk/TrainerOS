#include "MockAdventureAdapter.h"

namespace trainer {
MockAdventureAdapter::MockAdventureAdapter() {
    setCapabilities("ruby-demo", {true, false, false});
    setCapabilities("sapphire-demo", {false, false, false});
}
void MockAdventureAdapter::setCapabilities(const QString& adventureId, AdventureCapabilities capabilities) {
    overrides_.insert(adventureId, capabilities);
}
AdventureCapabilities MockAdventureAdapter::capabilities(const Adventure& adventure) const {
    if (adventure.adapterId != id() || adventure.id.isEmpty()) return {};
    return overrides_.value(adventure.id, {true, true, false});
}
AdventureResult MockAdventureAdapter::launch(const Adventure& adventure) {
    if (!capabilities(adventure).launch) return {false, "This Adventure needs setup before it can be opened."};
    if (failNextLaunch_) {
        failNextLaunch_ = false;
        return {false, "This sample Adventure couldn't open. Try again; your library and progress have been kept."};
    }
    return {true, QStringLiteral("Demo launch ready: %1. No Adventure was launched.").arg(adventure.title)};
}
ResumeAvailability MockAdventureAdapter::resumeAvailability(const Adventure& adventure, const ResumePoint& point) const {
    const auto status = AdventureAdapter::resumeAvailability(adventure, point);
    if (status != ResumeAvailability::Exact) return status;
    if (point.source.integrationRevision != integrationRevision_) return ResumeAvailability::Incompatible;
    if (sources_.contains(point.source.sourceId)) {
        const auto revision = sources_.value(point.source.sourceId);
        if (revision.isEmpty()) return ResumeAvailability::Missing;
        if (revision != point.source.revision) return ResumeAvailability::Stale;
    }
    return ResumeAvailability::Exact;
}
AdventureResult MockAdventureAdapter::resume(const Adventure& adventure, const ResumePoint& point) {
    if (resumeAvailability(adventure, point) != ResumeAvailability::Exact)
        return {false, "This resume point is unavailable."};
    return {true, QStringLiteral("Demo resume ready: %1 — %2. No Adventure was launched.")
                      .arg(adventure.title, point.location)};
}
}
