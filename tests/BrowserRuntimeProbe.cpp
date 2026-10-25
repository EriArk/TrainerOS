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
#include <QDir>
#include <QElapsedTimer>
#include <QDebug>
#include <QTemporaryDir>
#include "core/input/TextEntryController.h"
#include "core/input/ControllerInput.h"
#include "platform/files/FilesService.h"
int main(int argc,char** argv){
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QtWebEngineQuick::initialize();QGuiApplication app(argc,argv);
    if(qEnvironmentVariable("PROBE_VULKAN")=="1")QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    trainer::FilesService files;
    trainer::BrowserController browser;browser.configure(qEnvironmentVariable("PROBE_DATA"),"runtime-probe");browser.setFilesService(&files);browser.enter();browser.ensureEngine();
    QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("browser",&browser);
    engine.rootContext()->setContextProperty("probeUrl",qEnvironmentVariable("PROBE_URL"));
    engine.rootContext()->setContextProperty("probeData",qEnvironmentVariable("PROBE_DATA"));
    if(qEnvironmentVariable("PROBE_MODE")=="input") {
        trainer::TextEntryController keyboard;
        trainer::ControllerInput input;app.installEventFilter(&input);
        QObject::connect(&browser,&trainer::BrowserController::pageInputRequested,&keyboard,&trainer::TextEntryController::beginLive);
        QObject::connect(&browser,&trainer::BrowserController::pageInputEnded,&keyboard,&trainer::TextEntryController::cancel);
        QObject::connect(&keyboard,&trainer::TextEntryController::textCommitted,&browser,&trainer::BrowserController::commitPageText);
        QObject::connect(&keyboard,&trainer::TextEntryController::keyRequested,&browser,&trainer::BrowserController::pageKey);
        QObject::connect(&keyboard,&trainer::TextEntryController::keyStroke,&browser,&trainer::BrowserController::pageKeyStroke);
        QObject::connect(&keyboard,&trainer::TextEntryController::changed,&browser,[&]{input.setTextEntryEnabled(keyboard.isOpen());if(!keyboard.isOpen())browser.stopPageInput();});
        // Load the production components with their real singleton registration.
        QTemporaryDir qml;
        QDir source(QFileInfo(qEnvironmentVariable("PROBE_KEYBOARD")).absolutePath());
        for(const auto& name:source.entryList({"*.qml"},QDir::Files))QFile::copy(source.filePath(name),qml.filePath(name));
        QFile qmldir(qml.filePath("qmldir"));if(!qmldir.open(QIODevice::WriteOnly))return 8;
        qmldir.write("singleton Theme 1.0 Theme.qml\n");qmldir.close();
        engine.rootContext()->setContextProperty("keyboard",&keyboard);
        engine.rootContext()->setContextProperty("controllerInput",&input);
        engine.rootContext()->setContextProperty("engineSource",QUrl::fromLocalFile(qml.filePath("BrowserEngine.qml")));
        engine.rootContext()->setContextProperty("keyboardSource",QUrl::fromLocalFile(qml.filePath("KeyboardPanel.qml")));
        bool passed=false;
        QObject::connect(&engine,&QQmlEngine::exit,&app,[&](int code){
            passed=code==0&&!engine.rootObjects().isEmpty()&&engine.rootObjects().first()->property("passed").toBool();
            QFile receipt(qEnvironmentVariable("PROBE_DATA")+"/input.json");
            if(receipt.open(QIODevice::WriteOnly))receipt.write(QJsonDocument(QJsonObject{{"passed",passed},{"stage",engine.rootObjects().first()->property("stage").toInt()}}).toJson());
            if(passed)qInfo("INPUT_PASSED");app.exit(passed?0:2);
        });
        engine.load(QUrl::fromLocalFile(qEnvironmentVariable("PROBE_SCENARIO")));
        if(engine.rootObjects().isEmpty())return 2;
        QTimer::singleShot(40000,&app,[&]{qCritical("INPUT_TIMEOUT");app.exit(3);});
        const int result=app.exec();
        qDeleteAll(engine.rootObjects());
        return passed?result:3;
    }
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
    property string scriptResult: "waiting"
    function queueDownloads() {
        web.runJavaScript("['one','two','three'].forEach((name,i)=>setTimeout(()=>{let a=document.createElement('a');a.href='queue-'+name+'.bin';a.download='queue-'+name+'.bin';document.body.appendChild(a);a.click();},i*300));document.readyState",r=>scriptResult=String(r))
    }
    WebEngineView {id:web;objectName:"web";anchors.fill:parent;profile:browser.profile;url:probeUrl
        onLoadingChanged:function(info){console.log("BROWSER_LOAD",info.status,title);if(info.status===WebEngineView.LoadSucceededStatus)console.log("BROWSER_RENDERER",renderProcessPid)}
    }
})QML");
    if(engine.rootObjects().isEmpty())return 2;
    const auto mode=qEnvironmentVariable("PROBE_MODE");
    const auto checkpoint=[&](const char* name){QFile f(qEnvironmentVariable("PROBE_DATA")+"/"+name+".json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument::fromVariant(browser.downloads()).toJson());};
    if(mode=="queue") {
        const auto data=qEnvironmentVariable("PROBE_DATA");QDir().mkpath(data+"/target");QDir().mkpath(data+"/alternate");
        QFile existing(data+"/target/queue-one.bin");if(!existing.open(QIODevice::WriteOnly))return 5;existing.write("keep existing");existing.close();
        int step=0,stable=0;QString one,two,three;qint64 pausedBytes=-1;
        QElapsedTimer elapsed;elapsed.start();
        QTimer timer;timer.setInterval(250);
        auto fail=[&](QString reason){qCritical()<<reason;checkpoint("queue-failed");
            auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
            qCritical()<<"window"<<window->isVisible()<<window->isExposed();window->grabWindow().save(data+"/queue-failed.png");
            qCritical()<<"script"<<window->property("scriptResult");
            if(auto* web=window->findChild<QObject*>("web"))qCritical()<<"web"<<web->property("url")<<web->property("loading")<<web->property("title")<<web->property("renderProcessPid");
            app.exit(6);};
        QObject::connect(&timer,&QTimer::timeout,&app,[&]{
            if(elapsed.elapsed()>65000){fail("QUEUE_TIMEOUT step "+QString::number(step));return;}
            const auto tasks=browser.downloads();
            auto row=[&](QString id){for(const auto& v:browser.downloads())if(v.toMap()["id"]==id)return v.toMap();return QVariantMap{};};
            if(step==0&&elapsed.elapsed()>1000){
                auto* web=engine.rootObjects().first()->findChild<QObject*>("web");
                if(!web||web->property("loading").toBool()||web->property("title").toString()!="Download queue test")return;
                if(!QMetaObject::invokeMethod(engine.rootObjects().first(),"queueDownloads")){fail("Download invocation failed");return;}
                ++step;
            }else if(step==1&&tasks.size()==3){
                for(const auto& v:tasks){auto r=v.toMap();if(r["title"]=="queue-one.bin")one=r["id"].toString();if(r["title"]=="queue-two.bin")two=r["id"].toString();if(r["title"]=="queue-three.bin")three=r["id"].toString();}
                if(row(one)["state"]!="running"||row(two)["state"]!="queued"||row(three)["state"]!="queued"){fail("Downloads did not form a serial queue");return;}
                browser.setDownloadDestination(two,data+"/target");browser.setDownloadDestination(one,data+"/alternate");browser.setDownloadDestination(three,data+"/target");
                if(row(two)["destination"]!=data+"/target"||row(one)["destination"]!=data+"/alternate"){fail("Queued/running destination not retained");return;}
                browser.controlDownload(three,"first");++step;
            }else if(step==2&&row(three)["state"]=="running"&&row(three)["received"].toLongLong()>0){
                if(row(one)["state"]!="queued"){fail("Promotion did not yield previous transfer");return;}
                browser.controlDownload(three,"pause");++step;
            }else if(step==3&&row(three)["state"]=="paused"){
                const auto received=row(three)["received"].toLongLong();if(pausedBytes==received)++stable;else stable=0;pausedBytes=received;
                if(stable<4)return;
                browser.controlDownload({},"pause-all");browser.moveDownloadBefore(two,one);
                int a=-1,b=-1;const auto reordered=browser.downloads();for(int i=0;i<reordered.size();++i){if(reordered[i].toMap()["id"]==two)a=i;if(reordered[i].toMap()["id"]==one)b=i;}
                if(a<0||b<0||a>=b){fail("Provider reorder did not persist");return;}
                browser.controlDownload(two,"cancel");browser.controlDownload(three,"first");browser.controlDownload(one,"resume");
                browser.setDownloadDestination(one,data+"/target");
                if(auto* web=engine.rootObjects().first()->findChild<QObject*>("web"))web->deleteLater();
                checkpoint("queue-resumed");++step;
            }else if(step==4&&row(one)["state"]=="done"&&row(three)["state"]=="done"){
                if(row(two)["state"]!="cancelled"||!row(one)["path"].toString().startsWith(data+"/target/")||!row(three)["path"].toString().startsWith(data+"/target/"))return;
                QFile original(data+"/target/queue-one.bin");if(!original.open(QIODevice::ReadOnly)||original.readAll()!="keep existing"){fail("Existing destination overwritten");return;}
                for(const auto& id:{one,three}){QFile f(row(id)["path"].toString());if(!f.open(QIODevice::ReadOnly)||f.size()!=8*1024*1024||f.readAll()!=QByteArray(8*1024*1024,'q')){fail("Transferred content mismatch");return;}}
                checkpoint("queue-passed");qInfo("QUEUE_PASSED priority pause reorder cancel destination move collision close-tab");app.exit(0);
            }
        });timer.start();return app.exec();
    }
    if(mode=="close-tab")QTimer::singleShot(2000,&app,[&]{auto* web=engine.rootObjects().first()->findChild<QObject*>("web");if(web){web->deleteLater();qInfo("PROBE_TAB_CLOSED");}});
    if(mode=="pause") {
        QTimer::singleShot(1500,&app,[&]{browser.controlDownload({},"pause-all");checkpoint("paused");});
        QTimer::singleShot(3500,&app,[&]{checkpoint("still-paused");browser.controlDownload({},"resume-all");});
    }
    if(mode=="cancel")QTimer::singleShot(1500,&app,[&]{browser.controlDownload({},"cancel-all");qInfo("PROBE_CANCELLED");});
    QTimer::singleShot(25000,&app,[&]{QFile f(qEnvironmentVariable("PROBE_DATA")+"/jobs.json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument::fromVariant(browser.downloads()).toJson());qobject_cast<QQuickWindow*>(engine.rootObjects().first())->grabWindow().save(qEnvironmentVariable("PROBE_DATA")+"/probe.png");app.quit();});
    return app.exec();
}
