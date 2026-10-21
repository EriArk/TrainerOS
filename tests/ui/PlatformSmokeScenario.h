#pragma once
#include "core/navigation/ShellController.h"
#include <QQuickWindow>
#include <QTimer>
#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QSettings>
#include <QProcess>
#include "app/PlatformBinding.h"
#include <memory>

// Real external example -> composition binding -> rendered host widget -> action
// callback -> process termination cleanup. Input is semantic, not physical proof.
inline void startPlatformSmoke(QQuickWindow* window, trainer::ShellController& shell,
    trainer::AdventureLaunchController& launch, const QString& directory,
    bool& completed, int& warnings, QStringList& diagnostics) {
    auto settings=std::make_shared<QTemporaryDir>();
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings->path());
    auto binding=std::make_shared<trainer::PlatformBinding>(shell,launch,settings->filePath("endpoint.json"));
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
        if(++*ticks>40){diagnostics<<"SDK UI process timed out";*step=5;}
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
