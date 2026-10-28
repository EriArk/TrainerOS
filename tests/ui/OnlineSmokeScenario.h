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
        case 14: {
            auto* account=shell.backendAccount();
            const QString player="11111111-1111-4111-8111-111111111111";
            const auto date=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            auto store=std::make_shared<QJsonObject>(QJsonObject{{"player_id",player},{"revision","31"},{"items",QJsonArray{}},{"equipped",QJsonValue()}});
            QJsonArray offers;
            for(const auto name:{"aurora","dusk","orbit"})offers.append(QJsonObject{{"id","profile."+QString(name)},{"appearance",name},{"title",QString(name).left(1).toUpper()+QString(name).mid(1)},
                {"description","A permanent illustrated world for your character."},{"price",250},{"minimum_level",1},{"revision","1"},{"kind","permanent_license"},{"available",true}});
            (*store)["catalogue"]=offers;auto balance=std::make_shared<int>(1000);
            account->setServices({[player,date,store,balance](QByteArray,QString path,QJsonObject body,QByteArray,trainer::BackendAccount::Reply done){
                if(path=="/v1/auth/refresh")done(200,{{"player_id",player},{"session_id","33333333-3333-4333-8333-333333333333"},{"fluxer_subject","12345"},
                    {"access_token",QString(43,'a')},{"refresh_token",QString(43,'r')},{"expires_at",QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODateWithMs)}});
                else if(path=="/v1/me/progression")done(200,{{"player_id",player},{"balance",*balance},{"revision",(*store)["revision"]},{"checked_at",date},
                    {"mastery",QJsonObject{{"xp",2000},{"level",5},{"level_xp",200},{"next_level_xp",700}}},
                    {"challenge",QJsonObject{{"challenge","TrainerOS-example-link-code"},{"username","Example"}}},
                    {"history",QJsonArray{QJsonObject{{"id","44444444-4444-4444-8444-444444444444"},{"source","ra.completion"},{"title","Example completed set"},{"xp",1100},{"sparks",550},{"earned_at",date}}}}});
                else if(path=="/v1/me/inventory")done(200,*store);
                else if(path=="/v1/me/purchases") {
                    for(const auto row:(*store)["catalogue"].toArray())if(row.toObject()["id"]==body["offer_id"]){auto owned=row.toObject();owned["purchase_id"]="55555555-5555-4555-8555-555555555555";(*store)["items"]=QJsonArray{owned};}
                    auto receipt=body;receipt["id"]="55555555-5555-4555-8555-555555555555";receipt["player_id"]=player;receipt["price"]=receipt.take("expected_price");
                    (*store)["receipt"]=receipt;(*store)["revision"]="32";*balance-=250;done(200,*store);
                } else if(path=="/v1/me/inventory/appearance") {(*store)["equipped"]=body["offer_id"];(*store)["revision"]=QString::number((*store)["revision"].toString().toInt()+1);done(200,*store);}
                else done(503,{});
            },[](QString,auto done){done(true,QJsonDocument(QJsonObject{{"refresh_token",QString(43,'r')}}).toJson());},[](QString,QByteArray,auto done){done(true);},[](QUrl){return false;},[](QString){return QByteArray();},[](QString,QByteArray){return true;}});
            account->configure("ui-progression-fixture");shell.openProfile();window->resize(1280,720);break;
        }
        case 15:
            check(shell.profileAccount()["levelText"]=="5" && shell.profileAccount()["sparksText"]=="1000","Profile projects verified server data");
            capture("progression-profile-wide");shell.activate(4,"profile");break;
        case 16:
            capture("progression-account-wide");shell.dispatch(Action::Down);shell.dispatch(Action::Down);shell.dispatch(Action::Down);
            check(shell.profileFocus()==3,"Reward controller actions");window->resize(960,540);break;
        case 17:
            capture("progression-account-compact");shell.dispatch(Action::Back);break;
        case 18:
            capture("progression-profile-compact");shell.dispatch(Action::Back);break;
        case 19:
            shell.openProfile();shell.activate(9,"profile");shell.backendAccount()->selectShopItem(1);window->resize(1280,720);break;
        case 20:
            check(shell.profileRoute()=="shop","Appearance route");
            shell.dispatch(Action::Left);shell.dispatch(Action::Confirm);
            check(shell.backendAccount()->shop()["review"].toMap().isEmpty(),"Controller Refresh does not buy");
            shell.dispatch(Action::Right);capture("shop-aurora-wide");shell.dispatch(Action::Confirm);break;
        case 21:
            check(!shell.backendAccount()->shop()["review"].toMap().isEmpty(),"Explicit review");capture("shop-confirm-wide");shell.dispatch(Action::Back);shell.backendAccount()->selectShopItem(2);break;
        case 22:
            capture("shop-dusk-wide");window->resize(960,540);shell.backendAccount()->selectShopItem(3);break;
        case 23:
            capture("shop-orbit-compact");shell.dispatch(Action::Confirm);break;
        case 24:
            capture("shop-confirm-compact");shell.dispatch(Action::Right);shell.dispatch(Action::Confirm);break;
        case 25:
            check(shell.profileAccount()["sparksText"]=="750","Purchase updates real projection");
            check(shell.backendAccount()->shop()["selected"].toMap()["owned"].toBool(),"Purchase delivers ownership");
            shell.dispatch(Action::Confirm);shell.dispatch(Action::Back);break;
        case 26:
            check(shell.backendAccount()->profileAppearance()=="orbit","Purchased appearance applied");capture("shop-applied-compact");
            shell.activate(9,"profile");shell.backendAccount()->selectShopItem(0);shell.dispatch(Action::Confirm);shell.dispatch(Action::Back);shell.dispatch(Action::Back);break;
        default:
            check(warnings==0,"QML warnings during Online navigation");
            if(!directory.isEmpty()){QFile f(directory+"/online-report.json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(QJsonObject{{"warnings",warnings},{"success",diagnostics.isEmpty()},{"diagnostics",QJsonArray::fromStringList(diagnostics)}}).toJson());}
            completed=true;timer->stop();QCoreApplication::exit(diagnostics.isEmpty()?0:1);
        }
    });timer->start();
}
