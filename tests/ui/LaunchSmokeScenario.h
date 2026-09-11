#pragma once
#include "core/storage/SessionState.h"
#include "core/input/ControllerInput.h"
#include <QQuickWindow>

class ProbeAdventureAdapter final : public trainer::AdventureAdapter {
public:
    std::function<bool()> request;
    QString id() const override { return "test-process"; }
    trainer::AdventureCapabilities capabilities(const trainer::Adventure&) const override { return {true, false, false}; }
    trainer::AdventureResult launch(const trainer::Adventure&) override {
        const bool accepted = request && request();
        return {accepted, accepted ? QString() : "A test Adventure is already running.", accepted};
    }
    trainer::AdventureResult resume(const trainer::Adventure&, const trainer::ResumePoint&) override { return {false, "Probe has no save states."}; }
};
void startLaunchSmoke(QQuickWindow*, trainer::ShellController&, trainer::SessionState&, trainer::LocalStateStore&,
                      trainer::ControllerInput&, ProbeAdventureAdapter&, SDL_Joystick*, const QString& screenshotDir,
                      bool& completed, int& warnings, QStringList& diagnostics);
