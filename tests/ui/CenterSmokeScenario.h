#pragma once
#include "core/storage/SessionState.h"
#include "core/input/ControllerInput.h"
#include <QQuickWindow>

void startCenterSmoke(QQuickWindow*,trainer::ShellController&,trainer::SessionState&,trainer::LocalStateStore&,
    trainer::ControllerInput&,SDL_Joystick*,const QString& dataDir,const QString& screenshots,
    bool& completed,int& warnings,QStringList& diagnostics);
