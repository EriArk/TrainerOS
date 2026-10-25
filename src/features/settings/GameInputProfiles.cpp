#include "GameInputProfiles.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

namespace trainer {
namespace {
const QStringList buttons{"south","east","west","north","leftShoulder","rightShoulder","leftTrigger","rightTrigger","leftStick","rightStick","select","start","up","down","left","right"};
QVariantMap row(QString title,QString detail={}) {return {{"title",title},{"detail",detail},{"kind","action"}};}
int targetGroup(const QString& t) {
    return t.startsWith("Ring ")?5:t.startsWith("Mouse")||t.startsWith("Wheel")?1:
        t.size()==1&&t[0].isLetter()?2:t.size()==1&&t[0].isDigit()?3:t.startsWith('F')&&t.size()>1?4:0;
}
QString hash(const QString& s){return QString::fromLatin1(QCryptographicHash::hash(s.toUtf8(),QCryptographicHash::Sha256).toHex());}
}
QStringList GameInputProfiles::targets(bool rings) {
    QStringList result{"None","MouseLeft","MouseRight","MouseMiddle","WheelUp","WheelDown","Space","Enter","Escape","Tab","Backspace","Shift","Ctrl","Alt","Ctrl+S","Ctrl+L","Alt+Enter","Up","Down","Left","Right","Insert","Delete","Home","End","PageUp","PageDown","Minus","Equal","Comma","Period","Slash","Backslash","Semicolon","Apostrophe","BracketLeft","BracketRight","Grave"};
    for(char c='A';c<='Z';++c)result.append(QString(QChar(c)));
    for(int i=0;i<10;++i)result.append(QString::number(i));
    for(int i=1;i<=12;++i)result.append("F"+QString::number(i));
    if(rings)for(int i=1;i<=4;++i)result.append("Ring "+QString::number(i));
    return result;
}
QJsonObject GameInputProfiles::defaults() {
    return {{"version",1},{"mode","native"},{"leftStick","wasd"},{"rightStick","mouse"},{"deadzone",20},{"sensitivity",600},
        {"modifier","none"},{"bindings",QJsonObject{{"south","Space"},{"east","Escape"},{"west","E"},{"north","Tab"},{"rightTrigger","MouseLeft"},{"leftTrigger","MouseRight"}}},
        {"shiftBindings",QJsonObject{}},{"rings",QJsonArray{}}};
}
bool GameInputProfiles::validTarget(const QString& target,bool rings) {
    if(targets(rings).contains(target))return true;
    const auto parts=target.split('+');if(parts.size()<2||parts.size()>4)return false;
    QStringList seen;
    for(int i=0;i<parts.size()-1;++i){if(!QStringList{"Ctrl","Alt","Shift"}.contains(parts[i])||seen.contains(parts[i]))return false;seen<<parts[i];}
    return targets(false).contains(parts.last())&&!parts.last().contains('+')&&!parts.last().startsWith("Mouse")&&!parts.last().startsWith("Wheel")&&parts.last()!="None"&&!seen.contains(parts.last());
}
namespace {
QString actionTarget(const QJsonValue& v,const QString& gesture) {return v.isString()?(gesture=="press"?v.toString():QString("None")):v.toObject()[gesture].toString("None");}
}
bool GameInputProfiles::valid(const QJsonObject& p) {
    if(p["version"].toInt()!=1 || !QStringList{"native","keyboard_mouse"}.contains(p["mode"].toString()))return false;
    if(!QStringList{"wasd","arrows","off"}.contains(p["leftStick"].toString()) || !QStringList{"mouse","scroll","off"}.contains(p["rightStick"].toString()))return false;
    const auto dz=p["deadzone"].toDouble(-1), speed=p["sensitivity"].toDouble(-1);
    if(!std::isfinite(dz)||dz<5||dz>40||!std::isfinite(speed)||speed<100||speed>1600)return false;
    if(!QStringList{"none","leftShoulder","rightShoulder"}.contains(p["modifier"].toString()))return false;
    if(!p["rings"].isArray() || p["rings"].toArray().size()>4)return false;
    for(const auto& r:p["rings"].toArray()) {
        if(!r.isArray()||r.toArray().size()<2||r.toArray().size()>8)return false;
        for(const auto& item:r.toArray())if(!item.isString()||!validTarget(item.toString(),false))return false;
    }
    for(const auto& key:{"bindings","shiftBindings"}) {
        if(!p[key].isObject())return false;
        const auto bindings=p[key].toObject();
        for(auto i=bindings.begin();i!=bindings.end();++i)if(!buttons.contains(i.key()) || (!i.value().isString()&&!i.value().isObject()))return false;
    }
    for(const auto& key:{"bindings","shiftBindings"}) {
        const auto bindings=p[key].toObject();
        for(auto v:bindings) {
            if(v.isString()){if(!validTarget(v.toString()))return false;continue;}
            const auto gestures=v.toObject();if(gestures.isEmpty())return false;
            for(auto it=gestures.begin();it!=gestures.end();++it)
                if(!QStringList{"press","hold","double"}.contains(it.key())||!it.value().isString()||!validTarget(it.value().toString(),false))return false;
        }
    }
    if(p.contains("ringPresentation")) {
        if(!p["ringPresentation"].isArray()||p["ringPresentation"].toArray().size()>4)return false;
        for(auto value:p["ringPresentation"].toArray()) {
            if(!value.isObject())return false;const auto r=value.toObject();
            if((r.contains("name")&&!r["name"].isString())||(r.contains("slots")&&!r["slots"].isArray())||r["name"].toString().size()>32||r["slots"].toArray().size()>8)return false;
            for(auto v:r["slots"].toArray()) {const auto slot=v.toObject();if(!v.isObject()||(slot.contains("label")&&!slot["label"].isString())||(slot.contains("icon")&&!slot["icon"].isString())||slot["label"].toString().size()>32||!QStringList{"","star","bag","sword","heart","bolt","target"}.contains(slot["icon"].toString()))return false;}
        }
    }
    return true;
}
void GameInputProfiles::configure(LibraryRepository* library,const QString& directory,const QString& owner) {
    library_=library;directory_=directory;owner_=owner;begin();
}
void GameInputProfiles::begin(){toolsOpen_=false;preview_=-1;game_.clear();draft_={};choosing_=copying_=false;gesture_="press";textRing_=textSlot_=-1;ring_=slot_=-1;focus_=page_=targetGroup_=0;status_.clear();emit changed();}
QVariantList GameInputProfiles::visibleRows() const {
    const auto all=rows();QVariantList result;
    for(int i=0;i<all.size();++i) {
        bool show=!editing()||copying_;
        if(copying_) {} else if(choosing_) {
            const auto t=targets(ring_<0)[i];
            const int group=trainer::targetGroup(t);
            show=group==targetGroup_;
        } else if(editing()) {
            show=page_==0?(i<6||i>=all.size()-3):page_==1?(i==6||(i>=7&&i<7+buttons.size())||i>=all.size()-2):(i>=7+buttons.size()&&i!=all.size()-3);
        }
        if(show){auto item=all[i].toMap();item["index"]=i;result.append(item);}
    }
    return result;
}
QString GameInputProfiles::selectedButton() const {return !choosing_&&focus_>=7&&focus_<7+buttons.size()?buttons[focus_-7]:binding_;}
void GameInputProfiles::setPage(int value){copying_=false;page_=std::clamp(value,0,2);choosing_=false;const auto list=visibleRows();focus_=list.isEmpty()?0:list.first().toMap()["index"].toInt();emit changed();}
void GameInputProfiles::setTargetGroup(int value){targetGroup_=std::clamp(value,0,ring_<0?5:4);const auto list=visibleRows();focus_=list.isEmpty()?0:list.first().toMap()["index"].toInt();emit changed();}
void GameInputProfiles::selectBinding(const QString& id){const int index=buttons.indexOf(id);if(index>=0){page_=1;activate(7+index);}}
QList<Adventure> GameInputProfiles::games() const {
    QList<Adventure> result;
    if(library_)for(const auto& a:library_->adventures())
        if(!a.collectionOnly && (a.platformId=="pc" || a.adapterId=="crates-native" || a.id.startsWith("crates:")))result.append(a);
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.title.localeAwareCompare(b.title)<0;});
    return result;
}
QString GameInputProfiles::path(const QString& game) const {
    if(owner_.isEmpty()||directory_.isEmpty())return {};
    return directory_+"/input/"+hash(owner_)+"/"+hash(game)+".json";
}
QJsonObject GameInputProfiles::snapshot(const QString& game) const {
    if(game.isEmpty()||path(game).isEmpty())return defaults();
    QFile file(path(game));if(!file.exists())return defaults();
    if(!file.open(QIODevice::ReadOnly)||file.size()>65536)return {};
    const auto data=QJsonDocument::fromJson(file.readAll()).object();
    return valid(data)?data:QJsonObject{};
}
QString GameInputProfiles::title() const {
    if(copying_)return "Copy another game layout";
    if(choosing_)return "Assign · "+(ring_>=0?"Ring "+QString::number(ring_+1)+" · slot "+QString::number(slot_+1):binding_);
    return game_.isEmpty()?"Game input profiles":gameTitle_;
}
QVariantList GameInputProfiles::rows() const {
    QVariantList result;
    if(copying_) {for(const auto& g:games())result.append(row(g.title,g.id==game_?"Current game":g.platformId));return result;}
    if(choosing_) {for(const auto& t:targets(ring_<0))result.append(row(t));return result;}
    if(game_.isEmpty()) {for(const auto& g:games())result.append(row(g.title,g.platformId=="pc"?"PC game":"Native game"));return result;}
    result={row("Input mode",draft_["mode"]=="native"?"Original game controller":"Keyboard & mouse"),
        row("Left stick",draft_["leftStick"].toString()),row("Right stick",draft_["rightStick"].toString()),
        row("Dead zone",QString::number(draft_["deadzone"].toInt())+"%"),
        row("Pointer speed",QString::number(draft_["sensitivity"].toInt())+" px/s"),
        row("Hold modifier",draft_["modifier"].toString()),row("Editing layer",layer_=="bindings"?"Base":"While modifier held")};
    const auto bindings=draft_[layer_].toObject();
    for(const auto& b:buttons)result.append(row(b,actionTarget(bindings[b],gesture_)));
    const auto rings=draft_["rings"].toArray();
    for(int i=0;i<rings.size();++i) {
        const auto sectors=rings[i].toArray();
        for(int j=0;j<sectors.size();++j)result.append(row(QString("Ring %1 · slot %2").arg(i+1).arg(j+1),sectors[j].toString()));
    }
    result.append(row("Add ring",rings.size()<4?"Eight editable sectors · assign a button to open":"Four rings maximum"));
    result.append(row("Remove last ring",rings.isEmpty()?"No rings":QString::number(rings.size())+" rings"));
    result.append(row("Reset draft","Original controller and default assignments"));
    result.append(row("Save profile","Applies at the next game launch"));
    result.append(row("Discard & back","Keep the previously saved profile"));
    return result;
}
bool GameInputProfiles::save() {
    const auto dest=path(game_);
    if(dest.isEmpty()){status_="Choose a Trainer before saving profiles.";return false;}
    if(!valid(draft_)){status_="Invalid input profile.";return false;}
    QDir().mkpath(QFileInfo(dest).absolutePath());QSaveFile file(dest);
    const auto data=QJsonDocument(draft_).toJson();
    if(!file.open(QIODevice::WriteOnly)||file.write(data)!=data.size()||!file.commit()) {status_="Could not save profile. Your previous profile is unchanged.";return false;}
    status_="Saved · applies on the next launch. Home is always reserved.";return true;
}
void GameInputProfiles::activate(int index) {
    if(index<0||index>=rows().size())return;
    focus_=index;
    if(copying_){const auto source=snapshot(games()[index].id);if(source.isEmpty())status_="Could not read that layout.";else {draft_=source;status_="Copied to this draft. Save to keep it.";}copying_=false;focus_=0;emit changed();return;}
    if(choosing_) {
        assignTarget(targets(ring_<0)[index]);return;
    }
    if(game_.isEmpty()) {
        const auto g=games()[index];game_=g.id;gameTitle_=g.title;draft_=snapshot(game_);layer_="bindings";focus_=0;
        if(draft_.isEmpty()){draft_=defaults();status_="Saved profile could not be read. Original controls remain active until you save a replacement.";}
        emit changed();return;
    }
    const auto cycle=[&](const QString& key,const QStringList& values) {draft_[key]=values[(values.indexOf(draft_[key].toString())+1)%values.size()];};
    if(index==0)cycle("mode",{"native","keyboard_mouse"});
    else if(index==1)cycle("leftStick",{"wasd","arrows","off"});
    else if(index==2)cycle("rightStick",{"mouse","scroll","off"});
    else if(index==3)draft_["deadzone"]=draft_["deadzone"].toInt()>=40?5:draft_["deadzone"].toInt()+5;
    else if(index==4)draft_["sensitivity"]=draft_["sensitivity"].toInt()>=1600?100:draft_["sensitivity"].toInt()+100;
    else if(index==5)cycle("modifier",{"none","leftShoulder","rightShoulder"});
    else if(index==6)layer_=layer_=="bindings"?"shiftBindings":"bindings";
    else if(index<7+buttons.size()) {binding_=buttons[index-7];ring_=-1;choosing_=true;returnFocus_=index;focus_=std::max(0,int(targets().indexOf(actionTarget(draft_[layer_].toObject()[binding_],gesture_))));}
    else {
        int relative=index-7-buttons.size();auto rings=draft_["rings"].toArray();
        for(int i=0;i<rings.size();++i) {
            const auto sectors=rings[i].toArray();
            if(relative<sectors.size()){ring_=i;slot_=relative;choosing_=true;returnFocus_=index;focus_=std::max(0,int(targets(false).indexOf(sectors[relative].toString())));targetGroup_=trainer::targetGroup(sectors[relative].toString());emit changed();return;}
            relative-=sectors.size();
        }
        if(relative==0 && rings.size()<4){rings.append(QJsonArray{"1","2","3","4","5","6","7","8"});draft_["rings"]=rings;}
        else if(relative==1 && !rings.isEmpty()){const auto removed="Ring "+QString::number(rings.size());rings.removeLast();draft_["rings"]=rings;auto meta=draft_["ringPresentation"].toArray();if(meta.size()>rings.size()){meta.removeLast();draft_["ringPresentation"]=meta;}
            for(const auto& key:{"bindings","shiftBindings"}){auto bindings=draft_[key].toObject();for(auto it=bindings.begin();it!=bindings.end();++it)if(it.value()==removed)it.value()="None";draft_[key]=bindings;}}
        else if(relative==2)draft_=defaults();
        else if(relative==3)save();
        else if(relative==4){begin();return;}
        focus_=std::min(focus_,int(rows().size())-1);
    }
    if(choosing_) {
        const auto target=targets(ring_<0).value(focus_);
        targetGroup_=trainer::targetGroup(target);
    }
    emit changed();
}
void GameInputProfiles::setGesture(const QString& value) {
    if(!QStringList{"press","hold","double"}.contains(value))return;gesture_=value;emit changed();
}
void GameInputProfiles::assignTarget(const QString& target) {
    if(!choosing_||!validTarget(target,ring_<0&&gesture_=="press"))return;
    if(ring_>=0){auto rings=draft_["rings"].toArray();auto sectors=rings[ring_].toArray();sectors[slot_]=target;rings[ring_]=sectors;draft_["rings"]=rings;}
    else {
        auto bindings=draft_[layer_].toObject();
        if(gesture_=="press"&&!bindings[binding_].isObject())bindings[binding_]=target;
        else {
            auto gestures=bindings[binding_].toObject();
            if(gestures.isEmpty()){const auto press=bindings[binding_].toString("None");gestures["press"]=press.startsWith("Ring ")?"None":press;}
            if(target.startsWith("Ring ")){status_="Rings use a single press binding. Clear hold and double actions first.";emit changed();return;}
            gestures[gesture_]=target;bindings[binding_]=(gestures["hold"].toString("None")=="None"&&gestures["double"].toString("None")=="None")?QJsonValue(gestures["press"].toString("None")):QJsonValue(gestures);
        }
        draft_[layer_]=bindings;
    }
    choosing_=false;focus_=returnFocus_;emit changed();
}
void GameInputProfiles::beginCopy(){if(!editing())return;copying_=true;choosing_=false;focus_=0;emit changed();}
void GameInputProfiles::editRingName(int ring,int slot) {
    const auto rings=draft_["rings"].toArray();if(ring<0||ring>=rings.size()||slot>=rings[ring].toArray().size())return;
    textRing_=ring;textSlot_=slot;const auto meta=draft_["ringPresentation"].toArray();const auto m=ring<meta.size()?meta[ring].toObject():QJsonObject{};
    const auto sectorsMeta=m["slots"].toArray();emit textRequested(slot<0?"Ring name":"Command label",slot<0?m["name"].toString():slot<sectorsMeta.size()?sectorsMeta[slot].toObject()["label"].toString():QString(),32);
}
void GameInputProfiles::applyText(const QString& value) {
    if(textRing_<0||textRing_>=draft_["rings"].toArray().size())return;
    auto meta=draft_["ringPresentation"].toArray();while(meta.size()<=textRing_)meta.append(QJsonObject{});auto m=meta[textRing_].toObject();
    if(textSlot_<0)m["name"]=value.trimmed().left(32);
    else {auto sectorsMeta=m["slots"].toArray();while(sectorsMeta.size()<=textSlot_)sectorsMeta.append(QJsonObject{});auto s=sectorsMeta[textSlot_].toObject();s["label"]=value.trimmed().left(32);sectorsMeta[textSlot_]=s;m["slots"]=sectorsMeta;}
    meta[textRing_]=m;draft_["ringPresentation"]=meta;textRing_=textSlot_=-1;emit changed();
}
void GameInputProfiles::cycleRingIcon(int ring,int slot) {
    const auto rings=draft_["rings"].toArray();if(ring<0||ring>=rings.size()||slot<0||slot>=rings[ring].toArray().size())return;
    auto meta=draft_["ringPresentation"].toArray();while(meta.size()<=ring)meta.append(QJsonObject{});auto m=meta[ring].toObject();auto sectorsMeta=m["slots"].toArray();while(sectorsMeta.size()<=slot)sectorsMeta.append(QJsonObject{});auto s=sectorsMeta[slot].toObject();
    const QStringList icons{"","star","bag","sword","heart","bolt","target"};s["icon"]=icons[(icons.indexOf(s["icon"].toString())+1)%icons.size()];sectorsMeta[slot]=s;m["slots"]=sectorsMeta;meta[ring]=m;draft_["ringPresentation"]=meta;emit changed();
}
void GameInputProfiles::ringPreset(int ring,const QString& preset) {
    auto rings=draft_["rings"].toArray();if(ring<0||ring>=rings.size())return;
    const QMap<QString,QJsonArray> presets{{"Numbers",{"1","2","3","4","5","6","7","8"}},{"Inventory",{"I","M","J","Tab","E","F","Q","Escape"}},{"Quick actions",{"Ctrl+S","Ctrl+L","F5","F9","Tab","Space","Enter","Escape"}}};
    if(!presets.contains(preset))return;rings[ring]=presets[preset];draft_["rings"]=rings;status_="Preset applied to draft. Match actions to your game's controls.";emit changed();
}
void GameInputProfiles::previewRing(int ring) {
    const auto rings=draft_["rings"].toArray();if(ring<0||ring>=rings.size())return;
    preview_=ring;previewSelection_=0;emit changed();
}
QStringList GameInputProfiles::tools() const {
    if(page_==2&&!choosing_)return {"Rename ring","Rename selected command","Change command icon","Preset · Numbers","Preset · Inventory","Preset · Quick actions","Preview ring","Close"};
    return {"Choose by pressing a button","Record a key or chord","Gesture · "+gesture_,"Copy another game layout","Close"};
}
void GameInputProfiles::openTools(){if(!editing())return;toolsOpen_=true;toolFocus_=0;emit changed();}
void GameInputProfiles::activateTool(int index) {
    if(index<0||index>=tools().size())return;toolsOpen_=false;
    if(page_==2&&!choosing_) {
        int relative=std::max(0,focus_-23),ring=0;const auto rings=draft_["rings"].toArray();
        for(;ring<rings.size()-1&&relative>=rings[ring].toArray().size();++ring)relative-=rings[ring].toArray().size();
        if(!rings.isEmpty())relative=std::clamp(relative,0,int(rings[ring].toArray().size())-1);
        if(index==0)editRingName(ring);
        else if(index==1)editRingName(ring,relative);
        else if(index==2)cycleRingIcon(ring,relative);
        else if(index>=3&&index<=5)ringPreset(ring,QStringList{"Numbers","Inventory","Quick actions"}[index-3]);
        else if(index==6)previewRing(ring);
    } else {
        if(index==0)emit captureRequested("button");
        else if(index==1){if(!choosing_)selectBinding(selectedButton().isEmpty()?"south":selectedButton());emit captureRequested("key");}
        else if(index==2){const QStringList gestures{"press","hold","double"};setGesture(gestures[(gestures.indexOf(gesture_)+1)%3]);}
        else if(index==3)beginCopy();
    }
    emit changed();
}
void GameInputProfiles::dispatch(Action action) {
    if(preview_>=0){const int count=draft_["rings"].toArray()[preview_].toArray().size();if(action==Action::Back)preview_=-1;else if(action==Action::Left||action==Action::Up)previewSelection_=(previewSelection_+count-1)%count;else if(action==Action::Right||action==Action::Down)previewSelection_=(previewSelection_+1)%count;emit changed();return;}
    if(toolsOpen_){if(action==Action::Back)toolsOpen_=false;else if(action==Action::Up)toolFocus_=std::max(0,toolFocus_-1);else if(action==Action::Down)toolFocus_=std::min(int(tools().size())-1,toolFocus_+1);else if(action==Action::Confirm){activateTool(toolFocus_);return;}emit changed();return;}
    if(action==Action::LocalAction){openTools();return;}

    if(action==Action::Back) {
        if(copying_){copying_=false;focus_=0;emit changed();}
        else if(choosing_){choosing_=false;focus_=returnFocus_;emit changed();}
        else if(editing()) {focus_=rows().size()-1;status_="Save your changes or choose Discard & back.";emit changed();}
        else emit backRequested();
    } else if(action==Action::Confirm)activate(focus_);
    else if(editing()&&!choosing_&&(action==Action::PreviousFace||action==Action::NextFace))setPage((page_+3+(action==Action::Left||action==Action::PreviousFace?-1:1))%3);
    else if(choosing_&&(action==Action::PreviousFace||action==Action::NextFace))setTargetGroup((targetGroup_+(ring_<0?6:5)+(action==Action::PreviousFace?-1:1))%(ring_<0?6:5));
    else if(action==Action::Up||action==Action::Down||action==Action::Left||action==Action::Right){
        const auto list=visibleRows();if(list.isEmpty())return;
        int index=0;for(int i=0;i<list.size();++i)if(list[i].toMap()["index"].toInt()==focus_)index=i;
        const bool vertical=action==Action::Up||action==Action::Down;
        const int step=vertical&&choosing_?(targetGroup_==2?7:4):vertical&&editing()&&page_==0&&focus_<6?2:1;
        index=std::clamp(index+(action==Action::Up||action==Action::Left?-step:step),0,int(list.size())-1);
        focus_=list[index].toMap()["index"].toInt();emit changed();
    }
}
}
