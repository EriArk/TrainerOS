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
        if(++*ticks>100){diagnostics<<"Avatar editor timed out";*stage=9;}
        switch(*stage){
        case 0:
            if(controller->avatarBusy()||controller->avatarDescriptor().isEmpty())break;
            shell.openProfile();++*stage;break;
        case 1:
            if(controller->avatarBusy())break;
            *original=controller->avatarDescriptor();shell.dispatch(trainer::Action::Down);shell.dispatch(trainer::Action::Confirm);
            check(controller->avatarEditing(),"Appearance did not open from controller");++*stage;break;
        case 2:
            if(controller->avatarBusy())break;
            shell.dispatch(trainer::Action::Right);++*stage;break;
        case 3: {
            if(controller->avatarBusy())break;
            const auto previews=window->findChildren<QObject*>("avatar-preview");
            if(!previews.isEmpty()&&!previews.first()->property("ready").toBool())break;
            check(!previews.isEmpty()&&previews.first()->property("ready").toBool(),"Native GLB preview failed to load");
            check(controller->avatarDescriptor()==*original,"Draft leaked into live avatar");
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/avatar-editor.png"),"Editor screenshot failed");
            const std::function<QQuickItem*(QQuickItem*)> find=[&](QQuickItem* item)->QQuickItem*{
                if(item->objectName()=="avatar-field-1"&&item->isVisible())return item;
                for(auto* c:item->childItems())if(auto* r=find(c))return r;return nullptr;};
            auto* row=find(window->contentItem());check(row,"Eye touch row missing");
            if(row){auto point=row->mapToScene({row->width()*.75,row->height()-10});auto global=QPointF(window->mapToGlobal(point.toPoint()));
                QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);}
            ++*stage;break;
        }
        case 4:
            if(controller->avatarBusy())break;
            check(controller->avatarRows()[1].toMap()["detail"]=="almond","Eye touch did not change appearance");
            for(int i=0;i<5;++i)shell.dispatch(trainer::Action::Down);
            ++*stage;break;
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
            check(controller->avatarDescriptor()["appearance"].toObject()["eyes"]=="almond","Saved appearance not live");
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/avatar-profile.png"),"Profile screenshot failed");
            controller->beginAvatarEdit();shell.dispatch(trainer::Action::Back);check(!controller->avatarEditing(),"Back did not cancel");++*stage;break;
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
