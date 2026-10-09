#pragma once
#include "DownloadsSmokeScenario.h"
#include "../fixtures/adapters/CounterExperience.h"
#include <QCoreApplication>
#include <QTimer>
#include <QImage>
#include <QFile>
inline void startExperienceSmoke(QQuickWindow* window,trainer::ShellController& shell,trainer::ControllerInput& input,
        SDL_Joystick* joystick,const QString& directory,bool& completed,int& warnings,QStringList& diagnostics) {
    auto timer=new QTimer(window);timer->setInterval(400);
    auto stage=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&input,&completed,&warnings,&diagnostics]{
        const auto check=[&](bool ok,QString message){if(!ok){*failed=true;diagnostics<<message;}};
        auto* rpg=static_cast<trainer::test::CounterExperience*>(shell.module("rpg"));
        auto* racing=static_cast<trainer::test::CounterExperience*>(shell.module("racing"));
        const auto select=[&](QString game){auto state=shell.navigationState();state["homeAdventure"]=game;state["page"]="home";shell.restoreNavigation(state);};
        const auto body=[&]{auto* item=window->findChild<QObject*>("fixture-body");return item?item->property("text").toString():QString();};
        const auto labels=[&]{QStringList result;if(auto* row=window->findChild<QQuickItem*>("shell-button-hints"))for(auto* item:row->childItems())if(item->isVisible() && item->property("label").isValid())result<<item->property("label").toString();return result;};
        const auto press=[&](SDL_GameControllerButton button){SDL_JoystickSetVirtualButton(joystick,button,1);input.poll();SDL_JoystickSetVirtualButton(joystick,button,0);input.poll();};
        switch((*stage)++) {
        case 0:select(rpg->matchGame);break;
        case 1:check(body().startsWith("rpg-home"),"Independent RPG presenter not mounted");shell.goToPage(2);break;
        case 2:press(SDL_CONTROLLER_BUTTON_B);break;
        case 3:check(rpg->count==1,"Controller did not reach registered module");check(body().endsWith("1"),"Presenter did not observe module progress");select(racing->matchGame);break;
        case 4:check(body().startsWith("racing-home"),"Second presenter did not replace Home");shell.goToPage(2);shell.activate(9,"fixture");break;
        case 5:check(body().endsWith("9"),"Touch dispatcher did not reach registered module");select(rpg->matchGame);break;
        case 6:check(body().endsWith("1"),"Module state did not survive game switch");
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/independent-module.png"),"Empty fixture capture");
            rpg->setEnabled(false);break;
        case 7:check(shell.homeView()=="pokemon-home","Disabled module did not fall back to the next compatible presenter");shell.goToTrainerFace("ra");break;
        case 8:shell.goToPage(0);break;
        case 9:check(labels().contains("Game actions") && !labels().contains("Refresh"),"Previous adapter face leaked hints into Home");shell.goToPage(4);break;
        case 10:{QStringList expected;for(const auto& hint:shell.social()->hints())expected<<hint.toMap()["label"].toString();expected<<"Options"<<"System";check(labels()==expected,"Hidden adapter leaked hints into Social");break;}
        default:
            timer->stop();completed=true;
            QFile report(directory+"/experience-check.txt");if(report.open(QIODevice::WriteOnly))report.write((diagnostics.join('\n')+QString("\nqmlWarnings=%1\n").arg(warnings)).toUtf8());
            QCoreApplication::exit(*failed || warnings?4:0);
        }
    });timer->start();
}
