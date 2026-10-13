#pragma once
#include "core/navigation/ShellController.h"
#include "core/input/ControllerInput.h"
#include "integrations/social/FluxerSession.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFile>
#include <QTimer>
#include <memory>

namespace trainer {
// Isolated provider state; exercises the actual QML and input, never an account.
class LobbySmokeScenario {
public:
    static void start(QQuickWindow* window,ShellController& shell,ControllerInput& input,SDL_Joystick* joystick,
                      QString directory,bool& completed,int& warnings,QStringList& diagnostics) {
        auto* social=shell.social();
        QObject::disconnect(social->session_,nullptr,social,nullptr);
        QObject::disconnect(social,nullptr,social->session_,nullptr);
        auto failed=std::make_shared<bool>(false);
        auto step=std::make_shared<int>(0);
        auto keeps=std::make_shared<int>(0);
        QObject::connect(social,&SocialController::commandRequested,window,[keeps](QString op,QVariantMap args){if(op=="recent-add"&&args["id"]=="2")++*keeps;});
        auto* guest=new LobbyCoordinator(window);guest->configure("2","Guest",true);
        QObject::connect(social->lobbyCoordinator(),&LobbyCoordinator::outgoing,guest,[=](QString peer,QJsonObject p){if(peer=="2")guest->receive("1","Host",p);});
        QObject::connect(guest,&LobbyCoordinator::outgoing,social,[=](QString,QJsonObject p){social->lobbyCoordinator()->receive("2","Guest",p);});
        const auto check=[&diagnostics,failed](bool ok,QString message){diagnostics.append((ok?"PASS ":"FAIL ")+message);if(!ok)*failed=true;};
        const auto press=[&input,joystick](SDL_GameControllerButton key){SDL_JoystickSetVirtualButton(joystick,key,1);input.poll();SDL_JoystickSetVirtualButton(joystick,key,0);input.poll();};
        const auto capture=[=](QString name){check(window->grabWindow().save(directory+"/"+name+".png"),"Capture "+name);};
        const auto click=[window](QString name) {
            const std::function<QQuickItem*(QQuickItem*)> find=[&](QQuickItem* item)->QQuickItem* {
                if(item->objectName()==name)return item;
                for(auto* child:item->childItems())if(auto* found=find(child))return found;
                return nullptr;
            };
            auto* item=find(window->contentItem());if(!item)return false;
            const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));
            const auto global=window->mapToGlobal(point.toPoint());
            QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QMouseEvent up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);return true;
        };
        auto* timer=new QTimer(window);timer->setInterval(220);
        QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&input,&completed,&warnings,&diagnostics]() {
            switch((*step)++) {
            case 0:
                social->receive(0,{{"state","connected"},{"userId","1"},{"name","Host"},{"lobbyDirectory",QVariantMap{{"rows",QVariantList{}},{"status","No open lobbies yet."}}}});
                social->setLobbyGames({QVariantMap{{"key","fixture"},{"id","fixture"},{"title","Shared game"},{"platform","Test"},{"mode","netplay"},{"seats",4}}});
                shell.goToPage(4);for(int i=0;i<3;++i)shell.dispatch(Action::NextFace);break;
            case 1:
                check(window->findChild<QQuickItem*>("social-lobby")!=nullptr,"Lobby QML instantiated");
                press(ControllerInput::ConfirmButton);press(ControllerInput::ConfirmButton);break;
            case 2:
                check(social->lobbyState()["phase"]=="choosing","Controller creates game-free lobby");
                check(social->lobbyCoordinator()->invite("2"),"Host invites guest");guest->answer(true);break;
            case 3:
                check(social->lobbyState()["members"].toList().size()==2,"Accepted guest appears in lobby");
                {
                    auto snapshot=social->snapshot_;snapshot["channel"]="300";
                    snapshot["voice"]=QVariantMap{{"available",true}};
                    snapshot["chats"]=QVariantList{QVariantMap{{"id","300"},{"name","Our people"},{"kind","groups"}}};
                    snapshot["messages"]=QVariantList{
                        QVariantMap{{"id","301"},{"author","2"},{"name","Guest"},{"text","What shall we play?"}},
                        QVariantMap{{"id","302"},{"author","1"},{"name","Host"},{"text","Let's pick a game together."},{"mine",true}}};
                    social->receive(0,snapshot);social->lobbyChatChannel_="300";social->lobbyChatStatus_.clear();social->selectLobby(0);
                    check(social->lobbyChatVisible(),"Lobby embeds existing conversation");
                }
                capture("lobby-choosing");
                social->lobbyGames_={QVariantMap{{"key","fixture"},{"id","fixture"},{"title","Shared game"},{"platform","Test"},{"mode","netplay"},{"seats",4},{"runtime",QVariantMap{{"privateProgress",true}}}}};
                social->lobbyAction("game:fixture");social->lobbyAction("seat:1");guest->seat(2);break;
            case 4:
                check(social->lobbyState()["game"].toMap()["title"]=="Shared game","Game choice retains lobby");
                {auto* notice=window->findChild<QQuickItem*>("lobby-progress-notice");check(notice&&notice->isVisible(),"Shared-save disclosure is visible before preparation");}
                capture("lobby-seats");
                shell.dispatch(Action::Secondary);
                check(shell.keyboard()->isOpen()&&social->textPurpose_=="message","Lobby X opens the shared message composer");
                shell.keyboard()->cancel();
                shell.keyboard()->begin("Message","",100,false,"Send",true);break;
            case 5:
                check(shell.keyboard()->keys().first().toMap()["label"]=="Q","QWERTY first key");capture("keyboard-qwerty");
                shell.dispatch(Action::NextFace);break;
            case 6:
                check(shell.socialFace()=="lobby"&&shell.keyboard()->language()=="ru","Keyboard trigger changes language, preserves Social face");
                capture("keyboard-russian");
                {QKeyEvent text(QEvent::KeyPress,Qt::Key_Q,Qt::NoModifier,"q");QCoreApplication::sendEvent(window,&text);}
                {QKeyEvent text(QEvent::KeyPress,0,Qt::NoModifier,QString::fromUtf8("ё"));QCoreApplication::sendEvent(window,&text);}
                break;
            case 7:
                check(shell.keyboard()->text()==QString::fromUtf8("qё"),"Physical keyboard inserts text instead of navigating pages");
                {QKeyEvent key(QEvent::KeyPress,Qt::Key_Backspace,Qt::NoModifier);QCoreApplication::sendEvent(window,&key);}
                check(shell.keyboard()->text()=="q","Physical Backspace deletes one grapheme");
                shell.keyboard()->cancel();window->resize(1280,720);break;
            case 8:capture("lobby-wide");window->resize(720,720);break;
            case 9:
                capture("lobby-square");social->lobbyAction("leave");
                SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);input.poll();
                SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_TRIGGERRIGHT,-32768);input.poll();break;
            case 10:
                check(shell.socialFace()=="recent","Physical R2 opens Recently together after Lobby");
                check(social->recentPeople().size()==1,"Recent list retains guest after lobby closes");
                check(window->findChild<QQuickItem*>("social-recent-together")!=nullptr,"Recent QML instantiated");
                {
                    auto snapshot=social->snapshot_;snapshot["friends"]=QVariantList{QVariantMap{{"id","2"},{"name","Guest"},{"type",1}}};
                    snapshot["temporaryContacts"]=QVariantMap{{"peers",QVariantMap{{"2",QVariantMap{{"temporary",true},{"canKeep",true},{"status","Temporary lobby friend"}}}}}};
                    social->receive(0,snapshot);
                }
                capture("recent-square");window->resize(1280,720);break;
            case 11:
                capture("recent-wide");
                check(social->recentPeople().first().toMap()["friendLabel"]=="Keep as friend","Recent exposes Keep for a temporary friend");
                press(SDL_CONTROLLER_BUTTON_Y);check(*keeps==1,"Controller X (Nintendo preset) keeps the selected temporary friend");
                press(ControllerInput::ConfirmButton);
                check(social->profileVisible()&&social->profileTarget_=="2","Recent A opens the selected profile");
                social->closeMenu();shell.dispatch(Action::NextFace);check(shell.socialFace()=="chats","Fifth Social tab wraps to Messages");
                shell.dispatch(Action::PreviousFace);check(shell.socialFace()=="recent","L2 returns to Recent");
                shell.selectFace(0);break;
            case 12:
                check(click("face-tab-4"),"Fifth tab has a visible pointer target");
                break;
            case 13:
                check(shell.socialFace()=="recent","Pointer activates the fifth Social tab");
                check(click("recent-friend-action"),"Keep has a visible pointer target");break;
            default:
                check(*keeps==2,"Pointer keeps the selected temporary friend");
                check(guest->state().isEmpty(),"Closing host lobby notifies guest");
                timer->stop();completed=true;
                QFile report(directory+"/lobby-check.txt");if(report.open(QIODevice::WriteOnly))report.write((diagnostics.join('\n')+QString("\nqmlWarnings=%1\n").arg(warnings)).toUtf8());
                QCoreApplication::exit(*failed||warnings?4:0);
            }
        });timer->start();
    }
};
}
