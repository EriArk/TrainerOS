#pragma once
#include <QString>

namespace trainer {
class PlatformService {
public:
    virtual ~PlatformService() = default;
    virtual bool canSwitchSession() const = 0;
    virtual QString sessionStatus() const = 0;
};
// Safe application mode. No compositor/session assumptions or system mutations.
class DevelopmentPlatformService final : public PlatformService {
public:
    bool canSwitchSession() const override { return false; }
    QString sessionStatus() const override {
        return "Session switching will be available after ArmadaOS device testing. "
               "TrainerOS, Steam Gaming Mode and KDE Plasma will remain available.";
    }
};
}
