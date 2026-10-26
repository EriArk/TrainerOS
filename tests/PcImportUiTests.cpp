#include <QtTest>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickView>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QImage>
#include <QSysInfo>
#include "features/pc/PcGamesController.h"
#include "features/settings/GamePerformance.h"
#include <QJsonArray>
#include "core/input/TextEntryController.h"
#include "platform/input/InvitationOverlayService.h"
using namespace trainer;
class UiLibrary : public LibraryRepository {
public:
    AdventureRegistration record;
    std::optional<AdventureRegistration> registration(const QString& id) const override {return id==record.adventure.id?std::optional(record):std::nullopt;}
    bool editable() const override {return true;}
    QList<World> worlds() const override {return {};}
    QList<Adventure> adventures() const override {return {};}
    QList<ResumePoint> resumePoints() const override {return {};}
    HomeSnapshot home() const override {return {};}
};
class PcImportUiTests : public QObject {
    Q_OBJECT
private slots:
    void perGamePerformanceAtTwoSizes() {
        QTemporaryDir temp;const QDir source(QString(TRAINER_SOURCE_DIR)+"/src/qml");
        for(const auto& name:source.entryList({"*.qml"},QDir::Files))QVERIFY(QFile::copy(source.filePath(name),temp.filePath(name)));
        QFile dir(temp.filePath("qmldir"));QVERIFY(dir.open(QIODevice::WriteOnly));dir.write("singleton Theme 1.0 Theme.qml\n");dir.close();
        QFile scene(temp.filePath("Probe.qml"));QVERIFY(scene.open(QIODevice::WriteOnly));scene.write("import QtQuick\nItem { width: 960; height: 540; property bool dark: false; onDarkChanged: Theme.themeId=dark ? 'dark' : 'light'; GamePerformancePopup { anchors.fill: parent; profile: probeProfile } }");scene.close();
        UiLibrary library;library.record.adventure.id="pc:power";library.record.adventure.title="An ordinary PC game";library.record.adventure.adapterId="pc-local";library.record.integrationConfig={{"runner","proton"}};
        GamePerformance profile(library);profile.configure(temp.path(),"owner");profile.begin("pc:power");
        profile.setCapabilities({{"profiles",QJsonArray{"Eco","Balanced","Performance"}},{"current","Balanced"},{"governors",QJsonArray{"conservative","schedutil","performance"}},{"gpuModes",QJsonArray{"auto","manual"}},{"gpuMin",200},{"gpuMax",1000}});
        QQuickView view;QStringList warnings;connect(view.engine(),&QQmlEngine::warnings,this,[&](const QList<QQmlError>& errors){for(const auto& e:errors)warnings<<e.toString();});
        view.rootContext()->setContextProperty("probeProfile",&profile);view.setResizeMode(QQuickView::SizeRootObjectToView);view.setSource(QUrl::fromLocalFile(scene.fileName()));
        QVERIFY2(view.status()==QQuickView::Ready,qPrintable(warnings.join('\n')));view.resize(960,540);view.show();QTest::qWait(180);
        QDir().mkpath("screenshots/game-performance");QVERIFY(view.grabWindow().save("screenshots/game-performance/light-960.png"));
        profile.dispatch(Action::Right);QCOMPARE(profile.draft()["power"].toString(),QString("Eco"));
        profile.dispatch(Action::Down);profile.dispatch(Action::Right);QCOMPARE(profile.draft()["gpu"].toInt(),200);
        QTest::qWait(50);
        std::function<QQuickItem*(QQuickItem*)> findSlider=[&](QQuickItem* item)->QQuickItem*{if(item->objectName()=="gpu-clock-slider"&&item->isVisible())return item;for(auto* child:item->childItems())if(auto* found=findSlider(child))return found;return nullptr;};
        auto* slider=findSlider(view.rootObject());QVERIFY(slider);
        const auto point=slider->mapToScene(QPointF(slider->width()*.8,slider->height()/2));
        QTest::mouseClick(&view,Qt::LeftButton,Qt::NoModifier,point.toPoint());QVERIFY(profile.draft()["gpu"].toInt()>=800);
        QTest::qWait(50);slider=findSlider(view.rootObject());QVERIFY(slider);QVERIFY(slider->parentItem()->property("clockRatio").toDouble()>.7);
        profile.save();profile.close();profile.begin("pc:power");QCOMPARE(profile.draft()["power"].toString(),QString("Eco"));
        view.rootObject()->setProperty("dark",true);view.resize(800,450);QTest::qWait(100);QVERIFY(view.grabWindow().save("screenshots/game-performance/dark-800.png"));
        profile.dispatch(Action::NextPage);QCOMPARE(profile.page(),1);QTest::qWait(100);QVERIFY(view.grabWindow().save("screenshots/game-performance/compatibility-800.png"));
        profile.dispatch(Action::Back);QVERIFY(!profile.isOpen());
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
    }
    void gameSettingsArtworkAndLaunchAtHandheldSize() {
        QTemporaryDir temp;const QDir source(QString(TRAINER_SOURCE_DIR)+"/src/qml");
        for(const auto& name:source.entryList({"*.qml"},QDir::Files))QVERIFY(QFile::copy(source.filePath(name),temp.filePath(name)));
        QFile dir(temp.filePath("qmldir"));QVERIFY(dir.open(QIODevice::WriteOnly));dir.write("singleton Theme 1.0 Theme.qml\n");dir.close();
        QFile scene(temp.filePath("Probe.qml"));QVERIFY(scene.open(QIODevice::WriteOnly));scene.write("import QtQuick\nItem { width: 960; height: 540; property bool dark: false; onDarkChanged: Theme.themeId=dark ? 'dark' : 'light'; PcGamePopup { anchors.fill: parent; flow: probeFlow } }");scene.close();
        UiLibrary library;library.record.adventure.id="pc:fixture";library.record.adventure.adapterId="pc-local";library.record.adventure.title="An ordinary PC game";
        library.record.integrationConfig={{"runner","native"},{"installRoot",temp.path()},{"workingDirectory",temp.path()}};
        FilesService files;PcGamesController flow(library,files);flow.configure(temp.filePath("state"),"ui");flow.edit("pc:fixture");
        QQuickView view;QStringList warnings;connect(view.engine(),&QQmlEngine::warnings,this,[&](const QList<QQmlError>& errors){for(const auto& e:errors)warnings<<e.toString();});
        view.rootContext()->setContextProperty("probeFlow",&flow);view.setResizeMode(QQuickView::SizeRootObjectToView);view.setSource(QUrl::fromLocalFile(scene.fileName()));
        QVERIFY2(view.status()==QQuickView::Ready,qPrintable(warnings.join('\n')));view.resize(960,540);view.show();QTest::qWait(150);
        QDir().mkpath("screenshots/pc-import");QVERIFY(view.grabWindow().save("screenshots/pc-import/settings-light.png"));
        view.rootObject()->setProperty("dark",true);view.resize(800,450);flow.activate(2);QTest::qWait(120);
        QVERIFY(view.grabWindow().save("screenshots/pc-import/artwork-dark-800.png"));
        flow.dispatch(Action::Back);flow.activate(0);QTest::qWait(120);QVERIFY(view.grabWindow().save("screenshots/pc-import/launch-dark-800.png"));
        flow.dispatch(Action::Down);QCOMPARE(flow.focusIndex(),1);flow.dispatch(Action::Back);QCOMPARE(flow.stage(),QString("settings"));
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
    }
    void externalKeyboardUsesExistingLayoutAndPointerPath() {
        QTemporaryDir temp;const QDir source(QString(TRAINER_SOURCE_DIR)+"/src/qml");
        for(const auto& name:source.entryList({"*.qml"},QDir::Files))QVERIFY(QFile::copy(source.filePath(name),temp.filePath(name)));
        QFile dir(temp.filePath("qmldir"));QVERIFY(dir.open(QIODevice::WriteOnly));dir.write("singleton Theme 1.0 Theme.qml\n");dir.close();
        TextEntryController keyboard;keyboard.configure(temp.path(),"keyboard-ui");keyboard.beginLive(0,"Keyboard");
        InvitationOverlayService bridge(false,{});QQmlEngine engine;QStringList warnings;
        connect(&engine,&QQmlEngine::warnings,this,[&](const QList<QQmlError>& errors){for(const auto& error:errors)warnings<<error.toString();});
        QQmlComponent component(&engine,QUrl::fromLocalFile(temp.filePath("ExternalKeyboardWindow.qml")));
        std::unique_ptr<QObject> object(component.createWithInitialProperties({{"keyboard",QVariant::fromValue(&keyboard)},{"bridge",QVariant::fromValue(&bridge)}}));
        QVERIFY2(object,qPrintable(component.errorString()));auto* window=qobject_cast<QQuickWindow*>(object.get());QVERIFY(window);
        window->resize(960,540);QTest::qWait(200);QVERIFY(window->isVisible());
        std::function<QQuickItem*(QQuickItem*)> find=[&](QQuickItem* item)->QQuickItem* {
            if(item->objectName()=="key-Q")return item;
            for(auto* child:item->childItems())if(auto* found=find(child))return found;
            return nullptr;
        };
        auto* key=find(window->contentItem());QVERIFY(key);
        const auto point=key->mapToScene(QPointF(key->width()/2,key->height()/2));
        QSignalSpy text(&keyboard,&TextEntryController::textCommitted);QSignalSpy stroke(&keyboard,&TextEntryController::keyStroke);
        bridge.forwardPointer("press",point);bridge.forwardPointer("release",point);
        QCOMPARE(text.size()+stroke.size(),1);
        QDir().mkpath("screenshots/pc-import");QVERIFY(window->grabWindow().save("screenshots/pc-import/keyboard-touch.png"));
        keyboard.setLayout("pc");window->resize(800,450);QTest::qWait(100);
        QVERIFY(window->grabWindow().save("screenshots/pc-import/keyboard-pc-800.png"));
        keyboard.dispatch(Action::Back);QVERIFY(!window->isVisible());
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
    }
    void compactChooserAtTwoSizes() {
        QTemporaryDir temp;
        const QDir source(QString(TRAINER_SOURCE_DIR)+"/src/qml");
        for(const auto& name:source.entryList({"*.qml"},QDir::Files))QVERIFY(QFile::copy(source.filePath(name),temp.filePath(name)));
        QFile dir(temp.filePath("qmldir"));QVERIFY(dir.open(QIODevice::WriteOnly));dir.write("singleton Theme 1.0 Theme.qml\n");dir.close();
        QFile scene(temp.filePath("Probe.qml"));QVERIFY(scene.open(QIODevice::WriteOnly));
        scene.write("import QtQuick\nItem { width: 960; height: 540; property bool dark: false; onDarkChanged: Theme.themeId=dark ? 'dark' : 'light'; PcGamePopup { anchors.fill: parent; flow: probeFlow } }");scene.close();
        UiLibrary library;FilesService files;LocalFileCatalog catalog;PcGamesController flow(library,files);
        flow.setCatalog(&catalog);flow.configure(temp.filePath("state"),"ui-probe");flow.begin();
        QQuickView view;QStringList warnings;
        connect(view.engine(),&QQmlEngine::warnings,this,[&](const QList<QQmlError>& errors){for(const auto& e:errors)warnings<<e.toString();});
        view.rootContext()->setContextProperty("probeFlow",&flow);
        view.setResizeMode(QQuickView::SizeRootObjectToView);view.setSource(QUrl::fromLocalFile(scene.fileName()));
        QVERIFY2(view.status()==QQuickView::Ready,qPrintable(warnings.join('\n')));view.resize(960,540);view.show();QTest::qWait(250);
        QDir().mkpath("screenshots/pc-import");
        QVERIFY(view.grabWindow().save("screenshots/pc-import/choices-light.png"));
        flow.dispatch(Action::Down);QCOMPARE(flow.focusIndex(),1);flow.dispatch(Action::Confirm);QCOMPARE(flow.stage(),QString("ready-source"));
        view.rootObject()->setProperty("dark",true);view.resize(800,450);QTest::qWait(150);
        QVERIFY(view.grabWindow().save("screenshots/pc-import/ready-dark-800.png"));
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join('\n')));
    }
};
QTEST_MAIN(PcImportUiTests)
#include "PcImportUiTests.moc"
