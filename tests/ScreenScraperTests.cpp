#include "integrations/scraper/ScreenScraper.h"
#include "core/repository/BatoceraLibrary.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QImage>
#include <QBuffer>
using namespace trainer;
using namespace trainer::scraper;
class ScreenScraperTests:public QObject {
    Q_OBJECT
    static void put(const QString& path,const QByteArray& bytes) {QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(bytes),bytes.size());}
    static QByteArray read(const QString& path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
    static Cancellation flag(){return std::make_shared<std::atomic_bool>(false);}
    static QJsonObject game(){return {{"id","42"},{"systeme",QJsonObject{{"id","12"}}},{"noms",QJsonArray{QJsonObject{{"region","us"},{"text","Fixture"}}}},
        {"synopsis",QJsonArray{QJsonObject{{"langue","fr"},{"text","French"}},QJsonObject{{"langue","en"},{"text","English description"}}}},
        {"dates",QJsonArray{QJsonObject{{"region","us"},{"text","2004-01-02"}}}},
        {"medias",QJsonArray{QJsonObject{{"type","ss"},{"region","wor"},{"url","https://www.screenscraper.fr/image.png"}},
            QJsonObject{{"type","video"},{"region","us"},{"url","https://untrusted.test/video.mp4"}}}}};}
    static Reply response(const QJsonObject& value){return {200,QJsonDocument(QJsonObject{{"response",value}}).toJson(),0};}
private slots:
    void mediaValidationAndIdempotentStorage() {
        QTemporaryDir dir;QImage image(12,8,QImage::Format_RGB32);image.fill(Qt::blue);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);QVERIFY(image.save(&buffer,"PNG"));
        const auto path=storeMedia(dir.path(),"42",bytes,false,flag());QVERIFY(!path.isEmpty());QCOMPARE(read(path),bytes);
        QCOMPARE(storeMedia(dir.path(),"42",bytes,false,flag()),path);
        QVERIFY(storeMedia(dir.path(),"../42",bytes,false,flag()).isEmpty());
        QVERIFY(storeMedia(dir.path(),"42","<html>Error</html>",false,flag()).isEmpty());
        QVERIFY(storeMedia(dir.path(),"42",bytes,true,flag()).isEmpty());
        auto cancel=flag();cancel->store(true);QVERIFY(storeMedia(dir.path(),"43",bytes,false,cancel).isEmpty());
    }
    void mapsEveryDiscoveredPlatform() {
        QTemporaryDir dir;const auto systems=platforms();QCOMPARE(systems.size(),33);
        auto supported=batoceraPlatforms();supported.sort();QCOMPARE(systems,supported);
        for(const auto& p:systems){QVERIFY(systemId(p)>0);put(dir.filePath(p+"/Fixture.zip"),"abc");}
        QCOMPARE(systemId("gamecube"),13);QCOMPARE(systemId("ps"),57);QCOMPARE(systemId("unknown"),0);
    }
    void stableStreamingHashesAndCancellation() {
        QTemporaryDir dir;const auto path=dir.filePath("File.gba");put(path,"123456789");const auto cancel=flag();
        const auto f=fingerprint(path,cancel);QVERIFY(f.valid());QVERIFY(f.unchanged());
        QCOMPARE(f.md5,"25f9e794323b453885f5181f1b624d0b");QCOMPARE(f.sha1,"f7c3bc1d808e04732adf679965ccc34ca7ae3441");QCOMPARE(f.crc,"cbf43926");
        cancel->store(true);QVERIFY(!fingerprint(path,cancel).valid());cancel->store(false);
        put(path,"changed");QVERIFY(!f.unchanged());
        put(dir.filePath("Archive.zip"),"123456789");QCOMPARE(fingerprint(dir.filePath("Archive.zip"),cancel).md5,f.md5);
    }
    void credentialsAreExplicitAndPrivate() {
        QTemporaryDir dir;const auto path=dir.filePath("account.json");
        QVERIFY(!writeCredentials(path,{}));Credentials c{"dev","dev-secret","user","user-secret"};
        QVERIFY(writeCredentials(path,c));QCOMPARE(readCredentials(path).developerId,c.developerId);
#ifdef Q_OS_UNIX
        QVERIFY(!(QFile::permissions(path)&(QFileDevice::ReadOther|QFileDevice::ReadGroup)));
#endif
        int calls=0;Client empty({},[&](auto,auto,auto){++calls;return Reply{};});
        QCOMPARE(empty.account(flag()).status,Status::MissingCredentials);QCOMPARE(calls,0);
    }
    void parsesEnglishAndNeverTrustsSearchAsExact() {
        QUrl observed;
        Client client({"dev","secret",{},{}},[&](const QUrl& url,qint64,const Cancellation&){
            observed=url;
            return response({{"jeux",QJsonArray{game()}}});
        });
        const auto r=client.search("gba","Fixture",flag());QCOMPARE(r.status,Status::Ready);QCOMPARE(r.games.size(),1);
        QCOMPARE(QUrlQuery(observed).queryItemValue("systemeid"),"12");
        const auto g=r.games[0];QCOMPARE(g.fields.value("desc"),"English description");QCOMPARE(g.fields.value("releasedate"),"20040102T000000");
        QVERIFY(!g.exactFile);QVERIFY(g.media.contains("screenshot"));QVERIFY(!g.media.contains("video"));
    }
    void matchingHashAndSystemRequired() {
        QTemporaryDir dir;const auto path=dir.filePath("Fixture.gba");put(path,"123456789");const auto f=fingerprint(path,flag());
        auto data=game();data["rom"]=QJsonObject{{"romsize","9"},{"rommd5",f.md5},{"romcrc",f.crc}};
        QUrl observed;
        Client client({"dev","secret",{},{}},[&](const QUrl& url,qint64,const Cancellation&){observed=url;return response({{"jeu",data}});});
        auto r=client.lookup("gba",f,flag());QCOMPARE(r.games.size(),1);QVERIFY(r.games[0].exactFile);
        QCOMPARE(QUrlQuery(observed).queryItemValue("romnom"),"Fixture.gba");
        data["rom"]=QJsonObject{{"romsize","9"},{"rommd5",f.md5},{"romcrc","00000000"}};
        QVERIFY(!client.lookup("gba",f,flag()).games[0].exactFile);
        data["systeme"]=QJsonObject{{"id","9"}};QCOMPARE(client.lookup("gba",f,flag()).status,Status::NotFound);
    }
    void errorsQuotaAndMalformedResponses() {
        Reply reply;
        for(const auto& [code,status]:QList<QPair<int,Status>>{{429,Status::Busy},{430,Status::Quota},{431,Status::Quota},{403,Status::Denied},{404,Status::NotFound},{503,Status::Offline}}) {
            Client client({"dev","secret",{},{}},[&](auto,auto,auto){return reply;});
            reply={code,{},37};const auto r=client.account(flag());QCOMPARE(r.status,status);QCOMPARE(r.retryAfter,37);
        }
        Client client({"dev","secret",{},{}},[&](auto,auto,auto){return reply;},[](qint64,const Cancellation& c){return !c->load();});
        reply={200,"not json",0};QCOMPARE(client.account(flag()).status,Status::InvalidResponse);
        reply={200,QByteArray(2*1024*1024+1,' '),0};QCOMPARE(client.account(flag()).status,Status::InvalidResponse);
        reply=response({{"ssuser",QJsonObject{{"maxthreads","1"},{"maxrequestsperday","20"},{"requeststoday","20"}}}});
        const auto r=client.account(flag());QCOMPARE(r.status,Status::Ready);QVERIFY(r.quota.exhausted());
        QCOMPARE(client.search("gba","Fixture",flag()).status,Status::Quota);
        auto cancel=flag();cancel->store(true);QCOMPARE(client.account(cancel).status,Status::Cancelled);
        QVERIFY(!allowedUrl(QUrl("https://screenscraper.fr.evil.test/x")));QVERIFY(!allowedUrl(QUrl("http://screenscraper.fr/x")));
        QVERIFY(!allowedUrl(QUrl("https://user:secret@screenscraper.fr/x")));QVERIFY(!allowedUrl(QUrl("file:///tmp/test")));
    }
    void atomicWritePreservesNamesUnknownFieldsAndUnrelatedEntries() {
        QTemporaryDir dir;const auto folder=dir.filePath("gba");const auto rom=folder+"/Fixture.gba",xml=folder+"/gamelist.xml";
        put(rom,"123");put(folder+"/media/preview.mp4","video");
        put(xml,"<gameList><!--keep--><game custom='yes'><path>./Fixture.gba</path><name>My name</name><desc>Existing</desc><custom>Keep</custom></game><game><path>./Other.gba</path><name>Other</name></game></gameList>");
        auto f=fingerprint(rom,flag());Game g;g.id="42";g.system=12;g.fields={{"name","Scraped name"},{"desc","New description"},{"genre","Adventure"}};
        QVERIFY(writeGamelist(folder,f,g,{{"video",folder+"/media/preview.mp4"}},false,flag()).isEmpty());
        auto bytes=read(xml);QVERIFY(bytes.contains("My name"));QVERIFY(bytes.contains("Existing"));QVERIFY(bytes.contains("<custom>Keep</custom>"));QVERIFY(bytes.contains("<name>Other</name>"));QVERIFY(bytes.contains("./media/preview.mp4"));
        const auto scan=scanBatoceraLibrary(dir.path(),{});QCOMPARE(scan.entries.size(),1);QVERIFY(!scan.entries[0].media.value("video").toString().isEmpty());
        QVERIFY(writeGamelist(folder,f,g,{},true,flag()).isEmpty());QVERIFY(read(xml).contains("New description"));QVERIFY(read(xml).contains("My name"));
        bytes=read(xml);auto cancel=flag();cancel->store(true);QVERIFY(!writeGamelist(folder,f,g,{},true,cancel).isEmpty());QCOMPARE(read(xml),bytes);
        put(rom,"changed");QVERIFY(!writeGamelist(folder,f,g,{},true,flag()).isEmpty());QCOMPARE(read(xml),bytes);
    }
    void rejectsInvalidXmlDuplicateEntriesAndOutsideMedia() {
        QTemporaryDir dir;const auto folder=dir.filePath("gba"),rom=folder+"/Fixture.gba",xml=folder+"/gamelist.xml";
        put(rom,"123");const auto f=fingerprint(rom,flag());Game g;g.id="1";g.system=12;g.fields["name"]="Fixture";
        for(const auto& bytes:QList<QByteArray>{"broken","<!DOCTYPE gameList><gameList/>","<gameList><game><path>./Fixture.gba</path></game><game><path>./Fixture.gba</path></game></gameList>"}) {
            put(xml,bytes);QVERIFY(!writeGamelist(folder,f,g,{},true,flag()).isEmpty());QCOMPARE(read(xml),bytes);
        }
        QFile::remove(xml);put(dir.filePath("outside.png"),"image");
        QVERIFY(!writeGamelist(folder,f,g,{{"image",dir.filePath("outside.png")}},false,flag()).isEmpty());QVERIFY(!QFile::exists(xml));
        QVERIFY(writeGamelist(folder,f,g,{},false,flag()).isEmpty());QVERIFY(read(xml).contains("./Fixture.gba"));
    }
};
QTEST_GUILESS_MAIN(ScreenScraperTests)
#include "ScreenScraperTests.moc"
