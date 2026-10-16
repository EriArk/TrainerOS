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
    auto timer=new QTimer(window);timer->setInterval(650);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&input,&completed,&warnings,&diagnostics]{
        const auto check=[&](bool ok,const QString& text){if(!ok){*failed=true;diagnostics<<QString("Stage %1: %2").arg(*step-1).arg(text);}};
        const auto press=[&](SDL_GameControllerButton b){SDL_JoystickSetVirtualButton(joystick,b,1);input.poll();SDL_JoystickSetVirtualButton(joystick,b,0);input.poll();};
        const auto capture=[&](const QString& name){const auto frame=window->grabWindow();check(!frame.isNull(),"Empty render");if(!directory.isEmpty())check(frame.save(directory+"/"+name+".png"),"Cannot save render");};
        const auto focus=[&]{auto* item=window->activeFocusItem();check(item && item->isVisible() && item->isEnabled(),"Hidden or missing focus");};
        constexpr auto a=SDL_CONTROLLER_BUTTON_B,b=SDL_CONTROLLER_BUTTON_A;
        switch((*step)++) {
        case 0: shell.goToPage(1);break;
        case 1: {
            check(shell.collectionsRoot(),"Games root missing");focus();capture("games-root-960");
            auto* ribbon=window->findChild<QQuickItem*>("series-grid");auto* grid=window->findChild<QQuickItem*>("games-platforms");
            QFile geometry(directory+"/root-geometry.txt"); if(geometry.open(QIODevice::WriteOnly))geometry.write(QString("ribbon %1 x %2, grid %3 x %4, platform %5").arg(ribbon?ribbon->width():0).arg(ribbon?ribbon->height():0).arg(grid?grid->width():0).arg(grid?grid->height():0).arg(shell.gamesPlatforms().front().toMap()["name"].toString()).toUtf8());
            check(ribbon && qAbs(ribbon->width()/ribbon->property("cell").toDouble()-6)<0.01,"Six circular collections must fit");
            check(grid && qAbs(grid->width()/grid->property("cellWidth").toDouble()-grid->property("columns").toInt())<0.01 && qAbs(grid->height()/grid->property("cellHeight").toDouble()-1)<0.01,"Platform ribbon must use adaptive columns in one row");
            for(auto* item:window->findChildren<QQuickItem*>())if(item->objectName().startsWith("games-platform-") && item->isVisible()) {
                check(qAbs(item->width()/item->height()-3.0/4.0)<0.001,"Platform cover proportions must be 3:4");
                check(qAbs(item->parentItem()->height()-item->y()-item->height()-6)<0.01,"Cover bottom inset must remain six units");
            }
            press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);break;
        }
        case 2: check(shell.gamesRootZone()=="platforms","Down must enter platforms");focus();capture("games-platform-focus");press(a);break;
        case 3: check(!shell.collectionsRoot() && shell.multiverse()->route()=="games","Platform must open games directly");capture("platform-browser");press(b);break;
        case 4: check(shell.collectionsRoot() && shell.gamesRootZone()=="platforms","Back must restore platform focus");press(SDL_CONTROLLER_BUTTON_DPAD_UP);break;
        case 5: check(shell.gamesRootZone()=="collections","Up must restore collections");shell.openGamesRootItem("collections",shell.gamesCollections().front().toMap()["id"].toString());break;
        case 6: check(!shell.collectionsRoot(),"Collection opens directly");capture("collection-browser");press(b);window->resize(1920,1080);break;
        case 7: capture("games-root-1080");window->resize(1280,800);break;
        case 8: capture("games-root-16-10");shell.openGamesRootItem("collections","_create");break;
        case 9: check(shell.collectionManager()->isOpen(),"Create card opens editor");capture("collection-create");press(b);window->resize(960,540);break;
        case 10: capture("collection-ribbon-end");shell.dispatch(Action::ToggleContinue);break;
        case 11: check(shell.drawerOpen(),"Global Y drawer unavailable");press(b);shell.dispatch(Action::Secondary);break;
        case 12: check(shell.quickAppsOpen(),"Global X drawer unavailable");press(b);shell.openGamesRootItem("collections","pokemon");press(b);break;
        default:
            check(shell.collectionsRoot(),"Return to root failed");check(warnings==0,"QML warnings");completed=true;timer->stop();
            if(!directory.isEmpty()){QFile f(directory+"/collections-verification.txt");if(f.open(QIODevice::WriteOnly))f.write(((*failed?QString("FAILED\n"):QString("PASSED\n"))+diagnostics.join('\n')).toUtf8());}
            QCoreApplication::exit(*failed?1:0);break;
        }
    });timer->start();
}
