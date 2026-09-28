#include "core/repository/BatoceraLibrary.h"
#include "core/repository/CollectionRepository.h"
#include "core/storage/LocalStateStore.h"
#include "features/settings/SettingsController.h"
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
    void discTracksAndPicoCartridgesStayInTheirOwnSystems() {
        QTemporaryDir dir;
        put(dir.filePath("neogeocd/Game.cue"),"FILE \"Game.img\" BINARY\n TRACK 01 MODE1/2352\n");
        put(dir.filePath("neogeocd/Game.img"));put(dir.filePath("neogeocd/Game.ccd"));put(dir.filePath("neogeocd/Game.sub"));
        put(dir.filePath("dreamcast/Game.gdi"),"1\n1 0 4 2352 \"track 1.bin\" 0\n");put(dir.filePath("dreamcast/track 1.bin"));
        put(dir.filePath("gba/cover.png"));put(dir.filePath("pico8/Actual.p8.png"));
        const auto scan=scanBatoceraLibrary(dir.path(),{});QCOMPARE(scan.entries.size(),3);
        for(const auto& e:scan.entries)QVERIFY(e.record.contentPath.endsWith(".cue") || e.record.contentPath.endsWith(".gdi") || e.record.contentPath.endsWith(".p8.png"));
    }
    void refreshConfiguresExistingGamesWithoutReplacingCustomBindings() {
        QTemporaryDir dir;const auto roms=dir.filePath("roms");
        put(roms+"/psx/Newly supported.chd");put(roms+"/c64/Computer.d64");
        put(roms+"/megacd/Disc.chd");put(roms+"/wswan/Handheld.ws");put(roms+"/msx1/Computer.rom");
        LocalStateStore store(dir.filePath("state"));store.open();QTRY_VERIFY(store.ready());
        BatoceraLibrary folders(store,roms);QSignalSpy finished(&folders,&BatoceraLibrary::scanFinished);
        folders.rescan();QTRY_COMPARE(finished.size(),1);QCOMPARE(store.adventures().size(),5);
        QSet<QString> platforms;for(const auto& a:store.adventures())platforms.insert(a.platformId);
        QCOMPARE(platforms,(QSet<QString>{"psx","c64","segacd","wonderswan","msx"}));
        const auto records=store.registrations();
        folders.prepareInstallation=[](AdventureRegistration& r){r.adventure.adapterId="custom-installed";r.integrationConfig["route"]="fixture";};
        folders.rescan();QTRY_COMPARE(finished.size(),2);
        for(const auto& old:records) {
            const auto current=store.registration(old.adventure.id);QVERIFY(current);
            QCOMPARE(current->contentPath,old.contentPath);QCOMPARE(current->adventure.title,old.adventure.title);
            QCOMPARE(current->adventure.adapterId,QString("custom-installed"));QCOMPARE(current->revision,old.revision+1);
        }
        folders.prepareInstallation=[](AdventureRegistration& r){r.adventure.adapterId="wrong-replacement";};
        folders.rescan();QTRY_COMPARE(finished.size(),3);
        for(const auto& old:records)QCOMPARE(store.registration(old.adventure.id)->adventure.adapterId,QString("custom-installed"));
    }
    void refreshControlKeepsBackAndLegacyTrashSeparate() {
        SettingsController settings;QSignalSpy refresh(&settings,&SettingsController::libraryRefreshRequested);
        QSignalSpy trash(&settings,&SettingsController::trashRequested);
        settings.selectCategory(8);settings.dispatch(Action::Down);settings.dispatch(Action::Confirm);
        QCOMPARE(refresh.size(),0);
        settings.setLibraryScanState(true,false);settings.dispatch(Action::Confirm);QCOMPARE(refresh.size(),1);
        settings.setLibraryScanState(true,true);settings.dispatch(Action::Confirm);QCOMPARE(refresh.size(),1);
        settings.dispatch(Action::Back);QVERIFY(!settings.controlsFocused());
        settings.setLegacyTrashAvailable(true);settings.selectCategory(8);settings.activateRow(3);QCOMPARE(trash.size(),1);
        settings.setLibraryScanState(true,false,"Library is up to date");
        QCOMPARE(settings.libraryStatus(),QString("Library is up to date"));
    }
    void auxiliaryFoldersAreNotGames() {
        QTemporaryDir dir;
        put(dir.filePath("bios/firmware.zip"));put(dir.filePath("incoming/Unsorted.gba"));
        put(dir.filePath("gba/images/Artwork.zip"));put(dir.filePath("gba/.staging/Partial.gba"));
        put(dir.filePath("gba/Empty.gba"),{});put(dir.filePath("gba/Family/Actual.gba"));
        // A misplaced, nonempty ROM inside a supported platform stays reachable for Move.
        put(dir.filePath("gba/Misplaced.chd"));
        const auto scan=scanBatoceraLibrary(dir.path(),{});
        QCOMPARE(scan.entries.size(),2);QVERIFY(scan.complete);
        for(const auto& entry:scan.entries)QVERIFY(entry.record.contentPath.contains("Actual") || entry.record.contentPath.contains("Misplaced"));
    }
    void failedRefreshRetainsMediaAndValidRemovalClearsIt() {
        QTemporaryDir dir;const auto roms=dir.filePath("roms");
        put(roms+"/gba/Fixture.gba");put(roms+"/gba/art.png");
        const auto xml=roms+"/gba/gamelist.xml";
        put(xml,"<gameList><game><path>./Fixture.gba</path><image>./art.png</image></game></gameList>");
        LocalStateStore store(dir.filePath("state"));store.open();QTRY_VERIFY(store.ready());
        BatoceraLibrary folders(store,roms);QSignalSpy finished(&folders,&BatoceraLibrary::scanFinished);
        folders.rescan();QTRY_COMPARE(finished.size(),1);
        const auto id=store.adventures().first().id;const auto media=folders.artwork(id);QVERIFY(!media.isEmpty());
        put(xml,"<gameList><game>");folders.rescan();QTRY_COMPARE(finished.size(),2);
        QCOMPARE(folders.artwork(id),media);QVERIFY(!finished.last()[1].toStringList().isEmpty());
        QVERIFY(QDir().rename(roms,roms+"-offline"));folders.rescan();QTRY_COMPARE(finished.size(),3);
        QCOMPARE(folders.artwork(id),media);QCOMPARE(store.registrations().size(),1);
        QVERIFY(QDir().rename(roms+"-offline",roms));
        put(xml,"<gameList><game><path>./Fixture.gba</path></game></gameList>");
        folders.rescan();QTRY_COMPARE(finished.size(),4);QVERIFY(folders.artwork(id).isEmpty());
        QCOMPARE(store.adventures().first().id,id);
    }
    void refreshDuringScanRunsOneFollowup() {
        QTemporaryDir dir;put(dir.filePath("gba/First.gba"));
        LocalStateStore store(dir.filePath("state"));store.open();QTRY_VERIFY(store.ready());
        BatoceraLibrary folders(store,dir.path());QSignalSpy finished(&folders,&BatoceraLibrary::scanFinished);
        folders.rescan();QVERIFY(folders.busy());
        for(int i=0;i<20;++i)folders.rescan();
        QTRY_COMPARE(finished.size(),1);
        put(dir.filePath("gba/Second.gba"));
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(),2,12000);
        QCOMPARE(store.adventures().size(),2);QVERIFY(!folders.busy());
    }
    void copiedRomReusesPermanentlyRemovedIdentity() {
        QTemporaryDir dir;const auto file=dir.filePath("gba/Fixture.gba");put(file);
        LocalStateStore store(dir.filePath("state"));store.open();QTRY_VERIFY(store.ready());
        BatoceraLibrary folders(store,dir.path());QSignalSpy finished(&folders,&BatoceraLibrary::scanFinished);
        folders.rescan();QTRY_COMPARE(finished.size(),1);QCOMPARE(store.adventures().size(),1);
        const auto original=*store.registration(store.adventures()[0].id);bool done=false;
        store.editLibraryAsync({LibraryEditKind::RemoveGame,original.adventure.id,original.revision},this,[&](auto error){QVERIFY(error.isEmpty());done=true;});QTRY_VERIFY(done);
        QVERIFY(!QFileInfo::exists(file));QVERIFY(store.adventures().isEmpty());
        folders.rescan();QTRY_COMPARE(finished.size(),2);QVERIFY(store.adventures().isEmpty());
        put(file);folders.rescan();QTRY_COMPARE(finished.size(),3);QCOMPARE(store.adventures().size(),1);
        QCOMPARE(store.adventures()[0].id,original.adventure.id);QCOMPARE(store.registrations().size(),1);
    }
    void trashPlaylistKeepsDiscDependenciesOutOfTheWheel() {
        QTemporaryDir dir;put(dir.filePath("psx/Disc 1.chd"));
        const auto path=dir.filePath("psx/Story.m3u"),trash=dir.filePath("psx/.traineros-trash/id/Story.m3u");
        put(trash,"Disc 1.chd\n");
        AdventureRegistration record;record.adventure.id="story";record.adventure.platformId="psx";
        record.contentPath=path;record.trashPath=trash;record.removed=true;
        const auto scan=scanBatoceraLibrary(dir.path(),{record});
        QVERIFY(scan.entries.isEmpty());
        put(path,"Disc 1.chd\n"); // Interrupted trash intent: still known, never a new registration.
        const auto interrupted=scanBatoceraLibrary(dir.path(),{record});
        QCOMPARE(interrupted.entries.size(),1);QVERIFY(interrupted.entries[0].existing);QVERIFY(interrupted.entries[0].record.removed);
    }
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
