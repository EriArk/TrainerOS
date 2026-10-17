// Run as an ordinary user in the device compositor, never with --no-sandbox.
#include "features/browser/BrowserController.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
int main(int argc,char** argv){
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QtWebEngineQuick::initialize();QGuiApplication app(argc,argv);
    if(qEnvironmentVariable("PROBE_VULKAN")=="1")QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    trainer::BrowserController browser;browser.configure(qEnvironmentVariable("PROBE_DATA"),"runtime-probe");browser.enter();
    QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("browser",&browser);
    engine.rootContext()->setContextProperty("probeUrl",qEnvironmentVariable("PROBE_URL"));
    if(qEnvironmentVariable("PROBE_MODE")=="lifecycle") {
        engine.rootContext()->setContextProperty("Theme",QVariantMap{{"uiScale",1.0},{"paper","#fffdf5"}});
        engine.rootContext()->setContextProperty("engineSource",QUrl::fromLocalFile(qEnvironmentVariable("PROBE_ENGINE")));
        QObject::connect(&engine,&QQmlEngine::exit,&app,[&](int code){
            QFile receipt(qEnvironmentVariable("PROBE_DATA")+"/lifecycle.json");
            if(receipt.open(QIODevice::WriteOnly))receipt.write(QJsonDocument(QJsonObject{{"passed",code==0},{"exit",code}}).toJson());
            app.exit(code);
        });
        engine.load(QUrl::fromLocalFile(qEnvironmentVariable("PROBE_SCENARIO")));
        if(engine.rootObjects().isEmpty())return 2;
        QTimer::singleShot(35000,&app,[&]{qCritical("LIFECYCLE_TIMEOUT");app.exit(3);});
        return app.exec();
    }
    engine.loadData(R"QML(import QtQuick
import QtWebEngine
Window {
    width:960;height:540;visible:true;title:"TrainerOS Browser probe"
    WebEngineView {id:web;objectName:"web";anchors.fill:parent;profile:browser.profile;url:probeUrl
        onLoadingChanged:function(info){console.log("BROWSER_LOAD",info.status,title);if(info.status===WebEngineView.LoadSucceededStatus)console.log("BROWSER_RENDERER",renderProcessPid)}
    }
})QML");
    if(engine.rootObjects().isEmpty())return 2;
    const auto mode=qEnvironmentVariable("PROBE_MODE");
    const auto checkpoint=[&](const char* name){QFile f(qEnvironmentVariable("PROBE_DATA")+"/"+name+".json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument::fromVariant(browser.downloads()).toJson());};
    if(mode=="close-tab")QTimer::singleShot(2000,&app,[&]{auto* web=engine.rootObjects().first()->findChild<QObject*>("web");if(web){web->deleteLater();qInfo("PROBE_TAB_CLOSED");}});
    if(mode=="pause") {
        QTimer::singleShot(1500,&app,[&]{browser.controlDownload({},"pause-all");checkpoint("paused");});
        QTimer::singleShot(3500,&app,[&]{checkpoint("still-paused");browser.controlDownload({},"resume-all");});
    }
    if(mode=="cancel")QTimer::singleShot(1500,&app,[&]{browser.controlDownload({},"cancel-all");qInfo("PROBE_CANCELLED");});
    QTimer::singleShot(25000,&app,[&]{QFile f(qEnvironmentVariable("PROBE_DATA")+"/jobs.json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument::fromVariant(browser.downloads()).toJson());qobject_cast<QQuickWindow*>(engine.rootObjects().first())->grabWindow().save(qEnvironmentVariable("PROBE_DATA")+"/probe.png");app.quit();});
    return app.exec();
}
