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
};
class AdventureAdapter {
public:
    virtual ~AdventureAdapter() = default;
    virtual QString id() const = 0;
    virtual AdventureCapabilities capabilities(const Adventure&) const = 0;
    virtual AdventureResult launch(const Adventure&) = 0;
    virtual AdventureResult resume(const Adventure&, const ResumePoint&) = 0;
};
class UnconfiguredAdventureAdapter final : public AdventureAdapter {
public:
    QString id() const override { return "unconfigured"; }
    AdventureCapabilities capabilities(const Adventure&) const override { return {}; }
    AdventureResult launch(const Adventure&) override { return {false, "Play setup will be available after device validation."}; }
    AdventureResult resume(const Adventure&, const ResumePoint&) override { return {false, "There is no supported resume point for this Adventure yet."}; }
};
}
