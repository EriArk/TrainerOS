#include <QtTest>
#include <QFile>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QProcess>
#include <QSettings>
#include <QTcpSocket>
#include <QTemporaryDir>
#include "platform/sdk/PlatformHost.h"
#include "features/device/ControlsDashboard.h"
#include "features/trainer/AvatarAssets.h"
using namespace trainer;
namespace {
struct Client {
    QTcpSocket socket;
    QJsonObject discovery;
    int sequence=0;
    explicit Client(const QString& path) {
        QFile file(path);if(file.open(QIODevice::ReadOnly))discovery=QJsonDocument::fromJson(file.readAll()).object();
        socket.connectToHost(QHostAddress::LocalHost,discovery["port"].toInt());
        socket.waitForConnected(2000);
    }
    void send(const QString& method,const QJsonObject& params={}) {
        socket.write(QJsonDocument(QJsonObject{{"id",QString::number(++sequence)},{"method",method},{"params",params}}).toJson(QJsonDocument::Compact)+'\n');
        socket.flush();
    }
    QJsonObject receive(int timeout=3000) {
        QElapsedTimer timer;timer.start();
        while(!socket.canReadLine()&&timer.elapsed()<timeout){QCoreApplication::processEvents();QTest::qWait(1);}
        return QJsonDocument::fromJson(socket.readLine()).object();
    }
    QJsonObject hello(const QString& app="org.test.one",int version=1) {
        send("hello",{{"protocol",version},{"token",discovery["token"]},{"app",app}});return receive();
    }
};
QJsonObject context(const QString& owner="alice",const QString& game="game-a") {
    return {{"profile",owner},{"selected",QJsonObject{{"id",game}}},{"game",QJsonObject{{"state","idle"}}}};
}
struct Fixture {
    QTemporaryDir dir;
    ControlsDashboard board;
    PlatformHost host{board};
    QString endpoint=dir.filePath("endpoint.json");
    Fixture(){board.setOwner("alice");host.setContext(context());}
    bool start(){return host.start(endpoint);}
    QJsonObject widget(const QString& revision,const QString& id="hello") {
        return {{"revision",revision},{"id",id},{"title","Test widget"},{"text","Hello"}};
    }
};
}
class PlatformHostTests final: public QObject {
    Q_OBJECT
    QTemporaryDir settings_;
private slots:
    void initTestCase(){
        QCoreApplication::setOrganizationName("TrainerSdkTests");QCoreApplication::setApplicationName("Host");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings_.path());
    }
    void init(){QSettings().clear();}
    void discoveryAndExclusiveEndpoint(){
        Fixture f;QVERIFY(f.start());
        ControlsDashboard other;PlatformHost duplicate(other);QString error;
        QVERIFY(!duplicate.start(f.endpoint,&error));QVERIFY(!error.isEmpty());QVERIFY(QFile::exists(f.endpoint));
        Client client(f.endpoint);auto result=client.hello()["result"].toObject();
        QCOMPARE(result["protocol"].toInt(),1);QVERIFY(result["capabilities"].toObject().contains("context"));
        QVERIFY(!result["capabilities"].toObject().contains("wallet"));
        client.send("wallet.credit",{{"amount",50}});QCOMPARE(client.receive()["error"].toObject()["code"].toString(),"unsupported_method");
        f.host.stop();QVERIFY(!QFile::exists(f.endpoint));
    }
    void localAvatarSubscriptionAndRevocation(){
        Fixture f;f.host.setAvatar({},true);QVERIFY(f.start());Client c(f.endpoint);
        QCOMPARE(c.hello()["result"].toObject()["capabilities"].toObject()["avatar.local"].toInt(),1);
        c.send("avatar.subscribe");QCOMPARE(c.receive()["error"].toObject()["code"].toString(),"avatar_not_ready");
        QJsonObject avatar{{"schema",1},{"revision","one"},{"glb","/console/cache/one.glb"},{"profile","alice"}};
        f.host.setAvatar(avatar,true);QCOMPARE(c.receive()["avatar"].toObject(),avatar);
        c.send("avatar.get");QCOMPARE(c.receive()["result"].toObject()["avatar"].toObject(),avatar);
        c.send("avatar.set",{{"hair","bob"}});QCOMPARE(c.receive()["error"].toObject()["code"].toString(),"unsupported_method");
        f.host.setContext(context("bob"));QCOMPARE(c.receive()["event"].toString(),"context.revoked");
        Client b(f.endpoint);b.hello("org.test.bob");b.send("avatar.get");QCOMPARE(b.receive()["error"].toObject()["code"].toString(),"avatar_not_ready");
        f.host.setAvatar({},false);b.send("avatar.get");QCOMPARE(b.receive()["error"].toObject()["code"].toString(),"unsupported_method");
    }
    void asynchronousAvatarRecipes(){
        if(!AvatarAssets::available())QSKIP("Local pack required");
        Fixture f;auto own=AvatarAssets::resolve(AvatarAssets::defaults(),{},f.dir.filePath("own"));
        own["profile"]="alice";f.host.setAvatar(own,true);QVERIFY(f.start());
        Client c(f.endpoint);c.hello();
        const auto recipe=own["recipe"].toObject();
        c.send("avatar.resolve",{{"recipe",recipe}});
        c.send("avatar.resolve",{{"recipe",recipe}});
        QCOMPARE(c.receive()["error"].toObject()["code"].toString(),"avatar_busy");
        c.send("context.get");
        QCOMPARE(c.receive()["result"].toObject()["context"].toObject()["profile"].toString(),"alice");
        const auto result=c.receive(30000)["result"].toObject()["avatar"].toObject();
        QCOMPARE(result["sha256"],own["sha256"]);QVERIFY(!result.contains("profile"));
        const auto path=result["glb"].toString();QVERIFY(QFile::exists(path));
        c.send("avatar.get");QCOMPARE(c.receive()["result"].toObject()["avatar"].toObject()["revision"],own["revision"]);
        auto bad=recipe;bad["packRevision"]="unknown";c.send("avatar.resolve",{{"recipe",bad}});
        QCOMPARE(c.receive()["error"].toObject()["code"].toString(),"avatar_pack_mismatch");
        f.host.setContext(context("bob"));QCOMPARE(c.receive()["event"].toString(),"context.revoked");
        QTRY_VERIFY(!QFile::exists(path));
    }
    void realGodotAvatarConsumer(){
        const auto godot=qEnvironmentVariable("TRAINER_AVATAR_GODOT");
        if(godot.isEmpty()||!AvatarAssets::available())QSKIP("External Godot and local pack required");
        Fixture f;auto appearance=AvatarAssets::defaults();
        auto avatar=AvatarAssets::resolve(appearance,{},f.dir.filePath("cache"));
        QVERIFY(!avatar.contains("error"));avatar["profile"]="alice";
        f.host.setAvatar(avatar,true);QVERIFY(f.start());
        QProcess child;auto env=QProcessEnvironment::systemEnvironment();env.insert("TRAINEROS_SDK_ENDPOINT",f.endpoint);child.setProcessEnvironment(env);
        // Optional external consumer exercises the same real host/pack/lifecycle.
        const auto project=qEnvironmentVariable("TRAINER_AVATAR_CONSUMER_PROJECT",QString::fromUtf8(SDK_SOURCE)+"/sdk/godot");
        const auto script=qEnvironmentVariable("TRAINER_AVATAR_CONSUMER_SCRIPT","client_probe.gd");
        QStringList args{"--path",project,"--script",script};
        if(!qEnvironmentVariableIsSet("TRAINER_AVATAR_CONSUMER_RENDER"))args.prepend("--headless");
        else args<<"--rendering-method"<<"gl_compatibility";
        child.start(godot,args);
        QVERIFY(child.waitForStarted());QByteArray output;
        QTRY_VERIFY_WITH_TIMEOUT((output+=child.readAllStandardOutput()).contains("AVATAR_RECEIVED 1"),30000);
        appearance["hair"]="bob";avatar=AvatarAssets::resolve(appearance,{},f.dir.filePath("cache"));avatar["profile"]="alice";f.host.setAvatar(avatar,true);
        QTRY_VERIFY_WITH_TIMEOUT((output+=child.readAllStandardOutput()).contains("AVATAR_RECEIVED 2"),30000);
        f.host.setContext(context("bob"));QTRY_COMPARE_WITH_TIMEOUT(child.state(),QProcess::NotRunning,10000);
        output+=child.readAllStandardOutput();const auto errors=child.readAllStandardError();
        QVERIFY2(child.exitCode()==0,errors.constData());QVERIFY2(!errors.contains("ERROR"),errors.constData());QVERIFY(output.contains("AVATAR_REVOKED"));
    }
    void authVersionAndMalformedInput(){
        Fixture f;QVERIFY(f.start());
        Client bad(f.endpoint);bad.send("hello",{{"protocol",1},{"app","org.bad"},{"token","wrong"}});
        QCOMPARE(bad.receive()["error"].toObject()["code"].toString(),"unauthorized");
        Client future(f.endpoint);QCOMPARE(future.hello("org.future",2)["error"].toObject()["code"].toString(),"unsupported_version");
        Client oversized(f.endpoint);oversized.socket.write(QByteArray(65537,'x'));oversized.socket.flush();
        QTRY_COMPARE(oversized.socket.state(),QAbstractSocket::UnconnectedState);
        Client malformed(f.endpoint);malformed.socket.write("not json\n");malformed.socket.flush();
        QTRY_COMPARE(malformed.socket.state(),QAbstractSocket::UnconnectedState);
        QVERIFY(f.board.available().isEmpty());
    }
    void widgetActivationAndCrashCleanup(){
        Fixture f;QVERIFY(f.start());Client c(f.endpoint);
        const auto revision=c.hello()["result"].toObject()["context"].toObject()["revision"].toString();
        c.send("widget.put",f.widget(revision));QVERIFY(c.receive().contains("result"));
        QCOMPARE(f.board.available().size(),1);
        const QString key="sdk:org.test.one:hello";
        QSignalSpy builtin(&f.board,&ControlsDashboard::actionRequested);
        f.board.edit();f.board.pick();f.board.restore(key);f.board.edit();
        f.board.activate(key);
        QCOMPARE(c.receive()["event"].toString(),"widget.activated");QVERIFY(builtin.isEmpty());
        f.board.edit();f.board.activate(key);QVERIFY(f.board.moving());
        c.socket.abort();QTRY_VERIFY(f.board.available().isEmpty());
        QTRY_VERIFY(!f.board.moving());f.board.back();
        for(const auto& row:f.board.widgets())QVERIFY(row.toMap()["id"]!=key);
    }
    void namespaceAndStaleContext(){
        Fixture f;QVERIFY(f.start());Client a(f.endpoint),b(f.endpoint);
        const auto revision=a.hello()["result"].toObject()["context"].toObject()["revision"].toString();
        QVERIFY(b.hello("org.test.two").contains("result"));
        Client duplicate(f.endpoint);QCOMPARE(duplicate.hello()["error"].toObject()["code"].toString(),"app_in_use");
        a.send("widget.put",f.widget(revision));QVERIFY(a.receive().contains("result"));
        b.send("widget.remove",{{"id","hello"}});QVERIFY(b.receive().contains("result"));QCOMPARE(f.board.available().size(),1);
        f.host.setContext(context("alice","game-b"));
        a.send("widget.put",f.widget(revision));QCOMPARE(a.receive()["error"].toObject()["code"].toString(),"stale_context");
        b.send("context.get");const auto current=b.receive()["result"].toObject()["context"].toObject()["revision"].toString();
        b.send("widget.put",f.widget(current));QVERIFY(b.receive().contains("result"));
        f.board.setOwner("bob");f.host.setContext(context("bob"));QTRY_VERIFY(f.board.available().isEmpty());
        const auto bobKey="controls/v1/"+QString::fromLatin1(QCryptographicHash::hash("bob",QCryptographicHash::Sha256).toHex())+"/extensions";
        QVERIFY(QSettings().value(bobKey).toMap().isEmpty());
        QCOMPARE(a.receive()["event"].toString(),"context.revoked");
    }
    void pipelinedFramesAreNotOneOversizedMessage(){
        Fixture f;QVERIFY(f.start());Client c(f.endpoint);QVERIFY(c.hello().contains("result"));
        c.send("context.get",{{"padding",QString(40000,'x')}});
        c.send("context.get",{{"padding",QString(40000,'x')}});
        QVERIFY(c.receive().contains("result"));QVERIFY(c.receive().contains("result"));
    }
    void subscribeUnsubscribeAndNoProfile(){
        Fixture f;QVERIFY(f.start());Client c(f.endpoint);QVERIFY(c.hello().contains("result"));
        c.send("context.subscribe");QVERIFY(c.receive().contains("result"));
        f.host.setContext(context("alice","game-b"));QCOMPARE(c.receive()["event"].toString(),"context.changed");
        f.host.notifyLifecycle("wake");QCOMPARE(c.receive()["name"].toString(),"wake");
        c.send("context.unsubscribe");QVERIFY(c.receive().contains("result"));
        f.host.setContext(context("alice","game-c"));c.send("context.get");
        QCOMPARE(c.receive()["result"].toObject()["context"].toObject()["selected"].toObject()["id"].toString(),"game-c");
        f.host.setContext(context(""));Client empty(f.endpoint);
        const auto rev=empty.hello()["result"].toObject()["context"].toObject()["revision"].toString();
        empty.send("widget.put",f.widget(rev));QCOMPARE(empty.receive()["error"].toObject()["code"].toString(),"no_profile");
    }
    void layoutSurvivesReconnectWithoutChangingBuiltins(){
        Fixture f;QVERIFY(f.start());Client c(f.endpoint);const auto rev=c.hello()["result"].toObject()["context"].toObject()["revision"].toString();
        c.send("widget.put",f.widget(rev));c.receive();f.board.edit();f.board.pick();f.board.restore("sdk:org.test.one:hello");f.board.edit();
        c.socket.abort();QTRY_COMPARE(f.board.pages(),1);
        Client again(f.endpoint);QVERIFY(again.hello().contains("result"));again.send("widget.put",f.widget(rev));again.receive();
        QCOMPARE(f.board.pages(),2);QVERIFY(f.board.available().isEmpty());
        ControlsDashboard reopened;reopened.setOwner("alice");QCOMPARE(reopened.widgets().size(),10);
    }
    void realPythonProcess(){
        Fixture f;QVERIFY(f.start());QProcess child;
        child.start(QString::fromUtf8(SDK_PYTHON),{QString::fromUtf8(SDK_SOURCE)+"/tests/PlatformSdkClientProbe.py",f.endpoint});
        QVERIFY(child.waitForStarted());
        QTRY_COMPARE_WITH_TIMEOUT(f.board.available().size(),1,10000);
        f.board.edit();f.board.pick();f.board.restore("sdk:org.test.process:hello");f.board.edit();f.board.activate("sdk:org.test.process:hello");
        QTRY_COMPARE(f.board.widgets().first().toMap()["text"].toString(),"Callback received");
        f.host.setContext(context("alice","game-b"));
        QByteArray output;
        QTRY_VERIFY_WITH_TIMEOUT((output+=child.readAllStandardOutput()).contains("changed"),10000);
        f.host.setContext(context("bob"));
        QTRY_COMPARE_WITH_TIMEOUT(child.state(),QProcess::NotRunning,10000);
        output+=child.readAllStandardOutput();
        QVERIFY2(child.exitCode()==0,child.readAllStandardError().constData());
        QVERIFY(output.contains("activated"));QVERIFY(output.contains("revoked"));QVERIFY(f.board.available().isEmpty());
    }
};
QTEST_GUILESS_MAIN(PlatformHostTests)
#include "PlatformHostTests.moc"
