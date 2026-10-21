#pragma once
#include "core/navigation/ShellController.h"
#include <QQuickWindow>
#include <QTimer>
#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QSettings>
#include <QProcess>
#include <QMouseEvent>
#include <QQuickItem>
#include "app/PlatformBinding.h"
#include <memory>

class StartNetworkFixture final:public trainer::NetworkService {
public:
    using NetworkService::NetworkService;
    void request(const QJsonObject& request) override {
        const auto connected=request["op"]=="connect";
        QTimer::singleShot(0,this,[this,connected]{
            QJsonArray rows;
            for(int i=0;i<7;++i)rows.append(QJsonObject{{"id",QString("test:%1").arg(i)},
                {"title",i==0?"Home Wi-Fi":i==1?"Guest network":i==2?"Studio — secure network":QString("Nearby network %1").arg(i)},
                {"detail",i==2&&connected?"Connected":i==1?"Open · Good signal":"Secured · Good signal"},
                {"security",i==1?"open":"wpa-psk"},{"saved",i==0||connected&&i==2},{"connected",connected&&i==2}});
            emit event({{"event","snapshot"},{"rows",rows}});emit finished({});
        });
    }
    void cancel() override {emit finished("Cancelled");}
};

// Real external example -> composition binding -> rendered host widget -> action
// callback -> process termination cleanup. Input is semantic, not physical proof.
inline void startPlatformSmoke(QQuickWindow* window, trainer::ShellController& shell,
    trainer::AdventureLaunchController& launch, const QString& directory,
    bool& completed, int& warnings, QStringList& diagnostics) {
    auto settings=std::make_shared<QTemporaryDir>();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings->path());
    auto binding=std::make_shared<trainer::PlatformBinding>(shell,launch,settings->filePath("endpoint.json"));
    auto network=new StartNetworkFixture(window);shell.network()->configure(network);
    auto child=std::make_shared<QProcess>();
    child->start(QString::fromUtf8(SDK_PYTHON),{QString::fromUtf8(SDK_SOURCE)+"/sdk/examples/hello_traineros.py","--endpoint",settings->filePath("endpoint.json")});
    auto step=std::make_shared<int>(0);
    auto ticks=std::make_shared<int>(0);
    auto actions=std::make_shared<int>(0);
    auto timer=new QTimer(window);timer->setInterval(400);
    QObject::connect(shell.controls(),&trainer::ControlsDashboard::contributionActivated,timer,[actions](const QString&){++*actions;});
    QObject::connect(timer,&QTimer::timeout,window,[=,&shell,&completed,&warnings,&diagnostics]{
        Q_UNUSED(settings);
        Q_UNUSED(binding);
        auto* board=shell.controls();
        const auto check=[&](bool ok,const QString& why){if(!ok)diagnostics.append(why);};
        const std::function<QQuickItem*(QQuickItem*,QString)> find=[&](QQuickItem* item,QString name)->QQuickItem*{
            if(item->objectName()==name&&item->isVisible())return item;
            for(auto* child:item->childItems())if(auto* found=find(child,name))return found;return nullptr;
        };
        if(++*ticks>55){diagnostics<<"SDK/Start UI process timed out";*step=9;}
        switch(*step) {
        case 0:
            if(board->available().isEmpty())break;
            shell.dispatch(trainer::Action::SystemMenu);
            board->edit();board->pick();board->restore("sdk:org.example.hello:hello");board->edit();
            ++*step;break;
        case 1: {
            auto frame=window->grabWindow();check(!frame.isNull(),"Empty SDK widget frame");
            if(!directory.isEmpty())check(frame.save(directory+"/platform-widget.png"),"Cannot save SDK frame");
            board->dispatch(trainer::Action::Confirm);check(*actions==1,"SDK widget action did not dispatch");
            ++*step;break;
        }
        case 2:
            if(board->widgets().isEmpty()||!board->widgets().first().toMap()["text"].toString().contains("Activations: 1"))break;
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/platform-activated.png"),"Cannot save SDK callback frame");
            // A console Python process on Windows need not handle WM_CLOSE.
            // Abrupt exit also proves host cleanup needs no client cooperation.
            child->kill();++*step;break;
        case 3:
            if(child->state()!=QProcess::NotRunning||board->pages()!=1)break;
            ++*step;break;
        case 4:
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/start-compact.png"),"Cannot save Start frame");
            board->activate("wifi");++*step;break;
        case 5: {
            check(shell.controlsNetworkOpen()&&shell.network()->rows().size()==7,"Wi-Fi list missing in Start");
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/start-wifi.png"),"Cannot save Wi-Fi frame");
            // Exercise the actual pointer path through the modal and row.
            auto* popup=find(window->contentItem(),"start-wifi-popup");
            auto* row=popup?find(popup,"connection-2"):nullptr;
            check(row&&row->isVisible(),"Wi-Fi touch target missing");
            if(row){const auto point=row->mapToScene(QPointF(row->width()/2,row->height()/2));const auto global=QPointF(window->mapToGlobal(point.toPoint()));
                QMouseEvent down(QEvent::MouseButtonPress,point,global,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier),up(QEvent::MouseButtonRelease,point,global,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                QCoreApplication::sendEvent(window,&down);QCoreApplication::sendEvent(window,&up);}
            ++*step;break;
        }
        case 6: {
            check(shell.keyboard()->isOpen(),"Wi-Fi row did not open password keyboard");
            for(const auto& key:shell.keyboard()->keys()){
                const auto data=key.toMap();if(!data["visible"].toBool())continue;
                auto* item=find(window->contentItem(),"key-"+data["id"].toString());
                check(item&&item->mapToScene(QPointF(item->width(),item->height())).x()<=window->width(),"Keyboard key clipped at the screen edge");
            }
            if(!directory.isEmpty())check(window->grabWindow().save(directory+"/start-wifi-password.png"),"Cannot save password frame");
            shell.dispatch(trainer::Action::Back);check(!shell.keyboard()->isOpen()&&shell.controlsNetworkOpen(),"Password Back left Start");
            shell.dispatch(trainer::Action::Confirm);shell.keyboard()->insertText("test password");
            for(int k=0;k<shell.keyboard()->keys().size();++k)if(shell.keyboard()->keys()[k].toMap()["id"]=="apply"){shell.keyboard()->activate(k);break;}++*step;break;
        }
        case 7:
            check(shell.network()->rows().value(2).toMap()["connected"].toBool(),"Wi-Fi result not shown");
            shell.dispatch(trainer::Action::Back);check(!shell.controlsNetworkOpen()&&shell.menuOpen(),"Wi-Fi Back did not return to Start");
            ++*step;break;
        default:
            if(child->state()!=QProcess::NotRunning){child->kill();child->waitForFinished(2000);}
            const auto stderrText=child->readAllStandardError();check(stderrText.isEmpty(),QString::fromUtf8(stderrText));
            check(board->pages()==1,"SDK removal left a ghost page");
            check(warnings==0,"QML warnings during SDK widget rendering");
            for(const auto& message:diagnostics)qWarning().noquote()<<message;
            completed=true;timer->stop();QCoreApplication::exit(diagnostics.isEmpty()?0:1);
        }
    });
    timer->start();
}
