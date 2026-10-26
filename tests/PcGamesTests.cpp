#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSysInfo>
#include <archive.h>
#include <archive_entry.h>
#include "platform/files/ArchiveExtractor.h"
#include "features/pc/PcGamesController.h"
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
    void saveAdventureAsync(const AdventureRegistration& r,QObject*,std::function<void(LibraryWriteResult)> done) override {records.append(r);done({true,{}});}
};
class PcGamesTests : public QObject {
    Q_OBJECT
private slots:
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
