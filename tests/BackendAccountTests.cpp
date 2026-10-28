#include <QtTest>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QUuid>
#include "features/accounts/BackendAccount.h"
#include "platform/storage/KeychainRequest.h"
#include <QThread>
using namespace trainer;
namespace {
const QString Player="11111111-1111-4111-8111-111111111111", Attempt="22222222-2222-4222-8222-222222222222";
QJsonObject credentials() {
    return {{"player_id",Player},{"session_id","33333333-3333-4333-8333-333333333333"},
        {"fluxer_subject","12345"},{"access_token",QString(43,'a')},{"refresh_token",QString(43,'r')},
        {"expires_at",QDateTime::currentDateTimeUtc().addSecs(900).toString(Qt::ISODateWithMs)}};
}
struct Harness {
    bool shopEnabled=false,failCommerce=false;
    QJsonObject shopValue,purchaseBody;
    BackendAccount::Reply pendingPurchase;
    QHash<QString,QByteArray> commerceDisk;
    bool progressionOk=false,deferProgression=false;
    QJsonObject progress;
    BackendAccount::Reply pendingProgression;
    QHash<QString,QByteArray> disk;
    QUrl returnUri,browser;
    QList<QString> calls;
    QJsonObject claim;
    bool failStorage=false,loseRefresh=false,deferClaim=false,rejectDeleteOnce=false;
    BackendAccount::Reply pending;
    BackendAccount::Reply pendingGrant;
    bool deferGrant=false;
    bool deferAvatar=false;
    BackendAccount::Reply pendingAvatar;
    QList<QJsonObject> avatars;
    QList<QByteArray> playAuth;
    QList<QJsonObject> playBodies;
    QJsonObject grant() {return {{"grant_token",QString(43,'g')},{"ruleset",QJsonObject{{"id","test.v1"}}},{"trust","peer_confirmed"},
        {"expires_at",QDateTime::currentDateTimeUtc().addSecs(7200).toString(Qt::ISODateWithMs)}};}
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
                } else if(path=="/v1/me/progression" || path.startsWith("/v1/me/progression/ra/")) {
                    Q_ASSERT(auth==QByteArray(43,'a'));
                    if(deferProgression)pendingProgression=std::move(done);else done(progressionOk?200:503,progress);
                } else if(path=="/v1/me/inventory"||path.startsWith("/v1/me/purchases/")) {done(shopEnabled?200:503,shopValue);}
                else if(path=="/v1/me/purchases") {purchaseBody=body;pendingPurchase=std::move(done);}
                else if(path=="/v1/me/inventory/appearance") {shopValue["equipped"]=body["offer_id"];shopValue["revision"]=QString::number(shopValue["revision"].toString().toInt()+1);done(200,shopValue);}
                else if(path=="/v1/play/grants") {playAuth<<auth;playBodies<<body;if(deferGrant)pendingGrant=std::move(done);else done(200,grant());}
                else if(path=="/v1/play/commands") {playAuth<<auth;playBodies<<body;done(200,{{"state","accepted"}});}
                else if(path=="/v1/me/profile/avatar") {avatars<<body;Q_ASSERT(auth==QByteArray(43,'a'));if(deferAvatar)pendingAvatar=std::move(done);else done(200,{});}
                else if(method=="DELETE"&&!auth.isEmpty()) {const int status=rejectDeleteOnce?401:204;rejectDeleteOnce=false;done(status,{});}
                else done(500,{});
            },
            [this](QString key,auto done){done(true,disk.value(key));},
            [this](QString key,QByteArray bytes,auto done){if(!failStorage)disk[key]=bytes;done(!failStorage);},
            [this](QUrl url){browser=url;return true;},
            [this](QString key){return commerceDisk.value(key);},
            [this](QString key,QByteArray bytes){if(failCommerce)return false;commerceDisk[key]=bytes;return true;}
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
    static QJsonObject offer() {return {{"id","profile.aurora"},{"title","Aurora"},{"description","Northern forest"},{"appearance","aurora"},
        {"price",250},{"revision","1"},{"minimum_level",1},{"kind","permanent_license"},{"available",true}};}
    static QJsonObject inventory() {return {{"player_id",Player},{"revision","1"},{"items",QJsonArray{}},{"catalogue",QJsonArray{offer()}},{"equipped",QJsonValue()}};}
    static void settle(Harness& h) {
        auto item=offer();item["purchase_id"]="44444444-4444-4444-8444-444444444444";
        h.shopValue["items"]=QJsonArray{item};h.shopValue["revision"]="2";
        auto receipt=h.purchaseBody;receipt["id"]=item["purchase_id"];receipt["player_id"]=Player;receipt["price"]=receipt.take("expected_price");
        h.shopValue["receipt"]=receipt;
    }
