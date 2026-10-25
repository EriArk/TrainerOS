#include "GameCollections.h"
#include "core/model/GamePlayers.h"
#include "core/model/GameFavorite.h"
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
QString ruleLabel(const QString& key) {
    static const QMap<QString,QString> labels{{"all","All conditions"},{"any","Any condition"},{"exclude","None of these"},
        {"title","Title"},{"platform","System"},{"genre","Genre"},{"developer","Developer"},{"publisher","Publisher"},
        {"players","Players"},{"played","Played"},{"favorite","Favorite"},{"gte","at least"},{"equals","is"},{"prefix","starts with"},{"contains","contains"}};
    return labels.value(key,key);
}
QString ruleValue(const QJsonValue& value) {return value.isBool()?(value.toBool()?"Yes":"No"):value.toVariant().toString();}
QJsonObject legacyTree(const QJsonObject& rules) {
    QJsonArray nodes;
    for(const auto& field:QStringList{"title","platform","genre","developer","publisher"})if(!rules[field].toString().isEmpty())nodes.append(QJsonObject{{"field",field},{"op",field=="title"?"contains":"equals"},{"value",rules[field]}});
    if(rules["players"].toInt()>0)nodes.append(QJsonObject{{"field","players"},{"op","gte"},{"value",rules["players"]}});
    if(!rules["played"].toString().isEmpty())nodes.append(QJsonObject{{"field","played"},{"op","equals"},{"value",rules["played"]=="yes"}});
    if(rules["favorite"].toBool())nodes.append(QJsonObject{{"field","favorite"},{"op","equals"},{"value",true}});
    return {{"op","all"},{"children",nodes}};
}
bool validRule(const QJsonObject& node,int depth=0) {
    if(depth>4)return false;
    const auto op=node["op"].toString();
    if(node.contains("children")) {
        if(!QStringList{"all","any","exclude"}.contains(op)||!node["children"].isArray()||node["children"].toArray().size()>16)return false;
        for(const auto& child:node["children"].toArray())if(!child.isObject()||!validRule(child.toObject(),depth+1))return false;
        return true;
    }
    const auto field=node["field"].toString();const auto value=node["value"];
    if(field=="players")return (op=="gte"||op=="equals")&&value.isDouble()&&value.toDouble()==value.toInt()&&value.toInt()>=1&&value.toInt()<=128;
    if(field=="played"||field=="favorite")return op=="equals"&&value.isBool();
    return QStringList{"title","platform","genre","developer","publisher"}.contains(field)&&QStringList{"contains","equals","prefix"}.contains(op)&&value.isString()&&!value.toString().trimmed().isEmpty()&&value.toString().size()<=80;
}
QJsonObject replaceNode(QJsonObject node,const QList<int>& path,int depth,const QJsonObject& value,bool remove=false) {
    if(depth==path.size())return value;
    auto children=node["children"].toArray();const int index=path[depth];
    if(index<0||index>=children.size())return node;
    if(remove&&depth==path.size()-1)children.removeAt(index);
    else children[index]=replaceNode(children[index].toObject(),path,depth+1,value,remove);
    node["children"]=children;return node;
}
}
void GameCollections::configure(const QString& directory,const QString& owner) {
    close();definitions_={};hidden_.clear();file_.clear();writable_=!owner.isEmpty();error_.clear();
    if(writable_) {
        const auto key=QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex());
        file_=QDir(directory).filePath("collections/"+key+".json");
        QFile input(file_);
        if(input.exists()) {
            if(!input.open(QIODevice::ReadOnly) || input.size()>4*1024*1024) { writable_=false;error_="Couldn't read your collections. Existing data has been kept."; }
            else {
                QJsonParseError parse;const auto doc=QJsonDocument::fromJson(input.readAll(),&parse);
                if(parse.error!=QJsonParseError::NoError || !doc.isObject() || (doc["version"].toInt()!=1 && doc["version"].toInt()!=2) || !doc["collections"].isArray()) {
                    writable_=false;error_="Couldn't read your collections. Existing data has been kept.";
                } else {
                    definitions_=doc["collections"].toArray();
                    for(const auto& value:definitions_)if(value.toObject().contains("tree")&&!validRule(value.toObject()["tree"].toObject())){writable_=false;error_="A collection uses invalid or unknown rules. The original file is preserved.";}
                    for(const auto& id:doc["hiddenCollections"].toArray())
                        if(id.isString() && id.toString()!="multiverse")hidden_.insert(id.toString());
                }
            }
        }
    }
    refresh();emit definitionsChanged();emit changed();
}
void GameCollections::refresh() {
    previewKey_.clear();
    played_.clear();for(const auto& s:library_.recentSessions())played_.insert(s.adventureId);
    for(const auto& a:library_.adventures())if(library_.recordedSeconds(a.id).value_or(0)>0)played_.insert(a.id);
    emit changed();
}
bool GameCollections::save(const QJsonArray& next) {
    for(const auto& value:next)if(value.toObject().contains("tree")&&!validRule(value.toObject()["tree"].toObject())){error_="Complete each condition before saving.";emit changed();return false;}
    if(!writable_) {if(error_.isEmpty())error_="Choose a Trainer before editing collections.";emit changed();return false;}
    if(!file_.isEmpty()) {
        QDir().mkpath(QFileInfo(file_).absolutePath());QSaveFile output(file_);
        auto hidden=hidden_.values();hidden.sort();
        const auto bytes=QJsonDocument(QJsonObject{{"version",2},{"collections",next},
            {"hiddenCollections",QJsonArray::fromStringList(hidden)}}).toJson();
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
    if(collection=="auto:favorites")return gameFavorite(data);
    if(collection=="auto:unplayed")return !played(game.id);
    if(collection=="auto:multiplayer")return gamePlayers(data).maximum>=2;
    for(const auto& value:definitions_) {
        const auto d=value.toObject();if(d["id"]!=collection)continue;
        if(!d["dynamic"].toBool())return d["games"].toArray().contains(game.id);
        return matches(d,game);
    }
    return false;
}
bool GameCollections::matches(const QJsonObject& d,const Adventure& game) const {
    if(d["exclude"].toArray().contains(game.id))return false;
    if(d["include"].toArray().contains(game.id))return true;
    const auto tree=d.contains("tree")?d["tree"].toObject():legacyTree(d["rules"].toObject());
    if(!validRule(tree))return false;
    const auto data=library_.artwork(game.id);
    std::function<std::optional<bool>(const QJsonObject&)> evaluate=[&](const QJsonObject& n)->std::optional<bool>{
        const auto op=n["op"].toString();
        if(n.contains("children")) {
            bool any=false,all=true,unknown=false,knownFalse=false;
            for(const auto& c:n["children"].toArray()){const auto v=evaluate(c.toObject());unknown|=!v.has_value();knownFalse|=v.has_value()&&!v.value();any|=v.value_or(false);all&=v.value_or(false);}
            if(op=="all"){if(unknown&&!knownFalse)return std::nullopt;return all;}
            if(op=="any"){if(unknown&&!any)return std::nullopt;return any;}
            if(unknown&&!any)return std::nullopt;
            return !any;
        }
        const auto field=n["field"].toString();const auto value=n["value"];
        if(field=="played")return played(game.id)==value.toBool();
        if(field=="favorite")return gameFavorite(data)==value.toBool();
        if(field=="players"){const auto count=gamePlayers(data).maximum;if(count<=0)return std::nullopt;return op=="gte"?count>=value.toInt():count==value.toInt();}
        const auto text=(field=="title"?game.title:field=="platform"?game.platformId:data.value(field).toString()).normalized(QString::NormalizationForm_KC).simplified().toCaseFolded();
        if(text.isEmpty())return std::nullopt;
        const auto wanted=value.toString().normalized(QString::NormalizationForm_KC).simplified().toCaseFolded();
        return op=="equals"?text==wanted:op=="prefix"?text.startsWith(wanted):text.contains(wanted);
    };
    return evaluate(tree).value_or(false);
}
QJsonObject GameCollections::ruleNode() const {
    auto node=draft_["tree"].toObject();for(const int index:rulePath_)node=node["children"].toArray().at(index).toObject();return node;
}
void GameCollections::replaceRule(const QJsonObject& node){draft_["tree"]=replaceNode(draft_["tree"].toObject(),rulePath_,0,node);}
void GameCollections::removeRule(){draft_["tree"]=replaceNode(draft_["tree"].toObject(),rulePath_,0,{},true);if(!rulePath_.isEmpty())rulePath_.removeLast();}
QVariantList GameCollections::preview() const {
    QVariantList result;if(!smart())return result;
    const auto key=QJsonDocument(draft_).toJson(QJsonDocument::Compact)+query_.toUtf8()+(route_=="preview"?"all":"matched")+QByteArray::number(library_.libraryRevision().value_or(0));
    if(key==previewKey_)return previewCache_;
    for(const auto& game:candidates()) {
        const bool match=matches(draft_,game);if(route_!="preview"&&!match)continue;
        const auto art=library_.artwork(game.id);
        result.append(QVariantMap{{"id",game.id},{"label",game.title},{"platform",platformLabel(game.platformId).badge},{"picture",art.value("displayPicture",art.value("cover"))},{"matches",match},
            {"state",draft_["exclude"].toArray().contains(game.id)?"Excluded":draft_["include"].toArray().contains(game.id)?"Included":match?"Matches":"Not matched"}});
    }previewKey_=key;previewCache_=result;return result;
}
void GameCollections::overrideGame(const QString& id) {
    if(!writable_||!smart())return;
    const auto games=candidates();if(std::none_of(games.begin(),games.end(),[&](const auto& g){return g.id==id;}))return;
    auto include=draft_["include"].toArray(),exclude=draft_["exclude"].toArray();
    const bool included=include.contains(id),excluded=exclude.contains(id);
    const auto remove=[&](QJsonArray& a){for(int i=a.size()-1;i>=0;--i)if(a[i]==id)a.removeAt(i);};remove(include);remove(exclude);
    if(included)exclude.append(id);else if(!excluded)include.append(id);
    draft_["include"]=include;draft_["exclude"]=exclude;emit changed();
}
QString GameCollections::title() const {
    if(route_=="membership")return "Add to collection";
    if(route_=="membership-new")return "Create collection";
    if(route_=="new")return "New collection";
    if(route_=="pick")return "Choose "+field_;
    if(route_=="games")return "Choose games";
    if(route_=="remove")return "Delete collection?";
    return draft_.value("name").toString("Collections");
}
QString GameCollections::detail() const {
    if(route_=="membership" || route_=="membership-new") {
        for(const auto& a:library_.adventures())if(a.id==game_)return a.title;
        return "Choose a collection for this game.";
    }
    if(route_=="remove")return "Only this collection is removed. Games, saves and history stay where they are.";
    if(route_=="games")return query_.isEmpty()?"Select games, then Save collection.":"Search: "+query_;
    if(route_=="rule-group")return "Group conditions together. The list on the right updates as you edit.";
    if(route_=="rule-leaf")return "Choose what to match and enter its value. Missing information does not count as a match.";
    if(route_=="preview")return "Tap a game to cycle Auto, Included and Excluded. Save the collection to keep your changes.";
    if(route_=="edit")return draft_["dynamic"].toBool()?"Combine conditions. Preview changes only this collection; tap a game to include or exclude it.":"Choose games from any system. No files are moved.";
    return "Manual: choose games yourself. Automatic: keep a collection updated by rules.";
}
void GameCollections::begin(const QString& id) {
    if(!writable_){route_="new";emit changed();return;}
    for(const auto& v:definitions_)if(v.toObject()["id"]==id){edit(v.toObject());return;}
    draft_={};route_="new";focus_=0;query_.clear();error_.clear();emit changed();
}
void GameCollections::beginMembership(const QString& game) {
    close();game_=game;route_="membership";
    if(writable_)error_.clear();
    emit changed();
}
void GameCollections::edit(const QJsonObject& value) { draft_=value;rulePath_.clear();route_="edit";focus_=0;error_.clear();emit changed(); }
void GameCollections::close() {route_.clear();draft_={};game_.clear();field_.clear();query_.clear();focus_=0;emit changed();}
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
        QVariantList result;for(const auto& value:definitions_) {
            const auto d=value.toObject();if(d["dynamic"].toBool())continue;
            const auto games=d["games"].toArray();
            result.append(QVariantMap{{"label",d["name"].toString()},{"member",games.contains(game_)},
                {"count",games.size()},{"enabled",writable_}});
        }
        result.append(row("Create collection"));result.append(row("Cancel"));return result;
    }
    if(route_=="membership-new")return {row(draft_["name"].toString().isEmpty()?"Collection name":draft_["name"].toString()),row("Create & add game"),row("Cancel")};
    if(route_=="games") {
        QVariantList result{row(query_.isEmpty()?"Search games":"Search: "+query_),row("Save collection")};
        const auto ids=draft_["games"].toArray();for(const auto& a:candidates())result.append(row((ids.contains(a.id)?"✓  ":"+  ")+a.title+" · "+platformLabel(a.platformId).badge));return result;
    }
    if(route_=="preview") {QVariantList r{row("Back to conditions")};for(const auto& v:preview()){auto m=v.toMap();r<<row(m["label"].toString()+" · "+m["state"].toString());}return r;}
    if(route_=="rule-group") {
        const auto node=ruleNode();const auto children=node["children"].toArray();
        QVariantList r{row("Match: "+ruleLabel(node["op"].toString("all")))};
        for(const auto& c:children){const auto n=c.toObject();r<<row(n.contains("children")?ruleLabel(n["op"].toString())+" · "+QString::number(n["children"].toArray().size())+" conditions":ruleLabel(n["field"].toString())+" "+ruleLabel(n["op"].toString())+" "+ruleValue(n["value"]));}
        r<<row("+ Condition")<<row("+ Group")<<row(rulePath_.isEmpty()?"Back to collection":"Remove this group");if(!rulePath_.isEmpty())r<<row("Back");return r;
    }
    if(route_=="rule-leaf") {const auto n=ruleNode();return {row("Field: "+ruleLabel(n["field"].toString())),row("Comparison: "+ruleLabel(n["op"].toString())),row("Value: "+ruleValue(n["value"])),row("Remove condition"),row("Back")};}
    if(route_!="edit")return {};
    QVariantList result{row("Name: "+draft_["name"].toString())};
    if(!draft_["dynamic"].toBool())result.append(row("Choose games · "+QString::number(draft_["games"].toArray().size())));
    else {result.append(row("Edit conditions"));result.append(row("Preview & exceptions · "+QString::number(preview().size())+" games"));}
    result.append(row("Save collection"));result.append(row("Delete collection"));result.append(row("Cancel"));return result;
}
void GameCollections::activate(int index) {
    if(!writable_||index<0||index>=rows().size())return;
    if(route_=="preview"){if(index==0){route_="edit";focus_=2;}else {const auto rows=preview();if(index<=rows.size())overrideGame(rows[index-1].toMap()["id"].toString());}emit changed();return;}
    if(route_=="rule-group") {
        auto node=ruleNode();auto children=node["children"].toArray();const int count=children.size();
        if(index==0){const QStringList ops{"all","any","exclude"};node["op"]=ops[(ops.indexOf(node["op"].toString())+1)%3];replaceRule(node);}
        else if(index<=count){rulePath_<<index-1;route_=ruleNode().contains("children")?"rule-group":"rule-leaf";}
        else if(index==count+1||index==count+2){
            if(QJsonDocument(draft_["tree"].toObject()).toJson().size()>12000||children.size()>=16||rulePath_.size()>=4){error_="Keep groups to four levels and sixteen conditions.";emit changed();return;}
            children.append(index==count+1?QJsonObject{{"field","title"},{"op","contains"},{"value",""}}:QJsonObject{{"op","all"},{"children",QJsonArray{}}});node["children"]=children;replaceRule(node);rulePath_<<count;route_=index==count+1?"rule-leaf":"rule-group";
        }else if(index==count+3&&!rulePath_.isEmpty()){removeRule();route_="rule-group";}
        else {dispatch(Action::Back);return;}focus_=0;emit changed();return;
    }
    if(route_=="rule-leaf") {
        auto node=ruleNode();const QStringList fields{"title","platform","genre","developer","publisher","players","played","favorite"};
        if(index==0){const auto field=fields[(fields.indexOf(node["field"].toString())+1)%fields.size()];node["field"]=field;node["op"]=field=="players"?"gte":"contains";node["value"]=field=="players"?QJsonValue(2):field=="played"||field=="favorite"?QJsonValue(true):QJsonValue("");if(field=="played"||field=="favorite")node["op"]="equals";replaceRule(node);}
        else if(index==1){const QStringList ops=node["field"]=="players"?QStringList{"gte","equals"}:node["field"]=="played"||node["field"]=="favorite"?QStringList{"equals"}:QStringList{"contains","equals","prefix"};node["op"]=ops[(ops.indexOf(node["op"].toString())+1)%ops.size()];replaceRule(node);}
        else if(index==2){if(node["field"]=="played"||node["field"]=="favorite"){node["value"]=!node["value"].toBool();replaceRule(node);}else {field_="rule-value";emit textRequested("Condition value",node["value"].toVariant().toString(),80);}}
        else if(index==3){removeRule();route_="rule-group";focus_=0;}else {dispatch(Action::Back);return;}emit changed();return;
    }
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
            if(!ids.contains(game_))ids.append(game_);
            d["games"]=ids;next[manual[index]]=d;if(save(next))close();
        } else if(index==manual.size()) {
            draft_={{"id","user:"+QUuid::createUuid().toString(QUuid::WithoutBraces)},
                {"name",""},{"dynamic",false},{"games",QJsonArray{game_}},{"rules",QJsonObject{}}};
            route_="membership-new";focus_=0;field_="name";emit textRequested("Collection name",{},48);
        } else {close();return;}
    } else if(route_=="membership-new") {
        if(index==0){field_="name";emit textRequested("Collection name",draft_["name"].toString(),48);}
        else if(index==1) {
            if(draft_["name"].toString().trimmed().isEmpty())error_="Give the collection a name.";
            else {auto next=definitions_;next.append(draft_);if(save(next))close();}
        } else {dispatch(Action::Back);return;}
    } else if(route_=="pick") {
        auto rules=draft_["rules"].toObject();rules[field_]=options(field_).value(index);draft_["rules"]=rules;route_="edit";focus_=0;
    } else if(route_=="games") {
        if(index==0){field_="search";emit textRequested("Search games",query_,48);}
        else if(index==1) {route_="edit";activate(draft_["dynamic"].toBool()?3:2);return;}
        else {auto ids=draft_["games"].toArray();const auto games=candidates();const auto id=games[index-2].id;
            if(ids.contains(id)){for(int i=ids.size()-1;i>=0;--i)if(ids[i]==id)ids.removeAt(i);}else ids.append(id);draft_["games"]=ids;}
    } else if(route_=="remove") {
        if(index==0){route_="edit";focus_=0;}else {auto next=definitions_;for(int i=next.size()-1;i>=0;--i)if(next[i].toObject()["id"]==draft_["id"])next.removeAt(i);if(save(next))close();}
    } else if(route_=="edit") {
        const int saveIndex=draft_["dynamic"].toBool()?3:2;
        if(index==saveIndex) {
            if(draft_["name"].toString().trimmed().isEmpty()){error_="Give the collection a name.";emit changed();return;}
            auto next=definitions_;bool replaced=false;for(int i=0;i<next.size();++i)if(next[i].toObject()["id"]==draft_["id"]){next[i]=draft_;replaced=true;break;}
            if(!replaced)next.append(draft_);
            if(save(next))close();
        } else if(index==saveIndex+1){route_="remove";focus_=0;}
        else if(index==saveIndex+2){close();return;}
        else if(index==0){field_="name";emit textRequested("Collection name",draft_["name"].toString(),48);}
        else if(!draft_["dynamic"].toBool()){route_="games";focus_=0;query_.clear();}
        else if(index==1){if(!draft_.contains("tree"))draft_["tree"]=legacyTree(draft_["rules"].toObject());rulePath_.clear();route_="rule-group";focus_=0;}
        else if(index==2){route_="preview";focus_=0;}
    }
    emit changed();
}
void GameCollections::applyText(const QString& text) {
    if(!isOpen() || field_.isEmpty() || !writable_)return;
    const auto value=text.trimmed().left(80);
    if(field_=="rule-value") {auto n=ruleNode();if(n["field"]=="players"){bool ok=false;const int v=value.toInt(&ok);if(!ok||v<1||v>128){error_="Enter a player count from 1 to 128.";emit changed();return;}n["value"]=v;}else n["value"]=value;replaceRule(n);error_.clear();emit changed();return;}
    if(field_=="name") {draft_["name"]=value;if(route_=="membership-new"){focus_=1;error_.clear();}}
    else if(field_=="search"){query_=value;focus_=0;}
    else {auto rules=draft_["rules"].toObject();rules[field_]=value;draft_["rules"]=rules;}
    emit changed();
}
void GameCollections::dispatch(Action action) {
    if(action==Action::Back) {
        if(route_=="rule-leaf"||route_=="rule-group"){if(rulePath_.isEmpty())route_="edit";else {rulePath_.removeLast();route_="rule-group";}focus_=0;}
        else if(route_=="preview"){route_="edit";focus_=2;}
        else if(route_=="membership-new"){draft_={};field_.clear();route_="membership";focus_=std::max(0,int(rows().size())-2);if(writable_)error_.clear();}
        else if(route_=="games" || route_=="pick" || route_=="remove"){route_="edit";focus_=0;}else close();
    } else if(action==Action::Confirm){activate(focus_);return;}
    else if(action==Action::Up || action==Action::Down)focus_=std::clamp(focus_+(action==Action::Up?-1:1),0,std::max(0,int(rows().size())-1));
    emit changed();
}
}
