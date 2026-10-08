#include "DownloadsSmokeScenario.h"
#include <QCoreApplication>
#include <QImage>
#include <QTimer>
#include <QFile>
#include <memory>

using namespace trainer;
void startDownloadsSmoke(QQuickWindow* window, ShellController& shell, ControllerInput& input,
        SDL_Joystick* joystick, const QString& directory, bool& completed, int& warnings, QStringList& diagnostics) {
    auto step=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);
    auto timer=new QTimer(window);timer->setInterval(350);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&input,&completed,&warnings,&diagnostics]{
        const auto check=[&](bool ok,const QString& reason){if(!ok){*failed=true;diagnostics<<reason;}};
        const auto press=[&](SDL_GameControllerButton button){
            SDL_JoystickSetVirtualButton(joystick,button,1);input.poll();
            SDL_JoystickSetVirtualButton(joystick,button,0);input.poll();
        };
        const auto capture=[&](const QString& name){
            const auto frame=window->grabWindow();check(!frame.isNull(),"Empty frame");
            if(!directory.isEmpty())check(frame.save(directory+'/'+name+".png"),"Could not save frame");
        };
        auto* flow=shell.downloads();
        switch((*step)++) {
        case 0: {
            const auto row=[](QString id,QString title,QString state,QString detail,QString platform){return QVariantMap{
                {"id",id},{"title",title},{"state",state},{"detail",detail},{"platform",platform},{"shape","handheld"},
                {"source","Isolated test provider"},{"terminal",state=="done"},
                {"actions",state=="done"?QVariantList{}:QVariantList{QVariantMap{{"id",state=="attention"?"open":"pause"},{"label",state=="attention"?"Choose match":"Pause after game"}}}}};};
            flow->publish("fixture",{row("done","Finished game","done","Completed","GBA"),
                row("choice","Game awaiting an edition","attention","Choose the matching edition","DS"),
                row("active","Currently downloading","running","Downloading selected artwork","SNES")});
            press(SDL_CONTROLLER_BUTTON_START);shell.activate(12);break;
        }
        case 1:
            check(flow->isOpen(),"Downloads did not open from Start");
            check(flow->tasks().first().toMap()["id"]=="active","Active task is not at the top");
            capture("downloads-active");press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);break;
        case 2:
            check(flow->tasks().value(flow->focusIndex()).toMap()["section"]=="Downloaded","Completed section is missing");
            capture("downloads-completed");window->resize(800,450);break;
        case 3:
            capture("downloads-completed-800");flow->select(1);break; // Same command as the touch row.
        case 4:
            check(flow->tasks().value(flow->focusIndex()).toMap()["id"]=="choice","Pointer selection changed identity");
            press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);check(flow->actionsFocused(),"Controller cannot reach task actions");
            capture("downloads-attention-800");press(SDL_CONTROLLER_BUTTON_A);break;
        case 5:
            check(!flow->isOpen(),"Back did not close Downloads");press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);break;
        default:
            check(shell.page()==1,"Controller cannot browse after closing Downloads");
            check(warnings==0,"QML warnings");completed=true;timer->stop();
            if(!directory.isEmpty()){QFile file(directory+"/downloads-verification.txt");if(file.open(QIODevice::WriteOnly))file.write(((*failed?QString("FAILED\n"):QString("PASSED\n"))+diagnostics.join('\n')).toUtf8());}
            QCoreApplication::exit(*failed?1:0);break;
        }
    });
    timer->start();
}
