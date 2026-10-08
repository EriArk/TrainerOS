#pragma once
#include "core/navigation/ShellController.h"
#include "core/input/ControllerInput.h"
#include <QQuickWindow>

void startDownloadsSmoke(QQuickWindow*, trainer::ShellController&, trainer::ControllerInput&,
                         SDL_Joystick*, const QString&, bool&, int&, QStringList&);
