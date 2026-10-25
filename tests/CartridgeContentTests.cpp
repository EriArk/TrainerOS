#include "integrations/adventure/CartridgeContent.h"
#include "CartridgeArchiveFixture.h"
#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
using namespace trainer;
class CartridgeContentTests final:public QObject {
    Q_OBJECT
    static bool write(const QString& p,const QByteArray& b){QFile f(p);return f.open(QIODevice::WriteOnly)&&f.write(b)==b.size();}
private slots:
    void rawAndPackedIdentityAndStaging_data() {
        QTest::addColumn<QString>("platform");QTest::addColumn<QString>("extension");QTest::addColumn<QString>("format");
        for(const auto& p:QStringList{"gb","gbc","gba","n64"})for(const auto& f:QStringList{"zip","7z"})
            QTest::newRow(qPrintable(p+'-'+f))<<p<<(p=="n64"?QString("v64"):p)<<f;
    }
    void rawAndPackedIdentityAndStaging() {
        QFETCH(QString,platform);QFETCH(QString,extension);QFETCH(QString,format);
        QTemporaryDir dir;std::atomic_bool cancel=false;QByteArray rom(32768,'r');
        const auto raw=dir.filePath("raw."+extension),packed=dir.filePath("Save Name."+format);
        const auto bytes=cartridgeArchive(format,{{"docs/README.txt","Original test data"},{("nested/Other."+extension.toUpper()).toUtf8(),rom}});
        QVERIFY(!bytes.isEmpty());QVERIFY(write(raw,rom));QVERIFY(write(packed,bytes));
        const auto plain=cartridge::read(raw,platform,cancel),image=cartridge::read(packed,platform,cancel);
        QCOMPARE(image.bytes,rom);QCOMPARE(image.extension,extension);QCOMPARE(image.digest(),plain.digest());
        const auto stage=cartridge::stage(image,packed,dir.filePath("private"),plain.digest());
        QCOMPARE(QFileInfo(stage).fileName(),"Save Name."+extension);
        QFile staged(stage);QVERIFY(staged.open(QIODevice::ReadOnly));QCOMPARE(staged.readAll(),rom);staged.close();
        QVERIFY(cartridge::stage(image,packed,dir.filePath("private"),plain.digest()).isEmpty());
        QVERIFY(cartridge::stage(image,packed,dir.filePath("wrong"),QString(64,'a')).isEmpty());
        cancel=true;QVERIFY(cartridge::read(packed,platform,cancel).bytes.isEmpty());
    }
    void malformedAndAmbiguousArchives_data() {
        QTest::addColumn<QString>("format");QTest::newRow("zip")<<QString("zip");QTest::newRow("7z")<<QString("7z");
    }
    void malformedAndAmbiguousArchives() {
        QFETCH(QString,format);QTemporaryDir dir;std::atomic_bool cancel=false;
        const auto path=dir.filePath("game."+format);const QByteArray rom(32768,'r');
        for(const auto& files:QList<QList<QPair<QByteArray,QByteArray>>>{
                {{"one.gb",rom},{"two.gb",rom}},{{"one.gb",rom},{"one.gb",rom}},
                {{"../escape.gb",rom}},{{"/absolute.gb",rom}},{{"C:\\drive.gb",rom}},
                {{"wrong.gba",rom}},{{"empty.gb",{}}},{{"one.gb",QByteArray(9*1024*1024,'r')}}}) {
            const auto archive=cartridgeArchive(format,files);QVERIFY(!archive.isEmpty());QVERIFY(write(path,archive));
            QVERIFY(cartridge::read(path,"gb",cancel).bytes.isEmpty());
        }
        const auto archive=cartridgeArchive(format,{{"one.gb",rom}});
        QVERIFY(write(path,archive.left(archive.size()/2)));QVERIFY(cartridge::read(path,"gb",cancel).bytes.isEmpty());
        QVERIFY(write(path,rom));QVERIFY(cartridge::read(path,"gb",cancel).bytes.isEmpty());
        QVERIFY(!QFileInfo::exists(dir.filePath("escape.gb")));
    }
};
QTEST_GUILESS_MAIN(CartridgeContentTests)
#include "CartridgeContentTests.moc"
