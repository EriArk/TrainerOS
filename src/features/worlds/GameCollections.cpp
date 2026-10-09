#include "GameCollections.h"
#include "core/model/GamePlayers.h"
#include "core/model/SeriesCatalog.h"
#include "core/repository/CollectionRepository.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>

namespace trainer {
namespace {
QVariantMap row(const QString& label) { return {{"label",label},{"enabled",true}}; }
bool favorite(const QVariantMap& data) { return data.value("favorite").toString()=="true" || data.value("favorite").toString()=="1"; }
}
void GameCollections::configure(const QString& directory,const QString& owner) {
    close();definitions_={};file_.clear();writable_=!owner.isEmpty();error_.clear();
    if(writable_) {
        const auto key=QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex());
        file_=QDir(directory).filePath("collections/"+key+".json");
        QFile input(file_);
        if(input.exists()) {
            if(!input.open(QIODevice::ReadOnly) || input.size()>4*1024*1024) { writable_=false;error_="Couldn't read your collections. Existing data has been kept."; }
            else {
                QJsonParseError parse;const auto doc=QJsonDocument::fromJson(input.readAll(),&parse);
                if(parse.error!=QJsonParseError::NoError || !doc.isObject() || doc["version"].toInt()!=1 || !doc["collections"].isArray()) {
                    writable_=false;error_="Couldn't read your collections. Existing data has been kept.";
                } else definitions_=doc["collections"].toArray();
            }
        }
    }
    refresh();emit definitionsChanged();emit changed();
}
void GameCollections::refresh() {
    played_.clear();for(const auto& s:library_.recentSessions())played_.insert(s.adventureId);
    for(const auto& a:library_.adventures())if(library_.recordedSeconds(a.id).value_or(0)>0)played_.insert(a.id);
    emit changed();
}
bool GameCollections::save(const QJsonArray& next) {
    if(!writable_) {if(error_.isEmpty())error_="Choose a Trainer before editing collections.";emit changed();return false;}
    if(!file_.isEmpty()) {
        QDir().mkpath(QFileInfo(file_).absolutePath());QSaveFile output(file_);
        const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"collections",next}}).toJson();
        if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit()) {
            error_="Couldn't save the collection. Your previous collections are unchanged.";emit changed();return false;
        }
    }
    definitions_=next;error_.clear();emit definitionsChanged();return true;
}
QVariantList GameCollections::automatic() const {
    return {QVariantMap{{"id","auto:recent"},{"name","Recently played"}},
        QVariantMap{{"id","auto:favorites"},{"name","Favorites"}},
        QVariantMap{{"id","auto:unplayed"},{"name","Not played yet"}},
        QVariantMap{{"id","auto:multiplayer"},{"name","Multiplayer games"}}};
}
bool GameCollections::known(const QString& id) const {
    for(const auto& d:seriesDefinitions())if(d.id==id)return true;
    for(const auto& d:automatic())if(d.toMap()["id"]==id)return true;
    for(const auto& d:definitions_)if(d.toObject()["id"]==id)return true;
    return false;
}
bool GameCollections::contains(const QString& collection,const Adventure& game) const {
    const auto data=library_.artwork(game.id);
    if(collection=="auto:recent")return played(game.id);
    if(collection=="auto:favorites")return favorite(data);
    if(collection=="auto:unplayed")return !played(game.id);
    if(collection=="auto:multiplayer")return gamePlayers(data).maximum>=2;
    for(const auto& value:definitions_) {
        const auto d=value.toObject();if(d["id"]!=collection)continue;
        if(!d["dynamic"].toBool())return d["games"].toArray().contains(game.id);
        const auto rules=d["rules"].toObject();
        if(!game.title.contains(rules["title"].toString(),Qt::CaseInsensitive))return false;
        if(!rules["platform"].toString().isEmpty() && rules["platform"]!=game.platformId)return false;
        for(const auto& key:QStringList{"genre","publisher","developer"})
            if(!rules[key].toString().isEmpty() && data.value(key).toString().compare(rules[key].toString(),Qt::CaseInsensitive)!=0)return false;
        if(rules["players"].toInt()>0 && gamePlayers(data).maximum<rules["players"].toInt())return false;
        if(rules["played"].toString()=="yes" && !played(game.id))return false;
        if(rules["played"].toString()=="no" && played(game.id))return false;
        if(rules["favorite"].toBool() && !favorite(data))return false;
        return true;
    }
    return false;
}
QString GameCollections::title() const {
    if(route_=="membership")return "Game collections";
    if(route_=="new")return "New collection";
    if(route_=="pick")return "Choose "+field_;
    if(route_=="games")return "Choose games";
    if(route_=="remove")return "Delete collection?";
    return draft_.value("name").toString("Collections");
}
QString GameCollections::detail() const {
    if(route_=="membership")return "A game can belong to several manual collections.";
    if(route_=="remove")return "Only this collection is removed. Games, saves and history stay where they are.";
    if(route_=="games")return query_.isEmpty()?"Select games, then Save collection.":"Search: "+query_;
    if(route_=="edit")return draft_["dynamic"].toBool()?"Games matching every rule are added automatically. Missing metadata does not match a rule.":"Choose games from any system. No files are moved.";
    return "Manual: choose games yourself. Automatic: keep a collection updated by rules.";
}
void GameCollections::begin(const QString& id) {
    if(!writable_){route_="new";emit changed();return;}
    for(const auto& v:definitions_)if(v.toObject()["id"]==id){edit(v.toObject());return;}
    draft_={};route_="new";focus_=0;query_.clear();error_.clear();emit changed();
}
void GameCollections::beginMembership(const QString& game) {game_=game;route_="membership";focus_=0;emit changed();}
void GameCollections::edit(const QJsonObject& value) { draft_=value;route_="edit";focus_=0;error_.clear();emit changed(); }
void GameCollections::close() {route_.clear();draft_={};query_.clear();focus_=0;emit changed();}
QList<Adventure> GameCollections::candidates() const {
    QList<Adventure> games;for(const auto& a:library_.adventures())if(!a.collectionOnly && a.title.contains(query_,Qt::CaseInsensitive)) {
        const auto r=library_.registration(a.id);if(r && r->removed)continue;games.append(a);
    }
    std::sort(games.begin(),games.end(),[](const auto& a,const auto& b){const auto cmp=a.title.compare(b.title,Qt::CaseInsensitive);return cmp?cmp<0:a.id<b.id;});return games;
}
QStringList GameCollections::options(const QString& field) const {
    QStringList result;for(const auto& a:library_.adventures())if(!a.collectionOnly) {
        const auto value=field=="platform"?a.platformId:library_.artwork(a.id).value(field).toString().trimmed();
        if(!value.isEmpty()&&!result.contains(value,Qt::CaseInsensitive))result.append(value);
    }
    result.sort(Qt::CaseInsensitive);result.prepend(QString());return result;
}
QVariantList GameCollections::rows() const {
    if(route_=="new")return {row("Manual collection"),row("Automatic collection"),row("Cancel")};
    if(route_=="remove")return {row("Cancel"),row("Delete collection")};
    if(route_=="pick") {QVariantList result;for(const auto& v:options(field_))result.append(row(v.isEmpty()?"Any":field_=="platform"?platformLabel(v).name:v));return result;}
    if(route_=="membership") {
        QVariantList result;for(const auto& d:definitions_)if(!d.toObject()["dynamic"].toBool())result.append(row((d.toObject()["games"].toArray().contains(game_)?"✓  ":"+  ")+d.toObject()["name"].toString()));
        result.append(row("New manual collection"));result.append(row("Done"));return result;
    }
    if(route_=="games") {
        QVariantList result{row(query_.isEmpty()?"Search games":"Search: "+query_),row("Save collection")};
        const auto ids=draft_["games"].toArray();for(const auto& a:candidates())result.append(row((ids.contains(a.id)?"✓  ":"+  ")+a.title+" · "+platformLabel(a.platformId).badge));return result;
    }
    if(route_!="edit")return {};
    QVariantList result{row("Name: "+draft_["name"].toString())};
    if(!draft_["dynamic"].toBool())result.append(row("Choose games · "+QString::number(draft_["games"].toArray().size())));
    else {
        const auto r=draft_["rules"].toObject();
        result.append(row("Title contains: "+r["title"].toString("Any")));
        for(const auto& key:QStringList{"platform","genre","publisher","developer"})result.append(row(key.left(1).toUpper()+key.mid(1)+": "+(r[key].toString().isEmpty()?"Any":key=="platform"?platformLabel(r[key].toString()).name:r[key].toString())));
        result.append(row("Players: "+(r["players"].toInt()?QString::number(r["players"].toInt())+" or more":"Any")));
        result.append(row(QString("Played: ")+(r["played"].toString().isEmpty()?"Any":r["played"].toString()=="yes"?"Yes":"No")));
        result.append(row(r["favorite"].toBool()?"Favorites only: Yes":"Favorites only: No"));
    }
    result.append(row("Save collection"));result.append(row("Delete collection"));result.append(row("Cancel"));return result;
}
void GameCollections::activate(int index) {
    if(index<0 || index>=rows().size() || !writable_)return;
    focus_=index;
    if(route_=="new") {
        if(index==2){close();return;}
        draft_={{"id","user:"+QUuid::createUuid().toString(QUuid::WithoutBraces)},{"name","New collection"},{"dynamic",index==1},{"games",QJsonArray{}},{"rules",QJsonObject{}}};
        route_="edit";focus_=0;field_="name";emit textRequested("Collection name",{},48);
    } else if(route_=="membership") {
        QList<int> manual;for(int i=0;i<definitions_.size();++i)if(!definitions_[i].toObject()["dynamic"].toBool())manual.append(i);
        if(index<manual.size()) {
            auto next=definitions_;auto d=next[manual[index]].toObject();auto ids=d["games"].toArray();
            if(ids.contains(game_)) { for(int i=ids.size()-1;i>=0;--i)if(ids[i]==game_)ids.removeAt(i); } else ids.append(game_);
            d["games"]=ids;next[manual[index]]=d;save(next);
        } else if(index==manual.size()) {begin();activate(0);draft_["games"]=QJsonArray{game_};} else {close();return;}
    } else if(route_=="pick") {
        auto rules=draft_["rules"].toObject();rules[field_]=options(field_).value(index);draft_["rules"]=rules;route_="edit";focus_=0;
    } else if(route_=="games") {
        if(index==0){field_="search";emit textRequested("Search games",query_,48);}
        else if(index==1) {route_="edit";activate(draft_["dynamic"].toBool()?9:2);return;}
        else {auto ids=draft_["games"].toArray();const auto games=candidates();const auto id=games[index-2].id;
            if(ids.contains(id)){for(int i=ids.size()-1;i>=0;--i)if(ids[i]==id)ids.removeAt(i);}else ids.append(id);draft_["games"]=ids;}
    } else if(route_=="remove") {
        if(index==0){route_="edit";focus_=0;}else {auto next=definitions_;for(int i=next.size()-1;i>=0;--i)if(next[i].toObject()["id"]==draft_["id"])next.removeAt(i);if(save(next))close();}
    } else if(route_=="edit") {
        const int saveIndex=draft_["dynamic"].toBool()?9:2;
        if(index==saveIndex) {
            if(draft_["name"].toString().trimmed().isEmpty()){error_="Give the collection a name.";emit changed();return;}
            auto next=definitions_;bool replaced=false;for(int i=0;i<next.size();++i)if(next[i].toObject()["id"]==draft_["id"]){next[i]=draft_;replaced=true;break;}
            if(!replaced)next.append(draft_);
            if(save(next))close();
        } else if(index==saveIndex+1){route_="remove";focus_=0;}
        else if(index==saveIndex+2){close();return;}
        else if(index==0){field_="name";emit textRequested("Collection name",draft_["name"].toString(),48);}
        else if(!draft_["dynamic"].toBool()){route_="games";focus_=0;query_.clear();}
        else if(index==1){field_="title";emit textRequested("Title contains",draft_["rules"].toObject()["title"].toString(),48);}
        else if(index<=5){field_=QStringList{"platform","genre","publisher","developer"}[index-2];route_="pick";focus_=0;}
        else {auto r=draft_["rules"].toObject();if(index==6)r["players"]=r["players"].toInt()==0?2:r["players"].toInt()==2?4:0;
            else if(index==7)r["played"]=r["played"].toString().isEmpty()?"yes":r["played"].toString()=="yes"?"no":"";
            else if(index==8)r["favorite"]=!r["favorite"].toBool();
            draft_["rules"]=r;}
    }
    emit changed();
}
void GameCollections::applyText(const QString& text) {
    const auto value=text.trimmed().left(48);
    if(field_=="name")draft_["name"]=value;
    else if(field_=="search"){query_=value;focus_=0;}
    else {auto rules=draft_["rules"].toObject();rules[field_]=value;draft_["rules"]=rules;}
    emit changed();
}
void GameCollections::dispatch(Action action) {
    if(action==Action::Back) {
        if(route_=="games" || route_=="pick" || route_=="remove"){route_="edit";focus_=0;}else close();
    } else if(action==Action::Confirm){activate(focus_);return;}
    else if(action==Action::Up || action==Action::Down)focus_=std::clamp(focus_+(action==Action::Up?-1:1),0,std::max(0,int(rows().size())-1));
    emit changed();
}
}
