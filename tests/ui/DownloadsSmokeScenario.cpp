#include "DownloadsSmokeScenario.h"
#include <QCoreApplication>
#include <QImage>
#include <QTimer>
#include <QFile>
#include <QQuickItem>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QQmlComponent>
#include <QQmlEngine>
#include <memory>

using namespace trainer;
void startDownloadsSmoke(QQuickWindow* window, ShellController& shell, ControllerInput& input,
        SDL_Joystick* joystick, const QString& directory, bool& completed, int& warnings, QStringList& diagnostics) {
    auto step=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);
    auto commands=std::make_shared<QStringList>();
    auto reorders=std::make_shared<QStringList>();
    QObject::connect(shell.downloads(),&DownloadsController::commandRequested,window,[commands](QString provider,QString,QString command){if(provider=="fixture")commands->append(command);});
    QObject::connect(shell.downloads(),&DownloadsController::reorderRequested,window,[reorders](QString provider,QString id,QString before){if(provider=="fixture")reorders->append(id+":"+before);});
    auto timer=new QTimer(window);timer->setInterval(350);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&input,&completed,&warnings,&diagnostics]{
        const QSignalBlocker guard(timer);
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
        const std::function<QQuickItem*(QQuickItem*,QString)> find=[&](QQuickItem* item,QString name)->QQuickItem*{
            if(item->objectName()==name&&item->isVisible())return item;
            for(auto* child:item->childItems())if(auto* found=find(child,name))return found;return nullptr;
        };
        const auto tap=[&](QString name){
            auto* item=find(window->contentItem(),name);check(item&&item->isEnabled(),"Missing Downloads touch target: "+name);if(!item)return;
            const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));const auto global=QPointF(window->mapToGlobal(point.toPoint()));
            QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);
        };
        switch((*step)++) {
        case 0: {
            const auto row=[](QString id,QString title,QString state,QString detail,QString platform){return QVariantMap{
                {"id",id},{"title",title},{"state",state},{"detail",detail},{"platform",platform},{"shape","handheld"},
                {"source","Isolated test provider"},{"terminal",state=="done"},
                {"description","A small adventure with a big world to explore. Download details stay beside the queue."},
                {"received",1024*1024*12},{"total",1024*1024*64},{"progress",.1875},
                {"destination","/home/player/Downloads"},
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
            break;
        case 5:
            capture("downloads-attention-800");press(static_cast<SDL_GameControllerButton>(input.confirmButton()));
            check(*commands==QStringList{"open"},"Controller action did not route exactly once");
            press(static_cast<SDL_GameControllerButton>(input.backButton()));
            check(flow->isOpen()&&!flow->actionsFocused(),"Back did not return from actions to the queue");
            tap("downloads-task-choice");check(!flow->actionsFocused(),"Touch row should select without a modal");
            break;
        case 6:
            tap("downloads-command-open");check(*commands==QStringList({"open","open"}),"Touch action did not route exactly once");
            tap("downloads-task-choice");check(flow->isOpen()&&!flow->actionsFocused(),"Touch queue did not retain Downloads");
            shell.settings()->cycleTheme(1);break;
        case 7:
            capture("downloads-dark-800");tap("downloads-pause-active");check(commands->last()=="pause","Direct row pause did not route");
            {const auto queued=[](QString id,QString title){return QVariantMap{{"id",id},{"title",title},{"state","queued"},{"reorderable",true},{"source","Crates"},
                {"description","Short races, small cars and a fresh challenge on every track."},{"picture","qrc:/crates/projects/mini-racing.png"},{"received",0},{"total",32*1024*1024},
                {"actions",QVariantList{QVariantMap{{"id","first"},{"label","Download first"}},QVariantMap{{"id","pause"},{"label","Pause"}},QVariantMap{{"id","cancel"},{"label","Cancel"}}}}};};
            flow->publish("fixture",{queued("a","Mini Racing"),queued("b","Second queued download")});}break;
        case 8:
            capture("downloads-queued-dark-800");tap("downloads-first-b");check(commands->last()=="first","Direct download-first button did not route");
            tap("downloads-cancel-a");check(commands->last()=="cancel","Direct cancel button did not route");
            {auto* from=find(window->contentItem(),"downloads-drag-b");auto* to=find(window->contentItem(),"downloads-drag-a");
            check(from&&to,"Drag handles missing");if(from&&to){
                const auto start=from->mapToScene(QPointF(from->width()/2,from->height()/2)),end=to->mapToScene(QPointF(to->width()/2,to->height()/2));
                const auto send=[&](QEvent::Type type,QPointF p,Qt::MouseButton button,Qt::MouseButtons buttons){QMouseEvent e(type,p,QPointF(window->mapToGlobal(p.toPoint())),button,buttons,Qt::NoModifier);QCoreApplication::sendEvent(window,&e);};
                send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);send(QEvent::MouseMove,end,Qt::NoButton,Qt::LeftButton);send(QEvent::MouseButtonRelease,end,Qt::LeftButton,Qt::NoButton);
                check(*reorders==QStringList{"b:a"},"Pointer drag did not route stable queue identities");
            }}flow->publish("fixture",{});break;
        case 9:
            capture("downloads-empty-dark-800");press(static_cast<SDL_GameControllerButton>(input.backButton()));
            check(!flow->isOpen(),"Back did not close Downloads");press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);break;
        case 10: {
            QQmlComponent fixture(qmlEngine(window));
            fixture.setData(R"(
import QtQuick
import TrainerOS
Rectangle {
    objectName: "scraper-fixture"; width: 740; height: 330; color: Theme.paper
    property alias clicked: dataModel.clicked
    QtObject { id: dataModel; property bool busy: true; property bool paused: false; property string status: ""; property int overviewFocus: 3; property int clicked: -1
        property var tasks: [
            {id:"active", title:"A small adventure", platform:"GBA", state:"running", detail:"Downloading selected artwork"},
            {id:"later", title:"Another game", state:"queued"},
            {id:"choice", title:"Choose the edition", state:"attention"},
            {id:"choice2", title:"Another game to identify", state:"attention"}]
        function overviewAction(index) { clicked=index }
        function showOverview() {}
    }
    ScrapeWidget { anchors.fill: parent; anchors.margins: 20; flow: dataModel; controllerFocus: true }
})",QUrl());
            auto* item=qobject_cast<QQuickItem*>(fixture.create());check(item,"Scraper widget fixture: "+fixture.errorString());
            if(item){item->setParent(window);item->setParentItem(window->contentItem());item->setPosition({30,55});item->setZ(1000);}
            break;
        }
        case 11:
            capture("scraper-information-dark-800");tap("scraper-review-0");
            if(auto* item=find(window->contentItem(),"scraper-fixture")){check(item->property("clicked").toInt()==3,"Scraper review card did not open the selected game");item->setWidth(500);item->setHeight(220);}
            shell.settings()->cycleTheme(1);break;
        case 12:
            capture("scraper-information-compact-light");
            if(auto* item=find(window->contentItem(),"scraper-fixture"))delete item;
            shell.scraper()->showOverview();break;
        case 13:
            check(shell.scraper()->overview()&&shell.scraper()->isOpen(),"Scraper overview did not open");
            press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);check(shell.scraper()->overviewFocus()==1,"Scraper controller navigation");
            press(static_cast<SDL_GameControllerButton>(input.backButton()));check(!shell.scraper()->isOpen(),"Scraper Back did not close overview");break;
        case 14: {
            check(shell.page()==1,"Controller cannot browse after closing Downloads");
            auto state=std::make_shared<DeviceSnapshot>();state->volume=0;
            state->audioStreams={QVariantMap{{"id","game:1"},{"index","1"},{"title","Adventure"},{"level",70},{"muted",false},{"available",true}},QVariantMap{{"id","music:2"},{"index","2"},{"title","Music Radio"},{"level",35},{"muted",false},{"available",true}},QVariantMap{{"id","voice:3"},{"index","3"},{"title","Voice call"},{"level",80},{"muted",false},{"available",true}}};
            state->audioInputs={QVariantMap{{"id","mic"},{"title","Internal microphone"},{"level",60},{"muted",true},{"available",true}},QVariantMap{{"id","headset"},{"title","USB headset"},{"level",85},{"muted",false},{"available",true}}};state->audioInput="mic";
            DeviceBackend backend;backend.read=[state]{return *state;};backend.write=[](auto,auto){return QString();};
            auto* service=new DeviceService(backend,window);shell.device()->configure(service,false);
            press(SDL_CONTROLLER_BUTTON_START);shell.controls()->audioOutputs();break;
        }
        case 15:
            check(shell.device()->audioOpen()&&shell.device()->mixer(),"Mixer did not open");capture("core-mixer-light-800");
            shell.dispatch(Action::NextFace);check(shell.device()->inputs(),"Controller cannot reach microphones");break;
        case 16:
            capture("core-microphones-light-800");shell.settings()->cycleTheme(1);break;
        case 17:
            capture("core-microphones-dark-800");shell.dispatch(Action::Back);check(!shell.device()->audioOpen(),"Mixer Back did not close");shell.closeControls();
            shell.collectionManager()->configure(directory,"core-smoke");shell.collectionManager()->begin();shell.collectionManager()->activate(1);shell.keyboard()->cancel();shell.collectionManager()->applyText("Weekend adventures");break;
        case 18:
            capture("core-collections-dark-800");shell.collectionManager()->activate(1);shell.collectionManager()->activate(1);shell.collectionManager()->activate(2);shell.keyboard()->cancel();shell.collectionManager()->applyText("Mario");break;
        case 19:
            capture("core-collection-condition-dark-800");shell.collectionManager()->dispatch(Action::Back);shell.collectionManager()->dispatch(Action::Back);shell.collectionManager()->close();
            shell.settings()->healthRequested();break;
        case 20:
            check(shell.diagnostics()->health()&&shell.service()=="diagnostics","System health did not open");capture("core-health-dark-800");shell.settings()->cycleTheme(1);window->resize(960,540);break;
        case 21:
            shell.diagnostics()->healthAction(3);capture("core-health-light-960");shell.dispatch(Action::Back);check(shell.service()=="settings","Health Back did not return to Settings");break;
        default:
            check(shell.page()==1,"Controller cannot browse after closing Downloads");
            check(warnings==0,"QML warnings");completed=true;timer->stop();
            if(!directory.isEmpty()){QFile file(directory+"/downloads-verification.txt");if(file.open(QIODevice::WriteOnly))file.write(((*failed?QString("FAILED\n"):QString("PASSED\n"))+diagnostics.join('\n')).toUtf8());}
            QCoreApplication::exit(*failed?1:0);break;
        }
    });
    timer->start();
}
