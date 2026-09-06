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
AdventureResult MockAdventureAdapter::resume(const Adventure& adventure, const ResumePoint& point) {
    if (!capabilities(adventure).directResume || point.adventureId != adventure.id || point.id.isEmpty())
        return {false, "This resume point is unavailable."};
    return {true, QStringLiteral("Demo resume ready: %1 — %2. No Adventure was launched.")
                      .arg(adventure.title, point.location)};
}
}
