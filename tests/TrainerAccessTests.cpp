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
