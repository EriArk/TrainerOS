#pragma once
#include "LaunchSmokeScenario.h"
void startHomeSmoke(QQuickWindow*, trainer::ShellController&, trainer::SessionState&, trainer::LocalStateStore&,
                    trainer::ControllerInput&, ProbeAdventureAdapter&, SDL_Joystick*, bool reopen,
                    const QString& screenshotDir, bool& completed, int& warnings, QStringList& diagnostics);
