#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSysInfo>
#include "features/pc/PcGamesController.h"
#include "features/worlds/MultiversePresentation.h"
#include "integrations/adventure/PcAdventureAdapter.h"
using namespace trainer;
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
        QFile f(d.filePath("nested game/play"));QVERIFY(f.open(QIODevice::WriteOnly));f.write(elf);f.close();
        QVERIFY(f.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        const auto result=PcGamesController::inspect(d.path());
        QVERIFY(result["error"].toString().isEmpty());const auto rows=result["candidates"].toList();QCOMPARE(rows.size(),1);
        QCOMPARE(rows[0].toMap()["runner"].toString(),QString("native"));
        QCOMPARE(rows[0].toMap()["subtitle"].toString(),QString("nested game/play"));
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
