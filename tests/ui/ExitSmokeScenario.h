#pragma once
#include "core/navigation/AdventureLaunchController.h"
#include "core/navigation/ShellController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include <QQuickWindow>
void startExitSmoke(QQuickWindow*, trainer::ShellController&, trainer::AdventureLaunchController&,
    trainer::AdventureExitPresentation&, trainer::ControllerInput&, SDL_Joystick*,
    const QString&, bool&, int&, QStringList&);
