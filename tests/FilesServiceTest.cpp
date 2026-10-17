#include "platform/files/FilesService.h"
#include "features/files/FilesController.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QUrl>
using namespace trainer;
namespace {
QString uri(const QString& p){return QUrl::fromLocalFile(p).toString();}
void write(const QString& p,const QByteArray& data){QFile f(p);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(data),data.size());}
QByteArray read(const QString& p){QFile f(p);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
QVariantMap last(FilesService& service){return service.tasks().last().toMap();}
}
class FilesServiceTest:public QObject {
    Q_OBJECT
private Q_SLOTS:
    void listingAndLateResults(){
        QTemporaryDir dir;QVERIFY(dir.isValid());QDir().mkdir(dir.path()+"/empty");write(dir.path()+"/имя.txt","hello");
        FilesService service;QSignalSpy spy(&service,&FilesService::listed);service.list(uri(dir.path()));service.list(uri(dir.path()+"/empty"));
        QTRY_VERIFY(!spy.empty());QCOMPARE(spy.last()[0].toString(),uri(dir.path()+"/empty"));QVERIFY(spy.last()[1].toList().isEmpty());QCOMPARE(spy.size(),1);
    }
    void copyAndMove(){
        FilesService service;if(!service.available())QSKIP("Linux libfm backend required");
        QTemporaryDir dir;QDir().mkdir(dir.path()+"/target");write(dir.path()+"/one.txt","contents");
        service.start("copy",{uri(dir.path()+"/one.txt")},uri(dir.path()+"/target"));QTRY_VERIFY(last(service)["terminal"].toBool());QCOMPARE(last(service)["state"].toString(),"done");QCOMPARE(read(dir.path()+"/target/one.txt"),QByteArray("contents"));
        service.start("rename",{uri(dir.path()+"/target/one.txt")},uri(dir.path()+"/target/renamed.txt"));QTRY_VERIFY(last(service)["terminal"].toBool());QVERIFY(!QFile::exists(dir.path()+"/target/one.txt"));QCOMPARE(read(dir.path()+"/target/renamed.txt"),QByteArray("contents"));
    }
    void collision_data(){QTest::addColumn<QString>("answer");QTest::newRow("skip")<<"skip";QTest::newRow("keep")<<"keep";QTest::newRow("replace")<<"replace";QTest::newRow("cancel")<<"cancel";}
    void collision(){
        QFETCH(QString,answer);FilesService service;if(!service.available())QSKIP("Linux libfm backend required");
        QTemporaryDir dir;QDir().mkdir(dir.path()+"/target");write(dir.path()+"/one.txt","new");write(dir.path()+"/target/one.txt","original");
        service.start("copy",{uri(dir.path()+"/one.txt")},uri(dir.path()+"/target"));QTRY_VERIFY(!last(service)["question"].toString().isEmpty());
        QCOMPARE(read(dir.path()+"/target/one.txt"),QByteArray("original"));service.answer(last(service)["id"].toString(),answer);QTRY_VERIFY(last(service)["terminal"].toBool());
        QCOMPARE(read(dir.path()+"/target/one.txt"),answer=="replace"?QByteArray("new"):QByteArray("original"));
        if(answer=="keep")QCOMPARE(read(dir.path()+"/target/one (2).txt"),QByteArray("new"));
        if(answer=="cancel")QCOMPARE(last(service)["state"].toString(),"cancelled");
    }
    void errorDoesNotSucceed(){
        FilesService service;if(!service.available())QSKIP("Linux libfm backend required");QTemporaryDir dir;
        service.start("copy",{uri(dir.path()+"/missing")},uri(dir.path()));QTRY_VERIFY(last(service)["terminal"].toBool());QVERIFY(last(service)["state"]!="done");QVERIFY(!last(service)["subtitle"].toString().isEmpty());
    }
    void closeWithPendingConflict(){
        QTemporaryDir dir;QDir().mkdir(dir.path()+"/target");write(dir.path()+"/one.txt","new");write(dir.path()+"/target/one.txt","original");
        auto service=std::make_unique<FilesService>();if(!service->available())QSKIP("Linux libfm backend required");
        service->start("copy",{uri(dir.path()+"/one.txt")},uri(dir.path()+"/target"));QTRY_VERIFY(!last(*service)["question"].toString().isEmpty());service.reset();QCOMPARE(read(dir.path()+"/target/one.txt"),QByteArray("original"));
    }
    void answeredConflictStaysClosed(){
        FilesController flow;if(!flow.service()->available())QSKIP("Linux libfm backend required");
        QTemporaryDir dir;QDir().mkdir(dir.path()+"/target");write(dir.path()+"/one.txt","new");write(dir.path()+"/target/one.txt","original");
        flow.service()->start("copy",{uri(dir.path()+"/one.txt")},uri(dir.path()+"/target"));QTRY_VERIFY(flow.modal());flow.answer(0);QVERIFY(!flow.modal());
        QTRY_VERIFY(last(*flow.service())["terminal"].toBool());QVERIFY(!flow.modal());QCOMPARE(read(dir.path()+"/target/one.txt"),QByteArray("original"));
    }
    void destructionDoesNotNotifyPartlyDestroyedOwners(){
        int notifications=0;auto service=std::make_unique<FilesService>();QObject receiver;
        QObject::connect(service.get(),&FilesService::changed,&receiver,[&]{++notifications;});
        service.reset();QCOMPARE(notifications,0);
    }
    void deleteNeedsConfirmationAndBadNamesCannotEscape(){
        QTemporaryDir dir;write(dir.path()+"/one.txt","original");FilesController flow;flow.navigate(uri(dir.path()));QTRY_COMPARE(flow.rows().size(),1);flow.context(0);flow.command("delete");QVERIFY(flow.modal());flow.answer(0);QCOMPARE(read(dir.path()+"/one.txt"),QByteArray("original"));
        flow.command("new");flow.applyText("../escape");QVERIFY(!flow.view()["error"].toString().isEmpty());QVERIFY(flow.service()->tasks().isEmpty());
    }
};
QTEST_GUILESS_MAIN(FilesServiceTest)
#include "FilesServiceTest.moc"
