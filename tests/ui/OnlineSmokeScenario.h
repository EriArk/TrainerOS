#pragma once
#include "core/navigation/ShellController.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <QImage>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMouseEvent>
#include "../fixtures/HomeSocialFixture.h"
#include <memory>

inline void startOnlineSmoke(QQuickWindow* window,trainer::ShellController& shell,const QString& directory,
    bool& completed,int& warnings,QStringList& diagnostics) {
    using trainer::Action;
    auto step=std::make_shared<int>(0);auto timer=new QTimer(window);timer->setInterval(700);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&completed,&warnings,&diagnostics] {
        auto check=[&](bool ok,const QString& why){if(!ok)diagnostics.append(why);};
        auto capture=[&](const QString& name){if(!directory.isEmpty())check(window->grabWindow().save(directory+"/"+name+".png"),"Capture "+name);};
        switch((*step)++) {
        case 0: window->resize(1280,720);shell.goToPage(0);shell.openOnlineFace("lobby");break;
        case 1:
            check(shell.primaryId()=="online" && shell.socialActive(),"Lobby entry");
            capture("online-lobby-wide");shell.dispatch(Action::NextFace);break;
        case 2:
            check(shell.onlineFace()=="bazaar" && !shell.socialActive(),"Bazaar independent of conversation");
            capture("online-bazaar-wide");shell.dispatch(Action::NextFace);break;
        case 3:
            check(shell.onlineFace()=="crates" && !shell.socialActive(),"Crates entry");
            capture("online-crates-wide");shell.dispatch(Action::NextFace);break;
        case 4:
            check(shell.onlineFace()=="chats" && shell.socialActive(),"Chats entry");
            capture("online-chats-wide");shell.dispatch(Action::Up);shell.dispatch(Action::Right);break;
        case 5:
            check(shell.socialFace()=="communities" && shell.chatNavigationFocused(),"Controller chat sections");
            capture("online-communities-wide");shell.dispatch(Action::Down);shell.openProfile();break;
        case 6:
            check(shell.service()=="profile","Profile overlay");capture("online-profile-return");
            shell.dispatch(Action::Back);window->resize(960,540);break;
        case 7:
            check(shell.onlineFace()=="chats" && shell.socialFace()=="communities" && !shell.serviceOpen(),"Profile returns to chats");
            capture("online-chats-compact");shell.openOnlineFace("crates");break;
        case 8:
            capture("online-crates-compact");shell.goToPage(4);break;
        case 9:
            check(shell.faceNames()==QStringList({"Apps","Browser","Files"}),"Apps faces");
            capture("apps-compact");shell.openGameHubFace("spaces");break;
        case 10:
            check(shell.faceNames().size()<=2,"GameHub excludes profile and lobby");
            capture("gamehub-compact");window->resize(1280,720);shell.openOnlineFace("chats");break;
        case 11: {
            trainer::HomeSocialFixture::isolateProvider(*shell.social());
            trainer::HomeSocialFixture::publish(*shell.social(),{{"state","connected"},{"userId","self"},{"name","Player"},{"channel","dm"},
                {"chats",QVariantList{trainer::HomeSocialFixture::chat("dm","friend","A friend")}},
                {"messages",QVariantList{QVariantMap{{"id","m1"},{"author","friend"},{"name","A friend"},{"text","Ready for another round?"}}}}});
            const std::function<QQuickItem*(QQuickItem*)> find=[&](QQuickItem* item)->QQuickItem* {
                if(item->objectName()=="online-chat-chats" && item->isVisible())return item;
                for(auto* child:item->childItems())if(auto* result=find(child))return result;
                return nullptr;
            };
            auto* tab=find(window->contentItem());
            check(tab && tab->isVisible(),"Touch chat selector missing");
            if(tab) {
                auto point=tab->mapToScene({tab->width()/2,tab->height()/2});auto global=QPointF(window->mapToGlobal(point.toPoint()));
                QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);
            }
            break;
        }
        case 12:
            check(shell.socialFace()=="chats","Touch did not select Messages");
            check(shell.social()->account()["state"]=="connected","Connected fixture was replaced");
            capture("online-connected-wide");window->resize(960,540);break;
        case 13:
            capture("online-connected-compact");shell.goToPage(0);break;
        default:
            check(warnings==0,"QML warnings during Online navigation");
            if(!directory.isEmpty()){QFile f(directory+"/online-report.json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(QJsonObject{{"warnings",warnings},{"success",diagnostics.isEmpty()},{"diagnostics",QJsonArray::fromStringList(diagnostics)}}).toJson());}
            completed=true;timer->stop();QCoreApplication::exit(diagnostics.isEmpty()?0:1);
        }
    });timer->start();
}
