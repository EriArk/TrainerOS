#include "core/storage/SessionState.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include "integrations/achievements/AchievementProvider.h"
#include "core/repository/PokedexRepository.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QUuid>

using namespace trainer;
namespace {
SecretPin pin(const char* digits) {auto p=emptyPin();for(;*digits;++digits)p->digit(*digits-'0');return p;}
TrainerProfile profile(const QString& id) {return {id,id,"leaf","eevee",QDateTime::currentDateTimeUtc()};}
struct Db {
    QSqlDatabase db;
    explicit Db(const QString& path) {db=QSqlDatabase::addDatabase("QSQLITE",QUuid::createUuid().toString());db.setDatabaseName(path+"/traineros.sqlite3");db.open();}
    ~Db(){const auto n=db.connectionName();db.close();db={};QSqlDatabase::removeDatabase(n);}
    bool exec(const QString& sql){QSqlQuery q(db);return q.exec(sql);}
};
}
class TrainerAccessTests final:public QObject {
    Q_OBJECT
    void create(LocalStateStore& store,const QString& id,SecretPin p={}) {
        if(p && p->size() && !store.familyProtected()) {
            bool family=false;store.changePin(emptyPin(),pin("987654"),true,this,[&](auto e){QVERIFY(e.isEmpty());family=true;});
            QTRY_VERIFY(family);QTRY_COMPARE(store.pending(),0);
        }
        bool done=false;store.createProtectedTrainer(profile(id),p,this,[&](auto r){QVERIFY2(r.success,qPrintable(r.error));done=true;});
        QTRY_VERIFY(done);QTRY_COMPARE(store.pending(),0);
    }
    void enter(TrainerAccessController* controller,const char* digits) {
        for(;*digits;++digits)controller->activate(*digits=='0'?10:*digits-'1');controller->activate(12);
    }
private slots:
    void removalRollsBackAndKeepsOtherTrainersAndSharedLibrary() {
        QTemporaryDir dir;
        {
            LocalStateStore s(dir.path());s.open();QTRY_VERIFY(s.ready());create(s,"one");create(s,"two");
            s.setFavoriteAsync("eevee",true,this,[](auto e){QVERIFY(e.isEmpty());});QTRY_COMPARE(s.pending(),0);
            Db c(dir.path());
            QVERIFY(c.exec("INSERT INTO pokedex_favorites VALUES('two','pikachu')"));
            QVERIFY(c.exec("INSERT INTO shell_state VALUES('two','user-library-v1','{\"version\":1,\"homeAdventure\":\"other-choice\"}')"));
            QVERIFY(c.exec("INSERT INTO adventures VALUES('game','kanto','A game',0,'','/untouched-rom','unconfigured','{}',1,'gba','','')"));
            for(const auto& owner:QStringList{"one","two"}) {
                QVERIFY(c.exec("INSERT INTO play_sessions VALUES('session-"+owner+"','game','2026-01-01T00:00:00.000Z','2026-01-01T00:01:00.000Z',60,'returned','"+owner+"')"));
                QVERIFY(c.exec("INSERT INTO hall_of_fame VALUES('memory-"+owner+"','game','A game','Kanto','',NULL,'[]','A memory','manual',1,'"+owner+"')"));
                QVERIFY(c.exec("INSERT INTO pokedex_records VALUES('"+owner+"','eevee',1,NULL,'A note',1)"));
                QVERIFY(c.exec("INSERT INTO exit_media VALUES('"+owner+"','pokemon','game','session-"+owner+"',1,'/untouched-rom',1,1,'fixture','2026-01-01T00:00:00.000Z',1,1,X'01','fixture')"));
            }
            const auto worlds=s.worlds().size();
            QVERIFY(c.exec("CREATE TRIGGER refuse_removal BEFORE DELETE ON trainer_profile BEGIN SELECT RAISE(ABORT,'fixture'); END"));
            bool done=false;s.removeCurrentTrainer(emptyPin(),this,[&](auto e){QVERIFY(!e.isEmpty());done=true;});QTRY_VERIFY(done);QTRY_COMPARE(s.pending(),0);
            QSqlQuery q(c.db);QVERIFY(q.exec("SELECT COUNT(*) FROM pokedex_favorites WHERE trainer_id='one'"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);q.finish();
            QCOMPARE(s.load()->id,"one");QCOMPARE(s.worlds().size(),worlds);
            QVERIFY(c.exec("DROP TRIGGER refuse_removal"));
            done=false;s.removeCurrentTrainer(emptyPin(),this,[&](auto e){QVERIFY2(e.isEmpty(),qPrintable(e));done=true;});QTRY_VERIFY(done);QTRY_COMPARE(s.pending(),0);
            bool refused=false;s.setFavoriteAsync("late",true,this,[&](auto e){refused=!e.isEmpty();});QVERIFY(refused);
            QVERIFY(q.exec("SELECT COUNT(*) FROM trainer_profile WHERE id='one'"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),0);q.finish();
            QVERIFY(q.exec("SELECT COUNT(*) FROM pokedex_favorites WHERE trainer_id='one'"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),0);q.finish();
            QVERIFY(q.exec("SELECT trainer_id FROM legacy_account_owner"));QVERIFY(q.next());QCOMPARE(q.value(0).toString(),"one");q.finish();
            QVERIFY(q.exec("PRAGMA foreign_key_check"));QVERIFY(!q.next());
            for(const auto& table:QStringList{"trainer_profile","exit_media","play_sessions","hall_of_fame","pokedex_records","adventures"}) {
                QVERIFY(q.exec("SELECT COUNT(*) FROM "+table));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);q.finish();
            }
        }
        LocalStateStore reopened(dir.path());reopened.enforceAccess({},true);reopened.open();QTRY_VERIFY(reopened.accessRequired());
        QVERIFY(!reopened.load());reopened.unlock("two",this,[](auto e){QVERIFY(e.isEmpty());});QTRY_VERIFY(reopened.ready());
        QVERIFY(reopened.progress("pikachu").favorite);QVERIFY(!reopened.progress("eevee").favorite);
        QCOMPARE(reopened.navigation()["homeAdventure"].toString(),"other-choice");
        QVERIFY(reopened.accountDirectory()!=dir.path());
        QCOMPARE(reopened.recentSessions().size(),1);QCOMPARE(reopened.loadArchive().entries.size(),1);
        QCOMPARE(reopened.progress("eevee").notes,"A note");QCOMPARE(reopened.adventures().size(),1);
    }
    void removingLastTrainerRequiresFamilyCodeAndRestartsCleanOnboarding() {
        QTemporaryDir dir;
        {
            LocalStateStore s(dir.path());s.open();QTRY_VERIFY(s.ready());create(s,"one",pin("1234"));
            bool done=false;s.removeCurrentTrainer(pin("000000"),this,[&](auto e){QVERIFY(!e.isEmpty());done=true;});QTRY_VERIFY(done);QTRY_COMPARE(s.pending(),0);
            QCOMPARE(s.trainers().size(),1);
            Db c(dir.path());QVERIFY(c.exec("UPDATE family_access SET retry_at=0"));
            done=false;s.removeCurrentTrainer(pin("987654"),this,[&](auto e){QVERIFY(e.isEmpty());done=true;});QTRY_VERIFY(done);QTRY_COMPARE(s.pending(),0);
        }
        LocalStateStore s(dir.path());s.enforceAccess();s.open();QTRY_VERIFY(s.accessRequired());QVERIFY(s.trainers().isEmpty());
        QVERIFY(s.ownerId()!="one");QVERIFY(s.familyProtected());QVERIFY(s.accountDirectory().isEmpty());
        create(s,"new");s.unlock("new",this,[](auto e){QVERIFY(e.isEmpty());});QTRY_VERIFY(s.ready());
        QVERIFY(s.accountDirectory()!=dir.path());QVERIFY(s.navigation().isEmpty());QVERIFY(s.recentSessions().isEmpty());
        bool refused=false;s.createProtectedTrainer(profile("one"),{},this,[&](auto r){refused=!r.success;});QTRY_VERIFY(refused);
    }
    void removalControllerDefaultsToKeepAndSessionRejectsUnsafeWork() {
        QTemporaryDir dir;LocalStateStore s(dir.path());
        MockAdventureAdapter adapter;DevelopmentPlatformService platform;MockPokedexRepository dex;MockAchievementProvider ra;
        ShellController shell(s,s,adapter,platform,dex,s,s,ra);SessionState session(shell,&s);
        session.start();QTRY_VERIFY(!session.blocked());create(s,"one");
        session.setAdventureActive(true);session.requestTrainerRemoval();QVERIFY(!session.access()->active());session.setAdventureActive(false);
        session.requestTrainerRemoval();QCOMPARE(session.access()->choices(),QStringList({"Keep Trainer","Remove Trainer"}));
        session.dispatch(Action::Confirm);QVERIFY(!session.access()->active());QCOMPARE(s.trainers().size(),1);
        session.requestTrainerRemoval();session.dispatch(Action::NextPage);QVERIFY(!session.access()->active());
        QTest::qWait(350);QTRY_COMPARE(s.pending(),0);
        session.requestTrainerRemoval();session.dispatch(Action::Down);session.setServiceActive(true);session.dispatch(Action::Confirm);
        QVERIFY(!session.access()->busy());QCOMPARE(s.trainers().size(),1);session.setServiceActive(false);
        session.setTrainerRemovalPreparation([]{return QString("Saved sign-in could not be removed.");});
        session.dispatch(Action::Down);session.dispatch(Action::Confirm);QVERIFY(session.access()->error().contains("sign-in"));
        session.setTrainerRemovalPreparation([]{return QString();});QSignalSpy restarted(&session,&SessionState::trainerRestartReady);
        session.dispatch(Action::Down);session.dispatch(Action::Confirm);QTRY_COMPARE(restarted.size(),1);QVERIFY(session.restartGrant().isEmpty());
    }
    void parentCodeMustExistBeforeChildrenCanEnablePins() {
        QTemporaryDir dir;LocalStateStore s(dir.path());s.open();QTRY_VERIFY(s.ready());create(s,"parent");
        bool refused=false;s.changePin(emptyPin(),pin("1234"),false,this,[&](auto e){refused=!e.isEmpty();});
        QTRY_VERIFY(refused);QTRY_COMPARE(s.pending(),0);QVERIFY(!s.pinProtected("parent"));
        refused=false;s.createProtectedTrainer(profile("child"),pin("2345"),this,[&](auto r){refused=!r.success;});
        QTRY_VERIFY(refused);QTRY_COMPARE(s.pending(),0);QCOMPARE(s.trainers().size(),1);
        s.changePin(emptyPin(),pin("987654"),true,this,[](auto e){QVERIFY(e.isEmpty());});QTRY_COMPARE(s.pending(),0);
        create(s,"child",pin("2345"));QVERIFY(s.pinProtected("child"));
        refused=false;s.changePin(emptyPin(),pin("111111"),true,this,[&](auto e){refused=!e.isEmpty();});
        QTRY_VERIFY(refused);QTRY_COMPARE(s.pending(),0);QVERIFY(s.familyProtected());
    }
    void schemaNineMigrationFailureKeepsOldVersionAndCanRetry() {
        QTemporaryDir dir;
        {LocalStateStore s(dir.path());s.open();QTRY_VERIFY(s.ready());create(s,"one");}
        {Db c(dir.path());
            QVERIFY(c.exec("DROP TRIGGER trainer_access_create"));QVERIFY(c.exec("DROP TABLE trainer_access"));
            QVERIFY(c.exec("DROP TABLE family_access"));QVERIFY(c.exec("PRAGMA user_version=9"));
            QVERIFY(c.exec("CREATE TABLE family_access(sentinel TEXT)"));}
        {LocalStateStore s(dir.path());s.open();QTRY_VERIFY(!s.opening());QVERIFY(!s.ready());}
        {Db c(dir.path());QSqlQuery q(c.db);QVERIFY(q.exec("PRAGMA user_version"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),9);q.finish();
            QVERIFY(q.exec("SELECT count(*) FROM sqlite_master WHERE name='trainer_access'"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),0);q.finish();
            QVERIFY(c.exec("DROP TABLE family_access"));}
        LocalStateStore s(dir.path());s.enforceAccess();s.open();QTRY_VERIFY(s.ready());QCOMPARE(s.load()->name,"one");QVERIFY(!s.pinProtected("one"));
    }
    void singleUnprotectedTrainerAndEmptyStartup() {
        QTemporaryDir dir;
        {
            LocalStateStore store(dir.path());store.enforceAccess();store.open();QTRY_VERIFY(store.accessRequired());
            QVERIFY(!store.ready());QVERIFY(!store.load());QVERIFY(store.accountDirectory().isEmpty());
            create(store,"one");bool done=false;store.unlock("one",this,[&](auto e){QVERIFY(e.isEmpty());done=true;});QTRY_VERIFY(done);QTRY_VERIFY(store.ready());
            QCOMPARE(store.load()->id,"one");
        }
        LocalStateStore store(dir.path());store.enforceAccess();store.open();QTRY_VERIFY(store.ready());QVERIFY(!store.accessRequired());
    }
    void protectedStartupDoesNotExposePersonalDataOrPermitWrites() {
        QTemporaryDir dir;
        {LocalStateStore s(dir.path());s.open();QTRY_VERIFY(s.ready());create(s,"one",pin("1234"));
            s.setFavoriteAsync("eevee",true,this,[](auto e){QVERIFY(e.isEmpty());});QTRY_COMPARE(s.pending(),0);}
        LocalStateStore store(dir.path());store.enforceAccess();store.open();QTRY_VERIFY(store.accessRequired());
        QVERIFY(!store.ready());QVERIFY(!store.load());QVERIFY(store.navigation().isEmpty());QVERIFY(store.recentSessions().isEmpty());
        QVERIFY(store.accountDirectory().isEmpty());QVERIFY(!store.progress("eevee").favorite);
        bool refused=false;store.setFavoriteAsync("eevee",false,this,[&](auto e){refused=!e.isEmpty();});QVERIFY(refused);
        store.unlock("one",this,[&](auto e){refused=!e.isEmpty();});QTRY_COMPARE(store.pending(),0);QVERIFY(refused);QVERIFY(!store.ready());
        bool verified=false;store.verifyPin("one",pin("1234"),false,this,[&](auto e){QVERIFY(e.isEmpty());verified=true;});QTRY_VERIFY(verified);QTRY_COMPARE(store.pending(),0);
        store.unlock("one",this,[](auto e){QVERIFY(e.isEmpty());});QTRY_VERIFY(store.ready());QVERIFY(store.progress("eevee").favorite);
    }
    void verifierSaltAndRetrySurviveRestart() {
        QTemporaryDir dir;
        {LocalStateStore s(dir.path());s.open();QTRY_VERIFY(s.ready());create(s,"one",pin("1234"));create(s,"two",pin("1234"));}
        {
            Db c(dir.path());QSqlQuery q(c.db);QVERIFY(q.exec("SELECT salt,verifier FROM trainer_access WHERE version=1 ORDER BY trainer_id"));QVERIFY(q.next());
            const auto salt=q.value(0).toByteArray(), hash=q.value(1).toByteArray();QCOMPARE(salt.size(),16);QCOMPARE(hash.size(),32);QVERIFY(q.next());
            QVERIFY(q.value(0).toByteArray()!=salt);QVERIFY(q.value(1).toByteArray()!=hash);q.finish();
            QVERIFY(!verifyTrainerPin(c.db,"one",pin("0000"),1000).success());
        }
        Db c(dir.path());QVERIFY(!verifyTrainerPin(c.db,"one",pin("1234"),1001).success());
        QVERIFY(verifyTrainerPin(c.db,"one",pin("1234"),1002).success());
        QVERIFY(verifyTrainerPin(c.db,"two",pin("1234"),1001).success());
        QVERIFY(c.exec("UPDATE trainer_access SET verifier=X'12' WHERE trainer_id='one'"));
        QVERIFY(!verifyTrainerPin(c.db,"one",pin("1234"),9999).success());
    }
    void familyResetAndAuthenticatedChangesKeepRecords() {
        QTemporaryDir dir;LocalStateStore s(dir.path());s.open();QTRY_VERIFY(s.ready());create(s,"one",pin("1234"));create(s,"two",pin("5678"));
        bool done=false;s.changePin(pin("987654"),pin("987654"),true,this,[&](auto e){QVERIFY(e.isEmpty());done=true;});QTRY_VERIFY(done);QTRY_COMPARE(s.pending(),0);QVERIFY(s.familyProtected());
        done=false;s.resetPin("two",pin("000000"),this,[&](auto e){QVERIFY(!e.isEmpty());done=true;});QTRY_VERIFY(done);QTRY_COMPARE(s.pending(),0);QVERIFY(s.pinProtected("two"));
        {Db c(dir.path());QVERIFY(c.exec("UPDATE family_access SET retry_at=0"));}
        done=false;s.resetPin("two",pin("987654"),this,[&](auto e){QVERIFY(e.isEmpty());done=true;});QTRY_VERIFY(done);QTRY_COMPARE(s.pending(),0);
        QVERIFY(!s.pinProtected("two"));QVERIFY(s.pinProtected("one"));QCOMPARE(s.trainers().size(),2);
        done=false;s.changePin(pin("1111"),emptyPin(),false,this,[&](auto e){QVERIFY(!e.isEmpty());done=true;});QTRY_VERIFY(done);QTRY_COMPARE(s.pending(),0);QVERIFY(s.pinProtected("one"));
        {Db c(dir.path());QVERIFY(c.exec("UPDATE trainer_access SET retry_at=0"));}
        done=false;s.changePin(pin("1234"),emptyPin(),false,this,[&](auto e){QVERIFY(e.isEmpty());done=true;});QTRY_VERIFY(done);QTRY_COMPARE(s.pending(),0);QVERIFY(!s.pinProtected("one"));
        QCOMPARE(s.load()->id,"one");
    }
    void creationAndFailedVerifierWritesNeverPartiallyGrantAccess() {
        QTemporaryDir dir;LocalStateStore s(dir.path());s.open();QTRY_VERIFY(s.ready());create(s,"one",pin("1234"));
        Db c(dir.path());QVERIFY(c.exec("CREATE TRIGGER reject_pin BEFORE UPDATE ON trainer_access BEGIN SELECT RAISE(ABORT,'fixture'); END"));
        bool failed=false;s.createProtectedTrainer(profile("two"),pin("2345"),this,[&](auto r){failed=!r.success;});QTRY_VERIFY(failed);QTRY_COMPARE(s.pending(),0);QCOMPARE(s.trainers().size(),1);
        QVERIFY(!verifyTrainerPin(c.db,"one",pin("1234"),9999).success());
        QSqlQuery q(c.db);QVERIFY(q.exec("SELECT count(*) FROM trainer_owners WHERE id='two'"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),0);
    }
    void startupControllerGateAndSwitchGrant() {
        QTemporaryDir dir;
        {LocalStateStore s(dir.path());s.open();QTRY_VERIFY(s.ready());create(s,"one",pin("1234"));create(s,"two");}
        LocalStateStore store(dir.path());store.enforceAccess();
        MockAdventureAdapter adapter;DevelopmentPlatformService platform;MockPokedexRepository dex;MockAchievementProvider ra;
        ShellController shell(store,store,adapter,platform,dex,store,store,ra);SessionState session(shell,&store);
        QSignalSpy opened(&store,&LocalStateStore::opened);QSignalSpy exited(&session,&SessionState::exitReady);
        session.start();QTRY_VERIFY(session.entryGate());QVERIFY(opened.isEmpty());QCOMPARE(shell.trainerSetup()->stage(),"chooser");
        for(auto action:{Action::Home,Action::PreviousPage,Action::NextPage,Action::PreviousFace,Action::NextFace,Action::ToggleContinue,Action::Back})session.dispatch(action);
        QVERIFY(session.entryGate());QVERIFY(!store.ready());QVERIFY(!shell.drawerOpen());QVERIFY(exited.isEmpty());
        session.requestTrainerSwitch("one");QVERIFY(session.access()->active());enter(session.access(),"1234");QTRY_VERIFY(store.ready());QTRY_VERIFY(!session.blocked());QCOMPARE(opened.size(),1);
        QCOMPARE(store.ownerId(),"one");QSignalSpy restarted(&session,&SessionState::trainerRestartReady);
        session.requestTrainerSwitch("two");QTRY_COMPARE(restarted.size(),1);QCOMPARE(session.restartGrant(),"two");
    }
    void recoveryControllerRequiresFreshConfirmationAndBackKeepsPin() {
        QTemporaryDir dir;LocalStateStore s(dir.path());s.open();QTRY_VERIFY(s.ready());create(s,"one",pin("1234"));
        QVERIFY(s.familyProtected());
        TrainerAccessController flow(&s);QSignalSpy unlocked(&flow,&TrainerAccessController::unlocked);
        flow.beginUnlock("one");flow.recover();enter(&flow,"987654");QTRY_VERIFY(!flow.busy());QCOMPARE(flow.choices(),QStringList({"Keep PIN","Remove PIN"}));
        flow.dispatch(Action::Confirm);QVERIFY(s.pinProtected("one"));QVERIFY(unlocked.isEmpty());
        flow.recover();enter(&flow,"987654");QTRY_VERIFY(!flow.busy());flow.dispatch(Action::Down);flow.dispatch(Action::Confirm);
        QTRY_COMPARE(unlocked.size(),1);QVERIFY(!s.pinProtected("one"));
    }
};
QTEST_GUILESS_MAIN(TrainerAccessTests)
#include "TrainerAccessTests.moc"
