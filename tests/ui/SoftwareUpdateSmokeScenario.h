#pragma once
#include "../SoftwareUpdateFixture.h"
#include "core/input/ControllerInput.h"
#include "core/navigation/ShellController.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QMouseEvent>
#include <QImage>
#include <memory>

inline void startSoftwareUpdateSmoke(QQuickWindow* window,trainer::ShellController& shell,
        trainer::ControllerInput& input,SDL_Joystick* joystick,const QString& fixture,const QString& directory,
        bool& completed,int& warnings,QStringList& diagnostics) {
    auto step=std::make_shared<int>(0), ticks=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);
    auto timer=new QTimer(window);timer->setInterval(300);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&input,&completed,&warnings,&diagnostics] {
        auto* flow=shell.settings()->updates();
        const auto check=[&](bool ok,const QString& text){if(!ok){*failed=true;diagnostics<<text;}};
        const auto press=[&](SDL_GameControllerButton button){SDL_JoystickSetVirtualButton(joystick,button,1);input.poll();SDL_JoystickSetVirtualButton(joystick,button,0);input.poll();};
        const auto capture=[&](const QString& name){const auto frame=window->grabWindow();check(!frame.isNull(),"Empty updates frame");if(!directory.isEmpty())check(frame.save(directory+"/"+name+".png"),"Failed update capture");};
        const std::function<QQuickItem*(QQuickItem*,const QString&)> find=[&](QQuickItem* item,const QString& name)->QQuickItem* {
            if(item->isVisible() && item->objectName()==name)return item;
            for(auto* child:item->childItems())if(auto* result=find(child,name))return result;
            return nullptr;
        };
        const auto tap=[&](const QString& name){
            auto* item=find(window->contentItem(),name);check(item,"Missing update touch target: "+name);if(!item)return;
            const auto p=item->mapToScene(QPointF(item->width()/2,item->height()/2));
            check(p.y()>0 && p.y()<window->height(),"Update action outside viewport: "+name);
            QMouseEvent down(QEvent::MouseButtonPress,p,p,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent up(QEvent::MouseButtonRelease,p,p,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);
        };
        if(++*ticks>170){check(false,"Updates UI timed out: "+flow->status());*step=99;}
        switch(*step) {
        case 0: window->resize(960,540);press(SDL_CONTROLLER_BUTTON_START);shell.activate(0);shell.settings()->selectCategory(5);++*step;break;
        case 1:
            capture("system-960");
            for(int i=0;i<4;++i)press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            if(shell.settings()->rowFocus()!=4){check(false,"Updates row was not focused");*step=99;break;}
            press(trainer::ControllerInput::ConfirmButton);++*step;break;
        case 2:
            check(shell.settings()->updatesOpen(),"Controller did not open System Updates");
            check(flow->status()=="unconfigured","Source build pretended to be OTA configured");capture("unconfigured-960");
            {
                auto source=softwareUpdateFixture(fixture,SDK_PYTHON);source.manager=source.helper;source.installationRoot=fixture;
                source.rollbackAvailable=true;flow->maintenanceGuard=[] {return QString();};flow->configure(source);
            }
            ++*step;break;
        case 3: press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);++*step;break;
        case 4: tap("updates-check");++*step;break;
        case 5:
            if(flow->busy())break;
            check(flow->status()=="available" && shell.downloads()->tasks().isEmpty(),"Check downloaded payload or lost available result");
            capture("available-960");for(int i=0;i<2;++i)press(SDL_CONTROLLER_BUTTON_DPAD_UP);*step=50;break;
        case 50:
            capture("overview-960");for(int i=0;i<5;++i)press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);*step=6;break;
        case 6: tap("updates-notes");++*step;break;
        case 7:
            check(flow->notesOpen(),"Release notes did not open");press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);capture("notes-960");tap("updates-notes-back");++*step;break;
        case 8: press(SDL_CONTROLLER_BUTTON_DPAD_UP);press(SDL_CONTROLLER_BUTTON_DPAD_UP);++*step;break;
        case 9: tap("updates-download");++*step;break;
        case 10:
            if(flow->progress()<=0)break;
            capture("download-960");check(!shell.downloads()->tasks().isEmpty(),"Software task missing from global Downloads");
            press(trainer::ControllerInput::ConfirmButton);++*step;break;
        case 11:
            if(flow->busy())break;
            check(flow->status()=="paused","Controller pause failed");capture("paused-960");
            press(trainer::ControllerInput::BackButton);check(!shell.settings()->updatesOpen() && shell.settings()->rowFocus()==4,"Back lost System Updates focus");
            press(trainer::ControllerInput::ConfirmButton);++*step;break;
        case 12:
            check(flow->status()=="paused","Reopening Settings discarded paused update");
            for(int i=0;i<3;++i)press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);++*step;break;
        case 13: tap("updates-downloads");++*step;break;
        case 14:
            check(shell.downloads()->isOpen(),"Downloads link failed");capture("downloads-960");
            shell.downloads()->activate(0);++*step;break;
        case 15:
            if(flow->busy())break;
            check(flow->status()=="prepared" && flow->installedVersion()=="fixture-6","Prepared update changed installed version");
            shell.downloads()->close();window->resize(1920,1080);++*step;break;
        case 16:
            for(int i=0;i<6;++i)press(SDL_CONTROLLER_BUTTON_DPAD_UP);++*step;break;
        case 17:
            capture("prepared-1080");window->resize(960,540);
            for(int i=0;i<8;++i)press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);*step=170;break;
        case 170: tap("updates-apply");*step=171;break;
        case 171:
            check(!flow->busy() && flow->status()=="prepared" && flow->message().contains("Confirm"),"Apply skipped confirmation");
            capture("apply-confirm-960");press(trainer::ControllerInput::BackButton);
            check(!flow->message().contains("Confirm") && flow->status()=="prepared","Back did not cancel apply confirmation");
            window->resize(1920,1080);flow->activate(0);softwareUpdateMode(fixture,"error");flow->check();*step=18;break;
        case 18:
            if(flow->busy())break;
            check(flow->status()=="error","Offline error was reported as success");capture("error-1080");
            for(int i=flow->focusIndex();i<flow->actions().size()-1;++i)press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);++*step;break;
        case 19: tap("updates-back");check(!shell.settings()->updatesOpen(),"Touch Back failed");++*step;break;
        default:
            check(warnings==0,"QML warnings in Software Updates");timer->stop();completed=true;
            if(!directory.isEmpty()){QFile report(directory+"/verification.txt");if(report.open(QIODevice::WriteOnly))report.write(((*failed?QString("FAILED\n"):QString("PASSED\n"))+diagnostics.join('\n')).toUtf8());}
            QCoreApplication::exit(*failed?1:0);
        }
    });timer->start();
}
