#pragma once
#include "core/navigation/ShellController.h"
#include "core/input/ControllerInput.h"
#include <QQuickWindow>

void startHallSmoke(QQuickWindow*, trainer::ShellController&, trainer::ControllerInput&,
                    trainer::MockHallOfFameRepository&, trainer::MockAchievementProvider&, SDL_Joystick*,
                    const QString& screenshotDir, bool& completed, int& warnings, QStringList& diagnostics);
