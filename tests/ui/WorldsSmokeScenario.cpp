#include "WorldsSmokeScenario.h"
#include <QCoreApplication>
#include <QQuickItem>
#include <QImage>
#include <QFile>
#include <QTimer>
#include <QSignalSpy>
#include <QTest>
#include <memory>
using namespace trainer;
void startWorldsSmoke(QQuickWindow* window, ShellController& shell, ControllerInput& input,
        MockAdventureAdapter&, SDL_Joystick* joystick, const QString& directory,
        bool& completed, int& warnings, QStringList& diagnostics) {
    auto step=std::make_shared<int>(0);auto failed=std::make_shared<bool>(false);
    auto selectedId=std::make_shared<QString>();
    auto timer=new QTimer(window);timer->setInterval(650);
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&input,&completed,&warnings,&diagnostics]{
        const auto check=[&](bool ok,const QString& text){if(!ok){*failed=true;diagnostics<<QString("Stage %1: %2").arg(*step-1).arg(text);}};
        const auto press=[&](SDL_GameControllerButton b){SDL_JoystickSetVirtualButton(joystick,b,1);input.poll();SDL_JoystickSetVirtualButton(joystick,b,0);input.poll();};
        const auto capture=[&](const QString& name){const auto frame=window->grabWindow();check(!frame.isNull(),"Empty render");if(!directory.isEmpty())check(frame.save(directory+"/"+name+".png"),"Cannot save render");};
        const auto focus=[&]{auto* item=window->activeFocusItem();check(item && item->isVisible() && item->isEnabled(),"Hidden or missing focus");};
        const auto visibleItem=[&](const QString& name)->QQuickItem* {
            std::function<QQuickItem*(QQuickItem*)> find=[&](QQuickItem* item)->QQuickItem* {
                if(!item->isVisible())return nullptr;
                if(item->objectName()==name)return item;
                for(auto* child:item->childItems())if(auto* found=find(child))return found;
                return nullptr;
            };return find(window->contentItem());
        };
        constexpr auto a=SDL_CONTROLLER_BUTTON_B,b=SDL_CONTROLLER_BUTTON_A;
        switch((*step)++) {
        case 0: shell.goToPage(1);break;
        case 1: {
            check(shell.collectionsRoot(),"Games root missing");focus();capture("games-root-960");
            auto* ribbon=window->findChild<QQuickItem*>("series-grid");auto* grid=window->findChild<QQuickItem*>("games-platforms");
            QFile geometry(directory+"/root-geometry.txt"); if(geometry.open(QIODevice::WriteOnly))geometry.write(QString("ribbon %1 x %2, grid %3 x %4, platform %5").arg(ribbon?ribbon->width():0).arg(ribbon?ribbon->height():0).arg(grid?grid->width():0).arg(grid?grid->height():0).arg(shell.gamesPlatforms().front().toMap()["name"].toString()).toUtf8());
            check(ribbon && qAbs(ribbon->width()/ribbon->property("cell").toDouble()-6)<0.01,"Six circular collections must fit");
            check(grid && qAbs(grid->width()/grid->property("cellWidth").toDouble()-grid->property("columns").toInt())<0.01 && qAbs(grid->height()/grid->property("cellHeight").toDouble()-1)<0.01,"Platform ribbon must use adaptive columns in one row");
            for(auto* item:window->findChildren<QQuickItem*>())if(item->objectName().startsWith("games-platform-") && item->isVisible()) {
                check(qAbs(item->width()/item->height()-3.0/4.0)<0.001,"Platform cover proportions must be 3:4");
                check(qAbs(item->parentItem()->height()-item->y()-item->height()-6)<0.01,"Cover bottom inset must remain six units");
            }
            press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);break;
        }
        case 2: check(shell.gamesRootZone()=="platforms","Down must enter platforms");focus();capture("games-platform-focus");press(a);break;
        case 3: {
            check(!shell.collectionsRoot() && shell.multiverse()->route()=="games","Platform must open games directly");capture("platform-browser");
            auto* media=visibleItem("library-detail-picture");
            auto* facts=visibleItem("library-detail-facts");
            check(media && facts && facts->x()+facts->width()<media->x(),"Media frame and metadata must not overlap");
            check(media && qAbs((media->width()-14)/(media->height()-14)-media->property("aspect").toDouble())<0.001,"Media frame must follow image proportions");
            auto* scroll=visibleItem("library-detail-scroll");
            auto* description=scroll?scroll->parentItem()->findChild<QQuickItem*>("multiverse-description"):nullptr;
            check(description,"Description fixture missing");
            if(description) description->setProperty("text",QString("A long game synopsis for deliberate reading. ").repeated(150));
            break;
        }
        case 4: {
            auto* scroll=visibleItem("library-detail-scroll");
            check(scroll && scroll->property("contentY").toDouble()==0,"Description must start without automatic scrolling");
            shell.dispatch(Action::ScrollDown);
            check(scroll && scroll->property("contentY").toDouble()>0,QString("Right stick scrolls the description: content=%1 height=%2 active=%3 paired=%4").arg(scroll?scroll->property("contentHeight").toDouble():-1).arg(scroll?scroll->height():-1).arg(scroll?scroll->parentItem()->property("active").toBool():false).arg(shell.pairedNavigationAvailable()));
            shell.dispatch(Action::Secondary);const auto offset=scroll?scroll->property("contentY").toDouble():0;
            shell.dispatch(Action::ScrollDown);check(scroll && scroll->property("contentY").toDouble()==offset,"Drawer blocks background scrolling");
            press(b);press(b);
            check(shell.collectionsRoot() && shell.gamesRootZone()=="platforms","Back must restore platform focus");press(SDL_CONTROLLER_BUTTON_DPAD_UP);break;
        }
        case 5: check(shell.gamesRootZone()=="collections","Up must restore collections");shell.openGamesRootItem("collections",shell.gamesCollections().front().toMap()["id"].toString());break;
        case 6: check(!shell.collectionsRoot(),"Collection opens directly");capture("collection-browser");window->resize(1920,1080);break;
        case 7: capture("browser-1080");window->resize(1280,800);break;
        case 8: capture("browser-16-10");press(b);shell.openGamesRootItem("collections","_create");break;
        case 9: check(shell.collectionManager()->isOpen(),"Create card opens editor");capture("collection-create");press(b);window->resize(960,540);break;
        case 10: capture("collection-ribbon-end");shell.dispatch(Action::ToggleContinue);break;
        case 11: check(shell.drawerOpen(),"Global Y drawer unavailable");press(b);shell.dispatch(Action::Secondary);break;
        case 12: check(shell.quickAppsOpen(),"Global X drawer unavailable");press(b);shell.openGamesRootItem("collections","pokemon");press(b);break;
        case 13: shell.openGamesRootItem("platforms",shell.gamesPlatforms().front().toMap()["id"].toString());*selectedId=shell.multiverse()->detail()["id"].toString();shell.libraryAction("view:list");break;
        case 14: {
            auto* list=visibleItem("library-paged-list");check(list,"List is not visible");focus();capture("list-960");
            check(shell.multiverse()->detail()["id"].toString()==*selectedId,"View change lost game identity");
            auto* media=visibleItem("library-detail-picture");auto* facts=visibleItem("library-detail-facts");
            check(media && facts && facts->x()+facts->width()<media->x(),"List lost shared information layout");
            auto* wheel=visibleItem("footer-hint-R3");
            check(wheel,"Touch R3 hint missing");if(wheel)QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,wheel->mapToScene(QPointF(wheel->width()/2,wheel->height()/2)).toPoint());
            break;
        }
        case 15: check(shell.multiverse()->viewStyle()=="shelves","Touch R3 did not select Shelves");press(SDL_CONTROLLER_BUTTON_RIGHTSTICK);check(shell.multiverse()->viewStyle()=="grid","R3 did not cycle to Grid");press(SDL_CONTROLLER_BUTTON_RIGHTSTICK);check(shell.multiverse()->viewStyle()=="wheel","R3 did not cycle to Wheel");check(shell.multiverse()->detail()["id"].toString()==*selectedId,"Picker lost ID");press(SDL_CONTROLLER_BUTTON_RIGHTSTICK);check(shell.multiverse()->viewStyle()=="list","R3 button did not select List");window->resize(1920,1080);break;
        case 16: {
            capture("list-1080");const auto before=shell.multiverse()->focusIndex();
            press(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            if(shell.multiverse()->games().size()>7)check(shell.multiverse()->focusIndex()==before+7,"Right must flip a whole list page");
            press(SDL_CONTROLLER_BUTTON_DPAD_LEFT);check(shell.multiverse()->focusIndex()==before,"Left must restore the previous list page");
            window->resize(1280,800);break;
        }
        case 17: {
            capture("list-16-10");window->resize(960,540);
            auto* list=visibleItem("library-paged-list");
            QVariantList fixture;for(int i=0;i<31;++i)fixture.append(QVariantMap{{"id",QString("fixture-%1").arg(i)},{"title",i==1?QString("A deliberately long game title that cannot fit in an ordinary list row"):QString("Page fixture %1").arg(i+1)},{"platformShort","GBA"},{"linked",i!=3}});
            if(list){list->setProperty("entries",fixture);list->setProperty("selectionIndex",0);}break;
        }
        case 18: {
            auto* list=visibleItem("library-paged-list");check(list,"Paged fixture missing");if(!list)break;
            const int count=list->property("pageSize").toInt();check(count==7,"List must contain seven rows per page");
            QSignalSpy changed(list,SIGNAL(focusRequested(int)));QSignalSpy launch(list,SIGNAL(activated(int)));
            const auto from=list->mapToScene(QPointF(list->width()/2,120)).toPoint();
            const auto to=list->mapToScene(QPointF(list->width()/2,50)).toPoint();
            QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(window,to);QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,to);
            check(changed.size()==1 && changed[0][0].toInt()==count,"Swipe must advance exactly one full page");
            check(launch.isEmpty(),"Swipe must never launch a row");
            list->setProperty("selectionIndex",count);break;
        }
        case 19: {
            auto* list=visibleItem("library-paged-list");check(list && list->property("page").toInt()==1,"Second page did not advance");capture("list-page-two");
            if(list)list->setProperty("selectionIndex",30);break;
        }
        case 20: {
            auto* list=visibleItem("library-paged-list");capture("list-last-page");
            if(list){QSignalSpy changed(list,SIGNAL(focusRequested(int)));QMetaObject::invokeMethod(list,"turnPage",Q_ARG(QVariant,1));check(changed.isEmpty(),"Last page must not wrap on swipe");
                for(auto* row:list->findChildren<QQuickItem*>())if(row->objectName().startsWith("library-list-row-") && row->isVisible())check(row->y()+row->height()<=list->height()-34,"A list row is cut off");}
            press(b);break;
        }
        case 21: shell.openGamesRootItem("platforms",shell.gamesPlatforms().front().toMap()["id"].toString());shell.libraryAction("view:shelves");break;
        case 22: {
            auto* shelves=visibleItem("library-shelves");check(shelves,"Shelves missing");
            check(!visibleItem("library-detail-picture"),"Shelves must not retain a visible information pane");
            QVariantList fixture;
            const QStringList groups{"fantasy","racing","sports","scifi","puzzle","action","neutral"};
            for(int i=0;i<103;++i)fixture.append(QVariantMap{{"id",QString("shelf-fixture-%1").arg(i)},
                {"title",i==0?"A very long adventure title for the selected cover caption":QString("Adventure %1").arg(i+1)},
                {"platformShort","GBA"},{"linked",i!=3},
                {"cover",i==2?"qrc:/series/kirby.png":""},
                {"fallbackCover",QString("qrc:/cover-fallbacks/%1-%2.png").arg(groups[i%groups.size()]).arg(i%2)}});
            if(shelves){shelves->setProperty("entries",fixture);shelves->setProperty("selectionIndex",0);}break;
        }
        case 23: {
            auto* shelves=visibleItem("library-shelves");auto* scroll=visibleItem("shelves-scroll");
            check(shelves && scroll,"Shelves fixture unavailable");focus();capture("shelves-960");
            if(shelves && scroll){
                check(shelves->property("columns").toInt()==5,"Five covers must fit a shelf");
                check(qAbs(shelves->property("rowHeight").toDouble()*2-scroll->height())<0.1,"Two complete shelf rows must fit");
                int coverCount=0;
                std::function<void(QQuickItem*)> countCovers=[&](QQuickItem* item){
                    if(item->objectName().startsWith("shelf-game-"))++coverCount;
                    for(auto* child:item->childItems())countCovers(child);
                };countCovers(scroll);
                check(coverCount>=10 && coverCount<=20,"Shelf delegates must be virtualized, not instantiate the whole library");
                auto* real=visibleItem("shelf-game-2");
                check(real && real->property("realCover").toBool(),"Real artwork must replace the generated placeholder");
                auto* image=real?real->findChild<QQuickItem*>("shelf-cover-art"):nullptr;
                check(image && qAbs(image->width()/image->height()-image->implicitWidth()/image->implicitHeight())<0.01,"Cover case must follow actual image proportions without letterboxing");
                QSignalSpy launch(shelves,SIGNAL(activated(int)));
                auto from=scroll->mapToScene(QPointF(60,scroll->height()-55)).toPoint();
                QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);
                for(int i=1;i<=8;++i)QTest::mouseMove(window,from-QPoint(0,i*22),25);
                QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,from-QPoint(0,176));
                check(launch.isEmpty(),"Shelf swipe must never launch a game");
            }break;
        }
        case 24: {
            auto* shelves=visibleItem("library-shelves");auto* scroll=visibleItem("shelves-scroll");
            check(scroll && scroll->property("contentY").toDouble()>1,"Vertical touch swipe must move shelves");capture("shelves-scrolled");
            if(shelves)shelves->setProperty("selectionIndex",102);break;
        }
        case 25: {
            auto* shelves=visibleItem("library-shelves");auto* scroll=visibleItem("shelves-scroll");
            check(visibleItem("shelf-game-102"),"Final partial shelf selection must be visible");capture("shelves-last-row");
            if(scroll && shelves){check(scroll->property("contentY").toDouble()<=scroll->property("contentHeight").toDouble()-scroll->height()+0.1,"Shelves overscrolled last row");shelves->setProperty("selectionIndex",0);}
            window->resize(1920,1080);break;
        }
        case 26: capture("shelves-1080");window->resize(1280,800);break;
        case 27: {
            auto* shelves=visibleItem("library-shelves");
            check(shelves && shelves->property("firstRow").toInt()==0,"Resizing must keep the selected first game on screen");
            capture("shelves-16-10");press(b);break;
        }
        case 28: shell.openGamesRootItem("platforms",shell.gamesPlatforms().front().toMap()["id"].toString());shell.libraryAction("view:grid");window->resize(960,540);break;
        case 29: {
            auto* grid=visibleItem("library-grid");check(grid,"Grid missing");
            check(!visibleItem("library-detail-picture") && !visibleItem("library-shelves"),"Grid must be its own full-area presentation");
            QVariantList fixture;
            for(int i=0;i<101;++i)fixture.append(QVariantMap{{"id",QString("grid-%1").arg(i)},{"title",i==0?"A long screenshot game title that must remain inside its own card":QString("Adventure %1").arg(i+1)},
                {"linked",true},{"artwork",QVariantMap{{"screenshot",i%3==0?"qrc:/series/kirby.png":i%3==1?"qrc:/series/zelda.png":""}}}});
            if(grid){grid->setProperty("entries",fixture);grid->setProperty("selectionIndex",0);}break;
        }
        case 30: {
            auto* grid=visibleItem("library-grid");auto* scroll=visibleItem("grid-scroll");check(grid && scroll,"Grid scroll missing");focus();capture("grid-960");
            if(grid && scroll){
                check(grid->property("columns").toInt()==shell.multiverse()->gridColumns(),"Controller columns must match rendered columns");
                auto* picture=visibleItem("grid-picture-0");check(picture && qAbs(picture->width()/picture->height()-picture->property("imageAspect").toDouble())<.01,"Grid frame must fit screenshot proportions without stretched fields");
                int count=0;std::function<void(QQuickItem*)> countItems=[&](QQuickItem* item){if(item->objectName().startsWith("grid-game-"))++count;for(auto* child:item->childItems())countItems(child);};countItems(scroll);
                check(count>0 && count<30,"Grid must virtualize large libraries");
                QSignalSpy launch(grid,SIGNAL(activated(int)));
                auto from=scroll->mapToScene(QPointF(70,scroll->height()-30)).toPoint();QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);
                for(int i=1;i<=8;++i)QTest::mouseMove(window,from-QPoint(0,i*20),25);
                QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,from-QPoint(0,160));check(launch.isEmpty(),"Grid swipe must not launch");
            }break;
        }
        case 31: {
            auto* grid=visibleItem("library-grid");auto* scroll=visibleItem("grid-scroll");
            check(scroll && scroll->property("contentY").toDouble()>1,"Grid swipe must scroll vertically");
            if(grid)grid->setProperty("selectionIndex",100);break;
        }
        case 32: {
            check(visibleItem("grid-game-100"),"Last partial grid row must be reachable");capture("grid-last-row");
            auto* grid=visibleItem("library-grid");if(grid)grid->setProperty("selectionIndex",0);window->resize(1920,1080);break;
        }
        case 33: capture("grid-1080");window->resize(1280,800);break;
        case 34: {
            auto* scroll=visibleItem("grid-scroll");check(scroll && qAbs(scroll->property("contentY").toDouble()-scroll->property("originY").toDouble())<1,"Grid resize must keep the first selected row at its origin");
            capture("grid-16-10");auto* grid=visibleItem("library-grid");if(grid)grid->setProperty("entries",QVariantList{});break;
        }
        case 35: capture("grid-empty");press(b);break;
        default:
            check(shell.collectionsRoot(),"Return to root failed");check(warnings==0,"QML warnings");completed=true;timer->stop();
            if(!directory.isEmpty()){QFile f(directory+"/collections-verification.txt");if(f.open(QIODevice::WriteOnly))f.write(((*failed?QString("FAILED\n"):QString("PASSED\n"))+diagnostics.join('\n')).toUtf8());}
            QCoreApplication::exit(*failed?1:0);break;
        }
    });timer->start();
}
