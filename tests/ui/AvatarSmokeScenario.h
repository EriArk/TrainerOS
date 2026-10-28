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
    auto timer=new QTimer(window);timer->setInterval(500);
    auto commerce=std::make_shared<QByteArray>();
    trainer::BackendAccount::Services services;
    services.read=[](QString,auto done){done(true,QByteArray());};
    services.readCommerce=[commerce](QString){return *commerce;};
    services.writeCommerce=[commerce](QString,QByteArray data){*commerce=data;return true;};
    services.request=[](QByteArray,QString,QJsonObject,QByteArray,auto done){done(0,{});};
    shell.backendAccount()->setServices(services);
    shell.backendAccount()->configure("wardrobe-ui-fixture");
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&completed,&warnings,&diagnostics]{
        auto* trainer=shell.trainer();auto* wardrobe=shell.outfits();
        auto check=[&](bool ok,QString why){if(!ok)diagnostics.append(why);};
        auto capture=[&](QString name){if(!directory.isEmpty())check(window->grabWindow().save(directory+"/"+name+".png"),"Screenshot failed: "+name);};
        const std::function<QQuickItem*(QQuickItem*,const QString&)> find=[&](QQuickItem* parent,const QString& name)->QQuickItem*{
            if(parent->objectName()==name&&parent->isVisible())return parent;
            for(auto* child:parent->childItems())if(auto* item=find(child,name))return item;return nullptr;};
        auto touch=[&](QString name){
            auto* item=find(window->contentItem(),name);check(item,"Missing touch target: "+name);if(!item)return;
            const auto point=item->mapToScene({item->width()/2,item->height()/2}),global=QPointF(window->mapToGlobal(point.toPoint()));
            QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);
        };
        if(++*ticks>240){diagnostics<<"Wardrobe timed out";*stage=99;}
        switch(*stage){
        case 0:
            if(trainer->avatarBusy()||trainer->avatarDescriptor().isEmpty())break;
            shell.openProfile();shell.activate(1,"profile");window->resize(1280,720);++*stage;break;
        case 1:
            if(wardrobe->view()["previewBusy"].toBool())break;
            check(shell.profileRoute()=="outfits"&&!trainer->avatarEditing(),"Wardrobe should open directly");
            touch("wardrobe-item-polo-short-sleeve");check(wardrobe->view()["chosen"].toMap()["id"]=="polo-short-sleeve","Touch focuses garment");
            touch("wardrobe-equip");++*stage;break;
        case 2:
            if(trainer->saving()||trainer->avatarBusy()||wardrobe->view()["previewBusy"].toBool())break;
            check(trainer->appearance()["top"]=="polo-short-sleeve","Equip auto-saves without Apply");
            {
                auto* rig=window->findChild<QObject*>("wardrobe-preview-rig");
                if(rig){QFile tree(directory+"/rig.txt");if(tree.open(QIODevice::WriteOnly))for(auto* node:rig->findChildren<QObject*>())tree.write((QString(node->metaObject()->className())+" "+node->objectName()+"\n").toUtf8());}
                check(rig&&wardrobe->posePreview(rig,0.5)>0,"Admiration must pose the avatar skeleton");
                if(rig)wardrobe->posePreview(rig,0);
            }
            capture("wardrobe-wide");touch("wardrobe-colour-1");++*stage;break;
        case 3:
            if(trainer->saving()||trainer->avatarBusy()||wardrobe->view()["previewBusy"].toBool())break;
            check(trainer->appearance()["shirtColor"]=="#a8c9bc","Palette updates equipped garment");
            touch("wardrobe-item-polo-sleeveless");touch("wardrobe-colour-2");++*stage;break;
        case 4:
            if(trainer->saving()||trainer->avatarBusy())break;
            check(trainer->appearance()["top"]=="polo-short-sleeve"&&trainer->appearance()["shirtColor"]=="#aabddd","Palette must not equip the focused item");
            wardrobe->selectCategory(0);wardrobe->selectItem(0);shell.dispatch(trainer::Action::Confirm);++*stage;break;
        case 5:
            if(trainer->saving()||trainer->avatarBusy()||wardrobe->view()["previewBusy"].toBool())break;
            check(trainer->appearance()["headwear"]=="golf-cap","Controller equips cap");
            wardrobe->chooseColour(8);window->resize(800,450);++*stage;break;
        case 6: {
            if(trainer->saving()||trainer->avatarBusy()||wardrobe->view()["previewBusy"].toBool())break;
            check(trainer->appearance()["headwearColor"]=="#efa58b","Headwear independent colour");
            auto* palette=find(window->contentItem(),"wardrobe-palette");auto* grid=find(window->contentItem(),"wardrobe-grid");
            check(palette&&grid&&grid->mapToScene({0,grid->height()}).y()<=palette->mapToScene({0,0}).y(),"Compact grid overlaps palette");
            check(grid&&qAbs(grid->width()/grid->property("cellWidth").toReal()-3)<.01&&qAbs(grid->height()/grid->property("cellHeight").toReal()-3)<.01,"Wardrobe must show a 3 by 3 grid");
            capture("wardrobe-compact");wardrobe->equip();++*stage;break;
        }
        case 7:
            if(trainer->saving()||trainer->avatarBusy())break;
            check(trainer->appearance()["headwear"]=="none","Unequip removes cap");
            shell.dispatch(trainer::Action::Back);check(shell.profileRoute()=="summary","Back returns to profile");
            shell.activate(1,"profile");check(wardrobe->view()["appearance"].toMap()["shirtColor"]=="#aabddd","Reopen retains autosaved colours");
            wardrobe->selectCategory(6);shell.settings()->cycleTheme();++*stage;break;
        case 8:
            if(wardrobe->view()["previewBusy"].toBool())break;
            capture("wardrobe-grid-nine");
            wardrobe->selectItem(0);shell.dispatch(trainer::Action::Down);
            check(wardrobe->view()["item"].toInt()==3,"Controller moves down by three columns");
            wardrobe->selectItem(13);shell.dispatch(trainer::Action::Confirm);++*stage;break;
        case 9:
            if(trainer->saving()||trainer->avatarBusy()||wardrobe->view()["previewBusy"].toBool())break;
            check(trainer->appearance()["eyes"]=="determined-feminine","All eye variants remain reachable");capture("wardrobe-dark-eyes");
            shell.dispatch(trainer::Action::Back);shell.dispatch(trainer::Action::Back);check(!shell.serviceOpen(),"Profile dismissal returns to shell");++*stage;break;
        default:
            check(warnings==0,"QML warnings during wardrobe");
            if(!directory.isEmpty()){QFile report(directory+"/avatar-report.json");if(report.open(QIODevice::WriteOnly))report.write(QJsonDocument(QJsonObject{{"warnings",warnings},{"diagnostics",QJsonArray::fromStringList(diagnostics)},{"success",diagnostics.isEmpty()}}).toJson());}
            for(const auto& d:diagnostics)qWarning().noquote()<<d;completed=true;timer->stop();QCoreApplication::exit(diagnostics.isEmpty()?0:1);
        }
    });timer->start();
}
