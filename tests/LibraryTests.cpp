#include "core/storage/SessionState.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFile>
#include <QDir>
#include <QUuid>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include "core/repository/CollectionRepository.h"
#include "core/repository/BatoceraLibrary.h"
#include "core/storage/LibraryFileMove.h"

using namespace trainer;
namespace {
void fixtureFile(const QString& path) { QFile f(path); if (f.open(QIODevice::WriteOnly)) f.write("Original test data. Not a game or save.\n"); }
AdventureRegistration candidate(const QString& path) {
    AdventureRegistration value;
    value.adventure.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    value.adventure.worldId = "hoenn"; value.adventure.title = "A new journey";
    value.adventure.adapterId = "unconfigured"; value.adventure.kind = AdventureKind::RomHack;
    value.adventure.additionalWorldIds = {"kanto", "johto"}; value.contentPath = path;
    return value;
}
class Connection {
public:
    QSqlDatabase db;
    explicit Connection(const QString& directory) {
        db = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
        db.setDatabaseName(directory + "/traineros.sqlite3"); db.open();
    }
    ~Connection() { const auto name = db.connectionName(); db.close(); db = {}; QSqlDatabase::removeDatabase(name); }
};
}
class LibraryTests final : public QObject {
    Q_OBJECT
private slots:
    void fileMovePreservesIdentityMetadataAndAdjacentSaves() {
        QTemporaryDir dir;const auto root=dir.filePath("roms/gba");QVERIFY(QDir().mkpath(root));
        const auto rom=root+"/fixture.gba",save=root+"/fixture.sav",xml=root+"/gamelist.xml";
        fixtureFile(rom);fixtureFile(save);fixtureFile(root+"/fixture.ips");
        const QByteArray metadata="<gameList><game id=\"17\"><path>./fixture.gba</path><name>Art name</name><desc>A &amp; B</desc><marquee>./images/logo.png</marquee><custom>keep</custom></game><game><path>./other.gba</path><name>Other</name></game></gameList>";
        {QFile f(xml);QVERIFY(f.open(QIODevice::WriteOnly));f.write(metadata);}
        LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
        auto record=candidate(rom);record.adventure.platformId="gba";record.adventure.domain="multiverse";
        record.adventure.worldId.clear();record.adventure.additionalWorldIds.clear();
        bool done=false;QString error;
        store.saveAdventureAsync(record,this,[&](auto r){error=r.error;done=true;});QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
        const auto id=record.adventure.id;
        BatoceraLibrary folders(store,dir.filePath("roms"));
        LocalFileCatalog files;LibraryToolsController tools(folders);tools.setCatalog(&files);
        tools.beginGame(id);tools.activate(1);QTRY_VERIFY(!tools.busy());QCOMPARE(tools.route(),"folder");
        tools.activate(3);QTRY_VERIFY(!tools.busy()); // GBA platform folder.
        tools.activate(1);tools.applyText("../outside");QVERIFY(!tools.error().isEmpty());QCOMPARE(tools.route(),"folder");
        tools.applyText("Favorites");QCOMPARE(tools.route(),"move-file");
        tools.dispatch(Action::Confirm);QTRY_VERIFY(!tools.busy());QCOMPARE(tools.route(),"folder"); // Cancel is default.
        QVERIFY(QFileInfo::exists(rom));QVERIFY(!QFileInfo::exists(root+"/Favorites"));
        tools.applyText("Favorites");tools.dispatch(Action::Down);tools.dispatch(Action::Confirm);
        QTRY_VERIFY(!tools.busy());QVERIFY2(tools.error().isEmpty(),qPrintable(tools.error()));QVERIFY(!tools.isOpen());
        QTRY_VERIFY(!folders.busy());
        const auto moved=store.registration(id);QVERIFY(moved);QCOMPARE(moved->revision,2);
        QCOMPARE(moved->contentPath,root+"/Favorites/fixture.gba");QCOMPARE(moved->adventure.title,record.adventure.title);
        QVERIFY(!QFileInfo::exists(rom));QVERIFY(!QFileInfo::exists(save));
        QVERIFY(QFileInfo::exists(root+"/Favorites/fixture.sav"));QVERIFY(QFileInfo::exists(root+"/Favorites/fixture.ips"));
        QCOMPARE(store.registrations().size(),1); // Scanner recognizes the same ID.
        {QFile f(xml);QVERIFY(f.open(QIODevice::ReadOnly));const auto data=f.readAll();QVERIFY(data.contains("./Favorites/fixture.gba"));QVERIFY(data.contains("<custom>keep</custom>"));QVERIFY(data.contains("./images/logo.png"));QVERIFY(data.contains("./other.gba"));}
        const auto run=[&](LibraryEdit edit){done=false;store.editLibraryAsync(edit,this,[&](const QString& e){error=e;done=true;});};
        LibraryEdit move{LibraryEditKind::MoveFile,id,2};move.storageRoot=root;move.text=root;
        fixtureFile(rom);run(move);QTRY_VERIFY(done);QVERIFY(error.contains("already contains"));QCOMPARE(store.registration(id)->revision,2);
        QFile::remove(rom);move.text=dir.path();run(move);QTRY_VERIFY(done);QVERIFY(!error.isEmpty());
        move.text=root;move.revision=1;run(move);QTRY_VERIFY(done);QVERIFY(error.contains("changed"));
        move.revision=2;run(move);QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
        QVERIFY(QFileInfo::exists(rom));QVERIFY(QFileInfo::exists(save));QCOMPARE(store.registration(id)->revision,3);
        QVERIFY(!QFileInfo::exists(dir.filePath("traineros.sqlite3.library-move.json")));
    }
    void misplacedPlayStationRomMovesOutOfGba() {
        QTemporaryDir dir;const auto root=dir.filePath("roms");QVERIFY(QDir().mkpath(root+"/gba/images"));QVERIFY(QDir().mkpath(root+"/psx"));
        const auto from=root+"/gba/Crash.chd",to=root+"/psx/Crash.chd";fixtureFile(from);fixtureFile(root+"/gba/images/logo.png");
        {QFile f(root+"/gba/gamelist.xml");QVERIFY(f.open(QIODevice::WriteOnly));f.write("<gameList><game><path>./Crash.chd</path><name>Crash</name><marquee>./images/logo.png</marquee><desc>Keep my description</desc></game></gameList>");}
        auto scan=scanBatoceraLibrary(root,{});QCOMPARE(scan.entries.size(),1); // Wrong-platform ROM isn't hidden.
        LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
        auto record=scan.entries.first().record;bool done=false;QString error;
        store.saveAdventureAsync(record,this,[&](auto r){error=r.error;done=true;});QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
        BatoceraLibrary folders(store,root);bool prepared=false;
        folders.prepareInstallation=[&](AdventureRegistration& r){prepared=true;QCOMPARE(r.adventure.platformId,"psx");QCOMPARE(r.contentPath,to);};
        LibraryEdit edit{LibraryEditKind::MoveFile,record.adventure.id,1};edit.text=root+"/psx";
        done=false;folders.editLibraryAsync(edit,this,[&](const QString& e){error=e;done=true;});QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));QTRY_VERIFY(!folders.busy());
        QVERIFY(prepared);const auto moved=store.registration(record.adventure.id);QVERIFY(moved);
        QCOMPARE(moved->adventure.platformId,"psx");QCOMPARE(moved->contentPath,to);QCOMPARE(moved->revision,2);
        QCOMPARE(store.registrations().size(),1);QVERIFY(QFileInfo::exists(to));QVERIFY(!QFileInfo::exists(from));
        QCOMPARE(folders.artwork(record.adventure.id)["desc"].toString(),"Keep my description");
        QVERIFY(folders.artwork(record.adventure.id)["marquee"].toString().endsWith("gba/images/logo.png"));
        {QFile f(root+"/gba/gamelist.xml");QVERIFY(f.open(QIODevice::ReadOnly));QVERIFY(!f.readAll().contains("Crash.chd"));}
    }
    void interruptedFileMoveRecoversBothSidesOfCommit() {
        for(const bool committed:{false,true}) {
            QTemporaryDir dir;const auto root=dir.filePath("gba");QVERIFY(QDir().mkpath(root+"/next"));
            const auto from=root+"/fixture.gba",to=root+"/next/fixture.gba";
            fixtureFile(from);auto record=candidate(from);record.adventure.platformId="gba";
            {
                LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());bool done=false;
                store.saveAdventureAsync(record,this,[&](auto r){QVERIFY(r.success);done=true;});QTRY_VERIFY(done);
            }
            const QFileInfo file(from);
            const QJsonObject item{{"from",from},{"to",to},{"size",file.size()},{"modified",file.lastModified().toMSecsSinceEpoch()}};
            QJsonObject job{{"version",1},{"id",record.adventure.id},{"revision",1},{"root",QFileInfo(root).canonicalFilePath()},{"files",QJsonArray{item}}};
            const auto xml=root+"/gamelist.xml",newXml=root+"/next/gamelist.xml";
            const QByteArray before="<gameList><game><path>fixture.gba</path></game></gameList>",after="<gameList/>";
            job["xmlFiles"]=QJsonArray{QJsonObject{{"path",xml},{"before",QString::fromLatin1(before.toBase64())},{"after",QString::fromLatin1(after.toBase64())}},
                QJsonObject{{"path",newXml},{"before",""},{"after",QString::fromLatin1(before.toBase64())}}};
            {QFile f(xml);QVERIFY(f.open(QIODevice::WriteOnly));f.write(after);}
            {QFile f(newXml);QVERIFY(f.open(QIODevice::WriteOnly));f.write(before);}
            const auto intent=dir.filePath("traineros.sqlite3.library-move.json");
            {QFile f(intent);QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(job).toJson());}
            QVERIFY(QFile::rename(from,to));
            if(committed){Connection c(dir.path());QSqlQuery q(c.db);q.prepare("UPDATE adventures SET content_path=?,revision=2 WHERE id=?");q.addBindValue(to);q.addBindValue(record.adventure.id);QVERIFY(q.exec());}
            // A collision during recovery must preserve BOTH files and intent.
            fixtureFile(from);
            {Connection c(dir.path());QVERIFY(!recoverLibraryFileMove(c.db).isEmpty());}
            QVERIFY(QFileInfo::exists(from));QVERIFY(QFileInfo::exists(to));QVERIFY(QFileInfo::exists(intent));QFile::remove(from);
            LocalStateStore reopened(dir.path());reopened.open();QTRY_VERIFY(reopened.ready());
            QCOMPARE(reopened.registration(record.adventure.id)->contentPath,committed?to:from);
            QVERIFY(QFileInfo::exists(committed?to:from));QVERIFY(!QFileInfo::exists(committed?from:to));QVERIFY(!QFileInfo::exists(intent));
            {QFile f(xml);QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),committed?after:before);}
            QCOMPARE(QFileInfo::exists(newXml),committed);
        }
    }
    void contextualMenuUsesWheelSelectionAndTrapsInput() {
        QTemporaryDir dir;const auto rom=dir.filePath("fixture.gba");fixtureFile(rom);
        LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
        auto record=candidate(rom);bool done=false;
        store.saveAdventureAsync(record,this,[&](auto result){QVERIFY(result.success);done=true;});QTRY_VERIFY(done);
        MockTrainerRepository profiles;MockAdventureAdapter adapter;DevelopmentPlatformService platform;
        MockPokedexRepository dex;MockHallOfFameRepository archive;MockAchievementProvider achievements;
        ShellController shell(store,profiles,adapter,platform,dex,dex,archive,achievements);
        shell.configureServices(nullptr,&store);
        shell.goToPage(1);shell.worlds()->activate(2);
        QVERIFY(shell.canHoldConfirm());QVERIFY(!shell.canEditWorld());
        shell.dispatch(Action::ContextMenu);QVERIFY(shell.libraryTools()->isOpen());QVERIFY(!shell.canHoldConfirm());
        shell.dispatch(Action::NextFace);QVERIFY(!shell.multiverseFace());
        shell.dispatch(Action::Confirm);QVERIFY(shell.keyboard()->isOpen());
        shell.dispatch(Action::Back);QVERIFY(!shell.keyboard()->isOpen());QVERIFY(shell.libraryTools()->isOpen());
        shell.dispatch(Action::Down);shell.dispatch(Action::Down);shell.dispatch(Action::Confirm);
        QCOMPARE(shell.libraryTools()->route(),"remove");
        shell.dispatch(Action::Confirm);QCOMPARE(shell.libraryTools()->route(),"game"); // Default is non-destructive.
        shell.dispatch(Action::Back);QVERIFY(!shell.libraryTools()->isOpen());QVERIFY(shell.canHoldConfirm());
        shell.dispatch(Action::Confirm);QCOMPARE(shell.worlds()->route(),"detail"); // Short A retains ordinary behavior.
        shell.dispatch(Action::Back);
        shell.settings()->activate(2);QTRY_VERIFY(!shell.settings()->saving());QVERIFY(shell.canEditWorld());
        shell.dispatch(Action::LocalAction);QCOMPARE(shell.libraryTools()->route(),"world");
        shell.dispatch(Action::NextPage);QCOMPARE(shell.page(),2);QVERIFY(!shell.libraryTools()->isOpen());
        shell.goToPage(1);shell.dispatch(Action::ContextMenu);QVERIFY(shell.libraryTools()->isOpen());
        shell.dispatch(Action::SystemMenu);shell.dispatch(Action::Confirm);
        QCOMPARE(shell.service(),"settings");QVERIFY(!shell.libraryTools()->isOpen());
    }
    void contextualEditsDeleteRomAndPreserveIdentity() {
        QTemporaryDir dir; const auto rom=dir.filePath("fixture.gba"),save=dir.filePath("fixture.sav");
        fixtureFile(rom);fixtureFile(save);
        LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
        auto record=candidate(rom);const auto id=record.adventure.id;
        record.adventure.catalogueId="emerald-gba";record.adventure.platformId="gba";
        bool done=false;QString error;
        store.saveAdventureAsync(record,this,[&](auto result){error=result.error;done=true;});
        QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
        const auto run=[&](LibraryEdit edit){
            done=false;store.editLibraryAsync(edit,this,[&](const QString& result){error=result;done=true;});
        };
        run({LibraryEditKind::RenameGame,id,1,"My adventure"});QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
        QCOMPARE(store.registration(id)->adventure.title,"My adventure");QCOMPARE(store.registration(id)->contentPath,rom);
        run({LibraryEditKind::RenameGame,id,1,"Stale"});QTRY_VERIFY(done);QVERIFY(!error.isEmpty());
        LibraryEdit worldEdit{LibraryEditKind::RenameWorld,"hoenn"};worldEdit.text="Our Hoenn";worldEdit.previousName="Hoenn";
        run(worldEdit);QTRY_VERIFY(done);QVERIFY(!error.isEmpty());
        auto preferences=store.preferences();preferences.worldEditing=true;done=false;
        store.savePreferences(preferences,this,[&](auto result){error=result;done=true;});QTRY_VERIFY(done);QVERIFY(error.isEmpty());
        run(worldEdit);QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
        CollectionRepository collection(store);bool renamed=false;
        for(const auto& w:collection.worlds())if(w.id=="hoenn")renamed=w.name=="Our Hoenn";
        QVERIFY(renamed);
        LibraryEdit move{LibraryEditKind::MoveGame,id,2};move.world={"kanto","Kanto",{}};
        run(move);QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
        QCOMPARE(store.registration(id)->adventure.worldId,"kanto");QVERIFY(store.registration(id)->adventure.additionalWorldIds.isEmpty());
        run({LibraryEditKind::RemoveGame,id,3});QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
        QVERIFY(store.adventures().isEmpty());QCOMPARE(store.registrations().size(),1);
        bool missingEdition=false;
        for(const auto& a:collection.adventures())if(a.catalogueId=="emerald-gba")missingEdition=a.collectionOnly;
        QVERIFY(missingEdition); // The full reference collection still includes the missing edition.
        const auto trash=store.registration(id)->trashPath;
        QVERIFY(trash.isEmpty());QVERIFY(!QFileInfo::exists(rom));QVERIFY(QFileInfo::exists(save));
        QVERIFY(!QFileInfo::exists(dir.filePath(".traineros-trash")));
        run({LibraryEditKind::RestoreGame,id,3});QTRY_VERIFY(done);QVERIFY(!error.isEmpty());
        fixtureFile(rom); // A newly added ROM can reuse the same library identity.
        run({LibraryEditKind::RestoreGame,id,3});QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
        QCOMPARE(store.adventures().size(),1);QCOMPARE(store.registration(id)->revision,3);
        QFile restored(rom);QVERIFY(restored.open(QIODevice::ReadOnly));QCOMPARE(restored.readAll(),QByteArray("Original test data. Not a game or save.\n"));restored.close();
        // Rehearse interruption after durable trash intent, before file rename.
        {Connection connection(dir.path());QSqlQuery q(connection.db);q.prepare("INSERT INTO library_removals VALUES(?,?)");q.addBindValue(id);q.addBindValue(dir.filePath("missing-trash/fixture.gba"));QVERIFY(q.exec());}
        run({LibraryEditKind::RestoreGame,id,3});QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
        QVERIFY(QFileInfo::exists(rom));QVERIFY(!store.registration(id)->removed);
        // Legacy trash remains recoverable but never overwrites a newer file.
        const auto oldTrash=dir.filePath("old-trash/fixture.gba");QVERIFY(QDir().mkpath(QFileInfo(oldTrash).absolutePath()));fixtureFile(oldTrash);
        {Connection connection(dir.path());QSqlQuery q(connection.db);q.prepare("INSERT INTO library_removals VALUES(?,?)");q.addBindValue(id);q.addBindValue(oldTrash);QVERIFY(q.exec());}
        run({LibraryEditKind::RestoreGame,id,3});QTRY_VERIFY(done);QVERIFY(!error.isEmpty());QVERIFY(QFileInfo::exists(oldTrash));
        QVERIFY(QFile::remove(rom));
        run({LibraryEditKind::RestoreGame,id,3});QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
        // Shared discs/aliases must not be removed as an independent game.
        fixtureFile(dir.filePath("set.m3u"));
        {QFile playlist(dir.filePath("set.m3u"));QVERIFY(playlist.open(QIODevice::WriteOnly));playlist.write("fixture.gba\n");}
        run({LibraryEditKind::RemoveGame,id,3});QTRY_VERIFY(done);QVERIFY(!error.isEmpty());QVERIFY(QFileInfo::exists(rom));
        QVERIFY(QFile::remove(dir.filePath("set.m3u")));
        QVERIFY(QFile::rename(rom,rom+".offline"));
        run({LibraryEditKind::RemoveGame,id,3});QTRY_VERIFY(done);QVERIFY(!error.isEmpty());
        QVERIFY(!store.registration(id)->removed);QVERIFY(QFile::rename(rom+".offline",rom));
        const auto now=QDateTime::currentDateTimeUtc();done=false;
        store.saveSessionAsync({"active",id,now,{},{},PlaySessionOutcome::Running},this,[&](auto result){error=result;done=true;});QTRY_VERIFY(done);QVERIFY(error.isEmpty());
        run({LibraryEditKind::RemoveGame,id,3});QTRY_VERIFY(done);QVERIFY(error.contains("running"));QVERIFY(QFileInfo::exists(rom));
    }
    void maintenanceConfiguresOnlyUnconfiguredExistingRecords() {
        QTemporaryDir dir; const auto data=dir.filePath("data"), manifest=dir.filePath("manifest.json");
        const auto content=dir.filePath("fixture.pce"); fixtureFile(content);
        AdventureRegistration saved;
        {
            LocalStateStore store(data);store.open();QTRY_VERIFY(store.ready());
            saved.adventure.id="cartridge";saved.adventure.title="Owner title";saved.adventure.description="Owner notes";
            saved.adventure.domain="multiverse";saved.adventure.platformId="pcengine";saved.adventure.adapterId="unconfigured";
            saved.contentPath=content;saved.integrationConfig={{"owner-field",true}};
            bool done=false;store.saveAdventureAsync(saved,this,[&](auto r){QVERIFY(r.success);done=true;});QTRY_VERIFY(done);
        }
        const auto integrations=data+"/integrations";QVERIFY(QDir().mkpath(integrations));
        fixtureFile(integrations+"/mednafen_pce_fast_libretro.so");fixtureFile(integrations+"/retroarch.cfg");
        QFile config(integrations+"/retroarch.json");QVERIFY(config.open(QIODevice::WriteOnly));
        config.write(QJsonDocument(QJsonObject{{"version",1},{"program",QCoreApplication::applicationFilePath()},
            {"prefixArguments",QJsonArray{}},{"coresDirectory",integrations},{"configFile",integrations+"/retroarch.cfg"}}).toJson());config.close();
        QFile file(manifest);QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(QJsonObject{{"version",1},{"entries",QJsonArray{QJsonObject{
            {"id","cartridge"},{"title","Manifest title"},{"platform","pcengine"},{"path",content}}}}}).toJson());file.close();
        const auto run=[&](bool setup) {
            QStringList args{data,manifest};if(setup)args<<"--configure-existing";
            QProcess child;child.start(QCoreApplication::applicationDirPath()+"/trainer_library_import",args);
            return child.waitForFinished(30000) && child.exitStatus()==QProcess::NormalExit && child.exitCode()==0;
        };
        QVERIFY(run(false));
        {
            LocalStateStore store(data);store.open();QTRY_VERIFY(store.ready());
            QCOMPARE(store.registration("cartridge")->adventure.adapterId,"unconfigured");
        }
        QVERIFY(run(true));QVERIFY(run(true));
        {
            LocalStateStore store(data);store.open();QTRY_VERIFY(store.ready());saved=*store.registration("cartridge");
            QCOMPARE(saved.revision,2);QCOMPARE(saved.adventure.title,"Owner title");QCOMPARE(saved.adventure.description,"Owner notes");
            QVERIFY(saved.integrationConfig["owner-field"].toBool());QCOMPARE(saved.integrationConfig["core"].toString(),"mednafen_pce_fast");
            QCOMPARE(saved.adventure.adapterId,"retroarch");
            saved.adventure.adapterId="owner-adapter";bool done=false;
            store.saveAdventureAsync(saved,this,[&](auto r){QVERIFY(r.success);done=true;});QTRY_VERIFY(done);
        }
        QVERIFY(run(true));
        LocalStateStore reopened(data);reopened.open();QTRY_VERIFY(reopened.ready());
        QCOMPARE(reopened.registration("cartridge")->adventure.adapterId,"owner-adapter");
        QCOMPARE(reopened.registration("cartridge")->revision,3);
    }
    void maintenanceImportIsIdempotentAndRejectsIdentityCollisions() {
        QTemporaryDir dir;const auto content=dir.filePath("fixture.gba");fixtureFile(content);
        const auto manifest=dir.filePath("manifest.json"),data=dir.filePath("data");
        const auto writeManifest=[&](const QString& id) {
            QFile f(manifest);QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QJsonDocument(QJsonObject{{"version",1},{"entries",QJsonArray{QJsonObject{{"id",id},{"title","General fixture"},{"platform","gba"},{"path",content}}}}}).toJson());
        };
        const auto run=[&] {
            QProcess child;child.start(QCoreApplication::applicationDirPath()+"/trainer_library_import",{data,manifest});
            if(!child.waitForFinished(30000))return -100;
            return child.exitStatus()==QProcess::NormalExit?child.exitCode():-101;
        };
        writeManifest("shared-title");QCOMPARE(run(),0);QCOMPARE(run(),0);
        writeManifest("duplicate-file");QVERIFY(run()!=0);
        LocalStateStore store(data);store.open();QTRY_VERIFY(store.ready());
        QCOMPARE(store.adventures().size(),1);QCOMPARE(store.adventures().front().domain,"multiverse");
        QVERIFY(store.adventures().front().worldId.isEmpty());QCOMPARE(store.worlds().size(),9);
        writeManifest("shared-title");QVERIFY(run()!=0); // Live shell/store lock is respected.
        QFile original(content);QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),QByteArray("Original test data. Not a game or save.\n"));
    }
    void multiverseRegistrationPersistsWithoutInventingWorlds() {
        QTemporaryDir dir; const auto path=dir.filePath("fixture.gba");fixtureFile(path);
        QString id;
        {
            LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
            auto value=candidate(path);id=value.adventure.id;
            value.adventure.domain="multiverse";value.adventure.worldId.clear();
            value.adventure.additionalWorldIds.clear();value.adventure.platformId="gba";
            bool done=false;QString error;
            store.saveAdventureAsync(value,this,[&](auto r){error=r.error;done=true;});QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));
            QCOMPARE(store.worlds().size(),9);QCOMPARE(store.registration(id)->adventure.domain,"multiverse");
            UnconfiguredAdventureAdapter adapter;WorldsController worlds(store,adapter);
            for(int i=0;i<9;++i){worlds.activate(i);QVERIFY(worlds.adventures().isEmpty());worlds.dispatch(Action::Back);}
            auto changed=*store.registration(id);changed.adventure.domain="pokemon";changed.adventure.worldId="hoenn";
            done=false;store.saveAdventureAsync(changed,this,[&](auto r){error=r.error;done=true;});QTRY_VERIFY(done);QVERIFY(!error.isEmpty());
            const auto now=QDateTime::currentDateTimeUtc();done=false;
            store.saveSessionAsync({"multiverse-session",id,now,{},{},PlaySessionOutcome::Running},this,[&](auto e){error=e;done=true;});
            QTRY_VERIFY(done);QVERIFY2(error.isEmpty(),qPrintable(error));done=false;
            store.saveSessionAsync({"multiverse-session",id,now,now,1,PlaySessionOutcome::Returned},this,[&](auto e){error=e;done=true;});
            QTRY_VERIFY(done);QVERIFY(error.isEmpty());QVERIFY(store.home().activeAdventureId.isEmpty());
        }
        LocalStateStore reopened(dir.path());reopened.open();QTRY_VERIFY(reopened.ready());
        const auto record=reopened.registration(id);QVERIFY(record);QVERIFY(record->adventure.worldId.isEmpty());
        QCOMPARE(record->adventure.domain,"multiverse");QCOMPARE(reopened.recordedSeconds(id),std::optional<qint64>(1));
    }
    void libraryIdentityRelationshipsAndReopening() {
        QTemporaryDir dir; const auto file = dir.path() + "/original fixture.bin"; fixtureFile(file);
        auto value = candidate(file); value.integrationConfig = {{"future-adapter-field", "literal `name` $(value)"}};
        {
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
            QCOMPARE(store.worlds().size(), 9); QVERIFY(store.adventures().isEmpty()); QVERIFY(store.resumePoints().isEmpty());
            QVERIFY(store.home().activeAdventureId.isEmpty()); QVERIFY(!store.home().badges.has_value());
            bool done = false;
            store.saveAdventureAsync(value, this, [&](auto result) { QVERIFY2(result.success, qPrintable(result.error)); QCOMPARE(result.revision, 1); done = true; });
            QVERIFY(store.adventures().isEmpty()); QTRY_VERIFY(done);
            auto saved = store.registration(value.adventure.id); QVERIFY(saved); QCOMPARE(saved->revision, 1);
            QVERIFY(!saved->adventure.status); QVERIFY(!saved->adventure.badges); QVERIFY(!saved->adventure.caught);
            UnconfiguredAdventureAdapter adapter;
            WorldsController worlds(store, adapter); worlds.activate(0); // Kanto contains a secondary relationship.
            QCOMPARE(worlds.adventures().size(), 1); worlds.activate(0); QCOMPARE(worlds.detail()["id"].toString(), value.adventure.id);
            QVERIFY(!worlds.actions().first().toMap()["enabled"].toBool()); // Not a falsely playable sample.
            const auto state = worlds.navigationState(); WorldsController restored(store, adapter); restored.restoreNavigation(state);
            QCOMPARE(restored.region()["id"].toString(), "kanto"); QCOMPARE(restored.detail()["id"].toString(), value.adventure.id);
            saved->adventure.title = "Renamed journey"; saved->adventure.additionalWorldIds = {"kanto", "kanto", "hoenn"};
            done = false; store.saveAdventureAsync(*saved, this, [&](auto result) { QVERIFY(result.success); done = true; }); QTRY_VERIFY(done);
            QCOMPARE(store.registration(value.adventure.id)->revision, 2);
        }
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
        QCOMPARE(store.adventures().size(), 1); const auto saved = *store.registration(value.adventure.id);
        QCOMPARE(saved.adventure.title, "Renamed journey"); QCOMPARE(saved.contentPath, file);
        QCOMPARE(saved.integrationConfig, value.integrationConfig); QCOMPARE(saved.adventure.additionalWorldIds, QStringList{"kanto"});
        QFile original(file); QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), QByteArray("Original test data. Not a game or save.\n"));
    }
    void staleEditMissingFileAndWorldRollback() {
        QTemporaryDir dir; const auto file = dir.path() + "/fixture.bin"; fixtureFile(file);
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
        auto value = candidate(file); bool done = false;
        store.saveAdventureAsync(value, this, [&](auto r) { QVERIFY(r.success); done = true; }); QTRY_VERIFY(done);
        value = *store.registration(value.adventure.id);
        auto stale = value; value.adventure.title = "Current title";
        done = false; store.saveAdventureAsync(value, this, [&](auto r) { QVERIFY(r.success); done = true; }); QTRY_VERIFY(done);
        stale.newWorld = World{"custom-island", "New Island", {}}; stale.adventure.worldId = "custom-island";
        bool failed = false; store.saveAdventureAsync(stale, this, [&](auto r) { failed = !r.success; }); QTRY_VERIFY(failed);
        QCOMPARE(store.worlds().size(), 9); QCOMPARE(store.registration(value.adventure.id)->adventure.title, "Current title");
        value = *store.registration(value.adventure.id); value.adventure.additionalWorldIds = {"missing-world"};
        failed = false; store.saveAdventureAsync(value, this, [&](auto r) { failed = !r.success; }); QTRY_VERIFY(failed);
        QCOMPARE(store.registration(value.adventure.id)->adventure.additionalWorldIds, QStringList({"kanto", "johto"}));
        value = *store.registration(value.adventure.id);
        QVERIFY(QFile::rename(file, file + ".moved"));
        failed = false; store.saveAdventureAsync(value, this, [&](auto r) { failed = !r.success; }); QTRY_VERIFY(failed);
        value.contentPath = file + ".moved"; value.newWorld = World{"custom-island", "New Island", {}}; value.adventure.worldId = "custom-island";
        done = false; store.saveAdventureAsync(value, this, [&](auto r) { QVERIFY2(r.success, qPrintable(r.error)); done = true; }); QTRY_VERIFY(done);
        QCOMPARE(store.worlds().size(), 10); QCOMPARE(store.registration(value.adventure.id)->adventure.worldId, "custom-island");
    }
    void migrationFromVersionOnePreservesPersonalData() {
        QTemporaryDir dir;
        {
            Connection connection(dir.path()); QSqlQuery q(connection.db);
            QVERIFY(q.exec("CREATE TABLE trainer_profile(slot INTEGER PRIMARY KEY,id TEXT,name TEXT,emblem TEXT,favorite TEXT,created_at TEXT)"));
            QVERIFY(q.exec("INSERT INTO trainer_profile VALUES(1,'trainer-id','ERI','leaf','eevee','2026-09-06T12:00:00.000Z')"));
            QVERIFY(q.exec("CREATE TABLE pokedex_favorites(entry_id TEXT PRIMARY KEY)")); QVERIFY(q.exec("INSERT INTO pokedex_favorites VALUES('eevee')"));
            QVERIFY(q.exec("CREATE TABLE shell_state(scope TEXT PRIMARY KEY,payload BLOB)"));
            QVERIFY(q.exec("INSERT INTO shell_state VALUES('prototype-library-v1','{\"version\":1,\"page\":\"worlds\"}')"));
            QVERIFY(q.exec("PRAGMA user_version=1"));
        }
        {
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
            QCOMPARE(store.load()->id, "trainer-id"); QCOMPARE(store.load()->name, "ERI"); QVERIFY(store.progress("eevee").favorite);
            QCOMPARE(store.worlds().size(), 9); QVERIFY(store.adventures().isEmpty()); QVERIFY(store.navigation().isEmpty());
        }
        Connection connection(dir.path()); QSqlQuery q(connection.db);
        QVERIFY(q.exec("PRAGMA user_version")); QVERIFY(q.next()); QCOMPARE(q.value(0).toInt(), 13);
        QVERIFY(q.exec("SELECT payload FROM shell_state WHERE scope='prototype-library-v1'")); QVERIFY(q.next()); QVERIFY(!q.value(0).toString().isEmpty());
    }
    void filePagingCancellationAndUnavailableDirectory() {
        QTemporaryDir dir; QVERIFY(QDir().mkpath(dir.path() + "/folder"));
        for (int i = 0; i < 90; ++i) fixtureFile(dir.path() + QString("/entry-%1.bin").arg(i, 3, 10, QChar('0')));
        LocalFileCatalog catalog; FilePickerController picker(&catalog);
        picker.begin(dir.path()); QTRY_VERIFY(!picker.busy()); QCOMPARE(picker.rows().size(), 80);
        QVERIFY(picker.rows().first().toMap()["title"].toString() == "folder");
        for (int i = 0; i < 80; ++i) picker.dispatch(Action::Down);
        QCOMPARE(picker.zone(), QString("actions"));
        picker.dispatch(Action::Up);
        QCOMPARE(picker.zone(), QString("list")); QCOMPARE(picker.rowIndex(), 79);
        picker.activate(3, "actions"); QTRY_VERIFY(!picker.busy()); QCOMPARE(picker.rows().size(), 11);
        picker.activate(2, "actions"); picker.cancel(); QTest::qWait(100); QVERIFY(!picker.isOpen());
        picker.begin(dir.path() + "/missing"); QTRY_VERIFY(!picker.busy()); QVERIFY(!picker.error().isEmpty()); QCOMPARE(picker.zone(), "actions");
        picker.dispatch(Action::Back); QVERIFY(!picker.isOpen());
    }
    void settingsAreCommittedAndRetryable() {
        QTemporaryDir dir;
        {
            LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
            SettingsController settings; settings.setRepository(&store);
            settings.activate(0); QVERIFY(settings.saving()); QCOMPARE(settings.theme(), "turquoise");
            QTRY_VERIFY(!settings.saving()); QCOMPARE(settings.theme(), "red");
            Connection connection(dir.path()); QSqlQuery q(connection.db); QVERIFY(q.exec("BEGIN IMMEDIATE"));
            settings.activate(1); QTRY_VERIFY(!settings.saving()); QVERIFY(!settings.error().isEmpty()); QVERIFY(!settings.reducedMotion());
            QVERIFY(q.exec("ROLLBACK")); settings.activate(1); QTRY_VERIFY(!settings.saving()); QVERIFY(settings.reducedMotion());
            settings.selectCategory(0,true);
            settings.dispatch(Action::Down); settings.dispatch(Action::Confirm);
            QTRY_VERIFY(!settings.saving()); QVERIFY(!settings.reducedMotion());
            QCOMPARE(settings.rowFocus(),1);
            settings.dispatch(Action::Back); QVERIFY(!settings.controlsFocused());
            settings.selectCategory(3,true); settings.activateRow(0);
            QCOMPARE(settings.category(),3); QVERIFY(settings.controlsFocused());
            settings.dispatch(Action::Back); QCOMPARE(settings.category(),3);
            settings.activate(1); QTRY_VERIFY(!settings.saving()); QVERIFY(settings.reducedMotion());
            settings.selectCategory(2,true); QVERIFY(settings.videoPreviews());
            settings.dispatch(Action::Confirm);QTRY_VERIFY(!settings.saving());QVERIFY(!settings.videoPreviews());
            settings.dispatch(Action::Right);QTRY_VERIFY(!settings.saving());QVERIFY(settings.videoPreviews());
            settings.dispatch(Action::Left);QTRY_VERIFY(!settings.saving());QVERIFY(!settings.videoPreviews());
        }
        LocalStateStore store(dir.path()); store.open(); QTRY_VERIFY(store.ready());
        QCOMPARE(store.preferences().theme, "red"); QVERIFY(store.preferences().reducedMotion);
        QVERIFY(!store.preferences().videoPreviews);
    }
};
QTEST_GUILESS_MAIN(LibraryTests)
#include "LibraryTests.moc"
