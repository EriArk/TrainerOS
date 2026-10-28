#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QtEndian>
#include "features/trainer/AvatarAssets.h"
#include "features/trainer/TrainerController.h"
using namespace trainer;
class AvatarTests:public QObject {
    Q_OBJECT
private slots:
    void savedLookAppliesWholeAppearanceAndPreservesTrainer() {
        if(!AvatarAssets::available())QSKIP("External avatar pack not installed");
        TrainerProfile p;p.id="looks-owner";p.name="Alice";p.createdAt=QDateTime::currentDateTimeUtc();p.avatar=AvatarAssets::defaults();
        MockTrainerRepository repo(p);TrainerController c(repo);
        auto look=AvatarAssets::defaults();look["hair"]="bob";look["shirtColor"]="#a8c9bc";look["eyes"]="almond";
        QVERIFY(c.applyAppearance(look));QCOMPARE(repo.load()->avatar,look);QCOMPARE(repo.load()->id,p.id);QCOMPARE(repo.load()->name,p.name);
        QTRY_COMPARE_WITH_TIMEOUT(c.avatarDescriptor()["appearance"].toObject(),look,30000);
        auto bad=look;bad["hair"]="missing";QVERIFY(!c.applyAppearance(bad));QCOMPARE(repo.load()->avatar,look);
        c.beginAvatarEdit();QVERIFY(!c.applyAppearance(p.avatar));c.finishAvatar(false);QCOMPARE(repo.load()->avatar,look);
    }
    void schema(){auto a=AvatarAssets::defaults();QVERIFY(AvatarAssets::valid(a));a["eyes"]="unknown";QVERIFY(!AvatarAssets::valid(a));a=AvatarAssets::defaults();a["skinColor"]="../../secret";QVERIFY(!AvatarAssets::valid(a));}
    void approvedVariants(){
        if(!AvatarAssets::available())QSKIP("External avatar pack not installed");
        QTemporaryDir cache;QSet<QString> hashes;int index=0;
        QFile manifest(AvatarAssets::packRoot()+"/manifest.json");QVERIFY(manifest.open(QIODevice::ReadOnly));
        const auto pack=QJsonDocument::fromJson(manifest.readAll()).object();QCOMPARE(pack["revision"].toInt(),6);
        QFile templateFile(AvatarAssets::packRoot()+"/template.glb");QVERIFY(templateFile.open(QIODevice::ReadOnly));
        const auto templateBytes=templateFile.readAll();
        const auto templateJson=QJsonDocument::fromJson(templateBytes.mid(20,qFromLittleEndian<quint32>(templateBytes.constData()+12))).object();
        const auto face=templateJson["extras"].toObject()["trainerFace"].toObject();
        QCOMPARE(face["schema"].toInt(),1);
        QCOMPARE(face["mouths"].toArray().size(),9);
        QCOMPARE(face["expressions"].toObject().size(),14);
        for(const auto& name:{"cat_smug","content","squeezed","love","shocked"})
            QVERIFY(face["expressions"].toObject().contains(name));
        QCOMPARE(pack["eyeBodyMeshes"].toObject()["round"],pack["preparedBodyMesh"]);
        QCOMPARE(pack["eyeBodyMeshes"].toObject()["almond"],pack["preparedBodyMesh"]);
        QCOMPARE(face["speech"].toBool(),false);
        QCOMPARE(pack["eyeNodes"].toObject().size(),14);
        const auto bodyPrimitive=templateJson["meshes"].toArray()[pack["legacyBodyMesh"].toInt()].toObject()["primitives"].toArray()[0].toObject();
        const auto bodyMaterial=templateJson["materials"].toArray()[bodyPrimitive["material"].toInt()].toObject();
        const auto textures=templateJson["textures"].toArray();
        const int colourTexture=bodyMaterial["pbrMetallicRoughness"].toObject()["baseColorTexture"].toObject()["index"].toInt();
        QCOMPARE(pack["bodyImage"],textures[colourTexture].toObject()["source"]);
        QVERIFY(pack["bodyImage"]!=textures[bodyMaterial["normalTexture"].toObject()["index"].toInt()].toObject()["source"]);
        const auto preparedPrimitives=templateJson["meshes"].toArray()[pack["preparedBodyMesh"].toInt()].toObject()["primitives"].toArray();
        for(const auto& primitive:preparedPrimitives)
            QVERIFY(primitive.toObject()["attributes"].toObject().contains("COLOR_0"));
        const int whiteIndex=preparedPrimitives[0].toObject()["attributes"].toObject()["COLOR_0"].toInt();
        const auto whiteAccessor=templateJson["accessors"].toArray()[whiteIndex].toObject();
        const auto whiteView=templateJson["bufferViews"].toArray()[whiteAccessor["bufferView"].toInt()].toObject();
        const int binaryStart=28+qFromLittleEndian<quint32>(templateBytes.constData()+12);
        const char* white=templateBytes.constData()+binaryStart+whiteView["byteOffset"].toInt();
        for(int i=0;i<whiteAccessor["count"].toInt()*4;++i)
            QCOMPARE(qFromLittleEndian<quint32>(white+i*4),quint32(0x3f800000));
        const auto destination=qEnvironmentVariable("TRAINER_AVATAR_TEST_OUTPUT");
        const auto cachePath=destination.isEmpty()?cache.path():QFileInfo(destination).absolutePath()+"/variants";
        for(const auto& eyes:AvatarAssets::choices("eyes")){
            auto a=AvatarAssets::defaults();a["eyes"]=eyes;a["hair"]=AvatarAssets::choices("hair")[index%4];
            a["skinColor"]=index%2?"#614132":"#f7d8bf";a["eyeColor"]="#547d9a";
            const auto r=AvatarAssets::resolve(a,AvatarAssets::packRoot(),cachePath);
            QVERIFY2(!r.contains("error"),qPrintable(eyes+": "+r["error"].toString()));
            QVERIFY(!hashes.contains(r["sha256"].toString()));hashes.insert(r["sha256"].toString());
            QFile f(r["glb"].toString());QVERIFY(f.open(QIODevice::ReadOnly));const auto bytes=f.readAll();
            QVERIFY(bytes.size()<64*1024*1024);
            const auto g=QJsonDocument::fromJson(bytes.mid(20,qFromLittleEndian<quint32>(bytes.constData()+12))).object();
            const auto nodes=g["nodes"].toArray();int visibleHair=0,visibleEyes=0;
            for(const auto& value:nodes){const auto node=value.toObject();if(!node.contains("mesh"))continue;
                for(const auto& primitive:g["meshes"].toArray()[node["mesh"].toInt()].toObject()["primitives"].toArray()){
                    const auto attrs=primitive.toObject()["attributes"].toObject();
                    for(const auto& accessor:attrs)QVERIFY(g["accessors"].toArray()[accessor.toInt()].toObject().contains("bufferView"));
                }
                if(node["name"].toString().startsWith("hair-"))++visibleHair;
                if(node["name"].toString().startsWith("eyes-")){++visibleEyes;QVERIFY(node["name"].toString().startsWith("eyes-"+eyes+"-"));}
            }
            QCOMPARE(visibleHair,1);
            const auto expected=pack["eyeNodes"].toObject()[eyes].toArray();QCOMPARE(visibleEyes,expected.size());
            const auto body=nodes[pack["bodyNode"].toInt()].toObject();
            QCOMPARE(body["mesh"],pack["eyeBodyMeshes"].toObject().contains(eyes)
                ?pack["eyeBodyMeshes"].toObject()[eyes]:pack[expected.isEmpty()?"legacyBodyMesh":"preparedBodyMesh"]);
            QCOMPARE(g["animations"].toArray().size(),8);
            QCOMPARE(g["extras"].toObject()["trainerFace"],face);
            const auto bodyMesh=g["meshes"].toArray()[body["mesh"].toInt()].toObject();
            for(const auto& primitive:bodyMesh["primitives"].toArray()){
                QCOMPARE(primitive.toObject()["targets"].toArray().size(),46);
                QVERIFY(primitive.toObject()["attributes"].toObject().contains("COLOR_0"));
            }
            int mouths=0;
            for(const auto& item:nodes)if(item.toObject()["name"]=="Face_Mouth"){
                ++mouths;const auto mouth=g["meshes"].toArray()[item.toObject()["mesh"].toInt()].toObject();
                QCOMPARE(mouth["extras"].toObject()["targetNames"].toArray().size(),8);
                for(const auto& weight:mouth["weights"].toArray())QCOMPARE(weight.toDouble(),0.0);
            }
            QCOMPARE(mouths,1);
            QCOMPARE(AvatarAssets::resolveRecipe(r["recipe"].toObject(),cachePath)["sha256"],r["sha256"]);
            if(!destination.isEmpty()){QFile descriptor(QFileInfo(destination).absolutePath()+"/"+eyes+".json");QVERIFY(descriptor.open(QIODevice::WriteOnly));descriptor.write(QJsonDocument(r).toJson());}
            ++index;
        }
        QCOMPARE(index,14);
    }
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
