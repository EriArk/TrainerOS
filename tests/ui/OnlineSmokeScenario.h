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
#include <QUuid>
#include "../fixtures/HomeSocialFixture.h"
#include <memory>

inline void startOnlineSmoke(QQuickWindow* window,trainer::ShellController& shell,const QString& directory,
    bool& completed,int& warnings,QStringList& diagnostics) {
    using trainer::Action;
    auto step=std::make_shared<int>(0);auto timer=new QTimer(window);timer->setInterval(700);
    auto grantSupport=std::make_shared<std::function<void()>>();
    auto pauseInventory=std::make_shared<bool>(false);
    auto heldInventory=std::make_shared<trainer::BackendAccount::Reply>();
    auto replayPrizes=std::make_shared<std::function<void()>>();
    auto prizeWait=std::make_shared<int>(0);
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
            auto supported=std::make_shared<bool>(false);
            *grantSupport=[store,balance,supported]{*supported=true;*balance+=2000;(*store)["revision"]="40";};
            account->setServices({[player,date,store,balance,supported](QByteArray,QString path,QJsonObject body,QByteArray,trainer::BackendAccount::Reply done){
                if(path=="/v1/auth/refresh")done(200,{{"player_id",player},{"session_id","33333333-3333-4333-8333-333333333333"},{"fluxer_subject","12345"},
                    {"access_token",QString(43,'a')},{"refresh_token",QString(43,'r')},{"expires_at",QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODateWithMs)}});
                else if(path=="/v1/me/support")done(200,{{"provider","patreon"},{"available",true},{"state",*supported?"connected":"unlinked"},{"multiplier",*supported?3:1}});
                else if(path=="/v1/me/progression")done(200,{{"player_id",player},{"balance",*balance},{"revision",(*store)["revision"]},{"checked_at",date},
                    {"support",QJsonObject{{"multiplier",*supported?3:1},{"verified_until",QDateTime::currentDateTimeUtc().addDays(1).toString(Qt::ISODateWithMs)}}},
                    {"support_preview",QJsonObject{{"policy","support-preview-2026-10-v1"},{"state","calculation_only"},{"activation_available",false},
                        {"eligible_rewards",31},{"base_sparks",1000},{"credited_reward_sparks",*supported?3000:1000},{"options",QJsonArray{
                            QJsonObject{{"id","support-x2"},{"title","Support x2"},{"multiplier",2},{"top_up",*supported?0:1000},{"potential_balance",*balance+(*supported?0:1000)}},
                            QJsonObject{{"id","support-x3"},{"title","Support x3"},{"multiplier",3},{"top_up",*supported?0:2000},{"potential_balance",*balance+(*supported?0:2000)}}}}}},
                    {"mastery",QJsonObject{{"xp",2000},{"level",5},{"level_xp",200},{"next_level_xp",700}}},
                    {"challenge",QJsonObject{{"challenge","TrainerOS-example-link-code"},{"username","Example"}}},
                    {"history",QJsonArray{QJsonObject{{"id","44444444-4444-4444-8444-444444444444"},{"source",*supported?"support.topup":"ra.completion"},
                        {"title","Example completed set"},{"xp",*supported?0:1100},{"sparks",*supported?1100:550},{"support_multiplier",3},{"earned_at",date}}}}});
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
        case 27:
            shell.openProfile();shell.activate(10,"profile");window->resize(1280,720);break;
        case 28:
            check(shell.profileRoute()=="support" && shell.profileAccount()["previewKnown"].toBool(),"Server support preview route");
            check(shell.backendAccount()->progression()["support_preview"].toMap()["options"].toList()[1].toMap()["potential_balance"].toInt()==2750,"Support preview includes spent rewards");
            capture("support-wide");shell.backendAccount()->refreshSupport();window->resize(960,540);break;
        case 29:
            check(shell.profileAccount()["signedIn"].toBool() && shell.profileAccount()["previewKnown"].toBool(),"Support Refresh preserves login and calculation");
            capture("support-compact");shell.dispatch(Action::Back);
            check(shell.profileRoute()=="summary" && shell.profileFocus()==10,"Support returns to its entry");shell.dispatch(Action::Back);break;
        case 30:
            (*grantSupport)();shell.backendAccount()->refreshProgression(false);shell.openProfile();shell.activate(10,"profile");window->resize(1280,720);break;
        case 31:
            check(shell.profileAccount()["sparksText"]=="2750" && shell.profileAccount()["xp"].toInt()==2000,"Support credit does not multiply XP");
            check(shell.backendAccount()->progression()["support_preview"].toMap()["options"].toList()[1].toMap()["top_up"].toInt()==0,"Granted support is removed from remaining top-up");
            capture("support-granted-wide");shell.dispatch(Action::Back);shell.activate(4,"profile");window->resize(960,540);break;
        case 32:
            capture("support-history-compact");shell.dispatch(Action::Back);shell.dispatch(Action::Back);break;
        case 33: break;
        case 34: {
            auto* account=shell.backendAccount();
            account->setServices({[](QByteArray,QString,QJsonObject,QByteArray,trainer::BackendAccount::Reply done){done(503,{});},
                [](QString,auto done){done(true,{});},[](QString,QByteArray,auto done){done(true);},[](QUrl){return false;},
                [](QString){return QByteArray();},[](QString,QByteArray){return true;}});
            account->configure("ui-support-signed-out");shell.openProfile();break;
        }
        case 35: {
            const std::function<QQuickItem*(QQuickItem*)> find=[&](QQuickItem* item)->QQuickItem* {
                if(item->objectName()=="profile-support" && item->isVisible())return item;
                for(auto* child:item->childItems())if(auto* result=find(child))return result;
                return nullptr;
            };
            auto* entry=find(window->contentItem());check(entry,"Support touch entry missing");
            if(entry) {
                auto point=entry->mapToScene({entry->width()/2,entry->height()/2});auto global=QPointF(window->mapToGlobal(point.toPoint()));
                QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);
            }
            break;
        }
        case 36:
            check(shell.profileRoute()=="support" && !shell.profileAccount()["previewKnown"].toBool(),"Signed-out touch entry has no fake balance");
            capture("support-unavailable-compact");shell.dispatch(Action::Back);shell.dispatch(Action::Confirm);
            check(shell.profileRoute()=="support","Controller reopens focused support entry");
            shell.dispatch(Action::Back);shell.dispatch(Action::Back);break;
        case 37: {
            if(shell.trainer()->avatarDescriptor().isEmpty()){--*step;break;}
            const QString player="11111111-1111-4111-8111-111111111111";
            auto store=std::make_shared<QJsonObject>(QJsonObject{{"player_id",player},{"revision","1"},{"items",QJsonArray{}},{"catalogue",QJsonArray{}},{"equipped",QJsonValue()}});
            QJsonArray offers,items,looks;
            for(int i=1;i<=3;++i){
                QJsonObject slot{{"id","outfit.slot."+QString::number(i)},{"slot_number",i},{"purpose","outfit_slot"},{"title","Look slot "+QString::number(i)},
                    {"revision","1"},{"price",100},{"kind","permanent_license"}};
                offers.append(slot);if(i==3)continue;
                slot["purchase_id"]=QUuid::createUuid().toString(QUuid::WithoutBraces);items.append(slot);
                auto recipe=shell.trainer()->avatarDescriptor()["recipe"].toObject();auto a=recipe["appearance"].toObject();
                a["hair"]=i==1?"bob":"crop";a["shirtColor"]=i==1?"#aabddd":"#a8c9bc";a["shortsColor"]=i==1?"#536274":"#b98e7c";recipe["appearance"]=a;
                looks.append(QJsonObject{{"player_id",player},{"offer_id",slot["id"]},{"name",i==1?"Evening arcade":"Weekend games"},{"recipe",recipe},{"revision","1"}});
            }
            (*store)["outfit_catalogue"]=offers;(*store)["outfit_items"]=items;(*store)["outfits"]=looks;
            shell.backendAccount()->setServices({[store,player,pauseInventory,heldInventory](QByteArray,QString path,QJsonObject body,QByteArray,trainer::BackendAccount::Reply done){
                if(path=="/v1/auth/refresh")done(200,{{"player_id",player},{"session_id","33333333-3333-4333-8333-333333333333"},{"fluxer_subject","12345"},
                    {"access_token",QString(43,'a')},{"refresh_token",QString(43,'r')},{"expires_at",QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODateWithMs)}});
                else if(path=="/v1/me/inventory"){if(*pauseInventory)*heldInventory=done;else done(200,*store);}
                else if(path.startsWith("/v1/me/inventory/outfits/")){
                    auto values=(*store)["outfits"].toArray();for(int i=0;i<values.size();++i){auto row=values[i].toObject();if(row["offer_id"]==path.section('/',-1)){row["name"]=body["name"];row["recipe"]=body["recipe"];row["revision"]="2";values[i]=row;}}
                    (*store)["outfits"]=values;(*store)["revision"]="2";done(200,*store);
                }else done(503,{});
            },[](QString,auto done){done(true,QJsonDocument(QJsonObject{{"refresh_token",QString(43,'r')}}).toJson());},[](QString,QByteArray,auto done){done(true);},[](QUrl){return false;},[](QString){return QByteArray();},[](QString,QByteArray){return true;}});
            shell.backendAccount()->configure("ui-outfits-fixture");shell.openProfile();shell.activate(1,"profile");shell.outfits()->select(1);window->resize(1280,720);break;
        }
        case 38:
            if(shell.outfits()->view()["previewBusy"].toBool()){--*step;break;}
            check(shell.profileRoute()=="outfits","Wardrobe opens saved looks");
            check(shell.outfits()->view()["rows"].toList().size()==4,"Owned slots plus one purchase entry");capture("outfits-wide");
            *pauseInventory=true;shell.backendAccount()->refreshShop();
            check(shell.backendAccount()->outfits()["busy"].toBool()&&!shell.outfits()->view()["busy"].toBool(),"Slow inventory refresh does not block cached looks");
            shell.outfits()->select(1);break;
        case 39:
            if(shell.trainer()->saving()||shell.trainer()->avatarBusy()){--*step;break;}
            check(shell.trainer()->avatarDescriptor()["appearance"].toObject()["hair"]=="bob","Wear applies complete saved look");
            check(shell.outfits()->view()["wearing"].toBool(),"Saved look is marked as worn");
            if(*heldInventory){(*heldInventory)(503,{});*heldInventory={};}*pauseInventory=false;shell.backendAccount()->refreshShop();
            shell.outfits()->activate(2);shell.keyboard()->cancel();shell.outfits()->acceptName("Arcade night");window->resize(960,540);break;
        case 40:
            check(shell.outfits()->view()["selected"].toMap()["name"]=="Arcade night","Slot rename reaches inventory");capture("outfits-compact");
            shell.outfits()->selectCategory(1);shell.outfits()->selectItem(1);shell.outfits()->equip();break;
        case 41:
            if(shell.trainer()->saving()||shell.trainer()->avatarBusy()){--*step;break;}
            capture("outfits-autosaved");check(shell.trainer()->appearance()["top"]=="polo-short-sleeve","Set edits save without confirmation");
            shell.outfits()->select(3);break;
        case 42:
            capture("outfits-purchase");check(!shell.backendAccount()->shop()["review"].toMap().isEmpty(),"Slot has explicit purchase review");
            shell.dispatch(Action::Back);shell.dispatch(Action::Back);check(shell.profileRoute()=="summary"&&shell.profileFocus()==1,"Wardrobe returns to focused entry");shell.dispatch(Action::Back);break;
        case 43: {
            const QString player="11111111-1111-4111-8111-111111111111";
            const auto now=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            QJsonArray rows;
            int sequence=0;
            for(const auto kind:{"beaten","outfit_slot","clothing"}) {
                ++sequence;rows.append(QJsonObject{{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"player_id",player},
                    {"kind",kind},{"title",sequence==1?"A completed adventure":sequence==2?"Level 3 look slot":"Level 3 wardrobe reward"},
                    {"game_id",sequence==1?QJsonValue(7):QJsonValue()},{"level",sequence==1?QJsonValue():QJsonValue(3)},{"earned_at",now},{"sequence",sequence}});
            }
            auto prizes=std::make_shared<QJsonObject>(QJsonObject{{"player_id",player},{"revision","50"},{"pending",3},{"items",rows},{"waiting",rows},
                {"shelf",QJsonArray{}},{"upcoming",QJsonArray{QJsonObject{{"level",4},{"slot",false}},QJsonObject{{"level",5},{"slot",false}},QJsonObject{{"level",6},{"slot",true}}}}});
            *replayPrizes=[prizes,rows]{(*prizes)["waiting"]=rows;(*prizes)["pending"]=rows.size();};
            shell.backendAccount()->setServices({[player,prizes](QByteArray,QString path,QJsonObject body,QByteArray,trainer::BackendAccount::Reply done){
                if(path=="/v1/auth/refresh")done(200,{{"player_id",player},{"session_id","33333333-3333-4333-8333-333333333333"},{"fluxer_subject","12345"},
                    {"access_token",QString(43,'a')},{"refresh_token",QString(43,'r')},{"expires_at",QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODateWithMs)}});
                else if(path.startsWith("/v1/me/prizes")) {
                    if(path.endsWith("/seen")){QJsonArray left;for(const auto v:(*prizes)["waiting"].toArray())if(!body["ids"].toArray().contains(v.toObject()["id"]))left.append(v);(*prizes)["pending"]=left.size();(*prizes)["waiting"]=left;}
                    if(path.endsWith("/shelf"))(*prizes)["shelf"]=body["games"].toArray().isEmpty()?QJsonArray{}:QJsonArray{QJsonObject{{"game_id",7},{"title","A completed adventure"},{"beaten",true},{"completed",false}}};
                    done(200,*prizes);
                } else done(503,{});
            },[](QString,auto done){done(true,QJsonDocument(QJsonObject{{"refresh_token",QString(43,'r')}}).toJson());},[](QString,QByteArray,auto done){done(true);},[](QUrl){return false;},[](QString){return QByteArray();},[](QString,QByteArray){return true;}});
            shell.backendAccount()->configure("ui-prizes-fixture");shell.openProfile();shell.activate(11,"profile");window->resize(1280,720);break;
        }
        case 44:
            check(shell.profileRoute()=="prizes","Reward collection route");capture("prizes-wide");
            shell.activate(100,"profile");shell.activate(2,"profile");check(shell.backendAccount()->prizes()["shelf"].toList().size()==1,"Trophy added to shelf");
            shell.activate(1,"profile");break;
        case 45:
            check(shell.backendAccount()->prizes()["celebrating"].toBool(),"Explicit grouped celebration");capture("prizes-celebration-wide");
            shell.dispatch(Action::Back);check(shell.backendAccount()->prizes()["pending"].toInt()==3,"Later keeps unpresented grants");
            window->resize(960,540);shell.activate(101,"profile");break;
        case 46:
            capture("prizes-pending-item-compact");shell.activate(1,"profile");break;
        case 47: {
            capture("prizes-celebration-compact");shell.dispatch(Action::Confirm);
            check(shell.backendAccount()->prizes()["pending"].toInt()==2,"Only this game's moment acknowledged");
            check(shell.backendAccount()->prizes()["ceremony"].toMap()["slot"].toBool(),"Level groups clothing with slot");
            const std::function<QQuickItem*(QQuickItem*)> find=[&](QQuickItem* item)->QQuickItem* {
                if(item->objectName()=="prize-ceremony-action"&&item->isVisible())return item;
                for(auto* child:item->childItems())if(auto* result=find(child))return result;return nullptr;
            };
            auto* action=find(window->contentItem());check(action!=nullptr,"Ceremony touch action exists");
            if(action){const auto point=action->mapToScene({action->width()/2,action->height()/2});const auto global=QPointF(window->mapToGlobal(point.toPoint()));
                QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);
            }
            check(shell.profileRoute()=="outfits","Ceremony touch opens wardrobe");break;
        }
        case 48:
            shell.dispatch(Action::Back);check(shell.profileRoute()=="prizes"&&shell.backendAccount()->prizes()["celebrating"].toBool(),"Wardrobe returns to frozen ceremony");
            capture("prizes-level-compact");shell.dispatch(Action::Confirm);
            check(shell.backendAccount()->prizes()["pending"].toInt()==0,"Presentation acknowledges only shown rewards");
            shell.dispatch(Action::Back);check(shell.profileFocus()==11,"Collection returns to shelf entry");break;
        case 49:
            capture("prizes-profile-shelf");shell.dispatch(Action::Back);
            (*replayPrizes)();shell.backendAccount()->refreshPrizes();
            shell.setLiveGame({{"id","test-game"},{"title","Test game"}});shell.presentPostGamePrizes();*prizeWait=0;break;
        case 50:
            if(++*prizeWait<5){--*step;break;}
            check(!shell.serviceOpen(),"No ceremony over an active or minimized game");
            shell.setLiveGame({});shell.keyboard()->begin("Finishing a message","Draft",80,false,"Send");shell.presentPostGamePrizes();*prizeWait=0;break;
        case 51:
            if(++*prizeWait<5){--*step;break;}
            check(!shell.serviceOpen(),"Post-game ceremony waits for text input");shell.keyboard()->cancel();*prizeWait=0;break;
        case 52:
            if(!shell.serviceOpen()&&++*prizeWait<10){--*step;break;}
            check(shell.profileRoute()=="prizes"&&shell.backendAccount()->prizes()["celebrating"].toBool(),"Post-game pending awards open on safe shell");
            shell.dispatch(Action::LocalAction);check(shell.backendAccount()->prizes()["shelf"].toList().size()==1,"Ceremony pin is idempotent");
            capture("prizes-postgame");shell.dispatch(Action::Back);shell.dispatch(Action::Back);shell.dispatch(Action::Back);break;
        case 53: {
            const QString player="11111111-1111-4111-8111-111111111111";
            auto privacy=std::make_shared<QJsonObject>(QJsonObject{{"revision","0"},{"level",true},{"shelf",true},{"history",true},{"activity",false}});
            const auto recipe=shell.trainer()->avatarDescriptor()["recipe"].toObject();
            shell.backendAccount()->setServices({[player,privacy,recipe](QByteArray method,QString path,QJsonObject body,QByteArray,trainer::BackendAccount::Reply done){
                if(path=="/v1/auth/refresh")done(200,{{"player_id",player},{"session_id","33333333-3333-4333-8333-333333333333"},{"fluxer_subject","12345"},
                    {"access_token",QString(43,'a')},{"refresh_token",QString(43,'r')},{"expires_at",QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODateWithMs)}});
                else if(path=="/v1/me/profile/sharing") {if(method=="PUT"){*privacy=body;(*privacy)["revision"]="1";}done(200,*privacy);}
                else if(path.startsWith("/v1/profiles/")) {
                    QJsonObject value{{"player_id",player},{"display_name","Luna · Collector of worlds"},{"subject","2"},{"avatar",recipe},{"appearance","aurora"},{"visibility",*privacy}};
                    QJsonArray trophies;for(int i=0;i<5;++i)trophies.append(QJsonObject{{"game_id",i+1},{"title",QStringList{"Crystal adventure","Night racers","Starbound stories","Dream island","The final quest"}[i]},{"completed",i%2==0},{"earned_at","2026-10-28T12:00:00Z"}});
                    if((*privacy)["level"].toBool())value["level"]=18;
                    if((*privacy)["shelf"].toBool())value["shelf"]=trophies;
                    if((*privacy)["history"].toBool())value["history"]=trophies;
                    done(200,value);
                }else done(503,{});
            },[](QString,auto done){done(true,QJsonDocument(QJsonObject{{"refresh_token",QString(43,'r')}}).toJson());},[](QString,QByteArray,auto done){done(true);},[](QUrl){return false;},[](QString){return QByteArray();},[](QString,QByteArray){return true;}});
            shell.backendAccount()->configure("ui-visitor-fixture");window->resize(1280,720);shell.openProfile();shell.activate(12,"profile");*prizeWait=0;break;
        }
        case 54:
            if(shell.visitorProfile()->avatarPreview().isEmpty()&&++*prizeWait<30){--*step;break;}
            check(shell.profileRoute()=="visitor"&&shell.visitorProfile()->preview(),"Own profile uses visitor projection");
            check(shell.visitorProfile()->data()["shelf"].toList().size()==5,"Visitor shelf is server-backed");capture("visitor-wide");window->resize(960,540);break;
        case 55:
            capture("visitor-compact");shell.dispatch(Action::Back);check(shell.profileRoute()=="summary"&&shell.profileFocus()==12,"Preview returns to its own entry");
            shell.activate(13,"profile");shell.activate(2,"profile");break;
        case 56:
            check(!shell.visitorProfile()->sharing()["history"].toBool(),"Sharing writes the server choice");capture("visitor-sharing-compact");
            shell.dispatch(Action::Back);shell.activate(12,"profile");break;
        case 57:
            check(!shell.visitorProfile()->data().contains("history"),"Self preview respects private history");capture("visitor-private-compact");
            shell.dispatch(Action::Back);shell.dispatch(Action::Back);shell.openOnlineFace("chats");shell.social()->profile("2");break;
        case 58:
            check(shell.profileRoute()=="visitor"&&!shell.visitorProfile()->preview(),"Social person opens full profile");
            capture("visitor-friend-compact");shell.dispatch(Action::Back);check(!shell.serviceOpen()&&shell.onlineFace()=="chats","Friend profile returns to original Online face");
            shell.openVisitorProfile("11111111-1111-4111-8111-111111111111");check(shell.profileRoute()=="visitor","Stable player ID opens the same profile");shell.dispatch(Action::Back);break;
        case 59: {
            auto* social=shell.social();trainer::HomeSocialFixture::publish(*social,{{"state","connected"},{"userId","1"},{"name","Mika"}});
            shell.openOnlineFace("lobby");social->openLobbyForPeople();if(social->lobbyCreateOpen())social->submitLobbyDraft();
            auto* host=social->lobbyCoordinator();host->selectGame({"fixture16","A party for everyone","crates","netplay",16,{}});
            for(int i=2;i<=16;++i){
                auto* guest=new trainer::LobbyCoordinator(window);const auto id=QString::number(i),name="Player "+id;guest->configure(id,name,true);
                QObject::connect(host,&trainer::LobbyCoordinator::outgoing,guest,[guest,id](QString peer,QJsonObject packet){if(peer==id)guest->receive("1","Mika",packet);});
                QObject::connect(guest,&trainer::LobbyCoordinator::outgoing,window,[host,id,name](QString,QJsonObject packet){host->receive(id,name,packet);});
                host->invite(id);guest->answer(true);
            }
            social->chooseMyLobby(host->state()["id"].toString());window->resize(1280,720);break;
        }
        case 60:
            check(shell.social()->lobbyPlayers().size()==16,"Sixteen seats remain available");
            check(!shell.social()->myLobbiesOpen(),"Roster is visible without the lobby picker");capture("lobby-sixteen-wide");window->resize(960,540);break;
        case 61:
            capture("lobby-sixteen-compact");shell.social()->activateLobbyRoom("person:16");break;
        case 62:
            shell.social()->profile("16");check(shell.profileRoute()=="visitor","Last lobby player opens full profile");shell.dispatch(Action::Back);
            check(shell.onlineFace()=="lobby"&&shell.social()->lobbyRoomFocus()=="person:16","Profile returns to sixteenth roster player");break;
        case 63:
            shell.social()->profile("2");shell.backendAccount()->configure("ui-visitor-new-owner");
            check(shell.visitorProfile()->data().isEmpty(),"Account change clears the visible foreign profile");
            shell.activate(3,"profile");check(!shell.visitorProfile()->data().isEmpty(),"Profile refresh remains usable after account change");shell.dispatch(Action::Back);break;
        case 64:
            // Leaving a primary page restores the real local trainer owner.
            // Authenticate the isolated visitor fixture after that transition.
            shell.goToPage(0);shell.backendAccount()->configure("ui-visitor-home-options");break;
        case 65:
            shell.openVisitorProfile("11111111-1111-4111-8111-111111111111");shell.activate(2,"profile");
            check(!shell.serviceOpen()&&shell.primaryId()=="online"&&shell.onlineFace()=="chats"&&shell.social()->profileVisible(),"Social options are visible from a profile opened outside Online");
            shell.social()->closeMenu();shell.goToPage(0);shell.backendAccount()->configure("ui-visitor-home-message");break;
        case 66:
            shell.openVisitorProfile("11111111-1111-4111-8111-111111111111");shell.activate(0,"profile");
            check(shell.primaryId()=="online"&&shell.onlineFace()=="chats","Message from a UUID profile enters the existing conversation route");break;
        default:
            check(warnings==0,"QML warnings during Online navigation");
            if(!directory.isEmpty()){QFile f(directory+"/online-report.json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(QJsonObject{{"warnings",warnings},{"success",diagnostics.isEmpty()},{"diagnostics",QJsonArray::fromStringList(diagnostics)}}).toJson());}
            completed=true;timer->stop();QCoreApplication::exit(diagnostics.isEmpty()?0:1);
        }
    });timer->start();
}
