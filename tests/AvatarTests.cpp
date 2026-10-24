#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include "features/trainer/AvatarAssets.h"
#include "features/trainer/TrainerController.h"
using namespace trainer;
class AvatarTests:public QObject {
    Q_OBJECT
private slots:
    void schema(){auto a=AvatarAssets::defaults();QVERIFY(AvatarAssets::valid(a));a["eyes"]="unknown";QVERIFY(!AvatarAssets::valid(a));a=AvatarAssets::defaults();a["skinColor"]="../../secret";QVERIFY(!AvatarAssets::valid(a));}
    void editingIsolation(){
        if(!AvatarAssets::available())QSKIP("External avatar pack not installed");
        TrainerProfile p;p.id="alice";p.name="Alice";p.createdAt=QDateTime::currentDateTimeUtc();
        MockTrainerRepository repo(p);TrainerController c(repo);
        QTRY_VERIFY_WITH_TIMEOUT(!c.avatarDescriptor().isEmpty(),30000);
        const auto original=c.avatarDescriptor();c.reload();QCOMPARE(c.avatarDescriptor(),original);
        c.beginAvatarEdit();c.cycleAvatar(0);QCOMPARE(c.avatarDescriptor(),original);
        c.chooseAvatar(2,"#d1ab68");QCOMPARE(c.avatarRows()[2].toMap()["colour"].toString(),"#d1ab68");
        c.chooseAvatar(0,"missing");QCOMPARE(c.avatarRows()[0].toMap()["detail"].toString(),"ponytail");
        QCOMPARE(c.avatarDescriptor(),original);
        c.finishAvatar(false);QCOMPARE(repo.load()->avatar,p.avatar);
        c.beginAvatarEdit();c.cycleAvatar(1);c.finishAvatar(true);
        QCOMPARE(repo.load()->avatar["eyes"].toString(),"almond");
        QTRY_VERIFY_WITH_TIMEOUT(c.avatarDescriptor()["appearance"].toObject()["eyes"]=="almond",30000);
        QVERIFY(c.avatarDescriptor()["revision"]!=original["revision"]);
    }
    void externalPackAndResolvedGlb(){
        if(!AvatarAssets::available())QSKIP("External avatar pack not installed");
        QTemporaryDir cache;auto a=AvatarAssets::defaults();
        const auto destination=qEnvironmentVariable("TRAINER_AVATAR_TEST_OUTPUT");
        const auto cachePath=destination.isEmpty()?cache.path():QFileInfo(destination).absolutePath()+"/resolved";
        const auto r=AvatarAssets::resolve(a,AvatarAssets::packRoot(),cachePath);QVERIFY2(!r.contains("error"),qPrintable(r["error"].toString()));
        QVERIFY(QFile::exists(r["glb"].toString()));QCOMPARE(r,AvatarAssets::resolve(a,AvatarAssets::packRoot(),cachePath));
        auto recipe=r["recipe"].toObject();
        QVERIFY(QJsonDocument(recipe).toJson(QJsonDocument::Compact).size()<1024);
        const auto peer=AvatarAssets::resolveRecipe(recipe,cachePath);
        QCOMPARE(peer["sha256"],r["sha256"]);QVERIFY(!peer.contains("profile"));
        auto bad=recipe;bad["packRevision"]="older-pack";
        QCOMPARE(AvatarAssets::resolveRecipe(bad,cachePath)["error"].toString(),"avatar_pack_mismatch");
        bad=recipe;bad["glb"]="https://untrusted/model.glb";
        QCOMPARE(AvatarAssets::resolveRecipe(bad,cachePath)["error"].toString(),"invalid_avatar_recipe");
        bad=recipe;auto parts=a;parts["hair"]="../../other";bad["appearance"]=parts;
        QCOMPARE(AvatarAssets::resolveRecipe(bad,cachePath)["error"].toString(),"invalid_avatar_recipe");
        a["hair"]="bob";a["eyes"]="almond";a["skinColor"]="#614132";
        auto other=AvatarAssets::resolve(a,AvatarAssets::packRoot(),cachePath);QVERIFY(!other.contains("error"));QVERIFY(other["sha256"]!=r["sha256"]);
        QVERIFY(other["glb"]!=r["glb"]);QVERIFY(QFile::exists(r["glb"].toString()));
        if(!destination.isEmpty()){QFile file(destination);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(r).toJson());}
    }
};
QTEST_GUILESS_MAIN(AvatarTests)
#include "AvatarTests.moc"
