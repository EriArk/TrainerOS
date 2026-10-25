#include "features/browser/BrowserController.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "platform/files/FilesService.h"
using namespace trainer;
class BrowserTests:public QObject {
    Q_OBJECT
private slots:
    void completedDownloadMovesWithoutReplacingExistingFile() {
        FilesService files;if(!files.available())QSKIP("Linux libfm backend required");
        QTemporaryDir temp;QDir().mkpath(temp.path()+"/target");
        const auto source=temp.path()+"/one.txt",target=temp.path()+"/target/one.txt";
        const auto write=[](QString path,QByteArray text){QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(text)==text.size();};
        QVERIFY(write(source,"download"));QVERIFY(write(target,"keep"));QString state;
        {BrowserController b;b.configure(temp.path(),"test");state=b.storagePath();}
        QVERIFY(write(state+"/downloads.json",QJsonDocument(QJsonArray{QJsonObject{{"id","one"},{"name","one.txt"},{"path",source},{"state","done"}}}).toJson()));
        {BrowserController b;b.configure(temp.path(),"test");b.setFilesService(&files);b.setDownloadDestination("one",temp.path()+"/target");
            QTRY_COMPARE(b.downloads()[0].toMap()["state"].toString(),"done");
            QTRY_VERIFY(!QFile::exists(source));QTRY_VERIFY(b.downloads()[0].toMap()["path"].toString().endsWith("one (1).txt"));
        }
        QFile original(target);QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),QByteArray("keep"));
        BrowserController restored;restored.configure(temp.path(),"test");QVERIFY(restored.downloads()[0].toMap()["path"].toString().endsWith("one (1).txt"));
    }
    void interruptedDownloadRetainsChosenFolder() {
        QTemporaryDir temp;QString state;
        {BrowserController b;b.configure(temp.path(),"test");state=b.storagePath();}
        QFile f(state+"/downloads.json");QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(QJsonArray{QJsonObject{{"id","one"},{"name","one.txt"},{"path",temp.path()+"/one.txt"},{"state","queued"},{"destination",temp.path()+"/target"}}}).toJson());f.close();
        BrowserController b;b.configure(temp.path(),"test");QCOMPARE(b.downloads()[0].toMap()["destination"].toString(),temp.path()+"/target");
        QCOMPARE(b.downloads()[0].toMap()["state"].toString(),"error");QVERIFY(b.downloads()[0].toMap()["terminal"].toBool());
    }
    void navigationBoundary(){
        QCOMPARE(BrowserController::navigationUrl("example.com"),QString("https://example.com"));
        QCOMPARE(BrowserController::navigationUrl("https://example.com/path?q=a"),QString("https://example.com/path?q=a"));
        QVERIFY(BrowserController::navigationUrl("two words").contains("q=two%20words"));
        for(const auto& value:{"javascript:alert(1)","file:///etc/passwd","https://name:secret@example.com/","data:text/html,test"})QVERIFY2(BrowserController::navigationUrl(value).isEmpty(),value);
    }
    void sessionAndOwnerIsolation(){
        QTemporaryDir directory;QString first,selected;
        {BrowserController browser;browser.configure(directory.path(),"one");first=browser.storagePath();browser.newTab("https://example.com/");selected=browser.selected();browser.updateTab(selected,"https://example.com/","Example","",true);browser.toggleBookmark("https://example.com/","Example");}
        {BrowserController browser;browser.configure(directory.path(),"one");QCOMPARE(browser.tabs().size(),2);QVERIFY(browser.selected()!=selected);QCOMPARE(browser.tabs().first().toMap()["url"].toString(),QString("about:blank"));QCOMPARE(browser.bookmarks().size(),1);QCOMPARE(browser.history().size(),1);
            browser.enter();QVERIFY(!browser.profile()); // Dashboard never starts Chromium.
            browser.configure(directory.path(),"two");QVERIFY(browser.storagePath()!=first);QCOMPARE(browser.tabs().size(),1);QVERIFY(browser.bookmarks().isEmpty());QVERIFY(browser.history().isEmpty());
            browser.configure(directory.path(),"one");QVERIFY(browser.selected()!=selected);QCOMPARE(browser.tabs().size(),2);QCOMPARE(browser.bookmarks().size(),1);}
    }
    void lastTabAndBookmarkToggle(){BrowserController browser;browser.newTab();auto id=browser.selected();browser.closeTab(id);QCOMPARE(browser.tabs().size(),1);QVERIFY(browser.selected()!=id);browser.toggleBookmark("https://example.com/","Example");browser.toggleBookmark("https://example.com/","Example");QVERIFY(browser.bookmarks().isEmpty());}
    void historyDeduplicatesAndUnsafeRestore(){QTemporaryDir directory;{BrowserController browser;browser.configure(directory.path(),"one");auto id=browser.selected();browser.updateTab(id,"https://example.com/","First","",true);browser.updateTab(id,"https://example.com/","Second","",true);QCOMPARE(browser.history().size(),1);browser.updateTab(id,"blob:https://example.com/private","Blob","",false);}BrowserController restored;restored.configure(directory.path(),"one");QCOMPARE(restored.tabs().first().toMap()["url"].toString(),QString("about:blank"));}
    void localActionsAreSeparate(){BrowserController browser;QSignalSpy commands(&browser,&BrowserController::commandRequested);for(const auto action:{Action::PreviousFace,Action::NextFace,Action::Secondary,Action::ToggleContinue,Action::SystemMenu})browser.dispatch(action);QCOMPARE(commands.size(),0);browser.dispatch(Action::LocalAction);QCOMPARE(commands.takeFirst().first().toString(),QString("chrome"));browser.dispatch(Action::ContextMenu);QCOMPARE(commands.takeFirst().first().toString(),QString("context"));}
};
QTEST_GUILESS_MAIN(BrowserTests)
#include "BrowserTests.moc"
