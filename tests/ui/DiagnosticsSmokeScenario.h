#pragma once
#include "core/storage/SessionState.h"
#include <QQuickWindow>
void startDiagnosticsSmoke(QQuickWindow*, trainer::ShellController&, trainer::SessionState&, trainer::ControllerInput&,
    trainer::DiagnosticsService&, SDL_Joystick*, const QString&, bool&, int&, QStringList&);
