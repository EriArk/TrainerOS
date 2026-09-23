#include "core/repository/BatoceraLibrary.h"
#include "core/repository/CollectionRepository.h"
#include "core/storage/LocalStateStore.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QUrl>

using namespace trainer;
namespace {
void put(const QString& path,const QByteArray& bytes="Test fixture, not a ROM.") {
    QDir().mkpath(QFileInfo(path).absolutePath());QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(bytes),bytes.size());
}
}
class BatoceraTests final : public QObject {
    Q_OBJECT
private slots:
    void gameVariantsExcludeDependenciesAndDlc() {
        QTemporaryDir dir;
        put(dir.filePath("naomi/ikaruga.zip"));put(dir.filePath("naomi/ikaruga/gdl-0010.chd"));
        put(dir.filePath("naomi/hod2bios.zip"));put(dir.filePath("naomi/naomigd.7z"));
        put(dir.filePath("fbneo/qsound.zip"));put(dir.filePath("fbneo/qsound_hle.7z"));
        put(dir.filePath("wiiu/Super Smash Bros. for Wii U (USA) (DLC) (v304).wua"));
        put(dir.filePath("switch/Fixture (Update).nsp"));
        put(dir.filePath("n64/Super Smash Bros. (US) (LodgeNet).z64"));
        put(dir.filePath("gamecube/Super Smash Bros. Melee (Player's Choice)(USA).iso"));
        const auto scan=scanBatoceraLibrary(dir.path(),{});QCOMPARE(scan.entries.size(),3);
        int crossovers=0;
        for(const auto& entry:scan.entries)if(entry.record.adventure.worldId=="crossovers") {
            ++crossovers;QCOMPARE(entry.record.adventure.domain,"pokemon");
            QVERIFY(!entry.record.adventure.catalogueId.isEmpty());QVERIFY(!entry.record.adventure.variant.isEmpty());
        }
        QCOMPARE(crossovers,2);
        AdventureRegistration existing;existing.contentPath=dir.filePath("naomi/ikaruga/gdl-0010.chd");existing.adventure.id="legacy-disc";
        QCOMPARE(scanBatoceraLibrary(dir.path(),{existing}).entries.size(),4);
    }
    void filesystemMetadataAndCatalogue() {
        QTemporaryDir dir;
        put(dir.filePath("gba/Family/Pokemon - Emerald Version (USA, Europe).gba"));
        put(dir.filePath("gba/Pokemon Emerald Rogue.gba"));
        put(dir.filePath("gba/Pokemon Emerald (Hack).gba"));
        put(dir.filePath("gba/pokemon/romhacks/Pokemon Emerald.gba"));
        put(dir.filePath("gba/Pokemon Emerald.srm"));
        put(dir.filePath("gba/images/cover.png"));put(dir.filePath("gba/images/logo.png"));
        put(dir.filePath("gba/gamelist.xml"),R"(<gameList><game><path>./Family/Pokemon - Emerald Version (USA, Europe).gba</path><name>Emerald &amp; friends</name><image>./images/cover.png</image><marquee>./images/logo.png</marquee><video>https://invalid.example/movie.mp4</video><playcount>999</playcount></game></gameList>)");
        put(dir.filePath("gamecube/Adventure.rvz"));
        put(dir.filePath("psx/Disc 1.chd"));put(dir.filePath("psx/Disc 2.chd"));
        put(dir.filePath("psx/Story.m3u"),"Disc 1.chd\nDisc 2.chd\n");
        put(dir.filePath("psx/tracks.bin"));
        const auto scan=scanBatoceraLibrary(dir.path(),{});
        QVERIFY(scan.warnings.isEmpty());QCOMPARE(scan.entries.size(),6);
        int matched=0,hacks=0,discs=0,cubes=0;
        for(const auto& e:scan.entries) {
            const auto& a=e.record.adventure;
            if(a.catalogueId=="emerald-gba") {
                ++matched;QCOMPARE(a.worldId,"hoenn");QCOMPARE(a.title,"Emerald & friends");QVERIFY(!a.collectionOnly);
                QCOMPARE(e.media["cover"].toString(),QUrl::fromLocalFile(dir.filePath("gba/images/cover.png")).toString());
                QVERIFY(!e.media["marquee"].toString().isEmpty());QVERIFY(!e.media.contains("video"));QVERIFY(!e.media.contains("playcount"));
            } else if(a.domain=="pokemon") {++hacks;QVERIFY(a.catalogueId.isEmpty());QCOMPARE(a.worldId,"unclassified-pokemon");}
            else if(a.platformId=="psx") {++discs;QVERIFY(e.record.contentPath.endsWith("Story.m3u"));}
            else if(a.platformId=="gc")++cubes;
        }
        QCOMPARE(matched,1);QCOMPARE(hacks,3);QCOMPARE(discs,1);QCOMPARE(cubes,1);
    }
    void invalidXmlAndMissingMediaDoNotLoseFiles() {
        QTemporaryDir dir;put(dir.filePath("nes/Fixture.nes"));
        put(dir.filePath("nes/gamelist.xml"),"<gameList><game><path>./Fixture.nes</path><name>Partial</name>");
        auto scan=scanBatoceraLibrary(dir.path(),{});QCOMPARE(scan.entries.size(),1);QVERIFY(!scan.warnings.isEmpty());QVERIFY(scan.entries[0].media.isEmpty());
        put(dir.filePath("nes/gamelist.xml"),"<gameList><game><path>./Fixture.nes</path><image>./missing.png</image></game><game><path>./missing.nes</path></game></gameList>");
        scan=scanBatoceraLibrary(dir.path(),{});QCOMPARE(scan.entries.size(),1);QVERIFY(scan.warnings.isEmpty());QVERIFY(!scan.entries[0].media.contains("cover"));
        QVERIFY(scanBatoceraLibrary(dir.filePath("absent"),{}).entries.isEmpty());
    }
    void databaseRetryPreservesOwnerEditsAndMediaRevision() {
        QTemporaryDir dir;const auto roms=dir.filePath("roms");
        const auto file=QDir(roms).filePath("gba/Pokemon Emerald (USA).gba");put(file);
        LocalStateStore store(dir.filePath("state"));store.open();QTRY_VERIFY(store.ready());
        CollectionRepository collection(store);BatoceraLibrary folders(collection,roms);
        QSignalSpy finished(&folders,&BatoceraLibrary::scanFinished);
        folders.refreshContentAvailability();QTRY_COMPARE(finished.size(),1);
        QCOMPARE(finished[0][0].toInt(),1);QCOMPARE(store.adventures().size(),1);
        auto record=*store.registration(store.adventures()[0].id);QCOMPARE(record.adventure.catalogueId,"emerald-gba");
        record.adventure.title="Owner's edition";record.integrationConfig["owner-option"]="kept";
        bool saved=false;store.saveAdventureAsync(record,this,[&](auto r){QVERIFY(r.success);saved=true;});QTRY_VERIFY(saved);
        const auto revision=store.registration(record.adventure.id)->revision;
        put(QDir(roms).filePath("gba/art.png"));
        put(QDir(roms).filePath("gba/gamelist.xml"),"<gameList><game><path>./Pokemon Emerald (USA).gba</path><name>Scraper title</name><image>./art.png</image></game></gameList>");
        folders.rescan();QTRY_COMPARE(finished.size(),2);QCOMPARE(finished[1][0].toInt(),0);
        const auto current=*store.registration(record.adventure.id);
        QCOMPARE(current.revision,revision);QCOMPARE(current.adventure.title,"Owner's edition");
        QCOMPARE(current.integrationConfig,record.integrationConfig);QVERIFY(!folders.artwork(current.adventure.id).value("cover").toString().isEmpty());
        QSignalSpy updates(&folders,&BatoceraLibrary::changed);
        folders.rescan();QTRY_COMPARE(finished.size(),3);QCOMPARE(updates.size(),0);
        for(int i=0;i<20;++i)folders.refreshContentAvailability();
        QVERIFY(!folders.busy());QVERIFY(!folders.writing());QCOMPARE(finished.size(),3);
        QVERIFY(QFile::remove(file));folders.rescan();QTRY_COMPARE(finished.size(),4);
        QCOMPARE(store.adventures().size(),1);QCOMPARE(store.registration(record.adventure.id)->revision,revision);
        QVERIFY(!store.registration(record.adventure.id)->contentAvailable);
    }
    void hiddenAndExistingBindings() {
        QTemporaryDir dir;const auto file=dir.filePath("gba/Renamed.gba");put(file);
        put(dir.filePath("gba/gamelist.xml"),"<gameList><game><path>./Renamed.gba</path><hidden>true</hidden></game></gameList>");
        QVERIFY(scanBatoceraLibrary(dir.path(),{}).entries.isEmpty());
        AdventureRegistration existing;existing.contentPath=file;existing.adventure.id="old-id";existing.adventure.title="Custom name";existing.revision=17;
        const auto scan=scanBatoceraLibrary(dir.path(),{existing});QCOMPARE(scan.entries.size(),1);
        QVERIFY(scan.entries[0].existing);QCOMPARE(scan.entries[0].record.adventure.id,"old-id");QCOMPARE(scan.entries[0].record.revision,17);
    }
};
QTEST_GUILESS_MAIN(BatoceraTests)
#include "BatoceraTests.moc"
