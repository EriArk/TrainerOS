#pragma once
#include "core/storage/SessionState.h"
#include "core/input/ControllerInput.h"
#include <QQuickWindow>

void startLibrarySmoke(QQuickWindow*, trainer::ShellController&, trainer::SessionState&, trainer::LocalStateStore&,
                       trainer::ControllerInput&, SDL_Joystick*, const QString& phase, const QString& contentFolder,
                       const QString& screenshotDir, bool& completed, int& warnings, QStringList& diagnostics);
