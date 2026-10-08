#include "integrations/scraper/ScreenScraper.h"
#include "core/repository/BatoceraLibrary.h"
#include "core/model/GamePlayers.h"
#include "features/library/ScrapeController.h"
#include "features/downloads/DownloadsController.h"
#include "core/repository/RomPlatforms.h"
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
class ScrapeLibrary final:public LibraryRepository {
public:
    QString root;QList<AdventureRegistration> records;
    QList<World> worlds() const override{return {};}
    QList<Adventure> adventures() const override{QList<Adventure> r;for(const auto& record:records)r<<record.adventure;return r;}
    QList<ResumePoint> resumePoints() const override{return {};}
    HomeSnapshot home() const override{return {};}
    bool editable() const override{return true;}
    QString storageRootFor(const QString&) const override{return root;}
    std::optional<AdventureRegistration> registration(const QString& id) const override{for(const auto& r:records)if(r.adventure.id==id)return r;return {};}
    QList<AdventureRegistration> registrations() const override{return records;}
    void add(const QString& id,const QString& suffix="gba") {AdventureRegistration r;r.adventure.id=id;r.adventure.title="Owner "+id;r.adventure.platformId="gba";r.contentPath=root+"/gba/"+id+'.'+suffix;records<<r;}
};
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
    void systemPickerQueuesOnlySelectedAvailableRomPlatforms() {
        QTemporaryDir dir;ScrapeLibrary library;library.root=dir.filePath("roms");
        for(const auto& id:QStringList{"gba-one","gba-two","nes-one","unsupported","removed","store"})library.add(id);
        const QStringList platforms{"gba","gba","nes","c128","gb","snes"};
        for(int i=0;i<library.records.size();++i){
            auto& r=library.records[i];r.adventure.platformId=platforms[i];
            r.contentPath=library.root+'/'+platforms[i]+'/'+r.adventure.id+".bin";put(r.contentPath,"123456789");
        }
        library.records[4].removed=true;library.records[5].integrationConfig["oddcrate"]=QJsonObject{};
        const auto state=dir.filePath("state");QVERIFY(writeCredentials(state+"/secrets/screenscraper.json",{"dev","secret",{}, {}}));
        ScrapeController flow(library);flow.configure(state);std::atomic_int calls=0;
        flow.setTransport([&](const QUrl& url,qint64,const Cancellation&){
            if(url.path().endsWith("ssuserInfos.php"))return response({{"ssuser",QJsonObject{{"maxthreads","1"}}}});
            ++calls;auto g=game();g.remove("medias");
            g["systeme"]=QJsonObject{{"id",QUrlQuery(url).queryItemValue("systemeid")}};
            g["rom"]=QJsonObject{{"romsize","9"},{"rommd5","25f9e794323b453885f5181f1b624d0b"}};
            return response({{"jeu",g}});
        },[](qint64,const Cancellation& c){return !c->load();});
        flow.beginSystems();QVERIFY(flow.selectingSystems());QCOMPARE(flow.systemRows().size(),3);
        QCOMPARE(flow.selectedGameCount(),0);QVERIFY(!flow.rows()[4].toMap()["enabled"].toBool());
        flow.activate(4);QVERIFY(!flow.busy());QCOMPARE(calls.load(),0);
        int gba=-1,nes=-1,unsupported=-1;
        for(int i=0;i<flow.systemRows().size();++i){const auto r=flow.systemRows()[i].toMap();
            if(r["id"]=="gba")gba=i+1;if(r["id"]=="nes")nes=i+1;if(r["id"]=="c128")unsupported=i+1;
        }
        QVERIFY(gba>0&&nes>0&&unsupported>0);
        flow.activate(unsupported);QCOMPARE(flow.selectedGameCount(),0);
        flow.activate(0);QCOMPARE(flow.selectedGameCount(),3);
        flow.activate(0);QCOMPARE(flow.selectedGameCount(),0);
        flow.activate(gba);flow.activate(nes);QCOMPARE(flow.selectedGameCount(),3);
        flow.activate(nes);QCOMPARE(flow.selectedGameCount(),2);
        flow.dispatch(Action::Right);QCOMPARE(flow.focusIndex(),4);
        flow.dispatch(Action::Left);QCOMPARE(flow.focusIndex(),nes);
        flow.canStart=[] {return false;};flow.activate(4);QVERIFY(!flow.busy());QCOMPARE(calls.load(),0);
        flow.canStart=[] {return true;};flow.activate(4);
        QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),3000);QCOMPARE(calls.load(),2);
        QCOMPARE(flow.downloadTasks().size(),2);
        QVERIFY(read(library.root+"/gba/gamelist.xml").contains("gba-one.bin"));
        QVERIFY(read(library.root+"/gba/gamelist.xml").contains("gba-two.bin"));
        QVERIFY(!QFile::exists(library.root+"/nes/gamelist.xml"));
        flow.beginSystems();QCOMPARE(flow.selectedGameCount(),0);
        QSignalSpy closed(&flow,&ScrapeController::systemSelectionClosed);flow.dispatch(Action::Back);
        QVERIFY(!flow.isOpen());QCOMPARE(closed.size(),1);
        library.records.clear();flow.beginSystems();QVERIFY(flow.systemRows().isEmpty());
        QVERIFY(!flow.rows()[0].toMap()["enabled"].toBool());QVERIFY(!flow.rows()[1].toMap()["enabled"].toBool());
    }
    void sharedDownloadsKeepSelectionAndRouteProviderCommands() {
        DownloadsController downloads;QSignalSpy commands(&downloads,&DownloadsController::commandRequested);
        const auto task=[](const QString& id){return QVariantMap{{"id",id},{"title",id},{"actions",QVariantList{QVariantMap{{"id","pause"},{"label","Pause"}}}}};};
        downloads.publish("first",{task("one"),task("two")});downloads.publish("second",{task("three")});
        downloads.begin();downloads.select(1);downloads.publish("first",{task("two"),task("one")});
        QCOMPARE(downloads.focusIndex(),0);downloads.activate(0);
        QCOMPARE(commands.last(),QVariantList({QString("first"),QString("two"),QString("pause")}));
        downloads.controlAll("cancel-all");QCOMPARE(commands.size(),3);
        QCOMPARE(commands.last()[0].toString(),"second");
        downloads.dispatch(Action::Back);QVERIFY(!downloads.isOpen());QCOMPARE(downloads.tasks().size(),3);
    }
    void downloadQueueReordersPausesAndCancelsIndividualGames() {
        QTemporaryDir dir;ScrapeLibrary library;library.root=dir.filePath("roms");
        for(const auto& id:QStringList{"one","two","three","four"})library.add(id);
        for(const auto& r:library.records)put(r.contentPath,"123456789");
        const auto state=dir.filePath("state");QVERIFY(writeCredentials(state+"/secrets/screenscraper.json",{"dev","secret",{}, {}}));
        std::atomic_int calls=0;std::atomic_bool hold=true;QStringList order;ScrapeController flow(library);flow.configure(state);
        flow.setTransport([&](const QUrl& url,qint64,const Cancellation& cancel){
            if(url.path().endsWith("ssuserInfos.php"))return response({{"ssuser",QJsonObject{{"maxthreads","1"}}}});
            order<<QUrlQuery(url).queryItemValue("romnom");++calls;
            while(hold.load()&&!cancel->load())QThread::msleep(5);
            auto g=game();g.remove("medias");g["rom"]=QJsonObject{{"romsize","9"},{"rommd5","25f9e794323b453885f5181f1b624d0b"}};return response({{"jeu",g}});
        },[](qint64,const Cancellation& c){return !c->load();});
        flow.begin();flow.activate(1);QTRY_COMPARE_WITH_TIMEOUT(calls.load(),1,3000);
        flow.downloadCommand("three","earlier");flow.downloadCommand("two","pause");flow.downloadCommand("four","cancel");
        QCOMPARE(flow.downloadTasks()[1].toMap()["id"].toString(),"three");
        flow.hide();QVERIFY(!flow.isOpen());QVERIFY(flow.busy());
        flow.downloadCommand("one","cancel");hold=false;
        QTRY_COMPARE_WITH_TIMEOUT(calls.load(),2,3000);
        QTRY_COMPARE_WITH_TIMEOUT(flow.downloadTasks()[1].toMap()["state"].toString(),"done",3000);
        QCOMPARE(flow.downloadTasks()[0].toMap()["state"].toString(),"cancelled");
        QVERIFY(flow.busy());flow.downloadCommand("two","resume");
        QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),3000);
        QCOMPARE(order,QStringList({"one.gba","three.gba","two.gba"}));
        const auto xml=read(library.root+"/gba/gamelist.xml");QVERIFY(!xml.contains("one.gba"));QVERIFY(!xml.contains("four.gba"));
        QVERIFY(xml.contains("two.gba"));QVERIFY(xml.contains("three.gba"));
    }
    void collectionScopeUsesExistingSeriesAcrossPlatforms() {
        QTemporaryDir dir;ScrapeLibrary library;library.root=dir.filePath("roms");
        for(const auto& id:QStringList{"kirby1","kirby2","mario","unknown"})library.add(id);
        for(auto& r:library.records){r.adventure.domain="multiverse";put(r.contentPath,"fixture");}
        library.records[0].adventure.title="Kirby & The Amazing Mirror";
        library.records[1].adventure.title="Kirby's Adventure";
        library.records[1].adventure.platformId="nes";
        library.records[1].contentPath=library.root+"/nes/kirby.nes";put(library.records[1].contentPath,"fixture");
        library.records[2].adventure.title="Super Mario Advance";
        ScrapeController flow(library);flow.begin("kirby1");
        QVERIFY(flow.detail().startsWith("1 games"));
        flow.activate(0);QVERIFY(flow.detail().startsWith("3 games"));
        flow.activate(0);QCOMPARE(flow.rows()[0].toMap()["detail"].toString(),"Kirby collection");
        QVERIFY(flow.detail().startsWith("2 games"));
        flow.activate(0);QVERIFY(flow.detail().startsWith("4 games"));
        flow.activate(0);QVERIFY(flow.detail().startsWith("1 games"));
        flow.configure(dir.filePath("state"));flow.activateSetting(4);
        QCOMPARE(flow.settingsRows()[4].toMap()["detail"].toString(),"ru (English fallback)");
        flow.dispatchSettings(Action::Left);
        QCOMPARE(flow.settingsRows()[4].toMap()["detail"].toString(),"en (English fallback)");
        flow.dispatchSettings(Action::Right);
        QCOMPARE(flow.settingsRows()[4].toMap()["detail"].toString(),"ru (English fallback)");
    }
    void allCataloguePlatformsHaveAnExplicitServiceBoundary() {
        QCOMPARE(systemId("msx"),113);QCOMPARE(systemId("msx2"),116);QCOMPARE(systemId("msx2+"),117);QCOMPARE(systemId("msxturbor"),118);
        const QStringList unavailable{"c128","enterprise","systemsp","videopacplus"};
        for(const auto& platform:romPlatforms())QVERIFY2(systemId(platform.id)>0||unavailable.contains(platform.id),qPrintable(platform.id));
    }
    void languageRegionAndDownloadPreferences() {
        auto g=game();g["noms"]=QJsonArray{QJsonObject{{"region","us"},{"text","US"}},QJsonObject{{"region","jp"},{"text","Japan"}}};
        Client client({"dev","secret",{},{}},[&](auto,auto,auto){return response({{"jeux",QJsonArray{g}}});});
        Preferences prefs;prefs.language="fr";prefs.region="jp";prefs.cover="";prefs.logos=false;prefs.screenshots=false;prefs.fanart=true;
        client.setPreferences(prefs);const auto r=client.search("gba","Fixture",flag());
        QCOMPARE(r.games.first().fields["desc"],"French");QCOMPARE(r.games.first().fields["name"],"Japan");
        QCOMPARE(prefs.mediaTags(),QStringList{"fanart"});QCOMPARE(Preferences::fromJson(prefs.json()).json(),prefs.json());
    }
    void controllerDownloadsRescansAndUsesSecretFreeCache() {
        QTemporaryDir dir;ScrapeLibrary library;library.root=dir.filePath("roms");library.add("first");
        put(library.records.first().contentPath,"123456789");
        const auto state=dir.filePath("state");QVERIFY(writeCredentials(state+"/secrets/screenscraper.json",{"dev","private-dev","user","private-user"}));
        QImage image(12,8,QImage::Format_RGB32);image.fill(Qt::green);QByteArray png;QBuffer b(&png);b.open(QIODevice::WriteOnly);QVERIFY(image.save(&b,"PNG"));
        std::atomic_int lookups=0,downloads=0,accounts=0;
        ScrapeController flow(library);flow.configure(state);
        flow.setTransport([&](const QUrl& url,qint64,const Cancellation&){
            if(url.path().endsWith("ssuserInfos.php")){++accounts;return response({{"ssuser",QJsonObject{{"maxrequestsperday","100"},{"maxrequestspermin","1000"}}}});}
            if(url.path().endsWith("image.png")){++downloads;return Reply{200,png,0};}
            ++lookups;auto g=game();g["rom"]=QJsonObject{{"romsize","9"},{"rommd5","25f9e794323b453885f5181f1b624d0b"}};return response({{"jeu",g}});
        },[](qint64,const Cancellation& c){return !c->load();});
        QSignalSpy saved(&flow,&ScrapeController::saved);flow.begin("first");flow.activate(1);QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),5000);
        QCOMPARE(saved.size(),1);QCOMPARE(downloads.load(),1);QCOMPARE(lookups.load(),1);
        auto scan=scanBatoceraLibrary(library.root,library.records);QCOMPARE(scan.entries.size(),1);
        QCOMPARE(scan.entries.first().media["desc"].toString(),"English description");QVERIFY(!scan.entries.first().media["screenshot"].toString().isEmpty());
        QCOMPARE(read(library.records.first().contentPath),QByteArray("123456789"));
        const auto xml=read(library.root+"/gba/gamelist.xml");QVERIFY(xml.contains("Owner first"));
        for(const auto& f:QDir(state+"/cache/screenscraper").entryList({"*.json"},QDir::Files)){
            const auto cache=read(state+"/cache/screenscraper/"+f);QVERIFY(!cache.contains("https:"));QVERIFY(!cache.contains("private-"));
        }
        flow.close();flow.begin("first");flow.activate(1);QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),5000);
        QCOMPARE(downloads.load(),1);QCOMPARE(lookups.load(),1);QCOMPARE(accounts.load(),2);
        flow.activateSetting(13);flow.activateSetting(14);flow.activateSetting(16);
        ScrapeController restored(library);restored.configure(state);QCOMPARE(restored.displayPreferences(),flow.displayPreferences());
        BatoceraLibrary folders(library,library.root);folders.setDisplayPreferences(flow.displayPreferences());
        QCOMPARE(folders.artwork("first")["displayFacts"].toBool(),false);
    }
    void descriptorFallbackRequiresChoiceAndCanBeCancelled() {
        QTemporaryDir dir;ScrapeLibrary library;library.root=dir.filePath("roms");library.add("disc","cue");put(library.records.first().contentPath,"FILE disc.bin BINARY\n");
        const auto state=dir.filePath("state");QVERIFY(writeCredentials(state+"/secrets/screenscraper.json",{"dev","secret",{}, {}}));
        std::atomic_int lookup=0,search=0;ScrapeController flow(library);flow.configure(state);
        flow.setTransport([&](const QUrl& url,qint64,const Cancellation&){
            if(url.path().endsWith("ssuserInfos.php"))return response({{"ssuser",QJsonObject{{"maxthreads","1"}}}});
            if(url.path().endsWith("jeuInfos.php"))++lookup;else ++search;
            return response({{"jeux",QJsonArray{game(),game()}}});
        },[](qint64,const Cancellation& c){return !c->load();});
        flow.begin("disc");flow.activate(1);QTRY_COMPARE_WITH_TIMEOUT(flow.title(),QString("Choose the matching game"),5000);
        QCOMPARE(lookup.load(),0);QCOMPARE(search.load(),1);QVERIFY(!QFile::exists(library.root+"/gba/gamelist.xml"));
        flow.dispatch(Action::Back);QVERIFY(!flow.busy());QVERIFY(!QFile::exists(library.root+"/gba/gamelist.xml"));
    }
    void batchContinuesAfterFailureAndRetriesOnlyFailedGame() {
        QTemporaryDir dir;ScrapeLibrary library;library.root=dir.filePath("roms");library.add("bad");library.add("good");
        for(const auto& r:library.records)put(r.contentPath,"123456789");
        const auto state=dir.filePath("state");QVERIFY(writeCredentials(state+"/secrets/screenscraper.json",{"dev","secret",{}, {}}));
        std::atomic_bool fail=true;std::atomic_int bad=0,good=0;ScrapeController flow(library);flow.configure(state);
        flow.setTransport([&](const QUrl& url,qint64,const Cancellation&){
            if(url.path().endsWith("ssuserInfos.php"))return response({{"ssuser",QJsonObject{{"maxthreads","1"}}}});
            if(QUrlQuery(url).queryItemValue("romnom")=="bad.gba"){++bad;if(fail.load())return Reply{503,{},0};}else ++good;
            auto g=game();g.remove("medias");g["rom"]=QJsonObject{{"romsize","9"},{"rommd5","25f9e794323b453885f5181f1b624d0b"}};return response({{"jeu",g}});
        },[](qint64,const Cancellation& c){return !c->load();});
        flow.begin();flow.activate(1);QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),5000);QVERIFY(flow.detail().contains("1 failed"));QCOMPARE(good.load(),1);
        fail=false;flow.activate(0);QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),5000);QCOMPARE(bad.load(),2);QCOMPARE(good.load(),1);QVERIFY(flow.detail().contains("0 failed"));
    }
    void pauseBetweenGamesAndCancelInFlightPreserveCommittedXml() {
        QTemporaryDir dir;ScrapeLibrary library;library.root=dir.filePath("roms");library.add("one");library.add("two");
        for(const auto& r:library.records)put(r.contentPath,"123456789");
        const auto state=dir.filePath("state");QVERIFY(writeCredentials(state+"/secrets/screenscraper.json",{"dev","secret",{}, {}}));
        std::atomic_int calls=0;std::atomic_bool hold=true;ScrapeController flow(library);flow.configure(state);
        flow.setTransport([&](const QUrl& url,qint64,const Cancellation& cancel){
            if(url.path().endsWith("ssuserInfos.php"))return response({{"ssuser",QJsonObject{{"maxthreads","1"}}}});
            ++calls;while(hold.load()&&!cancel->load())QThread::msleep(5);
            auto g=game();g.remove("medias");g["rom"]=QJsonObject{{"romsize","9"},{"rommd5","25f9e794323b453885f5181f1b624d0b"}};return response({{"jeu",g}});
        },[](qint64,const Cancellation& c){return !c->load();});
        flow.begin();flow.activate(1);QTRY_COMPARE_WITH_TIMEOUT(calls.load(),1,3000);flow.activate(0);hold=false;
        QTRY_VERIFY_WITH_TIMEOUT(flow.detail().contains("1 / 2"),3000);QVERIFY(flow.busy());QCOMPARE(calls.load(),1);
        flow.activate(0);QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),3000);QCOMPARE(calls.load(),2);
        const auto xml=read(library.root+"/gba/gamelist.xml");
        flow.close();flow.activateSetting(11);hold=true;flow.begin("one");flow.activate(1);
        QTRY_COMPARE_WITH_TIMEOUT(calls.load(),3,3000);flow.dispatch(Action::Back);QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),3000);
        QCOMPARE(read(library.root+"/gba/gamelist.xml"),xml);
    }
    void partialMediaCacheAvoidsRepeatingSuccessfulDownloads() {
        QTemporaryDir dir;ScrapeLibrary library;library.root=dir.filePath("roms");library.add("one");put(library.records.first().contentPath,"123456789");
        const auto state=dir.filePath("state");QVERIFY(writeCredentials(state+"/secrets/screenscraper.json",{"dev","secret",{}, {}}));
        QImage image(12,8,QImage::Format_RGB32);image.fill(Qt::blue);QByteArray png;QBuffer b(&png);b.open(QIODevice::WriteOnly);QVERIFY(image.save(&b,"PNG"));
        std::atomic_int covers=0,shots=0;std::atomic_bool fail=true;ScrapeController flow(library);flow.configure(state);
        flow.setTransport([&](const QUrl& url,qint64,const Cancellation&){
            if(url.path().endsWith("ssuserInfos.php"))return response({{"ssuser",QJsonObject{{"maxthreads","1"}}}});
            if(url.path().endsWith("cover.png")){++covers;return Reply{200,png,0};}
            if(url.path().endsWith("image.png")){++shots;return fail.load()?Reply{503,{},0}:Reply{200,png,0};}
            auto g=game();auto media=g["medias"].toArray();media.append(QJsonObject{{"type","box-2D"},{"region","us"},{"url","https://www.screenscraper.fr/cover.png"}});g["medias"]=media;
            g["rom"]=QJsonObject{{"romsize","9"},{"rommd5","25f9e794323b453885f5181f1b624d0b"}};return response({{"jeu",g}});
        },[](qint64,const Cancellation& c){return !c->load();});
        flow.begin();flow.activate(1);QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),3000);QCOMPARE(covers.load(),1);QCOMPARE(shots.load(),1);
        QVERIFY(!QFile::exists(library.root+"/gba/gamelist.xml"));fail=false;flow.activate(0);QTRY_VERIFY_WITH_TIMEOUT(!flow.busy(),3000);
        QCOMPARE(covers.load(),1);QCOMPARE(shots.load(),2);QVERIFY(QFile::exists(library.root+"/gba/gamelist.xml"));
    }
    void playerCountsSurviveScrapeWriteAndFolderDiscovery() {
        QTemporaryDir dir;const auto folder=dir.filePath("gba"),rom=folder+"/Fixture.gba";
        put(rom,"test fixture");const auto file=fingerprint(rom,flag());
        auto data=game();data["joueurs"]=QJsonObject{{"text","1-4"}};
        Client client({"dev","secret",{},{}},[&](auto,auto,auto){return response({{"jeux",QJsonArray{data}}});});
        const auto found=client.search("gba","Fixture",flag());QCOMPARE(found.games.size(),1);
        QCOMPARE(found.games.first().fields.value("players"),QString("1-4"));
        QVERIFY(writeGamelist(folder,file,found.games.first(),{},false,flag()).isEmpty());
        auto scan=scanBatoceraLibrary(dir.path(),{});QCOMPARE(scan.entries.size(),1);
        QCOMPARE(gamePlayers(scan.entries.first().media).maximum,4);
        data["joueurs"]="1";
        const auto solo=client.search("gba","Fixture",flag());QCOMPARE(solo.games.size(),1);
        QVERIFY(writeGamelist(folder,file,solo.games.first(),{},true,flag()).isEmpty());
        scan=scanBatoceraLibrary(dir.path(),{});QVERIFY(gamePlayers(scan.entries.first().media).solo());
    }
    void mediaValidationAndIdempotentStorage() {
        QTemporaryDir dir;QImage image(12,8,QImage::Format_RGB32);image.fill(Qt::blue);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);QVERIFY(image.save(&buffer,"PNG"));
        const auto path=storeMedia(dir.path(),"42",bytes,false,flag());QVERIFY(!path.isEmpty());QCOMPARE(read(path),bytes);
        QCOMPARE(storeMedia(dir.path(),"42",bytes,false,flag()),path);
        QVERIFY(storeMedia(dir.path(),"../42",bytes,false,flag()).isEmpty());
        QVERIFY(storeMedia(dir.path(),"42","<html>Error</html>",false,flag()).isEmpty());
        QVERIFY(storeMedia(dir.path(),"42",bytes,true,flag()).isEmpty());
        auto cancel=flag();cancel->store(true);QVERIFY(storeMedia(dir.path(),"43",bytes,false,cancel).isEmpty());
    }
    void scrapingSupportIsIndependentOfOrdinaryLaunch() {
        QTemporaryDir dir;const auto systems=platforms();QCOMPARE(systems.size(),101);
        const auto supported=batoceraPlatforms();
        for(const auto& p:systems)QVERIFY(supported.contains(p));
        // New ARM64 launch routes do not invent external ScreenScraper IDs.
        QVERIFY(supported.contains("c64"));QCOMPARE(systemId("c64"),66);
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
