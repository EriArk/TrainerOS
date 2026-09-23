#include "core/storage/SessionState.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include "integrations/achievements/TrainerAchievementProvider.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QUuid>

using namespace trainer;
class TrainerProfilesTests final:public QObject {
    Q_OBJECT
    TrainerProfile profile(const QString& id) {return {id,id,"compass","eevee",QDateTime::currentDateTimeUtc()};}
    void create(LocalStateStore& store,const QString& id) {
        bool done=false;store.createTrainerAsync(profile(id),this,[&](auto r){QVERIFY2(r.success,qPrintable(r.error));done=true;});QTRY_VERIFY(done);QTRY_COMPARE(store.pending(),0);
    }
    void stage(LocalStateStore& store,const QString& id) {
        bool done=false;store.stageTrainerAsync(id,this,[&](auto e){QVERIFY2(e.isEmpty(),qPrintable(e));done=true;});QTRY_VERIFY(done);QTRY_COMPARE(store.pending(),0);
    }
private slots:
    void schemaEightMigrationRollsBackAndRetriesWithoutLosingProfile() {
        QTemporaryDir dir;
        { LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());create(store,"legacy");
          store.setFavoriteAsync("eevee",true,this,[](auto e){QVERIFY(e.isEmpty());});QTRY_COMPARE(store.pending(),0); }
        const auto name=QUuid::createUuid().toString();
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(dir.filePath("traineros.sqlite3"));QVERIFY(db.open());
            QSqlQuery q(db);
            // Restore the actual schema-8 profile shape. The conflicting account
            // table forces failure after profile replacement has begun.
            for(const auto& sql:QStringList{
                "CREATE TABLE legacy_profile(slot INTEGER PRIMARY KEY CHECK(slot=1),id TEXT NOT NULL UNIQUE,name TEXT NOT NULL,emblem TEXT NOT NULL,favorite TEXT NOT NULL,created_at TEXT NOT NULL)",
                "INSERT INTO legacy_profile SELECT 1,id,name,emblem,favorite,created_at FROM trainer_profile",
                "DROP TABLE trainer_profile","ALTER TABLE legacy_profile RENAME TO trainer_profile","DROP TRIGGER trainer_access_create","DROP TABLE trainer_access","DROP TABLE family_access","PRAGMA user_version=8"})QVERIFY(q.exec(sql));
        }
        QSqlDatabase::removeDatabase(name);
        { LocalStateStore store(dir.path());QSignalSpy opened(&store,&LocalStateStore::opened);store.open();QTRY_COMPARE(opened.size(),1);QVERIFY(!store.ready()); }
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(dir.filePath("traineros.sqlite3"));QVERIFY(db.open());QSqlQuery q(db);
            QVERIFY(q.exec("PRAGMA user_version"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),8);q.finish();
            QVERIFY(q.exec("SELECT id FROM trainer_profile WHERE slot=1"));QVERIFY(q.next());QCOMPARE(q.value(0).toString(),"legacy");q.finish();
            QVERIFY(q.exec("DROP TABLE legacy_account_owner"));
        }
        QSqlDatabase::removeDatabase(name);
        LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());QCOMPARE(store.load()->id,"legacy");
        QVERIFY(store.progress("eevee").favorite);QCOMPARE(store.accountDirectory(),dir.path());
    }
    void profileLimitAndFailedCreationDoNotChangeActiveOwner() {
        QTemporaryDir dir;LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
        for(int i=0;i<8;++i)create(store,QString::number(i));
        bool failed=false;store.createTrainerAsync(profile("ninth"),this,[&](auto r){failed=!r.success;});QTRY_VERIFY(failed);QTRY_COMPARE(store.pending(),0);
        QCOMPARE(store.trainers().size(),8);QCOMPARE(store.ownerId(),"0");
        bool rejected=false;store.stageTrainerAsync("ninth",this,[&](auto e){rejected=!e.isEmpty();});QTRY_VERIFY(rejected);
    }
    void twoProfilesKeepIndependentRecordsAndAccountsAcrossReopen() {
        QTemporaryDir dir;QString firstDirectory,secondDirectory;
        {
            LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
            create(store,"one");create(store,"two");QCOMPARE(store.ownerId(),"one");QCOMPARE(store.trainers().size(),2);
            store.setFavoriteAsync("eevee",true,this,[](auto e){QVERIFY(e.isEmpty());});
            store.saveNavigation({{"version",1},{"page","trainer"}},this,[](auto e){QVERIFY(e.isEmpty());});QTRY_COMPARE(store.pending(),0);
            firstDirectory=store.accountDirectory();QCOMPARE(firstDirectory,dir.path());
            stage(store,"two");bool rejected=false;
            store.setFavoriteAsync("pikachu",true,this,[&](auto e){rejected=!e.isEmpty();});QVERIFY(rejected);
        }
        {
            LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());
            QCOMPARE(store.load()->id,"two");QVERIFY(!store.progress("eevee").favorite);QVERIFY(store.navigation().isEmpty());
            QVERIFY(store.recentSessions().isEmpty());QVERIFY(store.loadArchive().entries.isEmpty());
            secondDirectory=store.accountDirectory();QVERIFY(secondDirectory!=firstDirectory);QVERIFY(secondDirectory.startsWith(dir.path()+"/trainers/"));
            store.setFavoriteAsync("pikachu",true,this,[](auto e){QVERIFY(e.isEmpty());});
            store.saveNavigation({{"version",1},{"page","pokedex"}},this,[](auto e){QVERIFY(e.isEmpty());});QTRY_COMPARE(store.pending(),0);
            stage(store,"one");
        }
        {
            LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());QCOMPARE(store.load()->id,"one");
            QVERIFY(store.progress("eevee").favorite);QVERIFY(!store.progress("pikachu").favorite);
            QCOMPARE(store.navigation()["page"].toString(),"trainer");QCOMPARE(store.accountDirectory(),firstDirectory);
            bool rejected=false;store.stageTrainerAsync("missing",this,[&](auto e){rejected=!e.isEmpty();});QTRY_VERIFY(rejected);QTRY_COMPARE(store.pending(),0);
            store.setFavoriteAsync("eevee",false,this,[](auto e){QVERIFY(e.isEmpty());});QTRY_COMPARE(store.pending(),0);
            bool failed=false;store.createTrainerAsync(profile("two"),this,[&](auto r){failed=!r.success;});QTRY_VERIFY(failed);QCOMPARE(store.trainers().size(),2);
            stage(store,"two");
        }
        LocalStateStore store(dir.path());store.open();QTRY_VERIFY(store.ready());QVERIFY(store.progress("pikachu").favorite);
        QCOMPARE(store.navigation()["page"].toString(),"pokedex");QCOMPARE(store.accountDirectory(),secondDirectory);
    }
    void sessionRefusesUnsafeSwitchAndDrainsNavigation() {
        QTemporaryDir dir;LocalStateStore store(dir.path());
        MockAdventureAdapter adapter;DevelopmentPlatformService platform;MockPokedexRepository dex;MockAchievementProvider ra;
        ShellController shell(store,store,adapter,platform,dex,store,store,ra);SessionState session(shell,&store);
        session.start();QTRY_VERIFY(!session.blocked());create(store,"one");create(store,"two");
        QSignalSpy restarted(&session,&SessionState::trainerRestartReady);QSignalSpy exited(&session,&SessionState::exitReady);
        session.setAdventureActive(true);session.requestTrainerSwitch("two");QTest::qWait(20);QVERIFY(restarted.isEmpty());
        session.setAdventureActive(false);session.setServiceActive(true);session.requestTrainerSwitch("two");QVERIFY(restarted.isEmpty());
        session.setServiceActive(false);session.setTrainerSwitchGuard([]{return false;});session.requestTrainerSwitch("two");QVERIFY(restarted.isEmpty());
        session.setTrainerSwitchGuard([]{return true;});
        shell.dispatch(Action::SystemMenu);shell.activate(6);QVERIFY(shell.powerMenu());shell.activate(2);
        QCOMPARE(shell.service(),"trainer-setup");QVERIFY(!shell.menuOpen());
        shell.dispatch(Action::Back);QVERIFY(shell.powerMenu());QCOMPARE(shell.focusIndex(),2);
        shell.goToPage(3);session.requestTrainerSwitch("two");
        QTRY_COMPARE(restarted.size(),1);QVERIFY(exited.isEmpty());QVERIFY(session.blocked());QCOMPARE(store.navigation()["page"].toString(),"trainer");
        session.dispatch(Action::Back);session.dispatch(Action::NextPage);QVERIFY(session.blocked());
    }
    void realFlowCreatesWithoutPreviewPinAndBoundsChooser() {
        TrainerSetupPresentation flow;QList<TrainerProfile> profiles;for(int i=0;i<8;++i)profiles.append(profile(QString::number(i)));
        flow.configure(profiles,"0");flow.begin();QCOMPARE(flow.stage(),"chooser");QCOMPARE(flow.rows().size(),9);
        for(int i=0;i<20;++i)flow.dispatch(Action::Down);QCOMPARE(flow.focusIndex(),8);
        flow.configure({profile("one")},"one");flow.setFamilyReady(true);flow.begin();flow.activate(1);flow.activate(0);flow.applyName("River");flow.activate(3);
        QCOMPARE(flow.stage(),"pin");QVERIFY(flow.keypad());flow.activate(13);
        QCOMPARE(flow.stage(),"review");QVERIFY(!flow.keypad());
        QSignalSpy created(&flow,&TrainerSetupPresentation::createRequested);flow.activate(0);QCOMPARE(created.size(),1);
        flow.setBusy(true);flow.activate(0);flow.dispatch(Action::Back);QCOMPARE(created.size(),1);QCOMPARE(flow.stage(),"review");
    }
    void failedActivationLeavesPreviousSessionUsableAndCanRetry() {
        QTemporaryDir dir;LocalStateStore store(dir.path());
        MockAdventureAdapter adapter;DevelopmentPlatformService platform;MockPokedexRepository dex;MockAchievementProvider ra;
        ShellController shell(store,store,adapter,platform,dex,store,store,ra);SessionState session(shell,&store);
        session.start();QTRY_VERIFY(!session.blocked());create(store,"one");create(store,"two");
        QSignalSpy restarted(&session,&SessionState::trainerRestartReady);
        const auto name=QUuid::createUuid().toString();
        {
            auto db=QSqlDatabase::addDatabase("QSQLITE",name);db.setDatabaseName(dir.filePath("traineros.sqlite3"));QVERIFY(db.open());QSqlQuery q(db);
            QVERIFY(q.exec("CREATE TRIGGER reject_switch BEFORE UPDATE ON local_owner BEGIN SELECT RAISE(ABORT,'fixture failure'); END"));
            session.requestTrainers();session.requestTrainerSwitch("two");
            QTRY_VERIFY(!session.blocked());QTRY_COMPARE(store.pending(),0);QVERIFY(restarted.isEmpty());
            QCOMPARE(store.ownerId(),"one");QVERIFY(!shell.trainerSetup()->error().isEmpty());
            store.setFavoriteAsync("eevee",true,this,[](auto e){QVERIFY(e.isEmpty());});QTRY_COMPARE(store.pending(),0);QVERIFY(store.progress("eevee").favorite);
            QVERIFY(q.exec("SELECT trainer_id FROM local_owner"));QVERIFY(q.next());QCOMPARE(q.value(0).toString(),"one");q.finish();
            QVERIFY(q.exec("DROP TRIGGER reject_switch"));
        }
        QSqlDatabase::removeDatabase(name);
        session.requestTrainerSwitch("two");QTRY_COMPARE(restarted.size(),1);
    }
    void accountsNeverFallBackToAnotherTrainerDirectory() {
        QTemporaryDir dir;MockLibraryRepository library;
        const auto first=dir.path()+"/one",second=dir.path()+"/two";
        QVERIFY(writeAchievementAccount(first+"/integrations/retroachievements-account.json",{"FixtureOne","PrivateFixtureToken123456"}));
        // No refresh fixture ROM is readable, so these checks require no network.
        {
            TrainerAchievementProvider provider(library);QVERIFY(provider.context().accountId.isEmpty());
            provider.bind(first);QCOMPARE(provider.context().accountId,"FixtureOne");
            provider.bind(second);QCOMPARE(provider.context().accountId,"FixtureOne"); // session binding is immutable
        }
        TrainerAchievementProvider provider(library);provider.bind(second);QVERIFY(provider.context().accountId.isEmpty());QVERIFY(provider.sets().isEmpty());
    }
    void removalSignsOutOnlyTheSelectedAccountAndReportsFilesystemFailure() {
        QTemporaryDir dir;MockLibraryRepository library;
        const auto one=dir.path()+"/one",two=dir.path()+"/two";
        QVERIFY(writeAchievementAccount(one+"/integrations/retroachievements-account.json",{"One","FixtureToken123456"}));
        QVERIFY(writeAchievementAccount(two+"/integrations/retroachievements-account.json",{"Two","OtherFixtureToken123456"}));
        RetroAchievementsProvider provider(library,one);QTRY_VERIFY(!provider.accountBusy());
        QVERIFY(provider.disconnectForRemoval());QVERIFY(provider.context().accountId.isEmpty());
        QVERIFY(!QFileInfo::exists(one+"/integrations/retroachievements-account.json"));
        QVERIFY(QFileInfo::exists(two+"/integrations/retroachievements-account.json"));
        // A directory at the credential path cannot be removed as a file.
        QVERIFY(QDir().mkpath(one+"/integrations/retroachievements-account.json"));
        QVERIFY(!provider.disconnectForRemoval());
    }
};
QTEST_GUILESS_MAIN(TrainerProfilesTests)
#include "TrainerProfilesTests.moc"
