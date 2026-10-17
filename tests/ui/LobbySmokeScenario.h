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
class LobbyRoomSmokeRuntime final : public LobbyRuntimeService {
public:
    using LobbyRuntimeService::LobbyRuntimeService;
    void prepare(QString,QJsonObject,Completion done) override {done(true,{});}
    void launch(QString,QString,QJsonObject,Completion done) override {done(false,"UI fixture does not launch games");}
    void cancel(QString) override {}
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
                press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);check(social->lobbyFocus()==2,"Right selects the adjacent column");
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
                capture("catalogue-960");social->selectLobby(0);if(social->myLobbiesOpen())social->chooseMyLobby(social->lobbyState()["id"].toString());break;
            case 7:
                if(social->lobbyCreateOpen())social->submitLobbyDraft();
                check(social->lobbyState()["phase"]=="choosing","Create enters existing game-free room");
                capture("catalogue-own-room");social->lobbyAction("browse");break;
            case 8:
                capture("catalogue-return");check(!social->lobbyDetailFocused_,"Back returns to catalogue and retains room");
                window->resize(1600,900);shell.settings()->cycleTheme(1);break;
            case 9:capture("catalogue-second-theme");press(ControllerInput::BackButton);break;
            case 10:
                check(social->lobbyRailFocused(),"B moves to categories");capture("catalogue-controller-rail");
                press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);press(ControllerInput::ConfirmButton);break;
            case 11:
                check(!social->lobbyRailFocused()&&social->lobbyTopic()=="racing"&&social->lobbyFocus()>0,"A applies category and focuses a room");
                capture("catalogue-controller-room");social->focusLobbyCard(1);press(SDL_CONTROLLER_BUTTON_DPAD_UP);break;
            case 12:
                check(social->lobbyFocus()==0,"Up from first row selects create/my lobby");capture("catalogue-controller-create");
                press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);break;
            case 13:
                check(social->lobbyFocus()==1,"Down returns to remembered room");
                press(ControllerInput::BackButton);window->resize(960,540);break;
            case 14:capture("catalogue-controller-rail-960");if(!qEnvironmentVariableIsSet("TRAINEROS_LOBBY_ROOM_SMOKE"))*step=99;break;
            case 15: {
                social->lobbyRuntime_.registerService("netplay",new LobbyRoomSmokeRuntime(window));
                social->selectLobby(0);if(social->myLobbiesOpen())social->chooseMyLobby(social->lobbyState()["id"].toString());if(social->lobbyCreateOpen())social->submitLobbyDraft();social->lobby_.setDetails("One more round",QDateTime::currentSecsSinceEpoch()+3600);
                for(int i=2;i<=4;++i) {
                    const auto id=QString::number(i),name=QStringList{"Ren","Luna With A Long Nickname","Nori"}[i-2];
                    auto* guest=new LobbyCoordinator(window);guest->configure(id,name,true);
                    QObject::connect(&social->lobby_,&LobbyCoordinator::outgoing,guest,[=](QString peer,QJsonObject p){if(peer==id)guest->receive("1","Mika",p);});
                    QObject::connect(guest,&LobbyCoordinator::outgoing,social,[=](QString,QJsonObject p){social->lobby_.receive(id,name,p);});
                    social->lobby_.invite(id);guest->answer(true);if(i==4)guest->spectate(true);
                }
                social->lobby_.selectGame({"fixture","Super Mario Kart","SNES","netplay",4,{}});
                social->lobbyGames_={QVariantMap{{"id","fixture"},{"mode","netplay"},{"modeLabel","NetPlay"}}};
                social->lobby_.setConversation("300");auto snapshot=social->snapshot_;snapshot["channel"]="300";snapshot["status"]="Connected";
                snapshot["voice"]=QVariantMap{{"available",true},{"state","connected"},{"channel","300"},{"muted",true},{"speakers",QStringList{"2"}}};
                snapshot["chats"]=QVariantList{QVariantMap{{"id","300"},{"name","One more round"},{"kind","groups"}}};
                snapshot["messages"]=QVariantList{
                    QVariantMap{{"id","301"},{"author","2"},{"name","Ren"},{"text","Anyone up for a few races?"}},
                    QVariantMap{{"id","302"},{"author","3"},{"name","Luna"},{"text","I'm in! Let's start with Mushroom Cup."}},
                    QVariantMap{{"id","303"},{"author","1"},{"name","Mika"},{"text","Perfect. Nori is watching this round."},{"mine",true}},
                    QVariantMap{{"id","304"},{"author","4"},{"name","Nori"},{"text","That last corner!"}},
                    QVariantMap{{"id","305"},{"author","4"},{"name","Nori"},{"text","Ren, watch out for the green shell 😂"}},
                    QVariantMap{{"id","306"},{"author","4"},{"name","Nori"},{"text","<Not markup> & still a message"}}};
                social->receive(0,snapshot);social->lobbyChatChannel_="300";social->lobbyChatStatus_.clear();social->selectLobby(0);if(social->myLobbiesOpen())social->chooseMyLobby(social->lobbyState()["id"].toString());
                social->lobbyRoomFocus_="person:2";window->resize(1600,900);break;
            }
            case 16:
                check(social->lobbyPlayers().size()==4&&social->lobbySpectators().size()==1,"Room shows verified seats and separate spectators");
                check(social->lobbyPlayers().first().toMap()["prepared"].toBool(),"Preparation runs automatically; Ready remains explicit");
                capture("room-filled");check(click("lobby-expand-chat"),"Touch expands shared chat");break;
            case 17:
                check(social->lobbyChatExpanded(),"Expanded chat uses the same conversation");capture("room-chat-expanded");
                press(ControllerInput::BackButton);check(!social->lobbyChatExpanded()&&social->lobbyDetailFocused(),"B collapses chat without leaving room");
                press(SDL_CONTROLLER_BUTTON_RIGHTSTICK);check(social->lobbyChatExpanded(),"R3 expands the chat through the shell");press(SDL_CONTROLLER_BUTTON_RIGHTSTICK);
                press(ControllerInput::BackButton);window->resize(960,540);break;
            case 18:
                capture("room-handheld");press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);capture("room-controls-focus");
                window->resize(1920,1080);shell.settings()->cycleTheme(1);break;
            case 19:
                capture("room-wide-theme");social->lobby_.selectGame({"fixture-many","Sixteen-seat fixture","Test","netplay",16,{}});
                social->lobbyRoomFocus_="seat:16";window->resize(960,540);break;
            case 20:
                check(social->lobbyPlayers().size()==16,"Runtime capacity is not fixed at four");capture("room-sixteen-seats");
                check(click("lobby-room-menu"),"Room menu is touch accessible");break;
            case 21:
                capture("room-options");check(social->menuCommands_.contains("lobby:leave-confirm"),"Room exit is separate from B");social->closeMenu();
                press(ControllerInput::BackButton);check(!social->lobbyDetailFocused()&&!social->lobbyState().isEmpty(),"B preserves the room while browsing");break;
            case 22:
                social->selectLobby(0);if(social->myLobbiesOpen())social->chooseMyLobby(social->lobbyState()["id"].toString());check(click("lobby-spectator-strip"),"Spectator strip switches the chat");break;
            case 23:
                check(social->lobbySpectatorChat()&&social->lobbyRoomMessages().size()==3,"Only spectator messages appear in stream chat");capture("room-spectator-chat");
                press(ControllerInput::ConfirmButton);check(!shell.keyboard()->isOpen()&&social->menu_.isEmpty(),"Spectator chat has no reply or compose action");
                press(SDL_CONTROLLER_BUTTON_LEFTSTICK);check(!social->lobbySpectatorChat(),"L3 returns to player chat through the shell");break;
            case 24:
                check(!social->lobbySpectatorChat()&&social->lobbyRoomMessages().size()==3,"Player chat excludes spectator messages");break;
            case 25: {
                const auto first=social->lobbyState()["id"].toString();
                auto* room=social->lobby_.createRoom();check(room!=nullptr,"Second scheduled membership is created");
                if(room){room->setDetails("Saturday kart club",QDateTime::currentSecsSinceEpoch()+7200);room->selectGame({"fixture-kart","Super Mario Kart","SNES","netplay",4,{}});}
                check(social->lobby_.state()["id"].toString()==first,"Browsing retains the runtime room");
                social->showMyLobbies();window->resize(960,540);break;
            }
            case 26:
                check(social->myLobbies().size()==2,"My Lobbies includes both rooms");capture("my-lobbies-handheld");
                press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);press(ControllerInput::ConfirmButton);break;
            case 27:
                check(!social->myLobbiesOpen()&&social->lobbyState()["title"]=="Saturday kart club","Controller selects the scheduled room");
                capture("scheduled-room");shell.goToPage(0);break;
            case 28:
                capture("home-my-lobbies");social->showMyLobbies();window->resize(1600,900);break;
            case 29:
                capture("my-lobbies-wide");press(ControllerInput::BackButton);check(shell.page()==0,"Closing My Lobbies preserves Home");break;
            case 30:
                social->createAnotherLobby();window->resize(960,540);break;
            case 31:
                check(social->lobbyCreateOpen(),"Creation opens a form, not a room");check(social->myLobbies().size()==2,"Opening the form does not create membership");capture("create-handheld");
                social->lobbyDraftAction("category:party-games");social->lobbyDraftAction("access:approval");window->resize(1600,900);break;
            case 32:
                capture("create-wide");social->cancelLobbyDraft();social->createAnotherLobby();check(social->lobbyDraft()["access"]=="private","Closing discards all entered fields");
                social->setLobbyGames({QVariantMap{{"key","kart"},{"id","kart-id"},{"title","Super Mario Kart"},{"platform","snes"},{"mode","netplay"},{"modeLabel","NetPlay"},{"seats",2}},QVariantMap{{"key","bomber"},{"id","bomber-id"},{"title","Bomberman"},{"platform","nes"},{"mode","netplay"},{"modeLabel","NetPlay"},{"seats",2}}});social->openLobbyPicker();break;
            case 33:
                capture("game-picker");social->lobbyPickerAction("pick:kart");social->lobbyPickerAction("pick:bomber");press(SDL_CONTROLLER_BUTTON_X);check(!social->lobbyPicker()["open"].toBool(),"Y applies the picker selection through controller routing");break;
            case 34:
                capture("create-vote-selection");social->submitLobbyDraft();check(social->myLobbies().size()==3,"Confirm creates exactly one room");social->lobbyAction("vote");break;
            case 35:
                capture("game-vote");social->lobbyPickerAction("vote-close");break;
            case 36:
                check(social->lobby_.activateRoom(social->lobbyState()["id"].toString()),"The viewed room becomes the active runtime room");
                social->lobby_.selectGame({"fixture","Super Mario Kart","SNES","netplay",4,{}});
                social->reconcileLobbyPreparation();social->requestLobbyReady();
                check(social->lobbyRoomGame()["canStart"].toBool(),"All ready exposes host Start without launching");
                check(social->lobbyState()["phase"]=="gathering","Ready does not launch the game");
                window->resize(960,540);break;
            case 37:
                capture("room-host-start-handheld");social->requestLobbyReady();
                check(!social->lobbyRoomGame()["canStart"].toBool(),"Not ready removes Start");
                social->requestLobbyReady();window->resize(1600,900);break;
            case 38:
                capture("room-host-start-wide");check(click("lobby-start"),"Touch activates the host Start button");
                check(!social->lobbyRoomGame()["canStart"].toBool(),"Failed fixture launch returns without an enabled Start");break;
            case 39: {
                social->viewedLobby().setConversation("300");
                auto snapshot=social->snapshot_;snapshot["channel"]="300";
                snapshot["messages"]=QVariantList{QVariantMap{{"id","800"},{"author","1"},{"name","Ren"},{"text","Here is the backup."},{"mine",true},
                    {"attachments",QVariantList{QVariantMap{{"id","801"},{"filename","save-backup.zip"},{"size",240123}}}}}};
                social->receive(0,snapshot);social->syncLobbyChat();
                QFile file(directory+"/fixture-backup.txt");check(file.open(QIODevice::WriteOnly),"Create local file fixture");file.write("Lobby attachment fixture");file.close();
                social->files()->picker()->selected(file.fileName());window->resize(960,540);break;
            }
            case 40:
                check(social->files()->selected().size()==1,"File tray retains original filename and size");capture("room-files-handheld");
                social->files()->picker()->begin(directory);break;
            case 41:
                check(social->files()->picker()->isOpen(),"Shared file picker opens over the room");capture("file-picker-handheld");
                press(ControllerInput::BackButton);check(!social->files()->picker()->isOpen(),"Controller B closes the file picker");
                social->files()->remove(0);social->lobbyChatTools();break;
            case 42:
                check(social->menuCommands_.contains("lobby-file"),"Controller menu can attach files");
                check(!social->menuCommands_.contains("picture")&&!social->menuCommands_.contains("record"),"Lobby file menu excludes Social media actions");
                capture("room-file-actions");social->closeMenu();
                for(auto* room:social->lobby_.rooms())room->forgetMembership();shell.goToPage(0);break;
            case 43:
                check(social->myLobbies().isEmpty(),"Empty membership fixture");capture("home-create-lobby");
                shell.activateDashboard("lobbies");check(social->lobbyCreateOpen(),"Home without rooms opens Create lobby");social->cancelLobbyDraft();break;
            default:
                timer->stop();completed=true;QFile report(directory+"/catalogue-check.txt");
                if(report.open(QIODevice::WriteOnly))report.write((diagnostics.join('\n')+QString("\nqmlWarnings=%1\n").arg(warnings)).toUtf8());
                QCoreApplication::exit(*failed||warnings?4:0);
            }
        });timer->start();
    }
    static void social65(QQuickWindow* window,ShellController& shell,QString directory,bool& completed,int& warnings,QStringList& diagnostics) {
        auto* social=shell.social();QObject::disconnect(social->session_,nullptr,social,nullptr);QObject::disconnect(social,nullptr,social->session_,nullptr);
        auto step=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);auto* timer=new QTimer(window);timer->setInterval(600);
        const auto check=[failed,&diagnostics](bool ok,QString text){diagnostics.append((ok?"PASS ":"FAIL ")+text);if(!ok)*failed=true;};
        const auto capture=[=](QString name){check(window->grabWindow().save(directory+"/"+name+".png"),name);};
        QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&completed,&warnings,&diagnostics]{
            switch((*step)++) {
            case 0: {
                QVariantList people,chats,messages;
                const QStringList names{"Mika","Ren","Sofia Moonlight","Alex","Nori","Jules"};
                for(int i=0;i<names.size();++i){const auto id=QString::number(i+2);people.append(QVariantMap{{"id",id},{"name",names[i]},{"type",i==3?3:i==4?4:1}});
                    if(i<3)chats.append(QVariantMap{{"id","dm"+id},{"name",names[i]},{"kind","chats"},{"detail",i?"See you this evening!":"That was a great round."},{"members",QVariantList{QVariantMap{{"id",id}}}}});}
                chats.append(QVariantMap{{"id","group"},{"name","Weekend crew"},{"kind","groups"},{"detail","Up for a game?"},{"owner","1"}});
                chats.append(QVariantMap{{"id","text"},{"name","general"},{"guild","guild"},{"kind","community-channel"}});
                for(int i=0;i<6;++i)messages.append(QVariantMap{{"id",QString::number(100+i)},{"author",i%2?"1":"2"},{"name",i%2?"You":"Mika"},{"mine",bool(i%2)},{"text",QStringList{"Hey! Ready for another adventure?","Absolutely. Which game are we playing?","Let's pick something together this evening.","Sounds good! I'll bring the snacks.","Perfect. See you at eight!","See you there!"}[i]},{"timestamp",QString("2026-10-17T18:%1:00Z").arg(i,2,10,QChar('0'))}});
                social->receive(0,{{"state","connected"},{"userId","1"},{"name","Ren"},{"channel","dm2"},{"friends",people},{"chats",chats},{"messages",messages},
                    {"communities",QVariantList{QVariantMap{{"id","guild"},{"name","TrainerOS Club"},{"kind","community"}}}}});
                shell.goToPage(ShellController::SocialPage);social->setFace("chats");window->resize(1600,900);break;
            }
            case 1:capture("social-messages");social->contextRow(0);break;
            case 2:check(social->menu().size()<=4 && social->menu().contains("Profile"),"Basic person context retains profile and removal");capture("social-person-menu");social->closeMenu();social->reading_=true;social->conversationControl_=11;emit social->changed();break;
            case 3:check(social->conversationControl_==11,"Visible controller focus on the composer");capture("social-composer");social->closeMenu();shell.dispatch(Action::NextFace);social->snapshot_["channel"]="group";social->snapshot_["messages"]=QVariantList{};emit social->changed();break;
            case 4:check(social->face_=="communities"&&social->rows().size()==2,"Groups and communities share second face");capture("social-communities");social->activate(1);social->snapshot_["channel"]="text";emit social->changed();break;
            case 5:check(social->communityOpen()&&social->rows().size()==1,"Community replaces left list with channels");capture("social-channels");social->dispatch(Action::Back);break;
            case 6:check(!social->communityOpen(),"B returns to groups and communities");shell.dispatch(Action::PreviousFace);social->snapshot_["channel"]="dm2";social->snapshot_["messages"]=QVariantList{};emit social->changed();window->resize(960,540);break;
            case 7:capture("social-empty-handheld");social->profile("5");break;
            case 8:check(social->menu().contains("Accept friend request"),"Request actions remain in profile");capture("social-request");social->closeMenu();break;
            default:timer->stop();completed=true;QFile report(directory+"/social-check.txt");if(report.open(QIODevice::WriteOnly))report.write((diagnostics.join('\n')+QString("\nqmlWarnings=%1\n").arg(warnings)).toUtf8());QCoreApplication::exit(*failed||warnings?4:0);
            }
        });timer->start();
    }
    static void social67(QQuickWindow* window,ShellController& shell,QString directory,bool& completed,int& warnings,QStringList& diagnostics) {
        auto* social=shell.social();QObject::disconnect(social->session_,nullptr,social,nullptr);QObject::disconnect(social,nullptr,social->session_,nullptr);
        auto step=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);auto* timer=new QTimer(window);timer->setInterval(600);
        const auto check=[failed,&diagnostics](bool ok,QString text){diagnostics.append((ok?"PASS ":"FAIL ")+text);if(!ok)*failed=true;};
        const auto capture=[=](QString name){check(window->grabWindow().save(directory+"/"+name+".png"),name);};
        QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&completed,&warnings,&diagnostics]{
            switch((*step)++) {
            case 0: {
                QVariantList people,communities;QJsonArray members{QJsonObject{{"id","1"},{"seat",1},{"connected",true}}};
                const QStringList names{"Mika","Ren","Sofia Moonlight","Alex","Nori","Jules","Kai"};
                for(int i=0;i<names.size();++i){const auto id=QString::number(i+2);people.append(QVariantMap{{"id",id},{"name",names[i]},{"type",1}});members.append(QJsonObject{{"id",id},{"name",names[i]},{"connected",true},{"seat",i+2}});}
                const QStringList clubs{"Handheld adventures","Co-op corner","Retro racers"};
                const QStringList descriptions{"Find your next adventure and share the little moments.","Good company for the games we love. Beginners welcome!","One more lap? Meet fellow racing fans and plan a session."};
                for(int i=0;i<3;++i)communities.append(QVariantMap{{"id",QString::number(i+100)},{"name",clubs[i]},{"description",descriptions[i]},{"kind","community"},{"action","Join"},{"members",24+i*37},{"language","English"}});
                social->receive(0,{{"state","connected"},{"userId","1"},{"name","Ren"},{"friends",people},{"searchResults",communities},{"searchTotal",3}});
                social->recentTogether_.configure({},"1");social->recentTogether_.observe({{"id","fixture"},{"phase","playing"},{"ticket","co-play-fixture"},{"game",QJsonObject{{"id","kart"},{"title","Mario Kart 64"}}},{"members",members}},1792230000);
                shell.goToPage(ShellController::SocialPage);social->setFace("chats");shell.dispatch(Action::NextFace);shell.dispatch(Action::NextFace);window->resize(1600,900);break;
            }
            case 1:check(social->face_=="friends","Discover is the third Social face");capture("discover-home");social->dispatch(Action::Down);social->dispatch(Action::Down);social->dispatch(Action::Confirm);break;
            case 2:check(!social->discoverPreview().isEmpty(),"Controller A opens a preview");capture("discover-preview");social->dispatch(Action::Back);social->discoverAction("all");break;
            case 3:check(social->discover()["all"].toBool(),"See all retains recent teammates");capture("discover-recent");social->discoverAction("back");window->resize(960,540);break;
            case 4:capture("discover-home-handheld");social->query_="Mika";social->searchKind_="people";social->snapshot_["searchResults"]=QVariantList{QVariantMap{{"id","2"},{"name","Mika"},{"tag","mika#1234"},{"kind","person"},{"detail","Friends"}}};social->discoverArea_="results";social->searchFocus_=0;emit social->changed();break;
            case 5:capture("discover-people");social->activateSearch(0);break;
            case 6:check(social->profileVisible(),"Person opens the shared profile");capture("discover-person");social->closeMenu();social->query_.clear();social->snapshot_["searchResults"]=QVariantList{};social->snapshot_["searchTotal"]=0;social->snapshot_["searchStatus"]="No public TrainerOS communities yet. You can still join with an invitation.";social->recentTogether_.configure({},"empty-fixture");emit social->changed();break;
            case 7:capture("discover-empty-handheld");social->snapshot_["communities"]=QVariantList{QVariantMap{{"id","123"},{"name","Our community"},{"kind","community"},{"owner","1"}}};shell.dispatch(Action::PreviousFace);social->contextRow(0);break;
            case 8:check(social->menuCommands_.contains("ask-delete-community"),"Owner has explicit community deletion");capture("community-owner-menu");social->selectMenu(social->menuCommands_.indexOf("ask-delete-community"));break;
            case 9:check(social->menuCommands_.value(0)=="cancel","Deletion defaults to cancel");capture("community-delete-confirmation");social->closeMenu();break;
            default:timer->stop();completed=true;QFile report(directory+"/social67-check.txt");if(report.open(QIODevice::WriteOnly))report.write((diagnostics.join('\n')+QString("\nqmlWarnings=%1\n").arg(warnings)).toUtf8());QCoreApplication::exit(*failed||warnings?4:0);
            }
        });timer->start();
    }
    static void start(QQuickWindow* window,ShellController& shell,ControllerInput& input,SDL_Joystick* joystick,
                      QString directory,bool& completed,int& warnings,QStringList& diagnostics) {
        if(qEnvironmentVariableIsSet("TRAINEROS_SOCIAL67_SMOKE")){social67(window,shell,directory,completed,warnings,diagnostics);return;}
        if(qEnvironmentVariableIsSet("TRAINEROS_SOCIAL65_SMOKE")){social65(window,shell,directory,completed,warnings,diagnostics);return;}
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
                    social->lobby_.setConversation("300");auto snapshot=social->snapshot_;snapshot["channel"]="300";
                    snapshot["voice"]=QVariantMap{{"available",true}};
                    snapshot["chats"]=QVariantList{QVariantMap{{"id","300"},{"name","Our people"},{"kind","groups"}}};
                    snapshot["messages"]=QVariantList{
                        QVariantMap{{"id","301"},{"author","2"},{"name","Guest"},{"text","What shall we play?"}},
                        QVariantMap{{"id","302"},{"author","1"},{"name","Host"},{"text","Let's pick a game together."},{"mine",true}}};
                    social->receive(0,snapshot);social->lobbyChatChannel_="300";social->lobbyChatStatus_.clear();social->selectLobby(0);if(social->myLobbiesOpen())social->chooseMyLobby(social->lobbyState()["id"].toString());
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
