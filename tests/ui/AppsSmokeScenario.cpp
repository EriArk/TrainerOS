#include "AppsSmokeScenario.h"
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QIcon>
#include <QFileInfo>
#include <QTimer>
#include <QFile>
#include <QImage>
#include <memory>
using namespace trainer;
void startAppsSmoke(QQuickWindow* window,ShellController& shell,ControllerInput& input,SDL_Joystick* joystick,
        const QString& directory,bool& completed,int& warnings,QStringList& diagnostics) {
    auto temporary=std::make_shared<QTemporaryDir>();auto step=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);
    auto timer=new QTimer(window);timer->setInterval(350);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&input,&completed,&warnings,&diagnostics]{
        auto* flow=shell.apps();
        const auto check=[&](bool ok,const QString& why){if(!ok){*failed=true;diagnostics<<why;}};
        const auto press=[&](SDL_GameControllerButton button){SDL_JoystickSetVirtualButton(joystick,button,1);input.poll();SDL_JoystickSetVirtualButton(joystick,button,0);input.poll();};
        const auto capture=[&](const QString& name){auto image=window->grabWindow();check(!image.isNull(),"Empty Apps frame");if(!directory.isEmpty())check(image.save(directory+'/'+name+".png"),"Failed to save Apps frame");};
        switch((*step)++) {
        case 0: {
            flow->configure(temporary->path(),"apps-smoke");QJsonArray rows;
            for(int i=0;i<28;++i)rows.append(QJsonObject{{"id",QString("app-%1.desktop").arg(i,2,10,QChar('0'))},{"name",QString("Application %1").arg(i,2,10,QChar('0'))}});
            flow->accept({{"event","catalogue"},{"rows",rows}});flow->accept({{"event","ready"}});
            shell.goToPage(4);press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);break;
        }
        case 1:
            check(shell.page()==5,"R1 did not reach Apps");check(shell.primaryNames().last()=="Apps","Apps is not a primary");
            check(window->findChild<QObject*>("apps-grid") && window->findChild<QObject*>("apps-grid")->property("count").toInt()==28,"Application catalog was not rendered");
            check(shell.navigationState()["page"].toString()=="apps","Apps checkpoint identity");
            {
                auto* screen=window->findChild<QObject*>("primary-screen");
                auto* dock=window->findChild<QObject*>("apps-chassis-dock");
                check(screen && dock && screen->property("width").toReal()<900
                    && dock->property("x").toReal()>=screen->property("x").toReal()+screen->property("width").toReal(),
                    "Dock must occupy the expanded chassis beyond the recessed screen");
            }
            press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);check(flow->focusIndex()==1,"Apps controller focus");
            press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);check(flow->focusIndex()==6,"Five-column navigation must preserve the focused column");
            press(SDL_CONTROLLER_BUTTON_DPAD_UP);check(flow->focusIndex()==1,"Up must return to the same column");
            press(SDL_CONTROLLER_BUTTON_BACK);break;
        case 2:
            check(flow->menu().size()==4,"Select did not open Apps options");capture("apps-options");
            press(SDL_CONTROLLER_BUTTON_B);check(flow->rail().size()==1,"Favorite missing");
            press(SDL_CONTROLLER_BUTTON_BACK);press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);press(SDL_CONTROLLER_BUTTON_B);break;
        case 3:
            check(flow->rows().size()==27,"Hide did not remove catalog entry");check(flow->rail().isEmpty(),"Hidden favorite stayed in rail");
            press(SDL_CONTROLLER_BUTTON_Y);break;
        case 4:
            check(flow->hiddenView(),"X did not open hidden apps");check(flow->rows().size()==1,"Hidden app missing");capture("apps-hidden");
            flow->options(0);flow->menuAction(1);flow->showHidden();
            flow->accept({{"event","state"},{"running",QJsonArray{"app-00.desktop"}},{"current",""}});
            press(SDL_CONTROLLER_BUTTON_X);break;
        case 5:
            check(flow->railFocused(),"Y cannot reach running/favorite apps");
            if (const auto preview=qEnvironmentVariable("TRAINEROS_APPS_PREVIEW_CATALOGUE");!preview.isEmpty()) {
                QFile file(preview);check(file.open(QIODevice::ReadOnly),"Cannot read real preview catalogue");
                const auto catalogue=QJsonDocument::fromJson(file.readAll()).object();
                int actualIcons=0;
                for(const auto& value:catalogue["rows"].toArray()) {
                    const auto icon=value.toObject()["icon"].toString();
                    if (!(QFileInfo(icon).isAbsolute()?QIcon(icon):QIcon::fromTheme(icon)).pixmap(64,64).isNull())++actualIcons;
                    else diagnostics<<"Missing installed icon: "+value.toObject()["name"].toString()+" ("+icon+")";
                }
                check(actualIcons>=10,"Real application icon theme did not resolve");
                diagnostics<<QString("Real catalogue icons resolved: %1 / %2").arg(actualIcons).arg(catalogue["rows"].toArray().size());
                flow->accept(catalogue);
                for(int i=0;i<4;++i){flow->options(i);flow->menuAction(0);}
                flow->select(0,true);
            }
            capture("apps-workspace");
            press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);check(shell.page()==4,"L1 did not leave Apps");press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            window->resize(800,450);break;
        case 6:
            capture("apps-800");flow->select(27);break;
        case 7:
            capture("apps-scrolled-800");check(flow->focusIndex()==27,"Pointer focus and scrolling");
            flow->accept({{"event","foreground"},{"id","app-00.desktop"}});flow->accept({{"event","controls"}});break;
        case 8:
            // The overlay test provider never launches an actual application.
            check(flow->controlsOpen(),"Application controls missing");flow->accept({{"event","shell"}});break;
        default:
            check(warnings==0,"Apps QML warnings");completed=true;timer->stop();
            if(!directory.isEmpty()){QFile file(directory+"/apps-verification.txt");if(file.open(QIODevice::WriteOnly))file.write(((*failed?QString("FAILED\n"):QString("PASSED\n"))+diagnostics.join('\n')).toUtf8());}
            QCoreApplication::exit(*failed?1:0);break;
        }
    });timer->start();
}
