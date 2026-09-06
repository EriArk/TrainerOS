#pragma once
#include "core/navigation/ShellController.h"
#include "core/input/ControllerInput.h"
#include <QQuickWindow>

void startPokedexSmoke(QQuickWindow*, trainer::ShellController&, trainer::ControllerInput&,
                      trainer::MockPokedexRepository&, SDL_Joystick*, const QString& screenshotDir,
                      bool& completed, int& qmlWarnings, QStringList& diagnostics);