private slots:
    void supportCreditsKeepXpAndAcceptOnlyValidBonusHistory() {
        Harness h;h.progressionOk=true;
        const auto date=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        h.progress={{"player_id",Player},{"balance",1000},{"revision","31"},{"history",QJsonArray{}},
            {"mastery",QJsonObject{{"xp",2000},{"level",5},{"level_xp",200},{"next_level_xp",700}}},{"checked_at",date}};
        BackendAccount a;a.setServices(h.services());a.configure("support-credit");a.activate();h.callback();
        QSignalSpy earned(&a,&BackendAccount::rewardsEarned);
        auto bonus=QJsonObject{{"id","44444444-4444-4444-8444-444444444444"},{"source","support.topup"},
            {"title","Completed set"},{"xp",0},{"sparks",1100},{"support_multiplier",3},{"earned_at",date}};
        h.progress["balance"]=2100;h.progress["revision"]="32";h.progress["history"]=QJsonArray{bonus};
        h.progress["support"]=QJsonObject{{"multiplier",3},{"verified_until",QDateTime::currentDateTimeUtc().addDays(1).toString(Qt::ISODateWithMs)}};
        a.refreshProgression(false);QCOMPARE(earned.size(),1);QCOMPARE(earned[0][0].toLongLong(),0);QCOMPARE(earned[0][1].toLongLong(),1100);
        QCOMPARE(a.progression()["mastery"].toMap()["xp"].toInt(),2000);QCOMPARE(a.progression()["support"].toMap()["multiplier"].toInt(),3);
        a.refreshProgression(false);QCOMPARE(earned.size(),1);
        bonus["xp"]=1;h.progress["history"]=QJsonArray{bonus};a.refreshProgression(false);QVERIFY(a.progression()["stale"].toBool());
        bonus["xp"]=0;bonus["support_multiplier"]=4;h.progress["history"]=QJsonArray{bonus};a.refreshProgression(false);QVERIFY(a.progression()["stale"].toBool());
        bonus["support_multiplier"]=3;h.progress["history"]=QJsonArray{bonus};h.progress["support"]=QJsonObject{{"multiplier",9}};
        a.refreshProgression(false);QVERIFY(!a.progression()["stale"].toBool());QVERIFY(a.progression()["support"].toMap().isEmpty());
    }
    void supportPreviewRejectsMalformedStaleAndUnsettledBalances() {
        Harness h;h.progressionOk=true;h.shopEnabled=true;h.shopValue=inventory();
        const QJsonObject preview{{"policy","support-preview-2026-10-v1"},{"state","calculation_only"},{"activation_available",false},
            {"eligible_rewards",31},{"base_sparks",1000},{"credited_reward_sparks",1000},{"options",QJsonArray{
                QJsonObject{{"id","support-x2"},{"title","Support x2"},{"multiplier",2},{"top_up",1000},{"potential_balance",1750}},
                QJsonObject{{"id","support-x3"},{"title","Support x3"},{"multiplier",3},{"top_up",2000},{"potential_balance",2750}}}}};
        h.progress={{"player_id",Player},{"balance",750},{"revision","32"},{"history",QJsonArray{}},{"support_preview",preview},
            {"mastery",QJsonObject{{"xp",2000},{"level",5},{"level_xp",200},{"next_level_xp",700}}},
            {"checked_at",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        BackendAccount a;a.setServices(h.services());a.configure("support-alice");a.activate();h.callback();
        auto known=[&]{return !a.progression()["support_preview"].toMap().isEmpty();};
        QVERIFY(known());
        for(int bad=0;bad<5;++bad) {
            auto invalid=preview;auto options=preview["options"].toArray();auto row=options[1].toObject();
            if(bad==0)row["potential_balance"]=3000;
            else if(bad==1)row["top_up"]=-1;
            else if(bad==2)row["multiplier"]=2;
            else if(bad==3)invalid["activation_available"]=true;
            else invalid["policy"]="unknown";
            options[1]=row;invalid["options"]=options;h.progress["support_preview"]=invalid;a.refreshProgression(false);
            QVERIFY(!known());QCOMPARE(a.progression()["balance"].toInt(),750);QVERIFY(!a.progression()["stale"].toBool());
        }
        h.progress["support_preview"]=preview;a.refreshProgression(false);QVERIFY(known());
        h.deferProgression=true;a.refreshProgression(false);QVERIFY(!known());
        auto reply=std::move(h.pendingProgression);reply(503,{});QVERIFY(!known());QVERIFY(a.progression()["stale"].toBool());
        h.deferProgression=false;a.refreshProgression(false);QVERIFY(known());
        a.selectShopItem(1);a.activateShopItem();a.confirmPurchase();QVERIFY(h.pendingPurchase);QVERIFY(!known());
        settle(h);h.deferProgression=true;h.pendingPurchase(200,h.shopValue);QVERIFY(!known());
        reply=std::move(h.pendingProgression);QVERIFY(reply);reply(200,h.progress);QVERIFY(known());
        h.deferProgression=true;a.refreshProgression(false);reply=std::move(h.pendingProgression);
        a.configure("support-bob");reply(200,h.progress);QVERIFY(!known());QVERIFY(!a.progression()["known"].toBool());
    }
    void purchaseNeedsReviewAndDurableIntentThenRestoresLostResponse() {
        Harness h;h.shopEnabled=true;h.shopValue=inventory();
        {
            BackendAccount a;a.setServices(h.services());a.configure("shop-alice");a.activate();h.callback();
            a.selectShopItem(1);a.activateShopItem();QVERIFY(!a.shop()["review"].toMap().isEmpty());QVERIFY(!h.pendingPurchase);
            h.failCommerce=true;a.confirmPurchase();QVERIFY(!h.pendingPurchase);QVERIFY(a.shop()["status"].toString().contains("Nothing was sent"));
            h.failCommerce=false;a.confirmPurchase();QVERIFY(h.pendingPurchase);QVERIFY(a.shop()["pending"].toBool());
            QVERIFY(QJsonDocument::fromJson(h.commerceDisk.values().first()).object()["pending"].toObject().contains("operation_id"));
            const auto body=h.purchaseBody;a.confirmPurchase();QCOMPARE(h.purchaseBody,body);
            settle(h);h.pendingPurchase(0,{});QVERIFY(a.shop()["pending"].toBool());
        }
        const auto charged=h.calls.count("/v1/me/purchases");
        BackendAccount restored;restored.setServices(h.services());restored.configure("shop-alice");
        QVERIFY(!restored.shop()["pending"].toBool()); // Receipt recovered after verified session restore.
        QCOMPARE(h.calls.count("/v1/me/purchases"),charged);
        restored.selectShopItem(1);QVERIFY(restored.shop()["selected"].toMap()["owned"].toBool());
        restored.activateShopItem();QCOMPARE(restored.profileAppearance(),"aurora");
        restored.selectShopItem(0);restored.activateShopItem();QVERIFY(restored.profileAppearance().isEmpty());
    }
    void purchaseLateOwnerResponseAndOfflineAppearanceAreIsolated() {
        Harness h;h.shopEnabled=true;h.shopValue=inventory();
        BackendAccount a;a.setServices(h.services());a.configure("shop-first");a.activate();h.callback();
        a.selectShopItem(1);a.activateShopItem();a.confirmPurchase();auto late=std::move(h.pendingPurchase);settle(h);
        a.configure("shop-second");late(200,h.shopValue);QVERIFY(a.profileAppearance().isEmpty());QVERIFY(!a.shop()["pending"].toBool());
        a.configure("shop-first");a.selectShopItem(1);a.activateShopItem();QCOMPARE(a.profileAppearance(),"aurora");
        a.configure("shop-third");h.loseRefresh=true;a.configure("shop-first");
        QCOMPARE(a.profileAppearance(),"aurora");QVERIFY(a.shop()["stale"].toBool());QVERIFY(!a.shop()["connected"].toBool());
        a.configure("shop-third");QVERIFY(a.profileAppearance().isEmpty());
    }
    void vaultJobsFromConcurrentShortLivedWorkersReturnOnReceiverThread() {
#ifndef Q_OS_WIN
        QSKIP("Windows credential-store regression; no desktop vault prompts on Linux CI");
#else
        int completed=0, abandoned=0;
        QList<QThread*> workers;
        for(int i=0;i<4;++i) {
            auto* worker=QThread::create([this,&completed,&abandoned]{
                for(int j=0;j<8;++j) {
                    QObject transient;
                    const auto key=QUuid::createUuid().toString();
                    KeychainRequest::run("TrainerOS.ReadOnlyRegression",key,KeychainRequest::Read,{},&transient,
                        [&abandoned](int,QByteArray){++abandoned;});
                    KeychainRequest::run("TrainerOS.ReadOnlyRegression",key,KeychainRequest::Read,{},this,
                        [this,&completed](int,QByteArray bytes){
                            QCOMPARE(QThread::currentThread(),thread());
                            QVERIFY(bytes.isEmpty());++completed;
                        });
                }
            });
            workers.append(worker);worker->start();
        }
        for(auto* worker:workers){worker->wait();delete worker;}
        QTRY_COMPARE_WITH_TIMEOUT(completed,32,10000);
        QCOMPARE(abandoned,0);
#endif
    }
    void progressionPersistsOnServerAndFencesOwnerAndMalformedReplies() {
        Harness h;h.progressionOk=true;
        h.progress={{"player_id",Player},{"balance",1000},{"revision","31"},{"history",QJsonArray{}},
            {"mastery",QJsonObject{{"xp",2000},{"level",5},{"level_xp",200},{"next_level_xp",700}}},
            {"checked_at",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        BackendAccount a;a.setServices(h.services());a.configure("progression-alice");a.activate();h.callback();
        QVERIFY(a.progression()["known"].toBool());QCOMPARE(a.progression()["balance"].toInt(),1000);
        QSignalSpy awards(&a,&BackendAccount::rewardsEarned);
        h.progress["balance"]=1015;h.progress["revision"]="32";
        auto mastery=h.progress["mastery"].toObject();mastery["xp"]=2030;mastery["level_xp"]=230;h.progress["mastery"]=mastery;
        a.refreshProgression(false);QCOMPARE(awards.size(),1);
        a.refreshProgression(false);QCOMPARE(awards.size(),1);
        h.progress["player_id"]="99999999-9999-4999-8999-999999999999";
        a.refreshProgression(false);QCOMPARE(a.progression()["balance"].toInt(),1015);QVERIFY(a.progression()["stale"].toBool());
        h.deferProgression=true;a.refreshProgression(false);auto reply=std::move(h.pendingProgression);
        a.configure("progression-bob");reply(200,h.progress);QVERIFY(!a.progression()["known"].toBool());
        QVERIFY(!QJsonDocument::fromVariant(a.progression()).toJson().contains("web_key"));
    }
    void progressionDoesNotSubmitRaCredentialsWithoutExplicitLink() {
        Harness h;h.progressionOk=true;
        h.progress={{"player_id",Player},{"balance",0},{"revision","0"},{"history",QJsonArray{}},
            {"mastery",QJsonObject{{"xp",0},{"level",1},{"level_xp",0},{"next_level_xp",300}}},
            {"checked_at",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
        BackendAccount a;a.setServices(h.services());int sources=0;
        a.rewardSource=[&]{++sources;return QJsonObject{{"username","Tester"},{"web_key",QString(32,'k')},{"games",QJsonArray{1}}};};
        a.configure("consent-alice");a.activate();h.callback();a.refreshProgression();QCOMPARE(sources,0);
        a.activateRewards();QCOMPARE(sources,1);QVERIFY(h.calls.contains("/v1/me/progression/ra/challenge"));
    }
    void initTestCase(){QStandardPaths::setTestModeEnabled(true);QCoreApplication::setOrganizationName("TrainerOSAccountTests");QCoreApplication::setApplicationName(QUuid::createUuid().toString(QUuid::Id128));}
    void publicAvatarCoalescesAndFencesOwnerChanges() {
        Harness h;BackendAccount a;a.setServices(h.services());a.configure("alice");a.activate();h.callback();
        const QJsonObject first{{"schema",1},{"packRevision",QString(64,'a')}},second{{"schema",1},{"packRevision",QString(64,'b')}};
        a.setPublicAvatar("other",first);QCOMPARE(h.avatars.size(),0);
        h.deferAvatar=true;a.setPublicAvatar("alice",first);QCOMPARE(h.avatars.size(),1);
        a.setPublicAvatar("alice",second);QCOMPARE(h.avatars.size(),1);
        auto complete=std::move(h.pendingAvatar);h.deferAvatar=false;complete(200,{});
        QCOMPARE(h.avatars.size(),2);QCOMPARE(h.avatars.last()["avatar"].toObject(),second);
        a.setPublicAvatar("alice",second);a.setPublicAvatar("alice",{});QCOMPARE(h.avatars.size(),2);
        h.deferAvatar=true;a.setPublicAvatar("alice",first);complete=std::move(h.pendingAvatar);
        a.configure("bob");complete(200,{});a.setPublicAvatar("alice",second);QCOMPARE(h.avatars.size(),3);
        QVERIFY(!QJsonDocument(a.snapshot()).toJson().contains("packRevision"));
    }
    void gameFacadeScopesCredentialsAndRejectsArbitraryProxy() {
        Harness h;BackendAccount a;a.setServices(h.services());a.configure("alice");a.activate();h.callback();
        QJsonObject result;int status=0;auto done=[&](int s,QJsonObject r){status=s;result=r;};
        a.playRequest("peer","org.test.game","play.open",{{"ruleset_id","test.v1"}},done);
        QCOMPARE(status,200);QVERIFY(!QJsonDocument(result).toJson().contains("token"));
        QCOMPARE(h.playAuth.last(),QByteArray(43,'a'));
        const QString operation=QUuid::createUuid().toString(QUuid::WithoutBraces);
        a.playRequest("peer","org.test.game","play.create",{{"operation_id",operation},{"payload",QJsonObject{{"mode","individual"}}}},done);
        QCOMPARE(h.playAuth.last(),QByteArray(43,'g'));QCOMPARE(h.playBodies.last()["operation_id"].toString(),operation);
        const int before=h.calls.size();
        a.playRequest("peer","org.test.game","play.proxy",{{"url","https://evil.invalid"}},done);
        QCOMPARE(status,409);QCOMPARE(h.calls.size(),before);
        a.playRequest("other","org.test.game","play.history",{{"operation_id",operation},{"payload",QJsonObject{}}},done);
        QCOMPARE(result["code"].toString(),"play_not_open");
        a.closePlay("peer");QCOMPARE(h.playBodies.last()["action"].toString(),"revoke");
        QCOMPARE(h.playAuth.last(),QByteArray(43,'g'));
    }
    void lateGameGrantIsRevokedAfterDisconnectOrOwnerChange() {
        Harness h;BackendAccount a;a.setServices(h.services());a.configure("alice");a.activate();h.callback();h.deferGrant=true;
        bool delivered=false;a.playRequest("peer","org.test.game","play.open",{{"ruleset_id","test.v1"}},[&](int,QJsonObject){delivered=true;});
        a.closePlay("peer");h.pendingGrant(200,h.grant());QVERIFY(!delivered);
        QCOMPARE(h.playBodies.last()["action"].toString(),"revoke");
        a.playRequest("peer2","org.test.game","play.open",{{"ruleset_id","test.v1"}},[&](int,QJsonObject){delivered=true;});
        a.configure("bob");h.pendingGrant(200,h.grant());QVERIFY(!delivered);
        QCOMPARE(h.playBodies.last()["action"].toString(),"revoke");
    }
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
