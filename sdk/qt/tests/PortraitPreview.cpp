// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QImage>
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);if(argc<3)return 2;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("fixture",QVariantMap{{"source",QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1]))}});
    engine.loadData(R"(import QtQuick
import QtQuick.Window
Window {width:500;height:260;visible:true;color:"#f4f0e6"
PlayerCard {x:20;y:28;width:300;height:180;descriptor:fixture;label:"YOU";detail:"MINT";current:true}
})",QUrl::fromLocalFile(QString(PORTRAIT_QML)+"Preview.qml"));
    if(engine.rootObjects().isEmpty())return 3;
    auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    QTimer::singleShot(6000,&app,[&]{
        auto* renderer=window->findChild<QObject*>("portraitRenderer");
        bool stopped=renderer&&!renderer->property("active").toBool();
        app.exit(window->grabWindow().save(QString::fromLocal8Bit(argv[2]))&&stopped?0:4);
    });
    return app.exec();
}
