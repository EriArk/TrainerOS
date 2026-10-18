// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QKeyEvent>
#include <QDir>
#include <QTimer>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <SDL.h>
int main(int argc,char** argv) {
    QtWebEngineQuick::initialize();QGuiApplication app(argc,argv);
    const QString data=qEnvironmentVariable("TRAINEROS_PACKAGE_DATA");if(data.isEmpty())return 2;
    QDir().mkpath(data+"/web");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("gameUrl",QUrl::fromLocalFile(QCoreApplication::applicationDirPath()+"/../web/index.html"));
    engine.rootContext()->setContextProperty("gameData",data);
    engine.load(QUrl("qrc:/Game.qml"));if(engine.rootObjects().isEmpty())return 3;
    auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    SDL_GameController* pad=nullptr;
    auto reopen=[&]{if(pad)SDL_GameControllerClose(pad);pad=nullptr;for(int i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)){pad=SDL_GameControllerOpen(i);break;}};
    reopen();QTimer input;
    QObject::connect(&input,&QTimer::timeout,[&]{SDL_Event event;while(SDL_PollEvent(&event)){
        if(event.type==SDL_QUIT){app.quit();continue;}
        if(event.type==SDL_CONTROLLERDEVICEADDED||event.type==SDL_CONTROLLERDEVICEREMOVED){reopen();continue;}
        if(event.type!=SDL_CONTROLLERBUTTONDOWN&&event.type!=SDL_CONTROLLERBUTTONUP)continue;
        int key=0;switch(event.cbutton.button){
            case SDL_CONTROLLER_BUTTON_DPAD_UP:key=Qt::Key_Up;break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:key=Qt::Key_Down;break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:key=Qt::Key_Left;break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:key=Qt::Key_Right;break;
            case SDL_CONTROLLER_BUTTON_A:key=Qt::Key_Return;break;
        }
        if(key){QKeyEvent e(event.type==SDL_CONTROLLERBUTTONDOWN?QEvent::KeyPress:QEvent::KeyRelease,key,Qt::NoModifier);QCoreApplication::sendEvent(window,&e);}
    }});input.start(16);
    const int result=app.exec();if(pad)SDL_GameControllerClose(pad);SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);return result;
}
