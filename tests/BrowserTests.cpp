#include "features/browser/BrowserController.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
using namespace trainer;
class BrowserTests:public QObject {
    Q_OBJECT
private slots:
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
