#include "AvatarAssets.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QColor>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtEndian>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace trainer {
QJsonObject AvatarAssets::defaults() {
    return {{"schema",1},{"pack","trainer-avatar-1"},{"hair","swept"},{"eyes","round"},
        {"hairColor","#62402c"},{"eyeColor","#98602d"},{"skinColor","#f0c4a5"},{"shirtColor","#ec9e8d"},{"shortsColor","#577c87"}};
}
QStringList AvatarAssets::keys(){return {"hair","eyes","hairColor","eyeColor","skinColor","shirtColor","shortsColor"};}
QStringList AvatarAssets::choices(const QString& key) {
    if(key=="hair")return {"swept","ponytail","crop","bob"};
    if(key=="eyes")return {"round","almond","rogue-base","rogue-feminine",
        "innocent-base","innocent-feminine","sly-base","sly-feminine",
        "calm-base","calm-feminine","sleepy-base","sleepy-feminine",
        "determined-base","determined-feminine"};
    if(key=="hairColor")return {"#302a2d","#62402c","#975743","#d1ab68","#e9dcd0","#a35a69","#64888c"};
    if(key=="eyeColor")return {"#98602d","#779486","#547d9a","#9794ae","#bb8b47","#6b6269"};
    if(key=="skinColor")return {"#f7d8bf","#f0c4a5","#d9ab86","#ba8764","#8c5e43","#614132"};
    if(key=="shirtColor")return {"#ec9e8d","#a8c9bc","#aabddd","#d4b5d6","#f0d58d","#f4eee6"};
    if(key=="shortsColor")return {"#577c87","#aebdd5","#8d799f","#b98e7c","#536274","#e0cba9"};
    return {};
}
bool AvatarAssets::valid(const QJsonObject& a) {
    if(a["schema"]!=1 || a["pack"]!="trainer-avatar-1" || a.size()!=9)return false;
    static const QRegularExpression color("^#[0-9a-fA-F]{6}$");
    for(const auto& key:keys()) {
        if(!a[key].isString())return false;
        if(key.endsWith("Color")){if(!color.match(a[key].toString()).hasMatch())return false;}
        else if(!choices(key).contains(a[key].toString()))return false;
    }
    return true;
}
QString AvatarAssets::packRoot(){
    const auto override=qEnvironmentVariable("TRAINEROS_AVATAR_PACK");if(!override.isEmpty())return override;
    QStringList candidates{QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/traineros/avatar/packs/trainer-avatar-1",
        QCoreApplication::applicationDirPath()+"/../share/traineros/avatar/packs/trainer-avatar-1"};
    for(const auto& location:QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))candidates.append(location+"/traineros/avatar/packs/trainer-avatar-1");
#ifdef TRAINER_AVATAR_SOURCE_PACK
    candidates.append(QString::fromUtf8(TRAINER_AVATAR_SOURCE_PACK));
