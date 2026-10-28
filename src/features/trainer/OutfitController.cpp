#include "OutfitController.h"
#include "TrainerController.h"
#include "AvatarAssets.h"
#include "features/accounts/BackendAccount.h"
#include <QtConcurrent>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QTimer>
#include <QQuaternion>
#include <QMatrix4x4>
#include <QQmlListReference>
#include <QJsonDocument>
#include <QJsonArray>
#include <QtEndian>
#include <cstring>
namespace trainer {
namespace {
const QStringList fields{"headwear","top","bottom","socks","shoes","hair","eyes","skinColor","top"};
const QStringList labels{"Headwear","Tops","Bottoms","Socks","Shoes","Hair","Eyes","Skin","Outfits"};
QString title(const QString& id) {
    static const QMap<QString,QString> names{{"tank","Sports tank"},{"base-shorts","Basic shorts"},{"none","None"},{"sandals","Sandals"},
        {"polo-short-sleeve","Polo"},{"polo-sleeveless","Sleeveless polo"},{"golf-shorts","Golf shorts"},{"pleated-skirt","Pleated skirt"},
        {"ankle-socks","Ankle socks"},{"crew-socks","Long socks"},{"golf-shoes-classic","Classic shoes"},{"golf-shoes-sport","Sport shoes"},
        {"golf-cap","Cap"},{"golf-visor","Visor"},{"swept","Side swept"},{"ponytail","Ponytail"},{"crop","Short crop"},{"bob","Bob"},
        {"sundress","Pleated sundress"},{"everyday-tee","Everyday T-shirt"},{"denim-breeches","Denim breeches"},{"canvas-shoes","Canvas shoes"},
        {"racing-suit","Racing suit"},{"rose-racing-suit","Rose racing suit"},{"racing-helmet","Racing helmet"},{"rose-racing-helmet","Rose helmet"},
        {"racing-boots","Racing boots"},{"rose-racing-boots","Rose racing boots"}};
    if(names.contains(id))return names[id];
    auto text=id;text.replace("-feminine"," - lashes");text.replace("-base","");if(!text.isEmpty())text[0]=text[0].toUpper();return text;
}
}
OutfitController::OutfitController(BackendAccount& account,TrainerController& trainer,QObject* parent)
    :QObject(parent),account_(account),trainer_(trainer) {
    connect(&account_,&BackendAccount::changed,this,[this]{
        if(!open_)return;const auto player=account_.snapshot()["player_id"].toString();
        if(!player_.isEmpty()&&!player.isEmpty()&&player_!=player){close();emit backRequested();return;}
        index_=qBound(0,index_,int(rows().size())-1);emit changed();
    });
    connect(&trainer_,&TrainerController::changed,this,[this]{
        if(open_&&trainer_.profile()["id"].toString()!=owner_){close();emit backRequested();return;}
        if(open_||livePending_){
            if(!trainer_.saving()&&!trainer_.error().isEmpty()){status_=trainer_.error();liveFailed_=true;}
            QTimer::singleShot(0,this,[this]{saveLive();});emit changed();
        }
    });
    connect(&job_,&QFutureWatcher<QJsonObject>::finished,this,[this]{
        if(jobGeneration_!=generation_){if(open_)startPreview();return;}
        const auto result=job_.result();previewError_=result["error"].toString();
        if(previewError_.isEmpty())previewUrl_=QUrl::fromLocalFile(result["preview"].toString()).toString();
        const auto files=QDir(cache_.path()).entryInfoList({"*.glb"},QDir::Files,QDir::Time);
        for(int i=4;i<files.size();++i)QFile::remove(files[i].absoluteFilePath());
        emit changed();
    });
}
OutfitController::~OutfitController(){job_.waitForFinished();}
int OutfitController::posePreview(QObject* loader,qreal amount){
    if(!loader)return 0;
    const QMap<QString,QVector3D> poses{{"head",{12,12,0}},{"neck",{3,0,0}},
        {"upper_arm.L",{0,0,-12}},{"upper_arm.R",{0,0,12}},
        {"forearm.L",{0,-16,0}},{"forearm.R",{0,16,0}}};
    const auto source=loader->property("source").toUrl();
    // RuntimeLoader does not retain glTF node names and Assimp may reduce each
    // mesh's joint list. Match bind matrices, never guessed QObject/list indices.
    if(loader->property("wardrobeBoundSource").toUrl()!=source) {
        QFile file(source.toLocalFile());
        if(!source.isLocalFile()||!file.open(QIODevice::ReadOnly))return 0;
        const auto header=file.read(20);
        if(header.size()!=20||qFromLittleEndian<quint32>(header.constData())!=0x46546c67)return 0;
        const auto jsonSize=qFromLittleEndian<quint32>(header.constData()+12);
        if(jsonSize>8*1024*1024)return 0;
        const auto gltf=QJsonDocument::fromJson(file.read(jsonSize)).object();
        const auto nodes=gltf["nodes"].toArray(),accessors=gltf["accessors"].toArray(),views=gltf["bufferViews"].toArray();
        QList<QPair<QMatrix4x4,QVector3D>> bindings;
        for(const auto entry:gltf["skins"].toArray()) {
            const auto skin=entry.toObject();const auto joints=skin["joints"].toArray();
            const auto accessor=accessors.at(skin["inverseBindMatrices"].toInt()).toObject();
            const auto view=views.at(accessor["bufferView"].toInt()).toObject();
            if(accessor["componentType"]!=5126||accessor["type"]!="MAT4")continue;
            for(int i=0;i<joints.size();++i) {
                const auto name=nodes.at(joints[i].toInt()).toObject()["name"].toString();
                if(!poses.contains(name))continue;
                const qint64 offset=28+qint64(jsonSize)+view["byteOffset"].toInt()+accessor["byteOffset"].toInt()+qint64(i)*view.value("byteStride").toInt(64);
                if(!file.seek(offset))continue;const auto bytes=file.read(64);if(bytes.size()!=64)continue;
                QMatrix4x4 matrix;
                for(int j=0;j<16;++j){const auto bits=qFromLittleEndian<quint32>(bytes.constData()+j*4);float value;std::memcpy(&value,&bits,4);matrix.data()[j]=value;}
                bindings.append({matrix,poses[name]});
            }
        }
        bool bound=false;
        for(auto* skin:loader->findChildren<QObject*>()) {
            if(!skin->property("inverseBindPoses").isValid())continue;
            const auto matrices=skin->property("inverseBindPoses").value<QList<QMatrix4x4>>();
            QQmlListReference joints(skin,"joints");
            for(int i=0;i<qMin(matrices.size(),joints.count());++i)for(const auto& binding:bindings) {
                bool same=true;for(int j=0;j<16;++j)same&=qAbs(matrices[i].constData()[j]-binding.first.constData()[j])<0.0002f;
                auto* node=joints.at(i);if(!same||!node)continue;
                node->setProperty("wardrobePose",QVariant::fromValue(binding.second));bound=true;break;
            }
        }
        if(bound)loader->setProperty("wardrobeBoundSource",source);
    }
    int count=0;
    for(auto* node:loader->findChildren<QObject*>()) {
        if(!node->property("wardrobePose").isValid()||!node->property("rotation").isValid())continue;
        if(!node->property("wardrobeNeutral").isValid())node->setProperty("wardrobeNeutral",node->property("rotation"));
        const auto neutral=node->property("wardrobeNeutral").value<QQuaternion>();
        node->setProperty("rotation",QVariant::fromValue(neutral*QQuaternion::fromEulerAngles(node->property("wardrobePose").value<QVector3D>()*float(amount))));
        ++count;
    }
    return count;
}
QVariantList OutfitController::rows() const {
    QVariantList result{QVariantMap{{"id","current"},{"name",account_.wardrobeState().value("freeName").toString("Everyday")},{"current",true}}};
    for(const auto item:account_.outfits()["items"].toList()) {
        auto row=item.toMap();const auto saved=row["saved"].toMap();row["name"]=saved.isEmpty()?"Set "+row["slot_number"].toString():saved["name"];result.append(row);
    }
    const auto next=account_.outfits()["next"].toMap();if(!next.isEmpty()){auto row=next;row["name"]="+";row["buy"]=true;result.append(row);}return result;
}
QVariantMap OutfitController::selected() const {return rows().value(index_).toMap();}
QVariantList OutfitController::items() const {
    QVariantList result;for(const auto& id:AvatarAssets::choices(fields[category_])) {
        if(id=="none")continue;
        if(fields[category_]=="top"&&(AvatarAssets::onePiece(id)!=(category_==8)))continue;
        const bool covered=category_==2&&AvatarAssets::onePiece(appearance_["top"].toString());
        result.append(QVariantMap{{"id",id},{"name",title(id)},{"equipped",!covered&&appearance_[fields[category_]]==id},{"thumbnail",QString("qrc:/wardrobe/%1.png").arg(id)}});
    }return result;
}
QString OutfitController::colourKey() const {return QStringList{"headwearColor","shirtColor","shortsColor","socksColor","shoesColor","hairColor","eyeColor","skinColor","shirtColor"}[category_];}
QStringList OutfitController::colours() const {
    auto values=AvatarAssets::choices(colourKey());const auto current=appearance_[colourKey()].toString();
    if(!current.isEmpty()&&!values.contains(current))values.append(current);return values;
}
QJsonObject OutfitController::recipe() const {
    QFile manifest(AvatarAssets::packRoot()+"/manifest.json");if(!manifest.open(QIODevice::ReadOnly))return {};
    return {{"schema",1},{"appearance",appearance_},{"packRevision",QString::fromLatin1(QCryptographicHash::hash(manifest.readAll(),QCryptographicHash::Sha256).toHex())}};
}
QVariantMap OutfitController::view() const {
    QVariantList categories;for(int i=0;i<fields.size();++i)categories.append(QVariantMap{{"id",i==8?"outfit":fields[i]},{"name",labels[i]}});
    const auto row=selected(),chosen=items().value(item_).toMap();const auto key=fields[category_];
    const bool full=AvatarAssets::onePiece(appearance_["top"].toString());
    const bool visible=!(category_==2&&full)&&!(category_==1&&full)&&!(category_==8&&!full);
    const bool canRemove=visible&&(category_<5||category_==8)&&appearance_[key]!=AvatarAssets::defaults()[key];
    const bool hasColour=visible&&appearance_[key]!="none";
    return {{"rows",rows()},{"selected",row},{"index",index_},{"focus",focus_},{"command",command_},
        {"categories",categories},{"category",category_},{"items",items()},{"item",item_},{"chosen",chosen},
        {"colours",hasColour?colours():QStringList()},{"colour",appearance_[colourKey()].toString()},
        {"colourIndex",colour_},{"equippedName",visible?title(appearance_[key].toString()):QString("None")},{"canRemove",canRemove},
        {"appearance",appearance_.toVariantMap()},{"preview",previewUrl_},{"previewBusy",job_.isRunning()},
        {"previewError",previewError_},{"busy",false},{"review",account_.outfits()["review"]},
        {"conflict",row["conflict"].toBool()},{"status",!status_.isEmpty()?status_:account_.outfits()["status"].toString()},
        {"wearing",true},{"replace",false}};
}
void OutfitController::open(){
    account_.cancelPurchaseReview();open_=true;owner_=trainer_.profile()["id"].toString();player_=account_.snapshot()["player_id"].toString();
    index_=0;focus_=2;status_.clear();liveFailed_=false;nameSlot_.clear();appearance_=trainer_.appearance();
    const auto state=account_.wardrobeState();free_=AvatarAssets::normalized(state["free"].toObject());if(free_.isEmpty())free_=appearance_;
    const auto all=rows();for(int i=1;i<all.size();++i)if(all[i].toMap()["id"]==state["active"].toString()&&!all[i].toMap()["buy"].toBool()) {
        const auto saved=QJsonObject::fromVariantMap(all[i].toMap()["saved"].toMap()["recipe"].toMap())["appearance"].toObject();
        if(!saved.isEmpty()&&AvatarAssets::normalized(saved)==appearance_)index_=i;
    }
    if(index_==0)free_=appearance_;selectCategory(category_);preview();account_.refreshShop();emit changed();
}
void OutfitController::close(){if(!open_)return;open_=false;++generation_;previewAppearance_={};nameSlot_.clear();account_.cancelPurchaseReview();}
void OutfitController::select(int index){
    if(!open_||!account_.outfits()["review"].toMap().isEmpty())return;
    const auto all=rows();index=qBound(0,index,int(all.size())-1);const auto row=all[index].toMap();
    if(row["buy"].toBool()){account_.reviewOutfitSlot();focus_=0;command_=0;emit changed();return;}
    const auto old=index_;index_=index;
    auto a=index==0?free_:AvatarAssets::normalized(QJsonObject::fromVariantMap(row["saved"].toMap()["recipe"].toMap())["appearance"].toObject());
    if(a.isEmpty())a=appearance_;if(!commit(a)){index_=old;emit changed();return;}
    selectCategory(category_);focus_=0;emit admired();emit changed();
}
bool OutfitController::commit(const QJsonObject& a){
    if(!AvatarAssets::valid(a))return false;
    const auto old=appearance_;appearance_=AvatarAssets::normalized(a);const auto row=selected();
    const auto free=index_==0?appearance_:free_;
    auto state=account_.wardrobeState();state["active"]=row["id"].toString();state["free"]=free;
    const bool stored=index_==0?account_.storeWardrobeState(state):account_.queueOutfit(row["id"].toString(),row["name"].toString(),recipe(),row["saved"].toMap().value("revision","0").toString(),state);
    if(!stored){appearance_=old;status_="Could not save this set. Please try again.";return false;}
    free_=free;status_.clear();liveFailed_=false;livePending_=true;saveLive();preview();emit changed();return true;
}
void OutfitController::saveLive(){
    if(!livePending_||liveFailed_||trainer_.saving())return;
    if(trainer_.profile()["id"].toString()!=owner_){livePending_=false;return;}
    if(trainer_.appearance()==appearance_){livePending_=false;return;}
    if(!trainer_.applyAppearance(appearance_)){status_="Could not update the current look.";liveFailed_=true;emit changed();}
}
void OutfitController::selectCategory(int category){
    if(!open_)return;category_=qBound(0,category,int(fields.size())-1);item_=0;
    const auto list=items();for(int i=0;i<list.size();++i)if(list[i].toMap()["equipped"].toBool())item_=i;
    colour_=qMax(0,colours().indexOf(appearance_[colourKey()].toString()));focus_=1;emit changed();
}
void OutfitController::selectItem(int index){if(!open_)return;item_=qBound(0,index,int(items().size())-1);focus_=2;emit changed();}
void OutfitController::equip(){
    if(!open_)return;const auto chosen=items().value(item_).toMap();if(chosen.isEmpty())return;
    const auto key=fields[category_];auto a=appearance_;
    const auto value=chosen["equipped"].toBool()&&(category_<5||category_==8)?AvatarAssets::defaults()[key].toString():chosen["id"].toString();
    a=AvatarAssets::equipped(a,key,value);if(a==appearance_)return;
    if(commit(a))emit admired();
}
void OutfitController::chooseColour(int index){
    const auto values=view()["colours"].toStringList();if(!open_||index<0||index>=values.size())return;
    auto a=appearance_;a[colourKey()]=values[index];colour_=index;focus_=3;commit(a);
}
void OutfitController::refresh(){status_.clear();liveFailed_=false;saveLive();account_.refreshShop();}
void OutfitController::preview(){if(appearance_==previewAppearance_)return;previewAppearance_=appearance_;previewError_.clear();++generation_;if(!job_.isRunning())startPreview();}
void OutfitController::startPreview(){
    jobGeneration_=generation_;if(!open_||!AvatarAssets::valid(previewAppearance_))return;
    const auto appearance=previewAppearance_;const auto cache=cache_.path();job_.setFuture(QtConcurrent::run([appearance,cache]{return AvatarAssets::resolve(appearance,{},cache);}));
}
void OutfitController::activate(int action){
    if(!open_)return;if(action==0){equip();return;}if(action==1){refresh();return;}
    if(action==2){nameSlot_=selected()["id"].toString();emit nameRequested(selected()["name"].toString());}if(action==3)emit admired();
}
void OutfitController::acceptName(const QString& name){
    if(!open_||nameSlot_.isEmpty())return;
    if(nameSlot_=="current") {
        if(!name.trimmed().isEmpty()){auto state=account_.wardrobeState();state["freeName"]=name.trimmed().left(32);if(!account_.storeWardrobeState(state))status_="Could not save the name.";}
        nameSlot_.clear();emit changed();return;
    }
    for(const auto row:rows()){const auto r=row.toMap();if(r["id"]==nameSlot_){const auto saved=r["saved"].toMap();account_.queueOutfit(nameSlot_,name,QJsonObject::fromVariantMap(saved["recipe"].toMap()),saved["revision"].toString());break;}}
    nameSlot_.clear();emit changed();
}
void OutfitController::confirm(bool yes){if(yes)account_.confirmPurchase();else account_.cancelPurchaseReview();emit changed();}
void OutfitController::conflict(bool keepLocal){account_.resolveOutfitConflict(selected()["id"].toString(),keepLocal);if(!keepLocal)select(index_);}
void OutfitController::dispatch(Action action){
    if(!open_)return;
    if(selected()["conflict"].toBool()){
        if(action==Action::Left)command_=0;else if(action==Action::Right)command_=1;
        else if(action==Action::Confirm)conflict(command_==1);
        else if(action==Action::Back){close();emit backRequested();}emit changed();return;
    }
    if(!account_.outfits()["review"].toMap().isEmpty()){
        if(action==Action::Back)confirm(false);else if(action==Action::Left)command_=0;else if(action==Action::Right)command_=1;
        else if(action==Action::Confirm)confirm(command_==1);emit changed();return;
    }
    if(action==Action::Back){close();emit backRequested();return;}
    if(action==Action::PreviousFace||action==Action::NextFace){selectCategory((category_+(action==Action::NextFace?1:fields.size()-1))%fields.size());return;}
    if(action==Action::ScrollUp||action==Action::ScrollDown){emit rotateRequested(action==Action::ScrollUp?-20:20);return;}
    if(action==Action::ContextMenu){activate(2);return;}
    if(action==Action::Confirm){
        if(focus_==0){select(index_);focus_=2;}else if(focus_==1)focus_=2;else if(focus_==2||focus_==4)equip();
        else if(focus_==3)chooseColour(colour_);else if(focus_==5)activate(command_==0?2:command_==1?1:3);emit changed();return;
    }
    const int delta=action==Action::Left||action==Action::Up?-1:1;
    if(action==Action::Up||action==Action::Down){
        if(focus_==0)focus_=2;else if(focus_==1)selectCategory(category_+delta);
        else if(focus_==2){if(item_+delta*3<0)focus_=0;else if(item_+delta*3>=items().size())focus_=3;else item_+=delta*3;}
        else if(focus_==3)focus_=delta<0?2:4;else if(focus_==4)focus_=delta<0?3:5;else if(focus_==5)focus_=delta<0?4:0;
    }else if(action==Action::Left||action==Action::Right){
        if(focus_==0)select(qBound(0,index_+delta,int(rows().size())-1));else if(focus_==1){if(delta<0)focus_=2;}
        else if(focus_==2){if(delta>0&&item_%3==2)focus_=1;else item_=qBound(0,item_+delta,int(items().size())-1);}
        else if(focus_==3)colour_=qBound(0,colour_+delta,qMax(0,int(view()["colours"].toStringList().size())-1));
        else if(focus_==5)command_=qBound(0,command_+delta,2);else if(focus_==4)focus_=1;
    }emit changed();
}
}
