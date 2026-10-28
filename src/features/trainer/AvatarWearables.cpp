#include "AvatarWearables.h"
#include <QFile>
#include <QColor>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QMap>
#include <QtEndian>
#include <cmath>
#include <algorithm>

namespace trainer {
namespace {
QByteArray hashedFile(const QString& root,const QJsonObject& entry) {
    const auto name=entry["file"].toString();
    if(name.isEmpty()||name.contains('/')||name.contains('\\')||name.contains(".."))return {};
    QFile file(root+"/"+name);
    if(!file.open(QIODevice::ReadOnly)||file.size()>16*1024*1024)return {};
    const auto bytes=file.readAll();
    return QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()==entry["sha256"].toString().toLatin1()?bytes:QByteArray();
}
QByteArray accessorBytes(const QJsonObject& g,const QByteArray& binary,int index) {
    const auto accessors=g["accessors"].toArray(),views=g["bufferViews"].toArray();
    if(index<0||index>=accessors.size())return {};
    const auto a=accessors[index].toObject();const int vi=a["bufferView"].toInt(-1);
    if(vi<0||vi>=views.size()||a.contains("sparse")||a["componentType"]!=5126)return {};
    const auto v=views[vi].toObject();
    const int components=a["type"]=="VEC3"?3:a["type"]=="MAT4"?16:0;
    const qint64 offset=qint64(v["byteOffset"].toInt())+a["byteOffset"].toInt();
    const qint64 size=qint64(a["count"].toInt())*components*4;
    if(!components||v.contains("byteStride")||size<=0||offset<0||offset+size>binary.size())return {};
    return binary.mid(offset,size);
}
float linear(float c){return c<=.04045f?c/12.92f:std::pow((c+.055f)/1.055f,2.4f);}
}
bool applyAvatarWearables(QJsonObject& g,QByteArray& binary,const QJsonObject& m,const QJsonObject& a,const QString& root) {
    const auto catalogue=m["wearables"].toObject();
    auto nodes=g["nodes"].toArray();
    QList<QPair<QString,QString>> garments;
    const auto top=catalogue[a["top"].toString()].toObject();
    for(const auto& slot:{"top","bottom","socks","shoes","headwear"}) {
        if(top["covers"].toArray().contains(QString(slot)))continue;
        garments.append({slot,a[slot].toString()});
    }
    for(const auto& companion:top["companions"].toArray()) {
        const auto id=companion.toString();garments.append({catalogue[id].toObject()["slot"].toString(),id});
    }
    for(const auto& garment:garments) {
        const auto slot=garment.first,id=garment.second;
        if(id=="none"||id=="tank"||id=="base-shorts"||id=="sandals")continue;
        const auto entry=catalogue[id].toObject();
        if(entry["slot"]!=slot)return false;
        const auto data=hashedFile(root,entry);
        if(data.size()<28||qFromLittleEndian<quint32>(data.constData())!=0x46546c67)return false;
        const auto jsonSize=qFromLittleEndian<quint32>(data.constData()+12);
        if(jsonSize>quint32(data.size()-28))return false;
        const auto w=QJsonDocument::fromJson(data.mid(20,jsonSize)).object();
        const auto wb=data.mid(28+jsonSize);const auto wn=w["nodes"].toArray();
        const auto skins=w["skins"].toArray();if(skins.size()!=1)return false;
        const auto skin=skins[0].toObject(),base=g["skins"].toArray()[0].toObject();
        const auto joints=skin["joints"].toArray(),baseJoints=base["joints"].toArray();
        if(joints.size()!=baseJoints.size()||joints.size()!=54)return false;
        for(int i=0;i<joints.size();++i)
            if(wn[joints[i].toInt()].toObject()["name"]!=nodes[baseJoints[i].toInt()].toObject()["name"])return false;
        const auto binds=accessorBytes(w,wb,skin["inverseBindMatrices"].toInt(-1));
        if(binds.isEmpty()||binds!=accessorBytes(g,binary,base["inverseBindMatrices"].toInt(-1)))return false;
        if(!w["images"].toArray().isEmpty()||!w["textures"].toArray().isEmpty())return false;
        while(binary.size()%4)binary.append('\0');
        const auto byteOffset=binary.size();
        auto views=g["bufferViews"].toArray(),accessors=g["accessors"].toArray(),materials=g["materials"].toArray(),meshes=g["meshes"].toArray();
        const int viewBase=views.size(),accessorBase=accessors.size(),materialBase=materials.size(),meshBase=meshes.size();
        for(const auto& value:w["bufferViews"].toArray()){
            auto v=value.toObject();v["buffer"]=0;v["byteOffset"]=v["byteOffset"].toInt()+byteOffset;views.append(v);
        }
        for(const auto& value:w["accessors"].toArray()){
            auto acc=value.toObject();if(acc.contains("bufferView"))acc["bufferView"]=acc["bufferView"].toInt()+viewBase;
            if(acc.contains("sparse")){
                auto sparse=acc["sparse"].toObject();
                for(const auto& field:{"indices","values"}){auto v=sparse[field].toObject();v["bufferView"]=v["bufferView"].toInt()+viewBase;sparse[field]=v;}
                acc["sparse"]=sparse;
            }accessors.append(acc);
        }
        for(const auto& value:w["materials"].toArray()){
            auto mat=value.toObject();
            const QString role=entry["tintRole"].toString(slot=="socks"?"socksColor":slot=="shoes"?"shoesColor":slot=="headwear"?"headwearColor":QString());
            const QString material=entry["tintMaterial"].toString(slot=="socks"||slot=="shoes"?"ivory":slot=="headwear"?"forest":QString());
            if(mat["name"]==material&&!role.isEmpty()){
                QColor c(a[role].toString());auto pbr=mat["pbrMetallicRoughness"].toObject();
                pbr["baseColorFactor"]=QJsonArray{linear(c.redF()),linear(c.greenF()),linear(c.blueF()),1};mat["pbrMetallicRoughness"]=pbr;
            }materials.append(mat);
        }
        for(const auto& value:w["meshes"].toArray()){
            auto mesh=value.toObject();auto primitives=mesh["primitives"].toArray();
            for(int i=0;i<primitives.size();++i){
                auto p=primitives[i].toObject(),attrs=p["attributes"].toObject();
                for(auto it=attrs.begin();it!=attrs.end();++it)it.value()=it.value().toInt()+accessorBase;
                p["attributes"]=attrs;
                if(p.contains("indices"))p["indices"]=p["indices"].toInt()+accessorBase;
                if(p.contains("material"))p["material"]=p["material"].toInt()+materialBase;
                auto targets=p["targets"].toArray();for(int t=0;t<targets.size();++t){auto target=targets[t].toObject();for(auto it=target.begin();it!=target.end();++it)it.value()=it.value().toInt()+accessorBase;targets[t]=target;}
                if(!targets.isEmpty())p["targets"]=targets;
                primitives[i]=p;
            }mesh["primitives"]=primitives;meshes.append(mesh);
        }
        int sourceNode=-1;QJsonObject node;
        for(int i=0;i<wn.size();++i)if(wn[i].toObject().contains("mesh")){if(sourceNode!=-1)return false;sourceNode=i;node=wn[i].toObject();}
        if(sourceNode<0||node["name"]!=id||node["skin"]!=0)return false;
        node["mesh"]=node["mesh"].toInt()+meshBase;node["skin"]=0;
        const int nodeIndex=nodes.size();nodes.append(node);
        auto animations=g["animations"].toArray();
        for(const auto& value:w["animations"].toArray()){
            const auto animation=value.toObject();int match=-1;
            for(int i=0;i<animations.size();++i)if(animations[i].toObject()["name"]==animation["name"])match=i;
            if(match<0)return false;
            auto dest=animations[match].toObject();auto samplers=dest["samplers"].toArray(),channels=dest["channels"].toArray();
            for(const auto& cv:animation["channels"].toArray()){
                auto channel=cv.toObject(),target=channel["target"].toObject();if(target["path"]!="weights")continue;
                if(target["node"]!=sourceNode)return false;
                auto sampler=animation["samplers"].toArray()[channel["sampler"].toInt()].toObject();
                sampler["input"]=sampler["input"].toInt()+accessorBase;sampler["output"]=sampler["output"].toInt()+accessorBase;
                channel["sampler"]=samplers.size();samplers.append(sampler);target["node"]=nodeIndex;channel["target"]=target;channels.append(channel);
            }dest["samplers"]=samplers;dest["channels"]=channels;animations[match]=dest;
        }
        // Base underwear stays under bottoms. Hide only the replaced separate garment.
        const QString replaced=QString(slot)=="top"?"shirt":QString(slot)=="shoes"?"shoes":QString();
        if(!replaced.isEmpty())for(int i=0;i<nodes.size();++i){auto n=nodes[i].toObject();if(n["name"]==replaced){n.remove("mesh");n.remove("skin");nodes[i]=n;}}
        auto scenes=g["scenes"].toArray();const int si=g["scene"].toInt();auto scene=scenes[si].toObject();auto roots=scene["nodes"].toArray();roots.append(nodeIndex);scene["nodes"]=roots;scenes[si]=scene;
        binary+=wb;g["bufferViews"]=views;g["accessors"]=accessors;g["materials"]=materials;g["meshes"]=meshes;g["animations"]=animations;g["scenes"]=scenes;
    }
    const bool hidesHair=catalogue[a["headwear"].toString()].toObject()["hidesHair"].toBool();
    if(hidesHair)for(const auto& index:m["hairNodes"].toObject()) {
        auto node=nodes[index.toInt()].toObject();node.remove("mesh");node.remove("skin");nodes[index.toInt()]=node;
    }
    g["nodes"]=nodes;
    if(a["headwear"]!="none"&&!hidesHair){
        const auto patches=m["headwearFits"].toObject()[a["headwear"].toString()+"/"+a["hair"].toString()].toArray();
        if(patches.isEmpty())return false;
        auto accessors=g["accessors"].toArray();const auto views=g["bufferViews"].toArray();
        for(const auto& value:patches){
            const auto patch=value.toObject();const int index=patch["accessor"].toInt(-1);
            const auto original=accessorBytes(g,binary,index),replacement=hashedFile(root,patch);
            if(original.isEmpty()||replacement.size()!=original.size()||QCryptographicHash::hash(original,QCryptographicHash::Sha256).toHex()!=patch["sourceSha256"].toString().toLatin1())return false;
            auto acc=accessors[index].toObject();const auto view=views[acc["bufferView"].toInt()].toObject();
            binary.replace(view["byteOffset"].toInt()+acc["byteOffset"].toInt(),replacement.size(),replacement);
            if(acc.contains("min")){acc["min"]=patch["min"];acc["max"]=patch["max"];}accessors[index]=acc;
        }g["accessors"]=accessors;
    }
    return true;
}
bool compactAvatar(QJsonObject& g,QByteArray& binary) {
    // Keep node identities and all clips/face controls. Drop invisible alternatives
    // and superseded image buffers so a full outfit fits the existing 64 MiB bound.
    auto nodes=g["nodes"].toArray();const auto sourceMeshes=g["meshes"].toArray();
    QJsonArray meshes;QMap<int,int> meshMap;QSet<int> accessorSet;
    for(int i=0;i<nodes.size();++i){auto node=nodes[i].toObject();if(!node.contains("mesh"))continue;
        const int index=node["mesh"].toInt(-1);if(index<0||index>=sourceMeshes.size())return false;
        if(!meshMap.contains(index)){meshMap[index]=meshes.size();meshes.append(sourceMeshes[index]);}
        node["mesh"]=meshMap[index];nodes[i]=node;
    }
    for(const auto& mv:meshes)for(const auto& pv:mv.toObject()["primitives"].toArray()){
        const auto p=pv.toObject();for(const auto& v:p["attributes"].toObject())accessorSet.insert(v.toInt());
        if(p.contains("indices"))accessorSet.insert(p["indices"].toInt());
        for(const auto& target:p["targets"].toArray())for(const auto& v:target.toObject())accessorSet.insert(v.toInt());
    }
    auto skins=g["skins"].toArray(),animations=g["animations"].toArray();
    for(const auto& v:skins)if(v.toObject().contains("inverseBindMatrices"))accessorSet.insert(v.toObject()["inverseBindMatrices"].toInt());
    for(const auto& v:animations)for(const auto& s:v.toObject()["samplers"].toArray()){
        accessorSet.insert(s.toObject()["input"].toInt());accessorSet.insert(s.toObject()["output"].toInt());
    }
    QMap<int,int> accessorMap;QJsonArray accessors;auto ordered=accessorSet.values();std::sort(ordered.begin(),ordered.end());
    const auto sourceAccessors=g["accessors"].toArray();QSet<int> viewSet;
    for(int index:ordered){if(index<0||index>=sourceAccessors.size())return false;
        auto a=sourceAccessors[index].toObject();accessorMap[index]=accessors.size();accessors.append(a);
        if(a.contains("bufferView"))viewSet.insert(a["bufferView"].toInt());
        if(a.contains("sparse"))for(const auto& key:{"indices","values"})viewSet.insert(a["sparse"].toObject()[key].toObject()["bufferView"].toInt());
    }
    auto images=g["images"].toArray();for(const auto& v:images)if(v.toObject().contains("bufferView"))viewSet.insert(v.toObject()["bufferView"].toInt());
    QMap<int,int> viewMap;QJsonArray views;QByteArray compact;auto sortedViews=viewSet.values();std::sort(sortedViews.begin(),sortedViews.end());
    const auto sourceViews=g["bufferViews"].toArray();
    for(int index:sortedViews){if(index<0||index>=sourceViews.size())return false;
        auto v=sourceViews[index].toObject();const qint64 start=v["byteOffset"].toInt(),length=v["byteLength"].toInt();
        if(start<0||length<=0||start+length>binary.size())return false;
        while(compact.size()%4)compact.append('\0');
        v["byteOffset"]=compact.size();v["buffer"]=0;viewMap[index]=views.size();views.append(v);compact+=binary.mid(start,length);
    }
    for(int i=0;i<accessors.size();++i){auto a=accessors[i].toObject();
        if(a.contains("bufferView"))a["bufferView"]=viewMap[a["bufferView"].toInt()];
        if(a.contains("sparse")){auto s=a["sparse"].toObject();for(const auto& key:{"indices","values"}){auto v=s[key].toObject();v["bufferView"]=viewMap[v["bufferView"].toInt()];s[key]=v;}a["sparse"]=s;}
        accessors[i]=a;
    }
    for(int i=0;i<images.size();++i){auto im=images[i].toObject();if(im.contains("bufferView"))im["bufferView"]=viewMap[im["bufferView"].toInt()];images[i]=im;}
    for(int i=0;i<meshes.size();++i){auto mesh=meshes[i].toObject();auto ps=mesh["primitives"].toArray();
        for(int j=0;j<ps.size();++j){auto p=ps[j].toObject(),attributes=p["attributes"].toObject();
            for(auto it=attributes.begin();it!=attributes.end();++it)it.value()=accessorMap[it.value().toInt()];
            p["attributes"]=attributes;
            if(p.contains("indices"))p["indices"]=accessorMap[p["indices"].toInt()];
            auto targets=p["targets"].toArray();for(int k=0;k<targets.size();++k){auto t=targets[k].toObject();for(auto it=t.begin();it!=t.end();++it)it.value()=accessorMap[it.value().toInt()];targets[k]=t;}
            if(p.contains("targets"))p["targets"]=targets;
            ps[j]=p;
        }mesh["primitives"]=ps;meshes[i]=mesh;
    }
    for(int i=0;i<skins.size();++i){auto skin=skins[i].toObject();if(skin.contains("inverseBindMatrices"))skin["inverseBindMatrices"]=accessorMap[skin["inverseBindMatrices"].toInt()];skins[i]=skin;}
    for(int i=0;i<animations.size();++i){auto animation=animations[i].toObject();auto samplers=animation["samplers"].toArray();
        for(int j=0;j<samplers.size();++j){auto s=samplers[j].toObject();s["input"]=accessorMap[s["input"].toInt()];s["output"]=accessorMap[s["output"].toInt()];samplers[j]=s;}
        animation["samplers"]=samplers;animations[i]=animation;
    }
    auto extras=g["extras"].toObject();extras.remove("uncoveredBodyIndices");g["extras"]=extras;
    g["nodes"]=nodes;g["meshes"]=meshes;g["accessors"]=accessors;g["bufferViews"]=views;g["images"]=images;g["skins"]=skins;g["animations"]=animations;binary=compact;
    return true;
}
}
