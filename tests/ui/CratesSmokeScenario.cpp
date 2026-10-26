#include "CratesSmokeScenario.h"
#include "../fixtures/crates/CatalogueFixture.h"
#include <QTimer>
#include <QImage>
#include <QQuickItem>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QSettings>
#include <QSysInfo>
#include <QDir>
#include <memory>
#include <functional>
using namespace trainer;
void startCratesSmoke(QQuickWindow* window,ShellController& shell,ControllerInput& input,SDL_Joystick* joystick,
        const QString& directory,bool& completed,int& warnings,QStringList& diagnostics) {
    auto step=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);
    auto folder=std::make_shared<QTemporaryDir>();
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,folder->path());
    auto timer=new QTimer(window);timer->setInterval(450);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&input,&completed,&warnings,&diagnostics] {
        auto* c=shell.crates();
        const auto check=[&](bool value,QString why){if(!value){*failed=true;diagnostics<<why;}};
        const auto press=[&](SDL_GameControllerButton b){SDL_JoystickSetVirtualButton(joystick,b,1);input.poll();SDL_JoystickSetVirtualButton(joystick,b,0);input.poll();};
        const auto shot=[&](QString name){if(!directory.isEmpty())check(window->grabWindow().save(directory+'/'+name+".png"),"Crates capture failed");};
        std::function<QQuickItem*(QQuickItem*,const QString&)> visibleItem = [&](QQuickItem* item,const QString& name)->QQuickItem* {
            if(!item||!item->isVisible())return nullptr;
            if(item->objectName()==name)return item;
            for(auto* child:item->childItems())if(auto* found=visibleItem(child,name))return found;
            return nullptr;
        };
        switch((*step)++) {
        case 0:check(c->loadCatalogue(cratesCatalogueFixture()),"Catalogue fixture rejected");shell.goToSection("apps");shell.selectFace(1);c->open(1,true);break;
        case 1:shot("catalogue");check(c->projects().size()==6,"Six cards missing");press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);break;
        case 2:check(c->projectIndex()==1,"SDL selection failed");press(SDL_CONTROLLER_BUTTON_B);break;
        case 3:check(c->detailOpen(),"SDL confirm failed");shot("project");press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);break;
        case 4:check(c->screenshotIndex()==1,"Screenshot navigation failed");press(SDL_CONTROLLER_BUTTON_BACK);break;
        case 5:check(!c->actionsOpen()&&c->actions().isEmpty(),"Uninstalled preview exposed obsolete actions");press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);break;
        case 6:check(c->detailOpen(),"Scrolling closed project");shot("project-scrolled");break;
        case 7:press(SDL_CONTROLLER_BUTTON_A);break;
        case 8:check(!c->detailOpen()&&c->projectIndex()==1,"Popup lost focus");c->turnPage(1);break;
        case 9:check(c->page()==1&&c->projectCount()==8,"Second page failed");shot("last-page");c->openProject(7);break;
        case 10:shot("missing-art");shell.toggleQuickApps();break;
        case 11:check(!c->detailOpen()&&shell.quickAppsOpen(),"Quick Apps did not dismiss details");press(SDL_CONTROLLER_BUTTON_A);break;
        case 12:check(c->projectIndex()==7&&c->page()==1,"Global drawer lost catalogue focus");shell.selectFace(0);shell.selectFace(1);break;
        case 13:check(c->opened()&&c->projectIndex()==7,"Face switch lost route");c->turnPage(-1);window->resize(1280,800);break;
        case 14:shot("catalogue-16x10");c->openProject(6);break;
        case 15:shot("long-title-16x10");c->closeDetail();c->back();c->open(3,true);break;
        case 16:check(c->projectCount()==0,"Empty crate not empty");shot("empty");break;
        case 17: {
            QFile catalogue(":/crates/catalogue.json");
            check(catalogue.open(QIODevice::ReadOnly)&&c->loadCatalogue(catalogue.readAll()),"Bundled catalogue rejected");
            c->back();c->open(0,true);window->resize(960,540);break;
        }
        case 18:check(c->projectCount()>0,"Bundled examples missing");shot("real-catalogue");c->openProject(1);break;
        case 19:shot("real-project");check(c->project()["releases"].toList().isEmpty(),"Examples must not advertise Crates packages");break;
        case 20: {
            c->closeDetail();c->back();check(c->loadCatalogue(cratesCatalogueFixture()),"Update preview catalogue rejected");
            QDir root(folder->path());check(root.mkpath("active"),"Preview receipt directory");
            QFile receipt(root.filePath("active/preview-0.json"));check(receipt.open(QIODevice::WriteOnly),"Preview receipt writable");
            receipt.write("{\"id\":\"preview-0\",\"version\":\"1\",\"kind\":\"game\"}");receipt.close();
            auto arch=QSysInfo::currentCpuArchitecture();if(arch=="arm64")arch="aarch64";
            c->installer()->replacementGuard=[]{return QString("Waiting for the running game to close");};
            c->installer()->configure("/not-executed",{},folder->path());
            c->installer()->useCatalogue({QVariantMap{{"id","preview-0"},{"title","Update preview"},{"version","2"},{"architecture",arch},{"compatibility","supported"}}},{});
            c->open(1,true);c->openProject(0);break;
        }
        case 21:check(c->primaryLabel()=="Update","Installed update did not replace Play");shot("update-primary");press(SDL_CONTROLLER_BUTTON_B);break;
        case 22:check(c->primaryLabel()=="View progress","Controller did not queue update");shot("update-queued");
            c->installer()->command({},"cancel-all");shell.dispatch(Action::Back);c->closeDetail();shell.systemAction("settings");shell.settings()->selectPage("software");break;
        case 23:shot("automatic-updates-off");shell.settings()->dispatch(Action::Down);shell.settings()->dispatch(Action::Down);shell.settings()->dispatch(Action::Right);break;
        case 24: {
            check(c->automaticUpdates(),"Settings did not enable automatic updates");
            auto* toggle=visibleItem(window->contentItem(),"settings-control-2");
            check(toggle&&toggle->property("checked").toBool(),"Automatic updates switch did not show On");
            shot("automatic-updates-on");shell.settings()->dispatch(Action::Left);break;
        }
        case 25: {
            check(!c->automaticUpdates(),"Settings did not disable automatic updates");
            auto* toggle=visibleItem(window->contentItem(),"settings-control-2");
            check(toggle&&!toggle->property("checked").toBool(),"Automatic updates switch did not show Off");break;
        }
        default:
            timer->stop();completed=true;check(warnings==0,"QML warnings in Crates");
            if(!directory.isEmpty()){QFile report(directory+"/result.txt");if(report.open(QIODevice::WriteOnly))
                report.write((QString("CRATES_SMOKE ")+(*failed?"FAILED":"PASSED")+"\n"+diagnostics.join('\n')+"\n").toUtf8());}
            for(const auto& message:diagnostics)qWarning().noquote()<<message;
            qInfo()<<"CRATES_SMOKE"<<(*failed?"FAILED":"PASSED");QCoreApplication::exit(*failed?4:0);break;
        }
    });timer->start();
}
