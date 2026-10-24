#pragma once
#include "core/storage/SessionState.h"
#include "core/input/ControllerInput.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QMouseEvent>
#include <QTimer>
#include <QFile>
#include <QStorageInfo>
#include <QImage>
#include <memory>
#include "../FirstRunAccountFixture.h"

inline void prepareFirstRunSmoke(trainer::SessionState& session, trainer::ShellController& shell, const QString& directory) {
    const auto root=directory+"/roms";
    session.firstRun()->configure(directory,root);
    session.firstRun()->clock()->read=[] { return trainer::ClockSnapshot{"UTC",{}, {"UTC"},true,true,true,true}; };
    session.firstRun()->locations=[root,directory](const QString&) {
        const QStorageInfo disk(directory);
        return QList<trainer::LibraryLocation>{{"Test storage",root,disk.rootPath(),QString::fromUtf8(disk.device()),disk.bytesAvailable()}};
    };
    if(!trainer::FirstRunAccountFixture::prepareScraper(shell,directory))qFatal("Cannot prepare local scraper fixture");
}

inline void startFirstRunSmoke(QQuickWindow* window, trainer::ShellController& shell, trainer::SessionState& session,
        trainer::LocalStateStore& store, trainer::ControllerInput& input, SDL_Joystick* joystick,
        const QString& directory, bool& completed, int& warnings, QStringList& diagnostics) {
    auto step=std::make_shared<int>(0), ticks=std::make_shared<int>(0);
    auto failed=std::make_shared<bool>(false);
    auto timer=new QTimer(window);timer->setInterval(350);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&session,&store,&input,&completed,&warnings,&diagnostics] {
        auto* flow=session.firstRun();
        const auto check=[&](bool ok,const QString& text){if(!ok){*failed=true;diagnostics<<text;}};
        const auto press=[&](SDL_GameControllerButton button){SDL_JoystickSetVirtualButton(joystick,button,1);input.poll();SDL_JoystickSetVirtualButton(joystick,button,0);input.poll();};
        const auto capture=[&](const QString& name){auto frame=window->grabWindow();check(!frame.isNull(),"Empty setup frame");if(!directory.isEmpty())check(frame.save(directory+"/"+name+".png"),"Failed setup capture");};
        const std::function<QQuickItem*(QQuickItem*,const QString&)> find=[&](QQuickItem* item,const QString& name)->QQuickItem*{
            if(item->isVisible() && item->objectName()==name)return item;
            for(auto* child:item->childItems())if(auto* result=find(child,name))return result;
            return nullptr;
        };
        const auto tap=[&](const QString& name) {
            auto* item=find(window->contentItem(),name);check(item,"Missing touch target: "+name);if(!item)return;
            const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));
            QMouseEvent down(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent up(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);
        };
        if(++*ticks>130){check(false,"Setup UI timed out");*step=99;}
        switch(*step) {
        case 0:
            if(!session.firstRunPage())break;
            window->resize(960,540);++*step;break;
        case 1: capture("welcome-960");press(trainer::ControllerInput::ConfirmButton);++*step;break;
        case 2: capture("controls-960");tap("setup-use-touch");++*step;break;
        case 3:
            check(flow->stage()=="network" && flow->checkedControls()==0,"Touch continuation pretended a physical check");
            press(trainer::ControllerInput::BackButton);++*step;break;
        case 4:
            for(auto button:{SDL_CONTROLLER_BUTTON_DPAD_UP,SDL_CONTROLLER_BUTTON_DPAD_DOWN,SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT,trainer::ControllerInput::ConfirmButton,trainer::ControllerInput::BackButton})press(button);
            check(flow->stage()=="network","SDL controller check failed");flow->activate(1);++*step;break;
        case 5:
            if(flow->busy())break;capture("time-960");flow->activate(3);++*step;break;
        case 6: capture("storage-960");flow->activate(0);++*step;break;
        case 7:
            if(flow->stage()!="trainer")break;
            shell.trainerSetup()->applyName("Setup Test");shell.trainerSetup()->activate(3);++*step;break;
        case 8: capture("trainer-review-960");shell.trainerSetup()->activate(0);++*step;break;
        case 9:
            if(flow->stage()!="accounts" || store.pending())break;
            check(store.trainers().size()==1,"Setup did not create exactly one Trainer");capture("accounts-960");
            flow->activate(4);check(flow->stage()=="accounts" && !flow->accountsReady(),"Mandatory accounts were skipped");
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);press(SDL_CONTROLLER_BUTTON_START);
            check(shell.page()==0 && !shell.menuOpen(),"Setup allowed a global escape");
            flow->activate(1);++*step;break;
        case 10: capture("ra-960");session.activateSetupService(1);++*step;break;
        case 11:
            check(shell.keyboard()->isOpen(),"RA did not reuse keyboard");capture("ra-keyboard-960");
            press(trainer::ControllerInput::BackButton);session.backInSetup();++*step;break;
        case 12:
            check(flow->service().isEmpty() && flow->focusIndex()==1,"RA return lost account focus");
            flow->activate(2);++*step;break;
        case 13: capture("scraper-960");shell.scraper()->activateSetting(12);check(!shell.scraper()->isOpen(),"Setup started hidden scrape job");tap("setup-back");++*step;break;
        case 14: check(flow->service().isEmpty(),"Touch Back did not return");flow->activate(0);++*step;break;
        case 15:
            capture("fluxer-960");session.backInSetup();
            trainer::FirstRunAccountFixture::connectSocial(shell);
            shell.scraper()->activateSetting(2);*step=151;break;
        case 151:
            if(shell.scraper()->busy())break;
            check(flow->accountsReady(),"Confirmed providers did not unlock setup");
            for(int i=0;i<4;++i)press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);*step=152;break;
        case 152:
            check(flow->focusIndex()==4,"Controller did not reach required-account Continue");
            capture("accounts-verified-960");tap("setup-choice-4");
            check(flow->stage()=="preferences","Touch Continue did not accept verified providers");*step=16;break;
        case 16:
            capture("comfort-960");for(int i=0;i<5;++i)press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);*step=160;break;
        case 160: {
            auto* choice=find(window->contentItem(),"setup-choice-5");
            check(flow->focusIndex()==5 && choice,"Controller did not reach Continue below the fold");
            if(choice){const auto p=choice->mapToScene(QPointF(choice->width()/2,choice->height()/2));check(p.y()>0 && p.y()<window->height()-30,"Focused Continue is outside the viewport");}
            capture("comfort-continue-960");
            for(int i=0;i<5;++i)press(SDL_CONTROLLER_BUTTON_DPAD_UP);press(trainer::ControllerInput::ConfirmButton);*step=17;break;
        }
        case 17: capture("appearance-960");session.activateSetupService(1);++*step;break;
        case 18:
            if(shell.settings()->saving())break;
            check(shell.settings()->reducedMotion(),"Appearance did not persist through shared service");
            window->resize(1920,1080);++*step;break;
        case 19: capture("appearance-1080");session.backInSetup();flow->activate(1);++*step;break;
        case 20: capture("sound-1080");session.backInSetup();flow->activate(4);*step=201;break;
        case 201: {
            auto* toggle=find(window->contentItem(),"setup-service-row-0");
            check(toggle && toggle->property("checked").toBool()==shell.settings()->controls()[0].toMap()["checked"].toBool(),"Nearby toggle does not reflect its own service");
            capture("nearby-1080");session.backInSetup();flow->activate(3);*step=21;break;
        }
        case 21: capture("communication-1080");session.backInSetup();flow->activate(5);++*step;break;
        case 22: capture("ready-1080");press(trainer::ControllerInput::BackButton);check(flow->stage()=="preferences","Back from summary lost setup");flow->activate(5);++*step;break;
        case 23: flow->activate(0);check(!session.blocked() && shell.page()==0,"Setup did not reach Home");++*step;break;
        default:
            capture("home-1080");check(warnings==0,"QML warnings during first setup");
            timer->stop();completed=true;
            if(!directory.isEmpty()){QFile report(directory+"/verification.txt");if(report.open(QIODevice::WriteOnly))report.write(((*failed?QString("FAILED\n"):QString("PASSED\n"))+diagnostics.join('\n')).toUtf8());}
            qInfo() << (*failed?"First-run UI FAILED":"First-run UI passed: local profile, SDL, pointer, services and Home");
            QCoreApplication::exit(*failed?1:0);
        }
    });
    timer->start();
}