#endif
    for(const auto& path:candidates)if(QFile::exists(path+"/manifest.json"))return QDir::cleanPath(path);
    return candidates.first();
}
bool AvatarAssets::available(){return QFile::exists(packRoot()+"/manifest.json")&&QFile::exists(packRoot()+"/template.glb");}
namespace {
QByteArray read(const QString& p,qint64 max=64*1024*1024){QFile f(p);if(!f.open(QIODevice::ReadOnly)||f.size()>max)return {};return f.readAll();}
bool write(const QString& p,const QByteArray& b){QSaveFile f(p);return f.open(QIODevice::WriteOnly)&&f.write(b)==b.size()&&f.commit();}
void u32(QByteArray& b,quint32 v){char raw[4];qToLittleEndian(v,reinterpret_cast<uchar*>(raw));b.append(raw,4);}
QByteArray glb(QJsonObject g,QByteArray binary){
    while(binary.size()%4)binary.append('\0');
    g["buffers"]=QJsonArray{QJsonObject{{"byteLength",binary.size()}}};
    auto json=QJsonDocument(g).toJson(QJsonDocument::Compact);while(json.size()%4)json.append(' ');
    QByteArray out;u32(out,0x46546c67);u32(out,2);u32(out,28+json.size()+binary.size());u32(out,json.size());u32(out,0x4e4f534a);out+=json;u32(out,binary.size());u32(out,0x004e4942);out+=binary;return out;
}
float linear(float c){return c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);}
int srgb(float c){c=std::clamp(c,0.f,1.f);return qRound(255*(c<=.0031308f?12.92f*c:1.055f*std::pow(c,1.f/2.4f)-.055f));}
}
QJsonObject AvatarAssets::resolve(const QJsonObject& a,const QString& explicitRoot,const QString& explicitCache) {
    auto fail=[](const QString& s){return QJsonObject{{"error",s}};};
    if(!valid(a))return fail("invalid_appearance");
    const auto root=explicitRoot.isEmpty()?packRoot():explicitRoot;
    const auto manifestBytes=read(root+"/manifest.json",65536);
    auto m=QJsonDocument::fromJson(manifestBytes).object();
    if(m["schema"]!=1 || m["pack"]!=a["pack"])return fail("avatar_pack_unavailable");
    auto revision=QString::fromLatin1(QCryptographicHash::hash(manifestBytes+QJsonDocument(a).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
    const auto cache=explicitCache.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/avatars":explicitCache;
    if(!QDir().mkpath(cache))return fail("avatar_cache_unavailable");
    const auto path=QDir(cache).absoluteFilePath(revision+".glb"),preview=QDir(cache).absoluteFilePath(revision+"-preview.glb");
    QByteArray output;
    if(!QFile::exists(path)||!QFile::exists(preview)) {
        const auto data=read(root+"/template.glb");
        if(data.size()<28||qFromLittleEndian<quint32>(data.constData())!=0x46546c67)return fail("invalid_avatar_pack");
        const auto size=qFromLittleEndian<quint32>(data.constData()+12);
        if(size>quint32(data.size()-28))return fail("invalid_avatar_pack");
        auto g=QJsonDocument::fromJson(data.mid(20,size)).object();auto binary=data.mid(28+size);
        if(a["eyes"]=="almond")for(const auto& entry:m["almondPatches"].toArray()) {
            const auto p=entry.toObject();const auto bytes=read(root+"/"+p["file"].toString());
            const int offset=p["offset"].toInt(-1),length=p["length"].toInt(-1);
            if(offset<0||length!=bytes.size()||offset>binary.size()-length)return fail("invalid_avatar_patch");
            binary.replace(offset,length,bytes);
        }
        const auto eyeNodes=m["eyeNodes"].toObject();
        const auto eyeStyle=a["eyes"].toString();
        const bool modularEyes=eyeNodes.contains(eyeStyle);
        if(eyeStyle!="round" && eyeStyle!="almond" && !modularEyes)return fail("avatar_pack_mismatch");
        const auto textureStyle=modularEyes?QStringLiteral("round"):eyeStyle;
        auto paintImage=[&](const QString& file,const QString& maskFile,int imageIndex)->bool {
        QImage face(root+"/"+file),mask(root+"/"+maskFile);
        if(face.isNull()||mask.size()!=face.size()||face.width()>4096||face.height()>4096)return false;
        face=face.convertToFormat(QImage::Format_RGB32);mask=mask.convertToFormat(QImage::Format_RGB32);
        const QColor skin(a["skinColor"].toString()),shorts(a["shortsColor"].toString()),eye(a["eyeColor"].toString());
        const float skinBase[]={.87137f,.55201f,.37626f};float tones[3][3];
        int k=0;for(const auto& c:{skin,shorts,eye}){tones[k][0]=linear(c.redF());tones[k][1]=linear(c.greenF());tones[k++][2]=linear(c.blueF());}
        float lut[256];for(int i=0;i<256;++i)lut[i]=linear(i/255.f);
        for(int y=0;y<face.height();++y){auto* pixels=reinterpret_cast<QRgb*>(face.scanLine(y));const auto* masks=reinterpret_cast<const QRgb*>(mask.constScanLine(y));
            for(int x=0;x<face.width();++x){float c[]={lut[qRed(pixels[x])],lut[qGreen(pixels[x])],lut[qBlue(pixels[x])]};const float weight[]={qRed(masks[x])/255.f,qGreen(masks[x])/255.f,qBlue(masks[x])/255.f};
                const float lum=c[0]*.2126f+c[1]*.7152f+c[2]*.0722f;int result[3];
                for(int ch=0;ch<3;++ch){float v=c[ch]*(1-weight[0])+c[ch]*tones[0][ch]/skinBase[ch]*weight[0];v=v*(1-weight[1])+tones[1][ch]*std::clamp(lum/.20f,.35f,1.30f)*weight[1];v=v*(1-weight[2])+tones[2][ch]*std::clamp(lum/.10f,.18f,1.65f)*weight[2];result[ch]=srgb(v);}
                pixels[x]=qRgb(result[0],result[1],result[2]);
            }
        }
        QByteArray image;QBuffer buffer(&image);buffer.open(QIODevice::WriteOnly);if(!face.save(&buffer,"PNG"))return false;
        while(binary.size()%4)binary.append('\0');
        auto views=g["bufferViews"].toArray();const int view=views.size();views.append(QJsonObject{{"buffer",0},{"byteOffset",binary.size()},{"byteLength",image.size()}});binary+=image;g["bufferViews"]=views;
        auto images=g["images"].toArray();if(imageIndex<0||imageIndex>=images.size())return false;
        auto im=images[imageIndex].toObject();im["bufferView"]=view;images[imageIndex]=im;g["images"]=images;
        return true;
        };
        if(!paintImage("face-"+textureStyle+".png","mask-"+textureStyle+".png",m["bodyImage"].toInt()))return fail("invalid_avatar_texture");
        if(modularEyes)for(const auto& entry:m["extraPaletteImages"].toArray()){
            const auto p=entry.toObject();
            if(!paintImage(p["file"].toString(),p["mask"].toString(),p["image"].toInt(-1)))return fail("invalid_avatar_texture");
        }
        // The prepared eye surface carries the original skin shading in COLOR_0.
        // Recolour it with the same linear-space ratio as the body texture.
        if(modularEyes)for(const auto& entry:m["vertexPalette"].toArray()){
            const auto p=entry.toObject();const auto accessors=g["accessors"].toArray();
            const int index=p["accessor"].toInt(-1);if(index<0||index>=accessors.size())return fail("invalid_avatar_palette");
            const auto acc=accessors[index].toObject();const auto views=g["bufferViews"].toArray();
            const int vi=acc["bufferView"].toInt(-1);if(vi<0||vi>=views.size()||acc["componentType"]!=5126||acc.contains("sparse"))return fail("invalid_avatar_palette");
            const auto v=views[vi].toObject();const int components=acc["type"]=="VEC4"?4:acc["type"]=="VEC3"?3:0;
            const qint64 offset=qint64(v["byteOffset"].toInt())+acc["byteOffset"].toInt();
            const int count=acc["count"].toInt(),stride=v["byteStride"].toInt(components*4);
            const auto base=p["base"].toArray();const QColor colour(a[p["role"].toString()].toString());
            if(!components||count<1||stride<components*4||offset<0||offset+qint64(count-1)*stride+components*4>binary.size()||base.size()!=3)return fail("invalid_avatar_palette");
            const float tones[]={linear(colour.redF()),linear(colour.greenF()),linear(colour.blueF())};
            for(int i=0;i<count;++i)for(int ch=0;ch<3;++ch){
                char* dest=binary.data()+offset+qint64(i)*stride+ch*4;
                const auto bits=qFromLittleEndian<quint32>(dest);float value;std::memcpy(&value,&bits,4);
                const double denominator=base[ch].toDouble();if(denominator<=0)return fail("invalid_avatar_palette");
                value=std::clamp(float(value*tones[ch]/denominator),0.f,1.f);
                quint32 result;std::memcpy(&result,&value,4);qToLittleEndian(result,dest);
            }
        }
        auto materials=g["materials"].toArray();const auto roles=m["materialRoles"].toObject();
        for(auto i=roles.begin();i!=roles.end();++i){const int index=i.key().toInt();auto mat=materials[index].toObject();auto pbr=mat["pbrMetallicRoughness"].toObject();QColor c(a[i.value().toString()].toString());pbr["baseColorFactor"]=QJsonArray{linear(c.redF()),linear(c.greenF()),linear(c.blueF()),1};mat["pbrMetallicRoughness"]=pbr;materials[index]=mat;}g["materials"]=materials;
        auto scenes=g["scenes"].toArray();int scene=g["scene"].toInt();auto sc=scenes[scene].toObject();QJsonArray roots;const auto hairs=m["hairNodes"].toObject();
        auto nodes=g["nodes"].toArray();
        if(!eyeNodes.isEmpty()){
            const auto bodyMeshes=m["eyeBodyMeshes"].toObject();
            const int body=m["bodyNode"].toInt(-1),mesh=bodyMeshes.contains(eyeStyle)
                ?bodyMeshes[eyeStyle].toInt(-1):m[modularEyes?"preparedBodyMesh":"legacyBodyMesh"].toInt(-1);
            if(body<0||body>=nodes.size()||mesh<0||mesh>=g["meshes"].toArray().size())return fail("invalid_avatar_pack");
            auto node=nodes[body].toObject();node["mesh"]=mesh;nodes[body]=node;
            for(auto it=eyeNodes.begin();it!=eyeNodes.end();++it)if(it.key()!=eyeStyle)
                for(const auto& id:it.value().toArray()){
                    const int index=id.toInt(-1);if(index<0||index>=nodes.size())return fail("invalid_avatar_pack");
                    auto hidden=nodes[index].toObject();hidden.remove("mesh");hidden.remove("skin");nodes[index]=hidden;
                }
        }
        for(auto i=hairs.begin();i!=hairs.end();++i)if(i.key()!=a["hair"].toString()){
            auto node=nodes[i.value().toInt()].toObject();node.remove("mesh");node.remove("skin");nodes[i.value().toInt()]=node;
        }
        g["nodes"]=nodes;
        for(const auto& n:sc["nodes"].toArray()){bool keep=true;for(auto i=hairs.begin();i!=hairs.end();++i)if(n==i.value()&&i.key()!=a["hair"].toString())keep=false;if(keep)roots.append(n);}sc["nodes"]=roots;scenes[scene]=sc;g["scenes"]=scenes;
        output=glb(g,binary);if(!write(path,output))return fail("avatar_cache_write_failed");
        // Qt profile preview needs the neutral surface only; full game GLB retains all clips.
        g.remove("animations");auto meshes=g["meshes"].toArray();for(int i=0;i<meshes.size();++i){auto mesh=meshes[i].toObject();mesh.remove("weights");auto ps=mesh["primitives"].toArray();for(int j=0;j<ps.size();++j){auto p=ps[j].toObject();p.remove("targets");ps[j]=p;}mesh["primitives"]=ps;meshes[i]=mesh;}g["meshes"]=meshes;
        if(!write(preview,glb(g,binary)))return fail("avatar_cache_write_failed");
    }
    if(output.isEmpty())output=read(path);
    const QJsonObject recipe{{"schema",1},{"packRevision",QString::fromLatin1(QCryptographicHash::hash(manifestBytes,QCryptographicHash::Sha256).toHex())},{"appearance",a}};
    return {{"schema",1},{"recipe",recipe},{"pack",m["pack"]},{"revision",revision},{"appearance",a},{"glb",path},{"preview",preview},
        {"sha256",QString::fromLatin1(QCryptographicHash::hash(output,QCryptographicHash::Sha256).toHex())},{"rig",m["rig"]},{"units","metres"},{"forward","+Z"},{"eyeHeight",m["eyeHeight"]},{"actions",m["actions"]}};
}
QJsonObject AvatarAssets::resolveRecipe(const QJsonObject& recipe,const QString& cache) {
    if(recipe.size()!=3||recipe["schema"]!=1||!valid(recipe["appearance"].toObject()))
        return {{"error","invalid_avatar_recipe"}};
    const auto manifest=read(packRoot()+"/manifest.json",65536);
    const auto revision=QString::fromLatin1(QCryptographicHash::hash(manifest,QCryptographicHash::Sha256).toHex());
    if(manifest.isEmpty()||recipe["packRevision"]!=revision)return {{"error","avatar_pack_mismatch"}};
    return resolve(recipe["appearance"].toObject(),{},cache);
}
}
