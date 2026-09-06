#pragma once
#include "core/navigation/ShellController.h"
#include "core/input/ControllerInput.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QQuickWindow>

void startWorldsSmoke(QQuickWindow*, trainer::ShellController&, trainer::ControllerInput&,
                     trainer::MockAdventureAdapter&, SDL_Joystick*, const QString& screenshotDir,
                     bool& completed, int& qmlWarnings, QStringList& diagnostics);
