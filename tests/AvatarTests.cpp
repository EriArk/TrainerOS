#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QtEndian>
#include "features/trainer/AvatarAssets.h"
#include "features/trainer/TrainerController.h"
#include "features/trainer/OutfitController.h"
#include "features/accounts/BackendAccount.h"
using namespace trainer;
class AvatarTests:public QObject {
    Q_OBJECT
private slots:
    void facialLinesDarkenWithSkinWithoutChangingOtherMaterialsOrGeometry() {
        if(!AvatarAssets::available())QSKIP("Avatar pack unavailable");
        QTemporaryDir cache;
        QMap<QString,QJsonObject> baseline;
        QMap<QString,double> previous;
        QMap<QString,QByteArray> geometry;
        auto tones=AvatarAssets::choices("skinColor");tones.insert(5,"#76503b");
        for(const auto& tone:tones){
            auto appearance=AvatarAssets::defaults();appearance["skinColor"]=tone;
            appearance["hairColor"]="#e9dcd0";appearance["eyes"]="rogue-feminine";
            const auto result=AvatarAssets::resolve(appearance,{},cache.path());
            QVERIFY2(!result.contains("error"),qPrintable(result["error"].toString()));
            QFile file(result["glb"].toString());QVERIFY(file.open(QIODevice::ReadOnly));
            const auto bytes=file.readAll();const int jsonSize=qFromLittleEndian<quint32>(bytes.constData()+12);
            const auto g=QJsonDocument::fromJson(bytes.mid(20,jsonSize)).object();
            const auto accessors=g["accessors"].toArray(),views=g["bufferViews"].toArray();
            for(const auto& entry:g["materials"].toArray()){
                const auto material=entry.toObject();const auto name=material["name"].toString();
                const bool line=name=="Warm brown eyebrow"||name=="Soft warm eyelid"||name=="Mouth Contour";
                if(!line){if(baseline.contains(name))QCOMPARE(material,baseline[name]);else baseline[name]=material;continue;}
                const auto colour=material["pbrMetallicRoughness"].toObject()["baseColorFactor"].toArray();
                const double light=colour[0].toDouble()+colour[1].toDouble()+colour[2].toDouble();
                QVERIFY(light>0);QCOMPARE(colour[3].toDouble(),1.0);
                if(previous.contains(name))QVERIFY2(light<previous[name],qPrintable(name+" did not darken for "+tone));
                previous[name]=light;
            }
            QCOMPARE(previous.size(),3);
            int meshId=0;
            for(const auto& mesh:g["meshes"].toArray()){
                int primitive=0;
                for(const auto& p:mesh.toObject()["primitives"].toArray()){
                    for(const auto& semantic:{"POSITION","NORMAL"}){
                        const auto acc=accessors[p.toObject()["attributes"].toObject()[semantic].toInt()].toObject();
                        const auto view=views[acc["bufferView"].toInt()].toObject();
                        const auto data=bytes.mid(28+jsonSize+view["byteOffset"].toInt()+acc["byteOffset"].toInt(),acc["count"].toInt()*12);
                        const auto key=QString::number(meshId)+"/"+QString::number(primitive)+"/"+semantic;
                        if(geometry.contains(key))QCOMPARE(data,geometry[key]);else geometry[key]=data;
                    }
                    ++primitive;
                }
                ++meshId;
            }
            QCOMPARE(AvatarAssets::resolve(appearance,{},cache.path())["sha256"],result["sha256"]);
        }
    }
    void starterCollectionsAndReversibleCoverage() {
        if(!AvatarAssets::available())QSKIP("Avatar pack unavailable");
        QTemporaryDir cache;const auto destination=qEnvironmentVariable("TRAINER_AVATAR_TEST_OUTPUT");
        const auto cachePath=destination.isEmpty()?cache.path():QFileInfo(destination).absolutePath()+"/collection";
        auto original=AvatarAssets::defaults();original["hair"]="bob";original["bottom"]="golf-shorts";
        auto dress=AvatarAssets::equipped(original,"top","sundress");
        QCOMPARE(dress["bottom"],original["bottom"]);QCOMPARE(dress["shirtColor"],QJsonValue("#eab2a1"));
        QCOMPARE(AvatarAssets::equipped(dress,"bottom","denim-breeches")["top"],QJsonValue("tank"));
        auto inspect=[&](const QJsonObject& a,const QString& name) {
            const auto r=AvatarAssets::resolve(a,{},cachePath);
            if(r.contains("error"))qWarning()<<r;
            QFile f(r["glb"].toString());if(!f.open(QIODevice::ReadOnly))return QJsonObject();
            auto bytes=f.readAll();if(bytes.size()>=64*1024*1024)return QJsonObject();
            if(!destination.isEmpty()){QFile d(QFileInfo(destination).absolutePath()+"/"+name+".json");if(d.open(QIODevice::WriteOnly))d.write(QJsonDocument(r).toJson());}
            return QJsonDocument::fromJson(bytes.mid(20,qFromLittleEndian<quint32>(bytes.constData()+12))).object();
        };
        auto visible=[](const QJsonObject& g){QStringList names;for(const auto& n:g["nodes"].toArray())if(n.toObject().contains("mesh"))names.append(n.toObject()["name"].toString());return names;};
        auto g=inspect(dress,"sundress");QVERIFY(!g.isEmpty());auto names=visible(g);
        QVERIFY(names.contains("sundress"));QVERIFY(!names.contains("golf-shorts"));QVERIFY(!names.contains("shirt"));
        auto denim=AvatarAssets::equipped(original,"top","everyday-tee");denim=AvatarAssets::equipped(denim,"bottom","denim-breeches");denim=AvatarAssets::equipped(denim,"shoes","canvas-shoes");
        g=inspect(denim,"denim");QVERIFY(!g.isEmpty());names=visible(g);
        for(const auto& name:{"everyday-tee","denim-breeches","canvas-shoes"})QVERIFY(names.contains(name));
        for(const auto& prefix:{QString(),QString("rose-")})for(const auto& hair:AvatarAssets::choices("hair")) {
            auto a=original;a["hair"]=hair;
            for(const auto& item:QList<QPair<QString,QString>>{{"top","suit"},{"shoes","boots"},{"headwear","helmet"}})
                a=AvatarAssets::equipped(a,item.first,prefix+"racing-"+item.second);
            g=inspect(a,prefix+"race-"+hair);QVERIFY(!g.isEmpty());names=visible(g);
            for(const auto& part:{"suit","gloves","boots","helmet"})QVERIFY(names.contains(prefix+"racing-"+part));
            QVERIFY(!names.contains("golf-shorts"));for(const auto& name:names)QVERIFY(!name.startsWith("hair-"));
            QCOMPARE(g["skins"].toArray().size(),1);QCOMPARE(g["animations"].toArray().size(),8);
            const auto bare=AvatarAssets::equipped(a,"headwear","none");QCOMPARE(bare["hair"],a["hair"]);
            g=inspect(bare,prefix+"bare-"+hair);QVERIFY(!g.isEmpty());QVERIFY(visible(g).contains("hair-"+hair));
        }
        // A new offline character has both starter sets without a purchase/level.
        TrainerProfile p;p.id="starter-owner";p.name="New player";p.avatar=AvatarAssets::defaults();
        MockTrainerRepository repo(p);TrainerController trainer(repo);BackendAccount account;
        BackendAccount::Services services;services.read=[](QString,auto done){done(true,QByteArray());};
        services.readCommerce=[](QString){return QByteArray();};services.writeCommerce=[](QString,QByteArray){return true;};
        services.request=[](QByteArray,QString,QJsonObject,QByteArray,auto done){done(0,{});};
        account.setServices(services);account.configure(p.id);
        OutfitController wardrobe(account,trainer);wardrobe.open();wardrobe.selectCategory(8);
        QCOMPARE(wardrobe.view()["items"].toList().size(),3);wardrobe.selectItem(0);wardrobe.equip();
        QCOMPARE(trainer.appearance()["top"],QJsonValue("sundress"));
        wardrobe.selectCategory(2);QVERIFY(wardrobe.view()["colours"].toStringList().isEmpty());
        wardrobe.selectItem(3);wardrobe.equip();QCOMPARE(trainer.appearance()["top"],QJsonValue("tank"));
        QCOMPARE(trainer.appearance()["bottom"],QJsonValue("denim-breeches"));wardrobe.close();
    }
    void shoePalettePreservesLegacyAndAppliesIvory() {
        auto legacy=AvatarAssets::defaults();legacy["schema"]=2;
        for(const auto& key:{"socksColor","shoesColor","headwearColor"})legacy.remove(key);
        QCOMPARE(AvatarAssets::normalized(legacy)["shoesColor"].toString(),QString("#699598"));
        legacy["shoes"]="golf-shoes-classic";
        QCOMPARE(AvatarAssets::normalized(legacy)["shoesColor"].toString(),QString("#f4f0df"));
        if(!AvatarAssets::available())QSKIP("Avatar pack unavailable");
        QTemporaryDir cache;auto appearance=AvatarAssets::defaults();appearance["shoesColor"]="#f4f0df";
        const auto resolved=AvatarAssets::resolve(appearance,{},cache.path());
        QVERIFY2(!resolved.contains("error"),qPrintable(resolved["error"].toString()));
        QFile file(resolved["glb"].toString());QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();
        const auto gltf=QJsonDocument::fromJson(bytes.mid(20,qFromLittleEndian<quint32>(bytes.constData()+12))).object();
        bool found=false;
        for(const auto entry:gltf["materials"].toArray())if(entry.toObject()["name"]=="SandalStrap"){
            const auto rgb=entry.toObject()["pbrMetallicRoughness"].toObject()["baseColorFactor"].toArray();
            QVERIFY(rgb[0].toDouble()>.89&&rgb[1].toDouble()>.86&&rgb[2].toDouble()>.73);found=true;
        }
        QVERIFY(found);
    }
    void wardrobeAutosaveAndOfflineSlots() {
        if(!AvatarAssets::available())QSKIP("Avatar pack unavailable");
        TrainerProfile p;p.id="wardrobe-autosave";p.name="Alice";p.avatar=AvatarAssets::defaults();
        MockTrainerRepository repo(p);TrainerController trainer(repo);
        auto look=AvatarAssets::defaults();look["hair"]="bob";
        QJsonObject saved{{"offer_id","outfit.slot.1"},{"player_id","11111111-1111-4111-8111-111111111111"},{"revision","1"},{"name","Weekend"},
            {"recipe",QJsonObject{{"schema",1},{"packRevision",QString(64,'a')},{"appearance",look}}}};
        QJsonObject slot{{"id","outfit.slot.1"},{"slot_number",1},{"purpose","outfit_slot"},{"kind","permanent_license"},{"revision","1"},{"price",100},{"title","Set"},{"purchase_id","44444444-4444-4444-8444-444444444444"}};
        QByteArray disk=QJsonDocument(QJsonObject{{"snapshot",QJsonObject{{"player_id",saved["player_id"]},{"revision","1"},{"items",QJsonArray{}},{"catalogue",QJsonArray{}},{"outfit_items",QJsonArray{slot}},{"outfit_catalogue",QJsonArray{slot}},{"outfits",QJsonArray{saved}}}}}).toJson();
        BackendAccount account;BackendAccount::Services services;
        services.read=[](QString,auto done){done(true,QByteArray());};services.readCommerce=[&](QString){return disk;};
        bool fail=false;services.writeCommerce=[&](QString,QByteArray data){if(fail)return false;disk=data;return true;};
        services.request=[](QByteArray,QString,QJsonObject,QByteArray,auto done){done(0,{});};
        account.setServices(services);account.configure(p.id);
        OutfitController wardrobe(account,trainer);wardrobe.open();wardrobe.select(1);
        QCOMPARE(trainer.appearance()["hair"],QJsonValue("bob"));
        wardrobe.selectCategory(1);wardrobe.selectItem(1);wardrobe.equip();
        QCOMPARE(trainer.appearance()["top"],QJsonValue("polo-short-sleeve"));
        wardrobe.selectItem(2);wardrobe.chooseColour(1);
        QCOMPARE(trainer.appearance()["top"],QJsonValue("polo-short-sleeve"));
        QCOMPARE(trainer.appearance()["shirtColor"],QJsonValue("#a8c9bc"));
        const auto pending=QJsonDocument::fromJson(disk).object()["outfitWrites"].toObject()["outfit.slot.1"].toObject();
        QCOMPARE(pending["recipe"].toObject()["appearance"].toObject(),trainer.appearance());
        wardrobe.select(0);QCOMPARE(trainer.appearance(),p.avatar);
        wardrobe.select(1);QCOMPARE(trainer.appearance()["shirtColor"],QJsonValue("#a8c9bc"));
        const auto before=trainer.appearance();fail=true;wardrobe.chooseColour(2);QCOMPARE(trainer.appearance(),before);fail=false;
        wardrobe.close();wardrobe.open();QCOMPARE(wardrobe.view()["index"].toInt(),1);
        QCOMPARE(repo.load()->name,p.name);QCOMPARE(repo.load()->id,p.id);
        wardrobe.close();
    }
    void wardrobeCompatibilityAndRig() {
        auto legacy=AvatarAssets::defaults();legacy["schema"]=1;
        for(const auto& key:{"socksColor","shoesColor","headwearColor"})legacy.remove(key);
        for(const auto& key:{"top","bottom","socks","shoes","headwear"})legacy.remove(key);
        QVERIFY(AvatarAssets::valid(legacy));QCOMPARE(AvatarAssets::normalized(legacy),AvatarAssets::defaults());
        auto invalid=legacy;invalid["top"]="tank";QVERIFY(!AvatarAssets::valid(invalid));
        if(!AvatarAssets::available())QSKIP("External avatar pack not installed");
        QTemporaryDir temporary;
        const auto destination=qEnvironmentVariable("TRAINER_AVATAR_TEST_OUTPUT");
        const auto cache=destination.isEmpty()?temporary.path():QFileInfo(destination).absolutePath()+"/wardrobe";
        const auto original=AvatarAssets::resolve(legacy,{},cache);QVERIFY(!original.contains("error"));
        QCOMPARE(AvatarAssets::resolve(AvatarAssets::defaults(),{},cache)["sha256"],original["sha256"]);
        int variant=0;
        for(const auto& hair:AvatarAssets::choices("hair"))for(const auto& hat:{"golf-cap","golf-visor"}) {
            auto a=AvatarAssets::defaults();const bool female=QString(hat)=="golf-visor";
            a["hair"]=hair;a["eyes"]="rogue-feminine";a["headwear"]=hat;
            a["top"]=female?"polo-sleeveless":"polo-short-sleeve";a["bottom"]=female?"pleated-skirt":"golf-shorts";
            a["socks"]=female?"crew-socks":"ankle-socks";a["shoes"]=female?"golf-shoes-sport":"golf-shoes-classic";
            a["shirtColor"]=female?"#234f43":"#f4f0df";a["shortsColor"]=female?"#f4f0df":"#234f43";
            a["skinColor"]=female?"#614132":"#f0c4a5";
            auto r=AvatarAssets::resolve(a,{},cache);QVERIFY2(!r.contains("error"),qPrintable(r["error"].toString()));
            QFile file(r["glb"].toString());QVERIFY(file.open(QIODevice::ReadOnly));const auto bytes=file.readAll();
            QVERIFY(bytes.size()<64*1024*1024);
            const auto g=QJsonDocument::fromJson(bytes.mid(20,qFromLittleEndian<quint32>(bytes.constData()+12))).object();
            const auto nodes=g["nodes"].toArray();const auto meshes=g["meshes"].toArray();QSet<int> wardrobeNodes;
            for(int i=0;i<nodes.size();++i){const auto node=nodes[i].toObject();
                if(node["name"]=="shirt"||node["name"]=="shoes")QVERIFY(!node.contains("mesh"));
                for(const auto& slot:{"top","bottom","socks","shoes","headwear"})if(node["name"]==a[slot]){
                    QVERIFY(node.contains("mesh"));QCOMPARE(node["skin"].toInt(),0);wardrobeNodes.insert(i);
                    const auto mesh=meshes[node["mesh"].toInt()].toObject();QVERIFY(!mesh["primitives"].toArray().isEmpty());
                }
            }
            QCOMPARE(wardrobeNodes.size(),5);QCOMPARE(g["skins"].toArray().size(),1);QCOMPARE(g["animations"].toArray().size(),8);
            for(const auto& av:g["animations"].toArray()){
                QSet<int> animated;for(const auto& cv:av.toObject()["channels"].toArray()){
                    auto target=cv.toObject()["target"].toObject();if(target["path"]=="weights")animated.insert(target["node"].toInt());
                }
                for(int n:wardrobeNodes){auto mesh=meshes[nodes[n].toObject()["mesh"].toInt()].toObject();
                    if(!mesh["weights"].toArray().isEmpty())QVERIFY(animated.contains(n));
                }
            }
            QCOMPARE(AvatarAssets::resolveRecipe(r["recipe"].toObject(),cache)["sha256"],r["sha256"]);
            if(!destination.isEmpty()){QFile descriptor(QFileInfo(destination).absolutePath()+"/wear-"+hair+"-"+hat+".json");QVERIFY(descriptor.open(QIODevice::WriteOnly));descriptor.write(QJsonDocument(r).toJson());}
            ++variant;
        }
        QCOMPARE(variant,8);
        // Unequip regenerates from the original source, never from fitted hair.
        QCOMPARE(AvatarAssets::resolve(legacy,{},cache)["sha256"],original["sha256"]);
        auto bad=AvatarAssets::defaults();bad["headwear"]="../../hat";QVERIFY(!AvatarAssets::valid(bad));
    }
    void savedLookAppliesWholeAppearanceAndPreservesTrainer() {
        if(!AvatarAssets::available())QSKIP("External avatar pack not installed");
        TrainerProfile p;p.id="looks-owner";p.name="Alice";p.createdAt=QDateTime::currentDateTimeUtc();p.avatar=AvatarAssets::defaults();
        MockTrainerRepository repo(p);TrainerController c(repo);
        auto look=AvatarAssets::defaults();look["hair"]="bob";look["shirtColor"]="#a8c9bc";look["eyes"]="almond";look["top"]="polo-sleeveless";look["bottom"]="pleated-skirt";look["headwear"]="golf-visor";
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
        const auto pack=QJsonDocument::fromJson(manifest.readAll()).object();QCOMPARE(pack["revision"].toInt(),10);
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
            const auto expectedMesh=pack["eyeBodyMeshes"].toObject().contains(eyes)
                ?pack["eyeBodyMeshes"].toObject()[eyes]:pack[expected.isEmpty()?"legacyBodyMesh":"preparedBodyMesh"];
            QCOMPARE(g["meshes"].toArray()[body["mesh"].toInt()].toObject()["name"],
                     templateJson["meshes"].toArray()[expectedMesh.toInt()].toObject()["name"]);
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
        c.chooseAvatar(7,"#d1ab68");QCOMPARE(c.avatarRows()[7].toMap()["colour"].toString(),"#d1ab68");
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
