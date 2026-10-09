#include "WorldsSmokeScenario.h"
#include <QCoreApplication>
#include <QQuickItem>
#include <QImage>
#include <QFile>
#include <QTimer>
#include <memory>
using namespace trainer;
void startWorldsSmoke(QQuickWindow* window, ShellController& shell, ControllerInput& input,
        MockAdventureAdapter&, SDL_Joystick* joystick, const QString& directory,
        bool& completed, int& warnings, QStringList& diagnostics) {
    auto step=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);
    auto timer=new QTimer(window);timer->setInterval(500);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&input,&completed,&warnings,&diagnostics]{
        const auto check=[&](bool ok,const QString& text){if(!ok){*failed=true;diagnostics<<QString("Stage %1: %2").arg(*step-1).arg(text);}};
        const auto press=[&](SDL_GameControllerButton b){SDL_JoystickSetVirtualButton(joystick,b,1);input.poll();SDL_JoystickSetVirtualButton(joystick,b,0);input.poll();};
        const auto capture=[&](const QString& name){const auto frame=window->grabWindow();check(!frame.isNull(),"Empty render");if(!directory.isEmpty())check(frame.save(directory+"/"+name+".png"),"Cannot save render");};
        const auto focus=[&]{auto* item=window->activeFocusItem();check(item && item->isVisible() && item->isEnabled(),"Hidden or missing focus");};
        auto* editor=shell.collectionManager();
        constexpr auto a=SDL_CONTROLLER_BUTTON_B,b=SDL_CONTROLLER_BUTTON_A;
        switch((*step)++) {
        case 0:
            check(input.connected(),"SDL controller missing");press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);break;
        case 1:
            check(shell.collectionsRoot(),"Collections root missing");capture("collections");focus();press(a);break;
        case 2:
            check(shell.multiverse()->collection()=="pokemon" && shell.multiverse()->route()=="games","Pokemon must open games directly");
            check(!shell.multiverse()->games().isEmpty(),"Pokemon games missing");capture("pokemon-collection");focus();
            press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);break;
        case 3:
            check(shell.multiverse()->focusIndex()==1,"Controller cannot browse game wheel");
            press(b);check(shell.collectionsRoot(),"Back did not return to collections");press(SDL_CONTROLLER_BUTTON_BACK);break;
        case 4:
            check(editor->isOpen(),"Select did not open collection creation");press(a);break;
        case 5:
            check(shell.keyboard()->isOpen(),"Collection name keyboard missing");press(b);
            editor->applyText("Weekend picks");press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);press(a);break;
        case 6:
            check(editor->title()=="Choose games","Manual game picker missing");
            press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);press(a);capture("collection-game-picker");
            press(SDL_CONTROLLER_BUTTON_DPAD_UP);press(a);break;
        case 7:
            check(!editor->isOpen() && editor->definitions().size()==1,"Collection not saved");
            shell.activate(shell.collections().size()-1);break;
        case 8:
            check(shell.multiverse()->games().size()==1,"Manual membership not reflected in game list");
            capture("custom-collection");press(b);window->resize(800,450);break;
        case 9:
            capture("collections-800");shell.editCollection(shell.collections().size()-1);break;
        case 10:
            check(editor->isOpen(),"Touch edit collection failed");capture("collection-editor-800");
            press(SDL_CONTROLLER_BUTTON_START);break;
        case 11:
            check(shell.menuOpen(),"Start unavailable over collection editor");press(b);press(b);
            shell.goToPage(0);break;
        case 12: {
            const auto id=shell.currentAdventureId();
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);input.poll();
            SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);input.poll();
            check(shell.currentAdventureId()==id && shell.faceNames().isEmpty(),"Home still cycles collections");
            press(SDL_CONTROLLER_BUTTON_X);break;
        }
        case 13:
            check(shell.drawerOpen(),"Recent games drawer missing");capture("recent-games-800");press(b);break;
        default:
            check(!shell.drawerOpen(),"Back did not dismiss recent games");check(warnings==0,"QML warnings");completed=true;timer->stop();
            if(!directory.isEmpty()){QFile f(directory+"/collections-verification.txt");if(f.open(QIODevice::WriteOnly))f.write(((*failed?QString("FAILED\n"):QString("PASSED\n"))+diagnostics.join('\n')).toUtf8());}
            QCoreApplication::exit(*failed?1:0);break;
        }
    });timer->start();
}
