// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QDir>
#include <QImage>
#include "features/crates/CratesController.h"
using namespace trainer;
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
    if(argc!=3)return 2;
    QTemporaryDir temp;
    const QString helpers=QString::fromLocal8Bit(argv[1]),out=QString::fromLocal8Bit(argv[2]);
    QDir().mkpath(out);
    CratesController flow;
    flow.installer()->configure(helpers+"/installer.py",helpers+"/packages.json",temp.path()+"/packages");
    flow.configureCatalogue(helpers+"/catalogue.py",temp.path()+"/catalogue");
    QQmlApplicationEngine engine;
    bool warning=false;
    QObject::connect(&engine,&QQmlEngine::warnings,&app,[&](auto){warning=true;});
    engine.rootContext()->setContextProperty("catalogue",&flow);
    engine.load(QUrl("qrc:/preview/Preview.qml"));
    if(engine.rootObjects().isEmpty())return 3;
    auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    flow.open(1,true);
    QTimer timer;timer.setInterval(700);int step=0;
    QObject::connect(&timer,&QTimer::timeout,&app,[&]{
        if(!flow.syncStatus().isEmpty())return;
        for(auto* item:window->findChildren<QQuickItem*>())
            if(item->property("status").toInt()==2 && item->property("source").isValid())return;
        if(flow.projectCount()!=2){app.exit(4);return;}
        if(step==0){if(!window->grabWindow().save(out+"/workshop.png")){app.exit(5);return;}flow.openProject(0);}
        else if(step==1){window->grabWindow().save(out+"/"+flow.project()["id"].toString()+".png");flow.closeDetail();flow.openProject(1);}
        else if(step==2){window->grabWindow().save(out+"/"+flow.project()["id"].toString()+".png");app.exit(warning?6:0);}
        ++step;
    });timer.start();QTimer::singleShot(30000,&app,[&]{app.exit(7);});return app.exec();
}
