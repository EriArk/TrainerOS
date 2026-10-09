#include "adapters/pokemon/PokemonExperience.h"
#include "CenterSmokeScenario.h"
#include "RenderSettlement.h"
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
    auto focusWait=std::make_shared<int>(0);
    auto timer=new QTimer(window);timer->setInterval(230);
    const auto rom=QDir(dataDir).filePath("content/center.gba"), save=rom+".srm";
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&session,&store,&input,&completed,&warnings,&diagnostics]{
        const auto check=[&](bool ok,const QString& message){if(!ok){*failed=true;diagnostics.append(QString("Center stage %1: %2").arg(*stage).arg(message));}};
        const auto write=[&](const QString& path,const QByteArray& bytes){QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);check(f.open(QIODevice::WriteOnly),"Fixture open");check(f.write(bytes)==bytes.size(),"Fixture write");};
        const auto read=[&]{QFile f(save);if(!f.open(QIODevice::ReadOnly)){check(false,"Fixture read");return QByteArray{};}return f.readAll();};
        const auto press=[&](SDL_GameControllerButton button,int count=1){for(int i=0;i<count;++i){SDL_JoystickSetVirtualButton(joystick,button,1);input.poll();SDL_JoystickSetVirtualButton(joystick,button,0);input.poll();}};
        const auto trigger=[&](SDL_GameControllerAxis axis){SDL_JoystickSetVirtualAxis(joystick,axis,32767);input.poll();SDL_JoystickSetVirtualAxis(joystick,axis,-32768);input.poll();};
        const auto focus=[&](const QString& name){return window->activeFocusItem()&&window->activeFocusItem()->objectName()==name;};
        const auto capture=[&](const QString& name){
            if(!screenshots.isEmpty())check(window->grabWindow().save(screenshots+"/"+name+".png"),"Screenshot");
            const auto item=window->activeFocusItem();check(item&&item->isVisible(),"Visible actual focus");
            if(item)check(QRectF(0,0,window->width(),window->height()).contains(item->mapRectToScene(item->boundingRect())),"Focused control inside screen");
        };
        if(session.blocked()||store.pending()||pokemonModule(shell).center()->busy())return;
        if(*stage==2 && pokemonModule(shell).face()=="center" && pokemonModule(shell).center()->clinicOpen()
            && waitForFocus(window,"clinic-action",*focusWait))return;
        const auto drawer=window->findChild<QQuickItem*>("continue-drawer");
        if(drawer && ((shell.drawerOpen() && drawer->height()<228) || (!shell.drawerOpen() && drawer->height()>53)))return;
        constexpr auto a=SDL_CONTROLLER_BUTTON_B,b=SDL_CONTROLLER_BUTTON_A,x=SDL_CONTROLLER_BUTTON_Y,y=SDL_CONTROLLER_BUTTON_X;
        constexpr auto down=SDL_CONTROLLER_BUTTON_DPAD_DOWN,start=SDL_CONTROLLER_BUTTON_START;
        constexpr auto select=SDL_CONTROLLER_BUTTON_BACK;
        switch((*stage)++) {
        case 0: {
            write(rom,"Original content-free fixture");write(save,"FIRST SAVE");
            AdventureRegistration r;r.adventure.id="center-fixture";r.adventure.title="Pocket Lab";r.adventure.worldId="hoenn";r.adventure.adapterId="backup-fixture";r.contentPath=rom;
            store.saveAdventureAsync(r,window,[failed,window,&store](LibraryWriteResult result){
                if(!result.success){*failed=true;return;}
                PlaySession played{"center-session","center-fixture",QDateTime::currentDateTimeUtc(),{},{},PlaySessionOutcome::Running};
                store.saveSessionAsync(played,window,[failed](const QString& error){if(!error.isEmpty())*failed=true;});
            });break;
        }
        case 1:
            shell.restoreNavigation({{"version",1},{"page","pokedex"},{"pokemonFace","center"},{"homeAdventure","center-fixture"},{"homeResume","old-moment"}});break;
        case 2:
            check(pokemonModule(shell).face()=="center"&&pokemonModule(shell).center()->clinicOpen()&&focus("clinic-action"),"Center opens the nurse directly");capture("clinic-ready");press(select);break;
        case 3:
            check(pokemonModule(shell).center()->canCreate()&&focus("center-check"),"Empty shelf offers backup");capture("empty");press(select);break;
        case 4:
            check(pokemonModule(shell).center()->rows().size()==1&&focus("center-row-0"),"New backup is selected");check(read()=="FIRST SAVE","Backup preserves source");capture("first-copy");
            press(a);check(pokemonModule(shell).center()->confirming(),"A asks before restore");press(b);check(!pokemonModule(shell).center()->confirming()&&read()=="FIRST SAVE","B cancels restore");
            write(save,"SECOND SAVE");press(x);break;
        case 5:press(a);break;
        case 6:
            check(focus("center-confirm"),"Restore confirmation traps focus");capture("confirm");
            press(y);trigger(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);press(SDL_CONTROLLER_BUTTON_GUIDE);press(start);
            check(pokemonModule(shell).center()->confirming()&&!shell.drawerOpen()&&!shell.menuOpen()&&shell.page()==2,"Navigation cannot retarget a restore");press(a);break;
        case 7:
            check(read()=="FIRST SAVE"&&pokemonModule(shell).center()->rows().size()==2,"Restore protects previous bytes");
            check(shell.navigationState()["homeResume"].toString().isEmpty(),"Restore clears old state choice");capture("restored");press(a);press(a);break;
        case 8:
            check(read()=="SECOND SAVE"&&pokemonModule(shell).center()->rows().size()==3,"Protection copy restores exact bytes");press(b);window->resize(1920,1080);break;
        case 9:
            check(pokemonModule(shell).center()->clinicOpen(),"B returns to the same Center face");capture("clinic-1080p");press(a);break;
        case 10:
            check(pokemonModule(shell).center()->treatment()=="done"&&read()=="HEALED SAVE","Healing performs verified treatment");
            check(pokemonModule(shell).center()->rows().size()==4,"Healing has one protection copy");capture("clinic-complete");press(select);break;
        case 11:press(a);press(a);break;
        case 12:
            check(read()=="SECOND SAVE","Healing can be restored exactly");press(b);trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT);window->resize(960,540);break;
        case 13:
            check(pokemonModule(shell).face()=="boxes","Previous peer is Boxes");capture("boxes-unavailable");trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT);break;
        case 14:
            check(pokemonModule(shell).face()=="party","Previous peer is Party");capture("party-unavailable");trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT);break;
        case 15:
            check(pokemonModule(shell).face()=="dex","Previous peer is Dex");capture("peer-dex");press(y);break;
        case 16:
            check(shell.drawerOpen()&&focus("resume-0"),"Shared Y chooses without launch");capture("pokedex-choose");press(a);break;
        case 17:
            check(!shell.drawerOpen()&&shell.currentAdventureId()=="center-fixture","Shared selection retained");
            trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT);break;
        case 18:
            check(pokemonModule(shell).face()=="shops","Previous wraps Dex to Shops");capture("shops-unavailable");trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT);break;
        case 19:
            check(pokemonModule(shell).face()=="playroom","Playroom is a peer");capture("playroom-unavailable");trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT);break;
        case 20:
            check(pokemonModule(shell).face()=="center"&&pokemonModule(shell).center()->clinicOpen(),"Full reverse loop returns to nurse");press(start);break;
        case 21:
            check(shell.menuOpen()&&shell.menuItems().contains("Switch Trainer")&&!shell.menuItems().contains("Pokémon Center"),"Start is system-only");capture("system-center");press(b);press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);break;
        case 22:
            check(shell.page()==3&&!pokemonModule(shell).center()->clinicOpen(),"L1/R1 leaves idle Center");press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);break;
        case 23:
            check(pokemonModule(shell).face()=="center"&&read()=="SECOND SAVE","Return preserves face and original save");
            check(warnings==0,"QML warnings");completed=true;timer->stop();
            if(!screenshots.isEmpty()){QFile report(screenshots+"/verification.txt");if(report.open(QIODevice::WriteOnly))report.write(((*failed?QString("FAILED\n"):QString("PASSED\n"))+diagnostics.join('\n')).toUtf8());}
            if(*failed){qCritical().noquote()<<diagnostics.join('\n');QCoreApplication::exit(1);}else window->close();break;
        }
    });timer->start();
}
