#include "adapters/pokemon/PokemonExperience.h"
#include "core/storage/SessionState.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include "integrations/achievements/AchievementProvider.h"
#include "core/repository/PokedexRepository.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QStorageInfo>
#include <QJsonDocument>

using namespace trainer;
namespace {
void controls(FirstRunController& flow) {
    for(auto action:{Action::Up,Action::Down,Action::Left,Action::Right,Action::Confirm,Action::Back})flow.dispatch(action);
}
void isolatedStorage(FirstRunController& flow,const QString& root) {
    flow.clock()->read=[] { return ClockSnapshot{"UTC", {}, {"UTC"}, true, true, true, true}; };
    flow.locations=[root](const QString&){
        QStorageInfo disk(QFileInfo(root).absolutePath());
        return QList<LibraryLocation>{{"Test storage",root,disk.rootPath(),QString::fromUtf8(disk.device()),disk.bytesAvailable()}};
    };
}
QByteArray read(const QString& path) {QFile file(path);if(!file.open(QIODevice::ReadOnly))return {};return file.readAll();}
}
class FirstRunTests : public QObject {
    Q_OBJECT
private slots:
    void timeStepResumesWithoutForcingItOnExistingTrainers() {
        QTemporaryDir dir;
        {
            FirstRunController flow;flow.configure(dir.path(),dir.path()+"/roms");isolatedStorage(flow,dir.path()+"/roms");
            flow.begin(false);flow.activate(0);controls(flow);flow.activate(1);
            QCOMPARE(flow.stage(),"clock");QTRY_VERIFY(!flow.busy());
        }
        FirstRunController resumed;resumed.configure(dir.path(),dir.path()+"/roms");isolatedStorage(resumed,dir.path()+"/roms");resumed.begin(false);
        QCOMPARE(resumed.stage(),"clock");QTRY_VERIFY(!resumed.busy());
        resumed.dispatch(Action::Confirm);QCOMPARE(resumed.stage(),"storage");
        resumed.dispatch(Action::Back);QCOMPARE(resumed.stage(),"clock");QTRY_VERIFY(!resumed.busy());
        resumed.dispatch(Action::Back);QCOMPARE(resumed.stage(),"network");
    }
    void storageSettingSurvivesAndOverridesOldSetupRoot() {
        QTemporaryDir dir;
        const auto target=dir.path()+"/new-library";
        QCOMPARE(saveLibraryRoot(dir.path(),target),QString());
        QCOMPARE(FirstRunController::libraryRoot(dir.path(),"old-library"),target);
        QVERIFY(!saveLibraryRoot(dir.path(),"relative/path").isEmpty());
        QCOMPARE(readLibraryRoot(dir.path(),"fallback"),target);
        // An unwritable state directory must not report a saved choice.
        QFile occupied(dir.path()+"/occupied");QVERIFY(occupied.open(QIODevice::WriteOnly));occupied.close();
        QVERIFY(!saveLibraryRoot(occupied.fileName(),target).isEmpty());
    }
    void settingsStorageRetainsChoiceOnFailureAndPreventsBusyExit() {
        QTemporaryDir dir;LibraryStorageController flow;
        flow.configure(dir.path()+"/old");
        QStorageInfo disk(dir.path());
        const auto target=dir.path()+"/new";
        flow.locations=[&](const QString&){return QList<LibraryLocation>{{"Card",target,disk.rootPath(),QString::fromUtf8(disk.device()),disk.bytesAvailable()}};};
        int applied=0;
        flow.apply=[&](const QString& root){++applied;return saveLibraryRoot(dir.path(),root);};
        flow.prepare=[](const LibraryLocation&){return QString("Card disconnected");};
        flow.begin();flow.activate(0);QVERIFY(flow.busy());flow.close();QVERIFY(flow.isOpen());
        QTRY_VERIFY(!flow.busy());QCOMPARE(applied,0);QVERIFY(flow.isOpen());QVERIFY(!flow.error().isEmpty());
        QCOMPARE(flow.root(),dir.path()+"/old");
        flow.prepare=prepareLibraryLocation;flow.activate(0);QTRY_VERIFY(!flow.busy());
        QCOMPARE(applied,1);QVERIFY(!flow.isOpen());QCOMPARE(flow.root(),target);
        QCOMPARE(readLibraryRoot(dir.path(),"fallback"),target);QVERIFY(QFileInfo(target+"/gba").isDir());
        flow.begin();flow.dispatch(Action::Back);QVERIFY(!flow.isOpen());
    }
    void freshSetupResumesAndKeepsExistingFiles() {
        QTemporaryDir dir;const auto root=dir.path()+"/roms";
        QVERIFY(QDir().mkpath(root+"/gba"));
        QFile game(root+"/gba/keep.gba");QVERIFY(game.open(QIODevice::WriteOnly));game.write("original");game.close();
        {
            FirstRunController flow;flow.configure(dir.path(),root);isolatedStorage(flow,root);flow.begin(false);
            QCOMPARE(flow.stage(),"welcome");flow.activate(0);controls(flow);QCOMPARE(flow.stage(),"network");
            flow.activate(1);QTRY_VERIFY(!flow.clock()->busy());flow.activate(3);QCOMPARE(flow.stage(),"storage");
            flow.activate(0);QTRY_COMPARE(flow.stage(),"trainer");
            flow.saveTrainerDraft({{"name","River"},{"emblem","leaf"},{"favorite","Squirtle"}});
            QCOMPARE(read(root+"/gba/keep.gba"),"original");QVERIFY(QFileInfo(root+"/psp").isDir());
        }
        FirstRunController resumed;resumed.configure(dir.path(),"unused");resumed.begin(false);
        QVERIFY(resumed.active());QCOMPARE(resumed.stage(),"trainer");QCOMPARE(resumed.trainerDraft()["name"].toString(),"River");
        QCOMPARE(FirstRunController::libraryRoot(dir.path(),"fallback"),root);
        resumed.trainerOpened();QCOMPARE(resumed.stage(),"accounts");resumed.activate(3);
        QCOMPARE(resumed.stage(),"preferences");resumed.activate(5);resumed.activate(0);QVERIFY(!resumed.active());
        QVERIFY(!read(dir.path()+"/first-run.json").contains("trainerDraft"));
        FirstRunController finished;finished.configure(dir.path(),root);finished.begin(true);QVERIFY(!finished.active());
    }
    void existingInstallationIsNotSentThroughSetup() {
        QTemporaryDir dir;FirstRunController flow;flow.configure(dir.path(),dir.path()+"/roms");flow.begin(true);
        QVERIFY(!flow.active());QVERIFY(!QFileInfo(dir.path()+"/first-run.json").exists());
    }
    void failedStorageDoesNotAdvanceOrOverwriteTheRoot() {
        QTemporaryDir dir;FirstRunController flow;flow.configure(dir.path(),"original");isolatedStorage(flow,dir.path()+"/roms");
        flow.prepare=[](const LibraryLocation&){return QString("Card disconnected");};
        flow.begin(false);flow.activate(0);controls(flow);flow.activate(1);QTRY_VERIFY(!flow.clock()->busy());flow.activate(3);flow.activate(0);
        QTRY_VERIFY(!flow.busy());QCOMPARE(flow.stage(),"storage");QVERIFY(flow.error().contains("disconnected"));
        QCOMPARE(FirstRunController::libraryRoot(dir.path(),"original"),"original");
        QStorageInfo disk(dir.path());LibraryLocation changed{"Card",dir.path()+"/absent",disk.rootPath(),"wrong-device",1000000};
        QVERIFY(!prepareLibraryLocation(changed).isEmpty());QVERIFY(!QFileInfo(changed.path).exists());
    }
    void corruptProgressIsPreservedAndBlocksAdvancing() {
        QTemporaryDir dir;QFile file(dir.path()+"/first-run.json");QVERIFY(file.open(QIODevice::WriteOnly));file.write("broken");file.close();
        FirstRunController flow;flow.configure(dir.path(),dir.path()+"/roms");flow.begin(true);
        QVERIFY(flow.active());QVERIFY(!flow.error().isEmpty());flow.activate(0);
        QCOMPARE(read(file.fileName()),"broken");QVERIFY(!flow.error().isEmpty());
    }
    void committedTrainerResumesWithoutDuplicateRegistration() {
        QTemporaryDir dir;const auto root=dir.path()+"/roms";
        {
            FirstRunController flow;flow.configure(dir.path(),root);isolatedStorage(flow,root);
            flow.begin(false);flow.activate(0);controls(flow);flow.activate(1);QTRY_VERIFY(!flow.clock()->busy());flow.activate(3);flow.activate(0);
            QTRY_COMPARE(flow.stage(),"trainer");
        }
        FirstRunController flow;flow.configure(dir.path(),root);QSignalSpy requested(&flow,&FirstRunController::trainerRequested);
        flow.begin(true);QCOMPARE(flow.stage(),"accounts");QCOMPARE(requested.size(),0);
    }
    void sessionGatesNavigationAndCompletesRealProfileCreation() {
        QTemporaryDir dir;LocalStateStore store(dir.path());store.enforceAccess();
        MockAdventureAdapter adapter;DevelopmentPlatformService platform;MockPokedexRepository dex;MockAchievementProvider ra;
        ShellController shell(store,store,adapter,platform,builtinExperiences(dex,store),store,ra);SessionState session(shell,&store);
        auto* flow=session.firstRun();flow->configure(dir.path(),dir.path()+"/roms");isolatedStorage(*flow,dir.path()+"/roms");
        session.start();QTRY_VERIFY(session.firstRunPage());
        for(auto action:{Action::Home,Action::NextPage,Action::SystemMenu,Action::ToggleContinue})session.dispatch(action);
        QCOMPARE(shell.page(),0);QVERIFY(!shell.menuOpen());QVERIFY(!shell.drawerOpen());
        session.dispatch(Action::Confirm);controls(*flow);flow->activate(0);QVERIFY(flow->connections());
        session.dispatch(Action::Back);QVERIFY(!flow->connections());
        flow->activate(1);QTRY_VERIFY(!flow->clock()->busy());flow->activate(3);flow->activate(0);QTRY_COMPARE(flow->stage(),"trainer");
        QCOMPARE(shell.trainerSetup()->stage(),"identity");shell.trainerSetup()->applyName("River");
        shell.trainerSetup()->activate(3);QCOMPARE(shell.trainerSetup()->stage(),"review");
        QCOMPARE(shell.trainerSetup()->rows().size(),3); // PIN setup follows creation when the family code is absent.
        shell.trainerSetup()->activate(0);QTRY_VERIFY(store.ready());QTRY_COMPARE(flow->stage(),"accounts");
        flow->activate(0);QCOMPARE(flow->service(),"social");
        session.dispatch(Action::NextPage);session.dispatch(Action::SystemMenu);
        QCOMPARE(shell.page(),0);QVERIFY(!shell.menuOpen());
        session.backInSetup();QVERIFY(flow->service().isEmpty());QCOMPARE(flow->focusIndex(),0);
        shell.scraper()->configure(dir.path());flow->activate(2);QCOMPARE(flow->service(),"scraper");
        shell.scraper()->activateSetting(1);shell.scraper()->applyText("temporary-only-secret");
        QVERIFY(shell.scraper()->settingsRows()[1].toMap()["detail"]!="Not set");
        // The keyboard is normally cancelled before leaving its containing service.
        shell.keyboard()->dispatch(Action::Back);
        shell.scraper()->activateSetting(12);QVERIFY(!shell.scraper()->isOpen());
        session.backInSetup();QVERIFY(flow->service().isEmpty());
        QCOMPARE(shell.scraper()->settingsRows()[1].toMap()["detail"].toString(),"Not set");
        QVERIFY(!read(dir.path()+"/first-run.json").contains("temporary-only-secret"));
        flow->activate(3);QCOMPARE(flow->stage(),"preferences");
        flow->activate(0);QCOMPARE(shell.settings()->category(),0);
        emit shell.settings()->messageRequested("Settings error shown inline");QVERIFY(shell.notice().isEmpty());
        const bool reduced=shell.settings()->reducedMotion();session.activateSetupService(1);
        QTRY_VERIFY(!shell.settings()->saving());QCOMPARE(shell.settings()->reducedMotion(),!reduced);
        session.backInSetup();QVERIFY(flow->service().isEmpty());
        flow->activate(5);QCOMPARE(flow->stage(),"ready");
        QCOMPARE(store.trainers().size(),1);QVERIFY(session.blocked());QVERIFY(!session.entryGate());
        QTRY_COMPARE(store.pending(),0);
        flow->activate(1);QVERIFY(session.access()->active());QCOMPARE(session.access()->minimumDigits(),6);
        const auto code=[&](const char* digits) {
            for(;*digits;++digits)session.access()->activate(*digits=='0'?10:*digits-'1');
            session.access()->activate(12);
        };
        code("987654");code("987654");QTRY_VERIFY(store.familyProtected());QTRY_VERIFY(!session.access()->busy());
        session.access()->activate(0);QTRY_COMPARE(session.access()->choices().value(0),"Set PIN");
        session.access()->activate(0);code("1234");code("1234");QTRY_VERIFY(store.pinProtected(store.ownerId()));
        QTRY_VERIFY(!session.access()->busy());session.access()->activate(0);QVERIFY(!session.access()->active());
        QVERIFY(!read(dir.path()+"/first-run.json").contains("987654"));
        session.dispatch(Action::Up);session.dispatch(Action::Confirm);QVERIFY(!session.blocked());QCOMPARE(shell.page(),0);
        QCOMPARE(store.load()->name,"River");
    }
    void touchContinuationDoesNotPretendPhysicalButtonsPassed() {
        QTemporaryDir dir;FirstRunController flow;flow.configure(dir.path(),dir.path()+"/roms");
        flow.begin(false);flow.useTouch();QCOMPARE(flow.stage(),"welcome");
        flow.activate(0);flow.useTouch();QCOMPARE(flow.stage(),"network");QCOMPARE(flow.checkedControls(),0);
        QCOMPARE(QJsonDocument::fromJson(read(dir.path()+"/first-run.json")).object()["inputChoice"].toString(),"touch");
    }
    void legacyCompleteAndReadyAreNotForcedThroughNewServices() {
        QTemporaryDir dir;
        for(const auto stage:{"complete","ready"}) {
            QFile file(dir.path()+"/first-run.json");QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QJsonDocument(QJsonObject{{"version",1},{"stage",stage},{"libraryRoot",dir.path()+"/roms"}}).toJson());file.close();
            FirstRunController flow;flow.configure(dir.path(),"fallback");flow.begin(true);
            QCOMPARE(flow.stage(),stage);QCOMPARE(flow.active(),QString(stage)!="complete");
            QCOMPARE(FirstRunController::libraryRoot(dir.path(),"fallback"),dir.path()+"/roms");
        }
    }
    void serviceResumeRequiresProfileAndKeepsPanelTransient() {
        QTemporaryDir dir;
        QFile file(dir.path()+"/first-run.json");QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"version":2,"stage":"preferences"})");file.close();
        FirstRunController flow;flow.configure(dir.path(),dir.path()+"/roms");flow.begin(true);
        flow.activate(3);QCOMPARE(flow.service(),"communication");flow.activate(5);QCOMPARE(flow.stage(),"preferences");
        flow.closeService();flow.dispatch(Action::Back);QCOMPARE(flow.stage(),"accounts");
        FirstRunController resumed;resumed.configure(dir.path(),dir.path()+"/roms");resumed.begin(true);
        QCOMPARE(resumed.stage(),"accounts");QVERIFY(resumed.service().isEmpty());
        FirstRunController missing;missing.configure(dir.path(),dir.path()+"/roms");missing.begin(false);
        QCOMPARE(missing.stage(),"trainer");
    }
    void restartedPersonalSetupWaitsForPinUnlock() {
        QTemporaryDir dir;
        {
            LocalStateStore store(dir.path());
            store.open();QTRY_VERIFY(store.ready());
            const auto pin=[](const char* digits){auto value=emptyPin();for(;*digits;++digits)value->digit(*digits-'0');return value;};
            bool family=false;store.changePin(emptyPin(),pin("987654"),true,this,[&](const QString& e){family=e.isEmpty();});
            QTRY_VERIFY(family);QTRY_COMPARE(store.pending(),0);
            bool created=false;
            store.createProtectedTrainer({"protected","River","compass",{},QDateTime::currentDateTimeUtc()},pin("1234"),this,[&](const ProfileWriteResult& result){QVERIFY2(result.success,qPrintable(result.error));created=true;});
            QTRY_VERIFY(created);QTRY_COMPARE(store.pending(),0);
        }
        QFile file(dir.path()+"/first-run.json");QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"version":2,"stage":"accounts"})");file.close();
        LocalStateStore reopened(dir.path());reopened.enforceAccess();
        MockAdventureAdapter adapter;DevelopmentPlatformService platform;MockPokedexRepository dex;MockAchievementProvider ra;
        ShellController shell(reopened,reopened,adapter,platform,builtinExperiences(dex,reopened),reopened,ra);SessionState session(shell,&reopened);
        session.firstRun()->configure(dir.path(),dir.path()+"/roms");session.start();QTRY_VERIFY(session.entryGate());
        QVERIFY(session.firstRun()->active());QVERIFY(!session.firstRunPage());
        session.activateSetupService(0);QVERIFY(!shell.hall()->account()->isOpen());
        QTRY_COMPARE(reopened.pending(),0);session.requestTrainerSwitch("protected");QVERIFY(session.access()->active());
        for(int digit=0;digit<4;++digit)session.access()->activate(digit);
        session.access()->activate(12);
        QTRY_VERIFY(reopened.ready());QTRY_VERIFY(session.firstRunPage());
        QVERIFY(!session.entryGate());QCOMPARE(session.firstRun()->stage(),"accounts");
    }
};
QTEST_GUILESS_MAIN(FirstRunTests)
#include "FirstRunTests.moc"
