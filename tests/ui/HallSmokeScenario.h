#pragma once
#include "core/navigation/ShellController.h"
#include "core/input/ControllerInput.h"
#include <QQuickWindow>
void startAchievementGallerySmoke(QQuickWindow*, trainer::ShellController&, trainer::ControllerInput&,
    trainer::MockAchievementProvider&, SDL_Joystick*, const QString&, bool&, int&, QStringList&);

void startHallSmoke(QQuickWindow*, trainer::ShellController&, trainer::ControllerInput&,
                    trainer::MockHallOfFameRepository&, trainer::MockAchievementProvider&, SDL_Joystick*,
                    const QString& screenshotDir, bool& completed, int& warnings, QStringList& diagnostics);
