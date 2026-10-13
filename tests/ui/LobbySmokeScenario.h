#pragma once
#include "core/navigation/ShellController.h"
#include "core/input/ControllerInput.h"
#include "integrations/social/FluxerSession.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QKeyEvent>
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
        auto* guest=new LobbyCoordinator(window);guest->configure("2","Guest",true);
        QObject::connect(social->lobbyCoordinator(),&LobbyCoordinator::outgoing,guest,[=](QString peer,QJsonObject p){if(peer=="2")guest->receive("1","Host",p);});
        QObject::connect(guest,&LobbyCoordinator::outgoing,social,[=](QString,QJsonObject p){social->lobbyCoordinator()->receive("2","Guest",p);});
        const auto check=[&diagnostics,failed](bool ok,QString message){diagnostics.append((ok?"PASS ":"FAIL ")+message);if(!ok)*failed=true;};
        const auto press=[&input,joystick](SDL_GameControllerButton key){SDL_JoystickSetVirtualButton(joystick,key,1);input.poll();SDL_JoystickSetVirtualButton(joystick,key,0);input.poll();};
        const auto capture=[=](QString name){check(window->grabWindow().save(directory+"/"+name+".png"),"Capture "+name);};
        auto* timer=new QTimer(window);timer->setInterval(220);
        QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&completed,&warnings,&diagnostics]() {
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
                social->lobbyGames_={QVariantMap{{"key","fixture"},{"id","fixture"},{"title","Shared game"},{"platform","Test"},{"mode","netplay"},{"seats",4}}};
                social->lobbyAction("game:fixture");social->lobbyAction("seat:1");guest->seat(2);break;
            case 4:
                check(social->lobbyState()["game"].toMap()["title"]=="Shared game","Game choice retains lobby");capture("lobby-seats");
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
            case 9:capture("lobby-square");social->lobbyAction("leave");break;
            default:
                check(guest->state().isEmpty(),"Closing host lobby notifies guest");
                timer->stop();completed=true;
                QFile report(directory+"/lobby-check.txt");if(report.open(QIODevice::WriteOnly))report.write((diagnostics.join('\n')+QString("\nqmlWarnings=%1\n").arg(warnings)).toUtf8());
                QCoreApplication::exit(*failed||warnings?4:0);
            }
        });timer->start();
    }
};
}
