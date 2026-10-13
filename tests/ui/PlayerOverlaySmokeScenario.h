#pragma once
#include "features/social/PlayerOverlayController.h"
#include "features/social/SocialController.h"
#include "integrations/social/FluxerSession.h"
#include <QMouseEvent>
#include <QPainter>
#include <QJSValue>

namespace trainer {
class PlayerOverlaySmokeScenario {
public:
    static void start(QQuickWindow* window,ShellController& shell,AdventureExitPresentation& view,
        AdventureExitController& exit,PlayerOverlayController& hud,QString directory,bool& completed,int& warnings,QStringList& diagnostics) {
        auto* social=shell.social();QObject::disconnect(social->session_,nullptr,social,nullptr);QObject::disconnect(social,nullptr,social->session_,nullptr);
        auto failed=std::make_shared<bool>(false);auto step=std::make_shared<int>(0);
        const auto check=[&diagnostics,failed](bool ok,QString message){diagnostics.append((ok?"PASS ":"FAIL ")+message);if(!ok)*failed=true;};
        auto* badge=window->findChild<QQuickWindow*>("invitation-badge-window");
        auto* overlay=window->findChild<QQuickWindow*>("adventure-exit-window");
        check(badge&&overlay,"Shared passive and active windows exist");
        if(!badge||!overlay){completed=true;QCoreApplication::exit(4);return;}
        const auto find=[](QQuickItem* root,const QString& name) {
            const std::function<QQuickItem*(QQuickItem*)> visit=[&](QQuickItem* item)->QQuickItem* {
                if(item->objectName()==name)return item;
                for(auto* child:item->childItems())if(auto* match=visit(child))return match;
                return nullptr;
            };return visit(root);
        };
        const auto capture=[=](QQuickWindow* target,QString name){check(target->grabWindow().save(directory+"/"+name+".png"),"Capture "+name);};
        const auto neutral=[&view]{view.setInputIsolated(true);view.setWindowFocused(true);view.updateInput(view.inputGeneration(),{true,true});};
        QObject::connect(&hud,&PlayerOverlayController::optionsRequested,&view,[&view]{view.requestMenu();});
        QObject::connect(&view,&AdventureExitPresentation::menuCaptureRequested,&view,[&view](quint64 token){
            QImage frame(960,540,QImage::Format_RGB32);QPainter p(&frame);QLinearGradient gradient(0,0,960,540);gradient.setColorAt(0,QColor("#52796f"));gradient.setColorAt(1,QColor("#253f52"));p.fillRect(frame.rect(),gradient);p.setPen(QColor("#bad4ce"));p.setFont(QFont("Arial",24));p.drawText(frame.rect(),Qt::AlignCenter,"Game preview · UI fixture");p.end();view.menuCaptureCompleted(token,frame);
        });
        auto* timer=new QTimer(window);timer->setInterval(240);
        QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&hud,&view,&exit,&completed,&warnings,&diagnostics]() {
            switch((*step)++) {
            case 0: {
                QImage avatar(96,96,QImage::Format_ARGB32);avatar.fill(QColor("#e89299"));
                {QPainter paint(&avatar);paint.fillRect(0,0,48,96,QColor("#80bccf"));paint.setBrush(QColor("#ffe09c"));paint.setPen(Qt::NoPen);paint.drawEllipse(24,24,48,48);}
                const auto avatarPath=QDir(directory).absoluteFilePath("avatar-fixture.png");check(avatar.save(avatarPath),"Create isolated avatar image fixture");
                QVariantList players;for(int seat=1;seat<=4;++seat)players.append(QVariantMap{{"peer",seat==1?QString():"online:"+QString::number(seat)},{"slot",seat},{"name",QStringList{"Eri","Luna","Kai","Mira"}[seat-1]},{"ready",true}});
                social->receive(0,{{"state","connected"},{"status","Connected"},{"userId","1"},{"channel","old"},
                    {"chats",QVariantList{QVariantMap{{"id","51"},{"name","Luna"},{"kind","chats"},{"avatar",QUrl::fromLocalFile(avatarPath).toString()},{"unread",3},{"members",QVariantList{QVariantMap{{"id","2"}}}}}}}});
                social->setGameParty({{"party","fixture"},{"running",true},{"host",true},{"members",players}});social->setGameActive(true);hud.setGame(42);hud.setPosition({0,0});
                badge->setProperty("playing",true);badge->resize(960,540);exit.beginSession(AdventureSavePolicy::ManualConfirm);exit.setAvailable(true);break;
            }
            case 1: {
                check(badge->isVisible(),"Passive player bubbles appear during the fixture game");
                check(badge->flags().testFlag(Qt::WindowDoesNotAcceptFocus),"Passive window cannot take keyboard focus");
                check(hud.players().size()==4&&hud.players()[1].toMap()["unread"].toInt()==3,"Four controller seats and provider unread are projected");
                auto* avatar=find(badge->contentItem(),"player-avatar-2");check(avatar&&avatar->property("status").toInt()==1,"Provider avatar image is loaded for the matching player");
                auto* bubbles=find(badge->contentItem(),"player-bubbles");check(bubbles,"Player component exists");
                if(bubbles) {
                    const auto pointer=[badge](QEvent::Type type,QPointF point,Qt::MouseButton button,Qt::MouseButtons buttons){
                        QMouseEvent event(type,point,badge->mapToGlobal(point.toPoint()),button,buttons,Qt::NoModifier);QCoreApplication::sendEvent(badge,&event);
                    };
                    pointer(QEvent::MouseButtonPress,{80,30},Qt::LeftButton,Qt::LeftButton);
                    pointer(QEvent::MouseMove,{460,410},Qt::NoButton,Qt::LeftButton);
                    pointer(QEvent::MouseButtonRelease,{460,410},Qt::LeftButton,Qt::NoButton);
                }
                check(hud.position().x()>0.4&&hud.position().y()>0.7,"Dragging persists a normalized safe-area position");capture(badge,"players-four");hud.openPlayer("2");break;
            }
            case 2:
                check(view.menuOpen()&&!badge->isVisible(),"Passive HUD is excluded while the game overlay is open");neutral();break;
            case 3:
                neutral();check(hud.chatOpen()&&!hud.conversationReady(),"Mini chat waits for the selected DM instead of revealing the previous chat");
                {auto snapshot=social->snapshot_;snapshot["channel"]="51";snapshot["messages"]=QVariantList{
                    QVariantMap{{"id","61"},{"author","2"},{"name","Luna"},{"text","Ready for the next round?"}},
                    QVariantMap{{"id","62"},{"author","1"},{"name","Eri"},{"text","One moment — saving my game."},{"mine",true}}};social->receive(0,snapshot);}break;
            case 4:
                check(hud.conversationReady(),"Correct provider conversation becomes visible");capture(overlay,"player-mini-chat");hud.dispatch(Action::Confirm);break;
            case 5:
                check(shell.keyboard()->isOpen(),"Controller opens the existing keyboard inside the game overlay");shell.keyboard()->insertText("See you in a moment");capture(overlay,"player-keyboard");
                if(auto* back=find(overlay->contentItem(),"keyboard-back")) {
                    const auto point=back->mapToScene(QPointF(back->width()/2,back->height()/2));const auto global=overlay->mapToGlobal(point.toPoint());
                    QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
                    QMouseEvent up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                    QCoreApplication::sendEvent(overlay,&down);QCoreApplication::sendEvent(overlay,&up);
                } else check(false,"Keyboard has a pointer Back target");break;
            case 6:
                check(!shell.keyboard()->isOpen()&&social->draft()=="See you in a moment","Back preserves the shared DM draft");hud.dispatch(Action::Back);break;
            case 7:
                check(!hud.chatOpen()&&!view.visible()&&badge->isVisible(),"Back restores the passive surface without ending the game session");
                hud.openPlayer("2");break;
            case 8: neutral();break;
            case 9: neutral();hud.send();break;
            case 10:
                check(!hud.chatOpen()&&social->draft().isEmpty(),"Sending queues the existing Social message and returns to play");
                hud.setPosition({1,1});badge->resize(540,540);break;
            case 11: {
                auto* bubbles=find(badge->contentItem(),"player-bubbles");const auto regionValue=bubbles?bubbles->property("regions"):QVariant();const auto regions=regionValue.metaType()==QMetaType::fromType<QJSValue>()?regionValue.value<QJSValue>().toVariant().toList():regionValue.toList();
                check(!regions.isEmpty(),"Resized HUD retains explicit input regions");
                for(const auto& v:regions) {const auto rect=v.toMap()["rect"].toRectF();check(rect.left()>=0&&rect.top()>=0&&rect.right()<=badge->width()&&rect.bottom()<=badge->height(),"Bubble stays within resized display");}
                capture(badge,"players-square");hud.setGame(0);break;
            }
            default:
                check(hud.players().isEmpty()&&!hud.chatOpen(),"Ending the owned game clears player HUD");timer->stop();completed=true;
                QFile report(directory+"/player-overlay-check.txt");if(report.open(QIODevice::WriteOnly))report.write((diagnostics.join('\n')+QString("\nqmlWarnings=%1\n").arg(warnings)).toUtf8());
                QCoreApplication::exit(*failed||warnings?4:0);
            }
        });timer->start();
    }
};
}
