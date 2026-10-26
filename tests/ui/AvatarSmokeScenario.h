#pragma once
#include "core/navigation/ShellController.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <QMouseEvent>
#include <QImage>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <memory>

inline void startAvatarSmoke(QQuickWindow* window,trainer::ShellController& shell,const QString& directory,
    bool& completed,int& warnings,QStringList& diagnostics) {
    auto stage=std::make_shared<int>(0),ticks=std::make_shared<int>(0);
    auto original=std::make_shared<QJsonObject>();auto timer=new QTimer(window);timer->setInterval(500);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&completed,&warnings,&diagnostics]{
        auto* controller=shell.trainer();
        auto check=[&](bool ok,QString why){if(!ok)diagnostics.append(why);};
        if(++*ticks>100){diagnostics<<"Avatar editor timed out";*stage=99;}
        switch(*stage){
        case 0:
            if(controller->avatarBusy()||controller->avatarDescriptor().isEmpty())break;
            shell.openProfile();++*stage;break;
        case 1:
            if(controller->avatarBusy())break;
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/profile-a-live.png"),"Profile screenshot failed");
            *original=controller->avatarDescriptor();shell.dispatch(trainer::Action::Down);shell.dispatch(trainer::Action::Confirm);
            check(controller->avatarEditing(),"Appearance did not open from controller");++*stage;break;
        case 2:
            if(controller->avatarBusy())break;
            shell.dispatch(trainer::Action::Right);++*stage;break;
        case 3: {
            if(controller->avatarBusy())break;
            const auto previews=window->findChildren<QObject*>("avatar-preview");
            if(!previews.isEmpty()&&!previews.first()->property("loadError").toString().isEmpty()){
                diagnostics<<"Native GLB import: "+previews.first()->property("loadError").toString();*stage=99;break;
            }
            if(!previews.isEmpty()&&!previews.first()->property("ready").toBool())break;
            check(!previews.isEmpty()&&previews.first()->property("ready").toBool(),"Native GLB preview failed to load");
            check(controller->avatarDescriptor()==*original,"Draft leaked into live avatar");
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/avatar-editor.png"),"Editor screenshot failed");
            const std::function<QQuickItem*(QQuickItem*)> find=[&](QQuickItem* item)->QQuickItem*{
                if(item->objectName()=="avatar-choice-rogue-feminine"&&item->isVisible())return item;
                for(auto* c:item->childItems())if(auto* r=find(c))return r;return nullptr;};
            auto* row=find(window->contentItem());check(row,"Eye touch row missing");
            if(row){auto point=row->mapToScene({row->width()/2,row->height()/2});auto global=QPointF(window->mapToGlobal(point.toPoint()));
                QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);}
            ++*stage;break;
        }
        case 4:
            if(controller->avatarBusy())break;
            check(controller->avatarRows()[1].toMap()["detail"]=="rogue-feminine","New eye touch did not change appearance");
            for(int i=0;i<10;++i)shell.dispatch(trainer::Action::Right);
            *stage=40;break;
        case 40:
            if(controller->avatarBusy())break;
            check(controller->avatarRows()[1].toMap()["detail"]=="determined-feminine","Controller did not reach the last eye variant");
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/avatar-new-eyes.png"),"New eyes screenshot failed");
            for(int i=0;i<5;++i)shell.dispatch(trainer::Action::Down);
            *stage=5;break;
        case 5:
        {
            const std::function<QQuickItem*(QQuickItem*,const QString&)> find=[&](QQuickItem* item,const QString& name)->QQuickItem*{
                if(item->objectName()==name&&item->isVisible())return item;
                for(auto* c:item->childItems())if(auto* r=find(c,name))return r;return nullptr;};
            auto* colour=find(window->contentItem(),"avatar-field-6");
            auto* save=find(window->contentItem(),"avatar-field-7");
            check(controller->focusIndex()==6,"Controller did not reach shorts palette");
            check(colour&&save&&colour->mapToScene({0,colour->height()}).y()<=save->mapToScene({0,0}).y(),"Palette overlaps footer in compact editor");
            shell.dispatch(trainer::Action::Down);shell.dispatch(trainer::Action::Confirm);
            ++*stage;break;
        }
        case 6:
            if(controller->saving()||controller->avatarBusy())break;
            check(controller->avatarDescriptor()["appearance"].toObject()["eyes"]=="determined-feminine","Saved appearance not live");
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/avatar-profile.png"),"Profile screenshot failed");
            controller->beginAvatarEdit();shell.dispatch(trainer::Action::Back);check(!controller->avatarEditing(),"Back did not cancel");++*stage;break;
        case 7:
            shell.activate(3,"profile");check(shell.profileRoute()=="ra","Profile RA did not open");++*stage;break;
        case 8:
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/profile-ra.png"),"RA screenshot failed");
            shell.dispatch(trainer::Action::Back);check(shell.profileRoute()=="summary","RA Back lost profile");
            window->resize(800,450);shell.settings()->cycleTheme();++*stage;break;
        case 9:
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/profile-a-compact-dark.png"),"Compact profile screenshot failed");
            shell.activate(2,"profile");++*stage;break;
        case 10:
            check(shell.profileRoute()=="story","Story did not open in profile");
            shell.dispatch(trainer::Action::Back);shell.dispatch(trainer::Action::Back);
            check(!shell.serviceOpen(),"Profile dismissal did not restore underlying screen");
            ++*stage;break;
        case 11: {
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/header-progress-compact.png"),"Header screenshot failed");
            const std::function<QQuickItem*(QQuickItem*)> find=[&](QQuickItem* item)->QQuickItem*{
                if(item->objectName()=="account-profile-toggle"&&item->isVisible())return item;
                for(auto* child:item->childItems())if(auto* result=find(child))return result;return nullptr;};
            auto* header=find(window->contentItem());check(header,"Header profile touch target missing");
            if(header) {
                auto point=header->mapToScene({header->width()/2,header->height()/2});auto global=QPointF(window->mapToGlobal(point.toPoint()));
                QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);
            }
            check(shell.service()=="profile","Header touch did not open the player profile");++*stage;break;
        }
        default:
            check(warnings==0,"QML warnings during avatar preview");
            if(!directory.isEmpty()){
                QFile report(directory+"/avatar-report.json");
                if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(QJsonObject{{"warnings",warnings},{"diagnostics",QJsonArray::fromStringList(diagnostics)},{"success",diagnostics.isEmpty()}}).toJson());
            }
            for(const auto& d:diagnostics)qWarning().noquote()<<d;
            completed=true;timer->stop();QCoreApplication::exit(diagnostics.isEmpty()?0:1);
        }
    });timer->start();
}
