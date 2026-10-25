#include <QtTest>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUuid>
#include "features/accounts/BackendAccount.h"
using namespace trainer;
namespace {
const QString Player="11111111-1111-4111-8111-111111111111", Attempt="22222222-2222-4222-8222-222222222222";
QJsonObject credentials() {
    return {{"player_id",Player},{"session_id","33333333-3333-4333-8333-333333333333"},
        {"fluxer_subject","12345"},{"access_token",QString(43,'a')},{"refresh_token",QString(43,'r')},
        {"expires_at",QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODateWithMs)}};
}
struct Harness {
    QHash<QString,QByteArray> disk;
    QUrl returnUri,browser;
    QList<QString> calls;
    QJsonObject claim;
    bool failStorage=false,loseRefresh=false,deferClaim=false,rejectDeleteOnce=false;
    BackendAccount::Reply pending;
    BackendAccount::Services services() {
        return {
            [this](QByteArray method,QString path,QJsonObject body,QByteArray auth,BackendAccount::Reply done){
                calls<<path;
                if(path=="/v1/auth/login-attempts") {
                    returnUri=QUrl(body["return_uri"].toString());
                    done(201,{{"attempt_id",Attempt},{"authorization_url","https://traineros-api.abysstail.art/v1/auth/fluxer/start?attempt_id="+Attempt}});
                } else if(path.endsWith("/claim")) {
                    claim=body;if(deferClaim)pending=std::move(done);else done(200,credentials());
                } else if(path=="/v1/auth/refresh") {
                    bool tombstone=false;for(const auto& bytes:disk)tombstone|=QJsonDocument::fromJson(bytes).object()["pending"].toBool();
                    if(!tombstone){done(500,{});return;}
                    done(loseRefresh?0:200,loseRefresh?QJsonObject():credentials());
                } else if(method=="DELETE"&&!auth.isEmpty()) {const int status=rejectDeleteOnce?401:204;rejectDeleteOnce=false;done(status,{});}
                else done(500,{});
            },
            [this](QString key,auto done){done(true,disk.value(key));},
            [this](QString key,QByteArray bytes,auto done){if(!failStorage)disk[key]=bytes;done(!failStorage);},
            [this](QUrl url){browser=url;return true;}
        };
    }
    QByteArray callback(QString suffix={},QString host={}) {
        QTcpSocket socket;socket.connectToHost(QHostAddress::LocalHost,returnUri.port());
        if(!socket.waitForConnected(1000))return {};
        if(suffix.isEmpty())suffix="?attempt_id="+Attempt+"&handoff_token="+QString(43,'h');
        if(host.isEmpty())host="127.0.0.1:"+QString::number(returnUri.port());
        socket.write("GET "+returnUri.path().toLatin1()+suffix.toLatin1()+" HTTP/1.1\r\nHost: "+host.toLatin1()+"\r\n\r\n");socket.flush();
        QElapsedTimer timer;timer.start();
        while(!socket.bytesAvailable()&&socket.state()!=QAbstractSocket::UnconnectedState&&timer.elapsed()<1500){QCoreApplication::processEvents();QTest::qWait(1);}
        return socket.readAll();
    }
};
}
class BackendAccountTests:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);QCoreApplication::setOrganizationName("TrainerOSAccountTests");QCoreApplication::setApplicationName(QUuid::createUuid().toString(QUuid::Id128));}
    void browserReturnPublishesOnlyIdentityAndLogoutRevokes() {
        Harness h;BackendAccount account;account.setServices(h.services());account.configure("alice");account.setSocialSubject("12345");account.activate();
        QVERIFY(!h.browser.isEmpty());QVERIFY(h.returnUri.host()=="127.0.0.1");
        QVERIFY(h.callback().startsWith("HTTP/1.1 200"));
        QCOMPARE(account.snapshot()["player_id"].toString(),Player);
        QCOMPARE(h.claim["handoff_token"].toString(),QString(43,'h'));
        QVERIFY(h.claim["claim_verifier"].toString().size()==43);
        const auto exposed=QJsonDocument(account.snapshot()).toJson();
        QVERIFY(!exposed.contains("token"));QVERIFY(!exposed.contains("12345"));
        account.activate();QCOMPARE(account.snapshot()["state"].toString(),"signed_out");
        QVERIFY(h.calls.last().startsWith("/v1/auth/sessions/"));
    }
    void invalidHostAndDuplicateQueryCannotClaim() {
        Harness h;BackendAccount account;account.setServices(h.services());account.configure("alice");account.activate();
        h.callback({},"attacker.invalid");QVERIFY(h.claim.isEmpty());
        h.callback("?attempt_id="+Attempt+"&handoff_token="+QString(43,'h')+"&handoff_token="+QString(43,'h'));QVERIFY(h.claim.isEmpty());
        QVERIFY(h.callback().startsWith("HTTP/1.1 200"));QVERIFY(!h.claim.isEmpty());
    }
    void cancelledAndOldOwnerCompletionsCannotPublish() {
        Harness h;h.deferClaim=true;BackendAccount account;account.setServices(h.services());account.configure("alice");account.activate();
        h.callback();QVERIFY(bool(h.pending));account.configure("bob");h.pending(200,credentials());
        QVERIFY(account.snapshot()["player_id"].toString().isEmpty());QVERIFY(h.disk.isEmpty());
        account.activate();account.cancel();QVERIFY(account.snapshot()["player_id"].toString().isEmpty());
    }
    void restartRefreshIsDurableAndAmbiguousFailureNeverReplays() {
        Harness h;
        {BackendAccount account;account.setServices(h.services());account.configure("alice");account.activate();h.callback();QCOMPARE(account.snapshot()["player_id"].toString(),Player);}
        h.loseRefresh=true;
        {BackendAccount account;account.setServices(h.services());account.configure("alice");QVERIFY(account.snapshot()["player_id"].toString().isEmpty());}
        QCOMPARE(h.calls.count("/v1/auth/refresh"),1);
        {BackendAccount account;account.setServices(h.services());account.configure("alice");QVERIFY(account.detail().contains("interrupted"));}
        QCOMPARE(h.calls.count("/v1/auth/refresh"),1);
    }
    void restartRestoresAndSocialChangeInvalidates() {
        Harness h;
        {BackendAccount account;account.setServices(h.services());account.configure("alice");account.activate();h.callback();}
        BackendAccount account;account.setServices(h.services());account.configure("alice");QCOMPARE(account.snapshot()["player_id"].toString(),Player);
        account.setSocialSubject("99999");QVERIFY(account.snapshot()["player_id"].toString().isEmpty());
    }
    void storageFailureNeverPublishesSession() {
        Harness h;h.failStorage=true;BackendAccount account;account.setServices(h.services());account.configure("alice");account.activate();h.callback();
        QVERIFY(account.snapshot()["player_id"].toString().isEmpty());QVERIFY(h.calls.last().startsWith("/v1/auth/sessions/"));
    }
    void expiredAccessDoesNotPretendLogoutRevokedSession() {
        Harness h;BackendAccount account;account.setServices(h.services());account.configure("alice");account.activate();h.callback();
        h.rejectDeleteOnce=true;account.activate();
        QCOMPARE(h.calls.count("/v1/auth/refresh"),1);
        QCOMPARE(account.snapshot()["state"].toString(),"signed_out");
        QCOMPARE(h.calls.count("/v1/auth/sessions/33333333-3333-4333-8333-333333333333"),2);
    }
    void browserCancellationDoesNotClaim() {
        Harness h;BackendAccount account;account.setServices(h.services());account.configure("alice");account.activate();
        h.callback("?attempt_id="+Attempt+"&error=login_cancelled");
        QVERIFY(h.claim.isEmpty());QVERIFY(account.snapshot()["player_id"].toString().isEmpty());
    }
    void delayedVaultWriteRetainsOldOwnerLock() {
        Harness h;std::function<void(bool)> pending;
        auto services=h.services();services.write=[&](QString key,QByteArray bytes,auto done){h.disk[key]=bytes;pending=std::move(done);};
        BackendAccount account;account.setServices(services);account.configure("alice");account.activate();h.callback();
        QVERIFY(bool(pending));account.configure("bob");account.configure("alice");
        QVERIFY(account.detail().contains("another process"));pending(true);pending={};
        QVERIFY(account.snapshot()["player_id"].toString().isEmpty());
    }
    void activeGameGuardPreventsBrowserLaunch() {
        Harness h;BackendAccount account;account.setServices(h.services());account.configure("alice");
        account.loginGuard=[]{return QString("Finish the current game");};account.activate();
        QVERIFY(h.calls.isEmpty());QVERIFY(h.browser.isEmpty());QCOMPARE(account.detail(),"Finish the current game");
    }
};
QTEST_GUILESS_MAIN(BackendAccountTests)
#include "BackendAccountTests.moc"
