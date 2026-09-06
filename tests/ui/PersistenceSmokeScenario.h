#pragma once
#include "core/storage/SessionState.h"
#include "core/input/ControllerInput.h"
#include <QQuickWindow>

void startPersistenceSmoke(QQuickWindow*, trainer::ShellController&, trainer::SessionState&,
                          trainer::ControllerInput&, SDL_Joystick*, const QString& phase,
                          const QString& screenshotDir, bool& completed, int& warnings, QStringList& diagnostics);
