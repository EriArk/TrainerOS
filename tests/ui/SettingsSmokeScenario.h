#pragma once
#include "core/navigation/ShellController.h"
#include "core/input/ControllerInput.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QtTest/QTest>
#include <QTimer>
#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QSettings>
#include <QDir>
#include <memory>
#include <functional>
class SettingsInputLibrary final : public trainer::LibraryRepository {
public:
    QList<trainer::World> worlds() const override{return {};}
    QList<trainer::Adventure> adventures() const override {
        trainer::Adventure a;a.id="pc:input-preview";a.title="My PC game";a.platformId="pc";a.adapterId="pc-local";return {a};
    }
    QList<trainer::ResumePoint> resumePoints() const override{return {};}
    trainer::HomeSnapshot home() const override{return {};}
};
inline void startSettingsSmoke(QQuickWindow* window, trainer::ShellController& shell,
    trainer::ControllerInput& input,const QString& directory,bool& completed,int& warnings,QStringList& diagnostics) {
    auto folder=std::make_shared<QTemporaryDir>();auto library=std::make_shared<SettingsInputLibrary>();
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,folder->path());
    auto step=std::make_shared<int>(0);auto timer=new QTimer(window);timer->setInterval(450);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&input,&completed,&warnings,&diagnostics]{
        auto* settings=shell.settings();auto* flow=settings->gameInput();
        const auto capture=[&](QString name){if(!directory.isEmpty()&&!window->grabWindow().save(QDir(directory).filePath(name+".png")))diagnostics<<"Settings capture failed: "+name;};
        const auto check=[&](bool ok,QString why){if(!ok)diagnostics<<why;};
        std::function<QQuickItem*(QQuickItem*,const QString&)> visibleItem = [&](QQuickItem* item,const QString& name)->QQuickItem* {
            if(!item || !item->isVisible())return nullptr;
            if(item->objectName()==name)return item;
            for(auto* child:item->childItems())if(auto* found=visibleItem(child,name))return found;
            return nullptr;
        };
        const auto diagram=[&](const QString& name){
            auto* item=window->findChild<QQuickItem*>(name);
            check(item&&item->isVisible()&&item->width()>=150&&item->height()>=140,"Usable diagram area: "+name);
            if(item)check(item->parentItem()->boundingRect().contains(item->mapRectToItem(item->parentItem(),item->boundingRect())),"Diagram stays in its pane: "+name);
        };
        const auto tap=[&](QQuickItem* item){
            check(item&&item->isVisible(),"Touch target is visible");
            if(item)QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,item->mapToScene(item->boundingRect().center()).toPoint());
        };
        switch((*step)++) {
        case 0: window->resize(960,540);shell.systemAction("settings");break;
        case 1: check(shell.service()=="settings","Settings opens from system actions");capture("settings-appearance");
            settings->selectPage("controller"); {auto p=input.profile();p["family"]="playstation";input.setProfile(p);} break;
        case 2: {
            capture("settings-controller-960");diagram("settings-controller-diagram");check(input.glyph("A")==QString::fromUtf8("×"),"Controller glyphs follow profile");
            // Touch the visible stepper, not its C++ action. It must not also
            // activate the underlying row and accidentally advance two families.
            auto* row=visibleItem(window->contentItem(),"settings-control-1");
            tap(visibleItem(row,"settings-step-next"));
            check(input.family()=="steam","One stepper tap changes one family");
            auto p=input.profile();p["family"]="playstation";input.setProfile(p);
            shell.dispatch(trainer::Action::NextFace);check(settings->categoryId()=="game-input","Shell routes triggers to Settings pages");
            flow->configure(library.get(),folder->path(),"preview-owner");break;
        }
        case 3: capture("settings-game-picker");flow->dispatch(trainer::Action::Confirm);flow->activate(0);break;
        case 4: {
            auto* grid=window->findChild<QQuickItem*>("settings-input-grid");
            auto* detail=visibleItem(window->contentItem(),"settings-input-detail-0");
            check(grid&&grid->property("columns").toInt()==2,"Setup has two compact columns");
            check(detail&&detail->isVisible()&&detail->property("text").toString()=="Keyboard & mouse","Current input mode is visible");
            capture("settings-game-setup");shell.dispatch(trainer::Action::NextFace);check(flow->page()==1,"Shell routes triggers to editor tabs");flow->selectBinding("south");break;
        }
        case 5: {
            auto* grid=window->findChild<QQuickItem*>("settings-input-grid");
            check(grid&&grid->property("columns").toInt()==4,"Action palette uses a grid");
            capture("settings-assignment");flow->dispatch(trainer::Action::Back);break;
        }
        case 6: capture("settings-buttons");flow->setPage(2);flow->activate(flow->rows().size()-5);break;
        case 7: capture("settings-ring-960");diagram("settings-ring-diagram");flow->activate(23);flow->setTargetGroup(2);flow->dispatch(trainer::Action::Confirm);
            check(flow->draft()["rings"].toList()[0].toList()[0].toString()=="A","Ring edited through action palette");
            window->resize(1280,720);break;
        case 8: capture("settings-ring-1280");flow->activate(flow->rows().size()-2);check(flow->status().startsWith("Saved"),"Game profile saved");window->resize(960,540);settings->selectPage("audio");break;
        case 9: capture("settings-audio");settings->selectPage("communication");break;
        case 10: capture("settings-communication");settings->selectPage("clock");break;
        case 11: capture("settings-clock");settings->selectPage("system");break;
        case 12: capture("settings-system");break;
        default:timer->stop();completed=true;qInfo()<<"Settings UI checks"<<diagnostics<<"warnings"<<warnings;QCoreApplication::exit(diagnostics.isEmpty()&&warnings==0?0:1);break;
        }
    });timer->start();
}
