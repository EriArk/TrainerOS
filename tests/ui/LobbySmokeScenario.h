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
#include <QLocalSocket>
#include <QPainter>
#include <QtEndian>
#include <memory>

namespace trainer {
class LobbySmokeRuntime final : public LobbyRuntimeService {
public:
    using LobbyRuntimeService::LobbyRuntimeService;
    void prepare(QString,QJsonObject,Completion done) override {done(false,"UI fixture only");}
    void launch(QString,QString,QJsonObject,Completion done) override {done(false,"UI fixture only");}
    void cancel(QString) override {}
    bool supportsLive(const QJsonObject&) const override {return true;}
};
// Isolated provider state; exercises the actual QML and input, never an account.
class LobbySmokeScenario {
public:
    static void catalogue(QQuickWindow* window,ShellController& shell,ControllerInput& input,SDL_Joystick* joystick,QString directory,bool& completed,int& warnings,QStringList& diagnostics) {
        auto* social=shell.social();
        QObject::disconnect(social->session_,nullptr,social,nullptr);QObject::disconnect(social,nullptr,social->session_,nullptr);
        auto step=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);auto* timer=new QTimer(window);timer->setInterval(450);
        const auto check=[failed,&diagnostics](bool ok,QString text){diagnostics.append((ok?"PASS ":"FAIL ")+text);if(!ok)*failed=true;};
        const auto capture=[=](QString name){check(window->grabWindow().save(directory+"/"+name+".png"),name);};
        const auto press=[&input,joystick](SDL_GameControllerButton key){SDL_JoystickSetVirtualButton(joystick,key,1);input.poll();SDL_JoystickSetVirtualButton(joystick,key,0);input.poll();};
        const auto click=[window](QString name) {
            const std::function<QQuickItem*(QQuickItem*)> find=[&](QQuickItem* item)->QQuickItem* {if(item->objectName()==name)return item;for(auto* child:item->childItems())if(auto* found=find(child))return found;return nullptr;};
            auto* item=find(window->contentItem());if(!item)return false;
            const auto point=item->mapToScene(QPointF(item->width()/2,item->height()/2));const auto global=window->mapToGlobal(point.toPoint());
            QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);return true;
        };
        QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&completed,&warnings,&diagnostics] {
            switch((*step)++) {
            case 0: {
                QVariantList rows;const auto now=QDateTime::currentSecsSinceEpoch();
                const QStringList hosts{"Mira","Ren","Alex","Nori","Sol","Jules"};
                const QStringList titles{"After-school arcade","A little friendly rivalry","Choose our next adventure","Saturday kart club","One more round","Co-op & good company"};
                const QStringList games{"Super Mario Kart","Street Fighter II","","Mario Kart: Super Circuit","Bomberman","Secret of Mana"};
                for(int i=0;i<14;++i)rows.append(QVariantMap{{"lobby",QUuid::createUuid().toString(QUuid::WithoutBraces)},
                    {"owner",QString::number(40+i)},{"name",hosts[i%6]},{"title",titles[i%6]},{"expires",now+180},
                    {"topic",QStringList{"racing","competitive","party-games","racing","other","adventure"}[i%6]},
                    {"status",i%6==2?"choosing":"gathering"},{"access",i%3==0?"approval":"public"},
                    {"seats",i%6==2?0:4},{"available",i%6==2?0:i%4},{"members",4-i%4},{"startsAt",i%3?0:now+7200},
                    {"game",QVariantMap{{"title",games[i%6]},{"platform","SNES"},{"mode","netplay"}}}});
                social->receive(0,{{"state","connected"},{"userId","1"},{"name","Host"},{"lobbyDirectory",QVariantMap{{"rows",rows}}}});
                shell.goToPage(2);social->setFace("lobby");emit social->lobbyRequested();social->lobbyAction("browse");
                social->turnLobbyPage(-1);window->resize(1600,900);break;
            }
            case 1:
                capture("catalogue-page-1");check(social->lobbyPageCount()==3,"Fourteen rooms form three pages");
                press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);break;
            case 2:
                capture("catalogue-page-2");check(social->lobbyPage()==1,"Right turns one page");
                check(click("lobby-card-0"),"Touch selects the visible card");break;
            case 3:
                capture("catalogue-join");check(!social->menu_.isEmpty(),"Room opens existing join consent");
                social->closeMenu();social->lobbyAction("browse");social->lobbyAction("category:party-games");break;
            case 4:
                capture("catalogue-category");check(social->lobbyRows().size()==3,"Category filters room cards");
                social->lobbyAction("category:missing");break;
            case 5:
                capture("catalogue-empty");social->lobbyAction("clear-filters");window->resize(960,540);break;
            case 6:
                capture("catalogue-960");social->selectLobby(0);break;
            case 7:
                check(social->lobbyState()["phase"]=="choosing","Create enters existing game-free room");
                capture("catalogue-own-room");social->lobbyAction("browse");break;
            case 8:
                capture("catalogue-return");check(!social->lobbyDetailFocused_,"Back returns to catalogue and retains room");
                window->resize(1600,900);shell.settings()->cycleTheme(1);break;
            case 9:capture("catalogue-second-theme");break;
            default:
                timer->stop();completed=true;QFile report(directory+"/catalogue-check.txt");
                if(report.open(QIODevice::WriteOnly))report.write((diagnostics.join('\n')+QString("\nqmlWarnings=%1\n").arg(warnings)).toUtf8());
                QCoreApplication::exit(*failed||warnings?4:0);
            }
        });timer->start();
    }
    static void start(QQuickWindow* window,ShellController& shell,ControllerInput& input,SDL_Joystick* joystick,
                      QString directory,bool& completed,int& warnings,QStringList& diagnostics) {
        if(qEnvironmentVariableIsSet("TRAINEROS_LOBBY_CATALOGUE_SMOKE")){catalogue(window,shell,input,joystick,directory,completed,warnings,diagnostics);return;}
        auto* social=shell.social();
        QObject::disconnect(social->session_,nullptr,social,nullptr);
        QObject::disconnect(social,nullptr,social->session_,nullptr);
        auto failed=std::make_shared<bool>(false);
        auto step=std::make_shared<int>(0);
        auto keeps=std::make_shared<int>(0);
        auto* streamWriter=new QLocalSocket(window);
        QObject::connect(social,&SocialController::commandRequested,window,[keeps](QString op,QVariantMap args){if(op=="recent-add"&&args["id"]=="2")++*keeps;});
        auto* guest=new LobbyCoordinator(window);guest->configure("2","Guest",true);
        social->lobbyRuntime()->registerService("netplay",new LobbySmokeRuntime(window));
        QObject::connect(social->lobbyCoordinator(),&LobbyCoordinator::outgoing,guest,[=](QString peer,QJsonObject p){if(peer=="2")guest->receive("1","Host",p);});
        QObject::connect(guest,&LobbyCoordinator::outgoing,social,[=](QString,QJsonObject p){social->lobbyCoordinator()->receive("2","Guest",p);});
        const auto check=[&diagnostics,failed](bool ok,QString message){diagnostics.append((ok?"PASS ":"FAIL ")+message);if(!ok)*failed=true;};
        const auto press=[&input,joystick](SDL_GameControllerButton key){SDL_JoystickSetVirtualButton(joystick,key,1);input.poll();SDL_JoystickSetVirtualButton(joystick,key,0);input.poll();};
        const auto capture=[=](QString name){check(window->grabWindow().save(directory+"/"+name+".png"),"Capture "+name);};
        const auto rendered=[=]{
            const auto* view=window->findChild<QQuickItem*>("game-stream-view");
            if(!view){check(false,"Stream view exists");return;}
            const auto h=qMin(view->height(),view->width()*360.0/640.0);
            const auto point=view->mapToScene(QPointF(view->width()/2,view->height()/2-h*.3));
            const auto image=window->grabWindow();const auto color=image.pixelColor((point*(double(image.width())/window->width())).toPoint());
            check(qAbs(color.red()-50)<8&&qAbs(color.green()-85)<8&&qAbs(color.blue()-132)<8,"Video pixels are rendered, not only received");
        };
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
                check(click("lobby-action-options"),"Touch opens lobby options");*step=40;break;
            case 40:
                capture("lobby-live-options");
                check(click("social-menu-action-1"),"Live join has a direct touch option");
                check(social->lobbyState()["liveJoining"].toBool(),"Touch enables joining during play");
                social->lobbyAction("options");social->menuFocus_=1;
                press(ControllerInput::ConfirmButton);
                check(!social->lobbyState()["liveJoining"].toBool(),"Controller disables joining during play");
                social->lobbyAction("live-joining");social->closeMenu();break;
            case 41:
                {auto* notice=window->findChild<QQuickItem*>("lobby-progress-notice");check(notice&&notice->isVisible(),"Shared-save disclosure is visible before preparation");}
                capture("lobby-seats");
                shell.dispatch(Action::Secondary);
                check(shell.keyboard()->isOpen()&&social->textPurpose_=="message","Lobby X opens the shared message composer");
                shell.keyboard()->cancel();
                shell.keyboard()->begin("Message","",100,false,"Send",true);*step=5;break;
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
            case 14: {
                check(*keeps==2,"Pointer keeps the selected temporary friend");
                check(guest->state().isEmpty(),"Closing host lobby notifies guest");
                shell.selectFace(3);social->closeMenu();
                auto snapshot=social->snapshot_;
                snapshot["voice"]=QVariantMap{{"state","connected"},{"channel","300"},{"name","Our people"},{"muted",true},
                    {"streams",QVariantList{QVariantMap{{"identity","user_2_test"},{"account","2"},{"name","Guest"}}}}};
                social->receive(0,snapshot);
                social->streamAction("watch:user_2_test");
                streamWriter->connectToServer(social->stream_.server_.fullServerName());
                check(streamWriter->waitForConnected(1000),"Viewer accepts its private frame channel");
                QImage picture(640,360,QImage::Format_RGB32);picture.fill(QColor("#325584"));
                QPainter painter(&picture);painter.fillRect(0,240,640,120,QColor("#71ad86"));
                painter.setPen(QColor("#fff7d7"));QFont font;font.setPixelSize(30);painter.setFont(font);
                painter.drawText(picture.rect(),Qt::AlignCenter,"Shared game · stream check");painter.end();
                QByteArray frame(16,'\0');auto* data=reinterpret_cast<uchar*>(frame.data());
                qToLittleEndian<quint32>(0x31565254,data);qToLittleEndian<quint32>(640,data+4);
                qToLittleEndian<quint32>(360,data+8);qToLittleEndian<quint32>(640*360*4,data+12);
                frame.append(reinterpret_cast<const char*>(picture.constBits()),640*360*4);streamWriter->write(frame);streamWriter->flush();break;
            }
            case 15:
                check(social->stream()->hasFrame(),"Decoded frame reaches the visible viewer");rendered();capture("stream-wide");
                // Replace the texture as real video does; one static frame cannot
                // catch ownership/lifetime bugs in a scene-graph renderer.
                {QImage next(640,360,QImage::Format_RGB32);next.fill(QColor("#325584"));
                 QByteArray packet(16,'\0');auto* data=reinterpret_cast<uchar*>(packet.data());
                 qToLittleEndian<quint32>(0x31565254,data);qToLittleEndian<quint32>(640,data+4);
                 qToLittleEndian<quint32>(360,data+8);qToLittleEndian<quint32>(640*360*4,data+12);
                 packet.append(reinterpret_cast<const char*>(next.constBits()),640*360*4);streamWriter->write(packet);streamWriter->flush();}
                press(ControllerInput::ConfirmButton);break;
            case 16:
                check(social->menuMode_=="streams","Viewer A opens stream choices");check(social->stream()->active(),"Opening stream choices preserves the viewer");capture("stream-menu");
                social->closeMenu();window->resize(720,720);break;
            case 17:check(social->stream()->active(),"Resize preserves the viewer");rendered();capture("stream-square");press(ControllerInput::BackButton);break;
            case 18:
                check(!social->stream()->active(),"Viewer B stops watching");
                check(social->snapshot_["voice"].toMap()["channel"]=="300","Stop watching preserves voice call");break;
            default:
                timer->stop();completed=true;
                QFile report(directory+"/lobby-check.txt");if(report.open(QIODevice::WriteOnly))report.write((diagnostics.join('\n')+QString("\nqmlWarnings=%1\n").arg(warnings)).toUtf8());
                QCoreApplication::exit(*failed||warnings?4:0);
            }
        });timer->start();
    }
};
}
