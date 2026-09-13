#include "CenterSmokeScenario.h"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QQuickItem>
#include <QTimer>
#include <memory>

using namespace trainer;
void startCenterSmoke(QQuickWindow* window,ShellController& shell,SessionState& session,LocalStateStore& store,
        ControllerInput& input,SDL_Joystick* joystick,const QString& dataDir,const QString& screenshots,
        bool& completed,int& warnings,QStringList& diagnostics) {
    auto stage=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);
    auto timer=new QTimer(window);timer->setInterval(230);
    const auto rom=QDir(dataDir).filePath("content/center.gba"), save=rom+".srm";
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&session,&store,&input,&completed,&warnings,&diagnostics]{
        const auto check=[&](bool ok,const QString& message){if(!ok){*failed=true;diagnostics.append(QString("Center stage %1: %2").arg(*stage).arg(message));}};
        const auto write=[&](const QString& path,const QByteArray& bytes){QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);check(f.open(QIODevice::WriteOnly),"Fixture open");check(f.write(bytes)==bytes.size(),"Fixture write");};
        const auto read=[&]{QFile f(save);if(!f.open(QIODevice::ReadOnly)){check(false,"Fixture read");return QByteArray{};}return f.readAll();};
        const auto press=[&](SDL_GameControllerButton button,int count=1){for(int i=0;i<count;++i){SDL_JoystickSetVirtualButton(joystick,button,1);input.poll();SDL_JoystickSetVirtualButton(joystick,button,0);input.poll();}};
        const auto focus=[&](const QString& name){return window->activeFocusItem()&&window->activeFocusItem()->objectName()==name;};
        const auto capture=[&](const QString& name){
            if(!screenshots.isEmpty())check(window->grabWindow().save(screenshots+"/"+name+".png"),"Screenshot");
            const auto item=window->activeFocusItem();check(item&&item->isVisible(),"Visible actual focus");
            if(item)check(QRectF(0,0,window->width(),window->height()).contains(item->mapRectToScene(item->boundingRect())),"Focused control inside screen");
        };
        if(session.blocked()||store.pending()||shell.center()->busy())return;
        constexpr auto a=SDL_CONTROLLER_BUTTON_A,b=SDL_CONTROLLER_BUTTON_B,x=SDL_CONTROLLER_BUTTON_X,y=SDL_CONTROLLER_BUTTON_Y;
        constexpr auto down=SDL_CONTROLLER_BUTTON_DPAD_DOWN,start=SDL_CONTROLLER_BUTTON_START;
        switch((*stage)++) {
        case 0: {
            write(rom,"Original content-free fixture");write(save,"FIRST SAVE");
            AdventureRegistration r;r.adventure.id="center-fixture";r.adventure.title="Pocket Lab";r.adventure.worldId="hoenn";r.adventure.adapterId="backup-fixture";r.contentPath=rom;
            store.saveAdventureAsync(r,window,[failed](LibraryWriteResult result){if(!result.success)*failed=true;});break;
        }
        case 1:
            shell.restoreNavigation({{"page","home"},{"homeAdventure","center-fixture"},{"homeResume","old-moment"}});
            press(start);press(down,2);press(a);break;
        case 2:
            check(shell.service()=="center"&&focus("center-row-0"),"System-menu Center opens linked Adventure list");capture("adventures");press(a);break;
        case 3:
            check(shell.center()->canCreate()&&focus("center-check"),"Empty shelf keeps focus and offers backup");capture("empty");press(y);break;
        case 4:
            check(shell.center()->rows().size()==1&&focus("center-row-0"),"New backup is selected");check(read()=="FIRST SAVE","Backup preserves source");capture("first-copy");
            press(a);check(shell.center()->confirming(),"A asks before restore");press(b);check(!shell.center()->confirming()&&read()=="FIRST SAVE","B cancels restore");
            write(save,"SECOND SAVE");press(x);break;
        case 5:
            press(a);break;
        case 6:
            check(focus("center-confirm"),"Restore confirmation traps focus");capture("confirm");press(start);press(b);check(shell.center()->confirming(),"Start overlays confirmation without accepting it");press(a);break;
        case 7:
            check(read()=="FIRST SAVE"&&shell.center()->rows().size()==2,"Restore succeeds and protects previous bytes");
            check(shell.navigationState()["homeResume"].toString().isEmpty(),"Restore clears Home's old moment selection");capture("restored");press(a);press(a);break;
        case 8:
            check(read()=="SECOND SAVE"&&shell.center()->rows().size()==3,"Protection copy can undo the restore");
            press(a);press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);check(shell.service().isEmpty()&&!shell.center()->confirming(),"Global section action discards confirmation");
            press(start);press(SDL_CONTROLLER_BUTTON_DPAD_UP,6);press(down,2);press(a);press(a);window->resize(1920,1080);break;
        case 9:
            capture("copies-1080p");press(y);press(b);break;
        case 10:
            check(shell.service().isEmpty()&&shell.menuOpen(),"Leaving a pending operation returns to its system-menu opener");
            press(a);press(a);window->resize(1024,768);break;
        case 11:
            check(shell.center()->rows().size()==4&&read()=="SECOND SAVE","Background backup finishes without replacing the source");capture("copies-letterbox");
            check(warnings==0,"QML warnings");completed=true;timer->stop();
            if(!screenshots.isEmpty()){QFile report(screenshots+"/verification.txt");if(report.open(QIODevice::WriteOnly))report.write(((*failed?QString("FAILED\n"):QString("PASSED\n"))+diagnostics.join('\n')).toUtf8());}
            if(*failed){qCritical().noquote()<<diagnostics.join('\n');QCoreApplication::exit(1);}else window->close();break;
        }
    });timer->start();
}
