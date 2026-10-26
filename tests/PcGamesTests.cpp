#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSysInfo>
#include <QImage>
#include <QJsonArray>
#include "features/pc/PcRuntime.h"
#include "core/repository/BatoceraLibrary.h"
#include <archive.h>
#include <archive_entry.h>
#include "platform/files/ArchiveExtractor.h"
#include "features/pc/PcGamesController.h"
#include "features/settings/GamePerformance.h"
#include "features/library/LibraryToolsController.h"
#include "features/worlds/MultiversePresentation.h"
#include "integrations/adventure/PcAdventureAdapter.h"
using namespace trainer;
namespace {
QByteArray nativeHeader() {
    QByteArray bytes(64,0);bytes.replace(0,4,QByteArray("\x7f" "ELF",4));bytes[5]=1;
    bytes[18]=QSysInfo::currentCpuArchitecture()=="arm64"?183:62;return bytes;
}
bool makeArchive(const QString& file,const QList<QPair<QString,QByteArray>>& entries,bool seven=false,bool link=false) {
    auto* a=archive_write_new();
    if(seven)archive_write_set_format_7zip(a);else archive_write_set_format_zip(a);
    bool ok=archive_write_open_filename(a,QFile::encodeName(file).constData())==ARCHIVE_OK;
    for(const auto& [name,bytes]:entries) {
        auto* e=archive_entry_new();archive_entry_set_pathname(e,name.toUtf8().constData());
        archive_entry_set_filetype(e,link?AE_IFLNK:AE_IFREG);archive_entry_set_perm(e,0755);
        if(link)archive_entry_set_symlink(e,"../outside");else archive_entry_set_size(e,bytes.size());
        ok=ok&&archive_write_header(a,e)==ARCHIVE_OK;
        if(ok&&!link)ok=archive_write_data(a,bytes.constData(),bytes.size())==bytes.size();
        archive_entry_free(e);
    }
    ok=archive_write_close(a)==ARCHIVE_OK&&ok;archive_write_free(a);return ok;
}
}
class PcLibrary : public LibraryRepository {
public:
    QList<AdventureRegistration> records;
    bool editable() const override { return true; }
    QList<World> worlds() const override { return {}; }
    QList<Adventure> adventures() const override {QList<Adventure> a;for(const auto& r:records)a.append(r.adventure);return a;}
    QList<ResumePoint> resumePoints() const override { return {}; }
    HomeSnapshot home() const override { return {}; }
    std::optional<AdventureRegistration> registration(const QString& id) const override {for(const auto& r:records)if(r.adventure.id==id)return r;return {};}
    void saveAdventureAsync(const AdventureRegistration& r,QObject*,std::function<void(LibraryWriteResult)> done) override {
        auto next=r;next.revision++;
        for(auto& existing:records)if(existing.adventure.id==r.adventure.id){if(existing.revision!=r.revision){done({false,"Revision conflict"});return;}existing=next;done({true,{},next.revision});return;}
        records.append(next);done({true,{},next.revision});
    }
};
class PcGamesTests : public QObject {
    Q_OBJECT
private slots:
    void universalGameSettingsPreservesBrowserContextActions() {
        PcLibrary library;AdventureRegistration r;r.adventure.id="emulator:game";r.adventure.title="Game";library.records<<r;
        LibraryToolsController tools(library);QSignalSpy settings(&tools,&LibraryToolsController::gameSettingsRequested),browser(&tools,&LibraryToolsController::browserActionRequested);
        tools.beginBrowsing(r.adventure.id,"grid");tools.activate(6);QCOMPARE(settings.size(),1);QCOMPARE(settings[0][0].toString(),r.adventure.id);
        tools.beginBrowsing(r.adventure.id,"grid");tools.activate(7);QCOMPARE(browser.size(),1);QCOMPARE(browser[0][0].toString(),QString("search"));
    }
    void gameProfilesAreOwnerScopedAndWrapTheActualCommand() {
        QTemporaryDir d;PcLibrary library;AdventureRegistration r;r.adventure.id="pc:profile";r.adventure.adapterId="pc-local";r.integrationConfig={{"runner","proton"}};library.records<<r;
        GamePerformance profile(library);profile.configure(d.path(),"one");profile.begin(r.adventure.id);
        profile.selectPage(1);profile.adjust(0,1);profile.adjust(0,1);profile.adjust(1,1);profile.save();
        QCOMPARE(profile.snapshot(r.adventure.id)["renderer"].toString(),QString("opengl"));
        ProcessCommand c;c.program="/original/game";c.arguments={"two words","$(literal)"};
        bool prepared=false,finalized=false;
        c.prepare=[&](ProcessCommand& p,const std::atomic_bool&){prepared=true;p.program="/prepared/game";p.finalize=[&](const ProcessOutcome&){finalized=true;return QString("original finalizer");};return QString();};
        auto wrapped=profile.prepare(c,r.adventure.id);std::atomic_bool cancel=false;
        QVERIFY(wrapped.prepare(wrapped,cancel).isEmpty());QVERIFY(prepared);
        QCOMPARE(wrapped.program,QString("/usr/bin/env"));QVERIFY(wrapped.arguments.contains("PROTON_USE_WINED3D=1"));QVERIFY(wrapped.arguments.contains("PROTON_NO_ESYNC=1"));
        QCOMPARE(wrapped.arguments.last(),QString("$(literal)"));QVERIFY(wrapped.arguments.contains("/prepared/game"));
        QCOMPARE(wrapped.finalize({}),QString("original finalizer"));QVERIFY(finalized);
        profile.configure(d.path(),"two");QVERIFY(profile.snapshot(r.adventure.id).isEmpty());
        profile.configure(d.path(),"one");QVERIFY(!profile.snapshot(r.adventure.id).isEmpty());QVERIFY(profile.snapshot("different-game").isEmpty());
        library.records[0].integrationConfig["runner"]="native";auto native=profile.prepare(c,r.adventure.id);QVERIFY(native.prepare(native,cancel).isEmpty());QCOMPARE(native.program,QString("/prepared/game"));
    }
    void gameProfileUnavailableControlsKeepSavedChoicesAndResetWorks() {
        QTemporaryDir d;PcLibrary library;AdventureRegistration r;r.adventure.id="emu:game";library.records<<r;
        GamePerformance p(library);p.configure(d.path(),"owner");p.begin(r.adventure.id);
        p.setCapabilities({{"profiles",QJsonArray{"Eco","Balanced"}}});p.adjust(0,1);p.save();
        p.setCapabilities({});p.adjust(0,1);QCOMPARE(p.draft()["power"].toString(),QString("Eco"));
        p.reset();p.save();QVERIFY(p.snapshot(r.adventure.id)["power"].toString().isEmpty());
        QVERIFY(!GamePerformance::valid({{"version",1},{"gpu",-1}}));QVERIFY(!GamePerformance::valid({{"version",1},{"renderer","arbitrary"}}));
    }
    void umuRuntimeManifestIsArchitectureBound() {
        QTemporaryDir d;const auto root=d.filePath("GE-aarch64");QDir().mkpath(root+"/files/bin");
        auto write=[](const QString& path,const QByteArray& bytes){QFile f(path);if(!f.open(QIODevice::WriteOnly))return false;return f.write(bytes)==bytes.size();};
        QVERIFY(write(root+"/proton","launcher"));QVERIFY(write(root+"/files/bin/wine",nativeHeader()));QVERIFY(write(d.filePath("umu-run"),"zipapp"));
        QJsonObject c{{"runner","proton"},{"proton",root},{"umu",d.filePath("umu-run")}};
        const auto arch=QSysInfo::currentCpuArchitecture();const auto appid=arch=="arm64"?"4185400":"4183110";
        QVERIFY(write(root+"/toolmanifest.vdf",QByteArray("\"require_tool_appid\" \"")+appid+"\""));QVERIFY(pc::runtimeIssue(c,arch).isEmpty());
        QVERIFY(write(root+"/toolmanifest.vdf","\"require_tool_appid\" \"99999\""));QVERIFY(pc::runtimeIssue(c,arch).startsWith("Unsupported"));
    }
    void settingsPersistIdentityArtworkArgumentsAndRespectConflicts() {
        QTemporaryDir d;PcLibrary library;FilesService files;LocalFileCatalog catalog;
        const auto root=d.filePath("game");QVERIFY(QDir().mkpath(root));QFile exe(root+"/play.exe");QVERIFY(exe.open(QIODevice::WriteOnly));exe.write(nativeHeader());exe.close();QVERIFY(exe.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        AdventureRegistration r;r.adventure.id="pc:65aec49c-67dc-4ee0-84cb-c3148c5f59a1";r.adventure.adapterId="pc-local";r.adventure.title="Original";r.adventure.platformId="pc";r.contentPath=exe.fileName();r.revision=4;
        r.integrationConfig={{"runner","native"},{"installRoot",root},{"workingDirectory",root},{"prefix",root+"/prefix"}};library.records<<r;
        PcGamesController c(library,files);c.setCatalog(&catalog);c.configure(d.filePath("state"),"owner");c.edit(r.adventure.id);QVERIFY(c.editing());
        c.activate(0);QCOMPARE(c.stage(),QString("launch-settings"));QVERIFY(!c.rows()[3].toMap()["enabled"].toBool());
        c.activate(2);c.applyText("--title \"two words\" $(literal)");c.dispatch(Action::Back);c.activate(2);c.activate(0);c.applyText("Renamed");
        QImage picture(60,80,QImage::Format_ARGB32);picture.fill(Qt::blue);QVERIFY(picture.save(d.filePath("cover.png")));
        c.activate(1);c.picker()->navigate(d.path());QTRY_VERIFY(!c.picker()->busy());
        int cover=-1;for(int i=0;i<c.picker()->rows().size();++i)if(c.picker()->rows()[i].toMap()["title"]=="cover.png")cover=i;
        QVERIFY(cover>=0);c.picker()->select(cover);c.picker()->choose();QVERIFY(!c.picker()->isOpen());QVERIFY(!c.artwork().isEmpty());
        QCOMPARE(library.records[0].adventure.title,QString("Original"));c.dispatch(Action::Back);c.activate(4);
        QCOMPARE(library.records.size(),1);QCOMPARE(library.records[0].adventure.title,QString("Renamed"));QCOMPARE(library.records[0].adventure.id,r.adventure.id);
        QCOMPARE(library.records[0].integrationConfig["arguments"].toArray(),QJsonArray({"--title","two words","$(literal)"}));
        QCOMPARE(library.records[0].integrationConfig["prefix"],r.integrationConfig["prefix"]);
        BatoceraLibrary presentation(library,d.filePath("roms"));QCOMPARE(presentation.artwork(r.adventure.id)["cover"].toString(),c.artwork());
        c.close();c.edit(r.adventure.id);QCOMPARE(c.title(),QString("Renamed"));
        c.activate(1);QCOMPARE(c.rows().size(),2);QVERIFY(!c.rows()[0].toMap()["title"].toString().contains("EXE"));c.dispatch(Action::Back);
        c.settingsGuard=[]{return QString("Game is running");};c.activate(4);QCOMPARE(c.message(),QString("Game is running"));c.settingsGuard={};
        library.records[0].revision++;c.activate(4);QVERIFY(c.message().contains("changed"));
        c.close();c.begin();QVERIFY(!c.editing());QCOMPARE(c.stage(),QString("source"));
    }
    void runtimeDiscoveryAndPinsStaySeparateForTwoGames() {
        QTemporaryDir d;PcLibrary library;FilesService files;const auto state=d.filePath("state");
        const auto arch=QSysInfo::currentCpuArchitecture();const auto suffix=arch=="arm64"?"-aarch64":QString();
        QVERIFY(QDir().mkpath(state+"/runtimes/umu/1.4.4"));QFile umu(state+"/runtimes/umu/1.4.4/umu-run");QVERIFY(umu.open(QIODevice::WriteOnly));umu.close();
        for(const auto& name:{"Runner-1","Runner-2","Broken"}) {
            const auto path=state+"/runtimes/proton/"+name+suffix;QVERIFY(QDir().mkpath(path+"/files/bin"));
            QFile proton(path+"/proton");QVERIFY(proton.open(QIODevice::WriteOnly));proton.close();
            QFile wine(path+"/files/bin/wine");QVERIFY(wine.open(QIODevice::WriteOnly));wine.write(nativeHeader());wine.close();
            if(QString(name)!="Broken"){QFile manifest(path+"/toolmanifest.vdf");QVERIFY(manifest.open(QIODevice::WriteOnly));manifest.write("standalone fixture");}
        }
        const auto choices=pc::runtimes(state,d.path(),arch);QCOMPARE(choices.size(),3);
        int valid=0;for(const auto& value:choices)if(value.toMap()["enabled"].toBool())++valid;QCOMPARE(valid,2);
        QJsonObject runtime=QJsonObject::fromVariantMap(choices[0].toMap()["config"].toMap());QVERIFY(pc::runtimeIssue(runtime,arch=="arm64"?"x86_64":"arm64").contains("Unsupported"));
        for(int i=0;i<2;++i) {
            const auto root=d.filePath("game"+QString::number(i));QVERIFY(QDir().mkpath(root));QFile exe(root+"/play.exe");QVERIFY(exe.open(QIODevice::WriteOnly));
            QByteArray pe(100,0);pe[0]='M';pe[1]='Z';pe[60]=64;pe.replace(64,4,QByteArray("PE\0\0",4));exe.write(pe);exe.close();
            AdventureRegistration r;r.adventure.id="pc:"+QString::number(i);r.adventure.title="Game";r.adventure.adapterId="pc-local";r.contentPath=exe.fileName();r.integrationConfig=runtime;
            r.integrationConfig["installRoot"]=root;r.integrationConfig["workingDirectory"]=root;r.integrationConfig["prefix"]=root+"/prefix";library.records<<r;
        }
        const auto other=library.records[1].integrationConfig;PcGamesController c(library,files);c.configure(state,"owner");c.edit("pc:0");c.activate(0);c.activate(3);
        int choice=-1;for(int i=0;i<c.rows().size();++i)if(c.rows()[i].toMap()["title"]=="Runner-1"+suffix)choice=i;QVERIFY(choice>=0);c.activate(choice);c.dispatch(Action::Back);c.activate(4);
        QVERIFY(library.records[0].integrationConfig["proton"].toString().endsWith("Runner-1"+suffix));QCOMPARE(library.records[1].integrationConfig,other);
        QVERIFY(!QFileInfo(library.records[0].integrationConfig["prefix"].toString()).exists());
        c.close();c.edit("pc:0");c.activate(0);c.activate(4);c.dispatch(Action::Back);c.activate(4);QCOMPARE(library.records[0].integrationConfig["proton"],runtime["proton"]);
    }
    void twoEntryChoicesAndArchiveImportSurvivesReopening() {
        PcLibrary library;FilesService files;QTemporaryDir d;
        const auto zip=d.filePath("Ready game.zip");QVERIFY(makeArchive(zip,{{"My game/play.exe",nativeHeader()},{"My game/data.txt","adjacent assets"}}));
        QVERIFY(QDir().mkpath(d.filePath("storage")));
        QString executable;
        {
            PcGamesController c(library,files);c.configure(d.filePath("state"),"owner");c.begin();
            QCOMPARE(c.rows().size(),2);c.activate(1);c.activate(1);
            c.picker()->selected(zip);QCOMPARE(c.stage(),QString("storage"));
            c.picker()->selected(d.filePath("storage"));QTRY_VERIFY_WITH_TIMEOUT(!c.busy(),10000);
            QCOMPARE(c.stage(),QString("candidates"));QCOMPARE(c.rows().size(),3);QVERIFY(library.records.isEmpty());
            c.activate(0);QCOMPARE(c.stage(),QString("confirm"));c.applyText("My imported game");
        }
        {
            PcGamesController c(library,files);c.configure(d.filePath("state"),"owner");c.begin();
            QCOMPARE(c.stage(),QString("failed"));c.activate(0);QTRY_VERIFY(!c.busy());c.activate(0);c.activate(2);
            QCOMPARE(c.stage(),QString("done"));QCOMPARE(library.records.size(),1);
            const auto& r=library.records.first();QCOMPARE(r.adventure.title,QString("My imported game"));
            executable=r.contentPath;QVERIFY(executable.startsWith(d.filePath("storage")+'/'));
            QVERIFY(QFile::exists(QFileInfo(executable).absolutePath()+"/data.txt"));QVERIFY(QFile::exists(zip));
        }
        PcGamesController c(library,files);c.configure(d.filePath("state"),"owner");c.begin();QCOMPARE(c.stage(),QString("source"));
    }
    void extractionFormatsAndBoundedWrites() {
        QTemporaryDir d;
        for(bool seven:{false,true}) {
            const auto archive=d.filePath(seven?"data.7z":"data.zip"),target=d.filePath(seven?"seven":"zip");QVERIFY(QDir().mkpath(target));
            QVERIFY(makeArchive(archive,{{"Game/nested/file.txt","contents"}},seven));
            double progress=0;const auto error=ArchiveExtractor::extract(archive,target,[]{return false;},[&](double p){progress=p;});
            QVERIFY2(error.isEmpty(),qPrintable(error));QCOMPARE(progress,1.0);QVERIFY(QFile::exists(target+"/Game/nested/file.txt"));
            QVERIFY(!ArchiveExtractor::extract(archive,target,[]{return false;},[](double){}).isEmpty()); // Never overwrite.
        }
    }
    void realRar5AndRar4LinkBoundary() {
        QTemporaryDir d;
        QVERIFY(QFile::copy(":/pc-fixtures/rar5.rar",d.filePath("rar5.rar")));
        QVERIFY(QDir().mkpath(d.filePath("five")));
        const auto error=ArchiveExtractor::extract(d.filePath("rar5.rar"),d.filePath("five"),[]{return false;},[](double){});
        QVERIFY2(error.isEmpty(),qPrintable(error));QFile text(d.filePath("five/helloworld.txt"));QVERIFY(text.open(QIODevice::ReadOnly));
        QCOMPARE(text.readAll(),QByteArray("hello libarchive test suite!\n"));
        QVERIFY(QFile::copy(":/pc-fixtures/rar4-link.rar",d.filePath("rar4.rar")));QVERIFY(QDir().mkpath(d.filePath("four")));
        QVERIFY(ArchiveExtractor::extract(d.filePath("rar4.rar"),d.filePath("four"),[]{return false;},[](double){}).contains("link"));
        QVERIFY(!QFile::exists(d.filePath("four/testlink")));
    }
    void rejectTraversalAbsoluteLinksAndDuplicateNames_data() {
        QTest::addColumn<QStringList>("paths");QTest::addColumn<bool>("link");
        QTest::newRow("traversal")<<QStringList{"../escaped"}<<false;
        QTest::newRow("absolute")<<QStringList{"/tmp/escaped"}<<false;
        QTest::newRow("windows")<<QStringList{"C:\\escaped"}<<false;
        QTest::newRow("backslash")<<QStringList{"..\\escaped"}<<false;
        QTest::newRow("device")<<QStringList{"NUL.txt"}<<false;
        QTest::newRow("duplicate")<<QStringList{"file","FILE"}<<false;
        QTest::newRow("link")<<QStringList{"linked"}<<true;
    }
    void rejectTraversalAbsoluteLinksAndDuplicateNames() {
        QFETCH(QStringList,paths);QFETCH(bool,link);QTemporaryDir d;
        QList<QPair<QString,QByteArray>> entries;for(const auto& path:paths)entries.append({path,"fixture"});
        QVERIFY(makeArchive(d.filePath("bad.zip"),entries,false,link));QVERIFY(QDir().mkpath(d.filePath("out")));
        QVERIFY(!ArchiveExtractor::extract(d.filePath("bad.zip"),d.filePath("out"),[]{return false;},[](double){}).isEmpty());
        QVERIFY(!QFile::exists(d.filePath("escaped")));
    }
    void expandedSizeLimitAndCancellation() {
        QTemporaryDir d;QVERIFY(makeArchive(d.filePath("large.zip"),{{"big",QByteArray(1024*1024,'a')}}));
        QVERIFY(QDir().mkpath(d.filePath("out")));
        auto error=ArchiveExtractor::extract(d.filePath("large.zip"),d.filePath("out"),[]{return false;},[](double){},1024);
        QVERIFY(error.contains("limit"));QVERIFY(!QFile::exists(d.filePath("out/big")));
        error=ArchiveExtractor::extract(d.filePath("large.zip"),d.filePath("out"),[]{return true;},[](double){});
        QVERIFY(error.contains("cancelled"));QVERIFY(!QFile::exists(d.filePath("out/big")));
    }
    void cancelledScanCannotAdvanceWizard() {
        QTemporaryDir d;QFile game(d.filePath("game.exe"));QVERIFY(game.open(QIODevice::WriteOnly));game.write(nativeHeader());game.close();
        PcLibrary library;FilesService files;PcGamesController flow(library,files);flow.configure(d.filePath("state"),"owner");flow.begin();
        flow.picker()->selected(d.path());QVERIFY(flow.busy());flow.activate(1);
        QTest::qWait(150);QCOMPARE(flow.stage(),QString("failed"));QVERIFY(!flow.busy());QVERIFY(library.records.isEmpty());
    }
    void installerSystemExecutablesAreNotCandidates() {
        QTemporaryDir d;QVERIFY(QDir().mkpath(d.filePath("windows/system32")));
        QFile game(d.filePath("game.exe"));QVERIFY(game.open(QIODevice::WriteOnly));game.write(nativeHeader());game.close();game.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        QVERIFY(QFile::copy(game.fileName(),d.filePath("windows/system32/tool.exe")));
        const auto rows=PcGamesController::inspect(d.path(),true)["candidates"].toList();QCOMPARE(rows.size(),1);
        QCOMPARE(rows.first().toMap()["title"].toString(),QString("game.exe"));
    }
    void installerPreparationNeverRegistersInstallerAsGame() {
        QTemporaryDir d;QVERIFY(QDir().mkpath(d.filePath("storage")));
        QFile installer(d.filePath("setup.exe"));QVERIFY(installer.open(QIODevice::WriteOnly));installer.write(nativeHeader());installer.close();
        installer.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        PcLibrary library;FilesService files;PcGamesController flow(library,files);flow.configure(d.filePath("state"),"owner");flow.begin();
        flow.activate(0);flow.picker()->selected(installer.fileName());QCOMPARE(flow.stage(),QString("storage"));
        flow.picker()->selected(d.filePath("storage"));QCOMPARE(flow.stage(),QString("install-ready"));QVERIFY(library.records.isEmpty());
        flow.installationGuard=[] {return QString("Isolated test: do not launch.");};flow.activate(0);
        QVERIFY(!flow.installing());QVERIFY(library.records.isEmpty());QCOMPARE(flow.stage(),QString("install-ready"));
    }
    void emptyPcIsAvailableWithoutRomExtensionRegistration() {
        PcLibrary library;PcAdventureAdapter adapter;MultiversePresentation view(library,adapter);
        bool found=false;for(const auto& row:view.allSystems())if(row.toMap()["id"]=="pc")found=true;
        QVERIFY(found);view.openSystem("pc");QCOMPARE(view.route(),QString("games"));QVERIFY(view.games().isEmpty());
    }
    void copyFolderThenExplicitlyConfirmOnlyTheCopiedExecutable() {
        FilesService files;if(!files.available())QSKIP("Native libfm-qt required");
        PcLibrary library;QTemporaryDir d;QVERIFY(QDir().mkpath(d.filePath("source/My game")));
        QVERIFY(QDir().mkpath(d.filePath("destination")));
        const auto source=d.filePath("source/My game");
        QFile executable(source+"/play");QVERIFY(QFile::copy("/usr/bin/true",executable.fileName()));
        QFile data(source+"/data.txt");QVERIFY(data.open(QIODevice::WriteOnly));data.write("assets stay together");data.close();
        PcGamesController c(library,files);c.configure(d.filePath("state"),"owner");c.begin();
        c.picker()->selected(source);QTRY_COMPARE(c.stage(),QString("storage"));QVERIFY(library.records.isEmpty());
        c.picker()->selected(d.filePath("destination"));QTRY_COMPARE_WITH_TIMEOUT(c.stage(),QString("candidates"),10000);
        QVERIFY(library.records.isEmpty());c.activate(0);QCOMPARE(c.stage(),QString("confirm"));
        c.applyText("A renamed game");c.activate(2);QCOMPARE(c.stage(),QString("done"));QCOMPARE(library.records.size(),1);
        const auto r=library.records[0];QVERIFY(r.adventure.id.startsWith("pc:"));QCOMPARE(r.adventure.title,QString("A renamed game"));
        QVERIFY(r.contentPath.startsWith(d.filePath("destination")+'/'));QVERIFY(QFile::exists(source+"/play"));
        QFile copy(QFileInfo(r.contentPath).absolutePath()+"/data.txt");QVERIFY(copy.open(QIODevice::ReadOnly));QCOMPARE(copy.readAll(),QByteArray("assets stay together"));
        QVERIFY(r.integrationConfig["prefix"].toString().startsWith(d.filePath("state")+'/'));
    }
    void rejectFolderLinksWithoutCopyingExternalContent() {
#ifdef Q_OS_LINUX
        QTemporaryDir d;QVERIFY(QFile::link("/etc/passwd",d.filePath("outside")));
        const auto result=PcGamesController::inspect(d.path());QVERIFY(!result["error"].toString().isEmpty());
#endif
    }
    void neverTrustExeExtension() {
        QTemporaryDir d;QFile f(d.filePath("game.exe"));QVERIFY(f.open(QIODevice::WriteOnly));f.write(QByteArray(128,'x'));f.close();
        const auto result=PcGamesController::inspect(d.path());
        QVERIFY(!result["error"].toString().isEmpty());QVERIFY(result["candidates"].toList().isEmpty());
    }
    void nativeArchitectureAndRelativeFolders() {
        QTemporaryDir d;QVERIFY(QDir().mkpath(d.filePath("nested game")));
        QByteArray elf(64,0);elf.replace(0,4,QByteArray("\x7f" "ELF",4));elf[5]=1;
        elf[18]=QSysInfo::currentCpuArchitecture()=="arm64"?183:62;
        QFile f(d.filePath("nested game/play.exe"));QVERIFY(f.open(QIODevice::WriteOnly));f.write(elf);f.close();
        QVERIFY(f.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        const auto result=PcGamesController::inspect(d.path());
        QVERIFY(result["error"].toString().isEmpty());const auto rows=result["candidates"].toList();QCOMPARE(rows.size(),1);
        QCOMPARE(rows[0].toMap()["runner"].toString(),QString("native"));
        QCOMPARE(rows[0].toMap()["subtitle"].toString(),QString("nested game/play.exe"));
    }
    void windowsGameRanksBeforeInstaller() {
        QTemporaryDir d;QByteArray pe(100,0);pe[0]='M';pe[1]='Z';pe[60]=64;pe.replace(64,4,QByteArray("PE\0\0",4));
        for(const auto& name:{"setup.exe","game.exe","unins000.exe"}) {QFile f(d.filePath(name));QVERIFY(f.open(QIODevice::WriteOnly));f.write(pe);}
        const auto result=PcGamesController::inspect(d.path());const auto rows=result["candidates"].toList();
        QCOMPARE(rows.size(),3);QCOMPARE(rows[0].toMap()["title"].toString(),QString("game.exe"));
    }
    void folderPickerDoesNotChangeOrdinaryFilePicker() {
        QTemporaryDir d;QFile file(d.filePath("play.exe"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("example");file.close();
        LocalFileCatalog catalog;
        FilePickerController picker(&catalog);QSignalSpy selected(&picker,&FilePickerController::selected);
        picker.begin(d.path(),true);QTRY_VERIFY(!picker.busy());QVERIFY(picker.choosingFolder());
        picker.activate(0,"list");QVERIFY(picker.isOpen());QCOMPARE(selected.size(),0);
        picker.choose();QCOMPARE(selected.takeFirst()[0].toString(),d.path());
        picker.begin(d.path());QTRY_VERIFY(!picker.busy());QVERIFY(!picker.choosingFolder());
        picker.activate(0,"list");QVERIFY(selected.isEmpty());picker.choose();QCOMPARE(selected.takeFirst()[0].toString(),d.filePath("play.exe"));
    }
    void noGameRegistrationBeforeConfirmation() {
        MockLibraryRepository library;FilesService files;PcGamesController c(library,files);
        c.configure("/unused","owner");c.begin();QVERIFY(!c.isOpen()); // Read-only preview cannot create games.
    }
};
QTEST_GUILESS_MAIN(PcGamesTests)
#include "PcGamesTests.moc"
