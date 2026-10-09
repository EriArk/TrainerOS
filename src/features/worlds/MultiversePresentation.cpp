#include "MultiversePresentation.h"
#include "core/model/SeriesCatalog.h"
#include "core/model/GamePlayers.h"
#include <algorithm>
#include <climits>
#include "core/repository/CollectionRepository.h"
#include <QFileInfo>
#include <QJsonArray>

namespace trainer {
MultiversePresentation::MultiversePresentation(bool sample, QObject* parent) : QObject(parent), sample_(sample) {
    if (sample_) entries_ = {{"sample-courier", "gb", "Star Courier", true},
        {"sample-lantern", "gb", "Lantern Valley", false}, {"sample-orbit", "snes", "Orbit Rally", true},
        {"sample-forest", "gc", "Forest of Echoes", false}, {"sample-neon", "ps", "Neon Circuit", true},
        {"sample-cloud", "psp", "Cloud Atlas", false}};
}
MultiversePresentation::MultiversePresentation(LibraryRepository& repository, AdventureAdapter& adapter, QObject* parent)
    : MultiversePresentation(!repository.editable(), parent) {
    repository_ = &repository; adapter_ = &adapter;
    collections_=new GameCollections(repository,this);
    connect(collections_,&GameCollections::definitionsChanged,this,&MultiversePresentation::refresh);
    refresh();
}
void MultiversePresentation::refresh() {
    if (!repository_) return;
    if(collections_)collections_->refresh();
    const auto focused = route_ == "systems" ? QString() : detail().value("id").toString();
    presentations_.clear(); collectionsCache_.clear(); systemsCache_.clear(); systemsCached_=false;
    entries_.clear();
    for (const auto& a : repository_->adventures()) if (!a.collectionOnly) {
        const auto r = repository_->registration(a.id);
        if(r && r->removed)continue;
        const auto title=r && a.title==QFileInfo(r->contentPath).completeBaseName()?seriesDisplayTitle(a.title):a.title;
        entries_.append({a.id,a.platformId,title,(r ? r->contentAvailable : sample_),a.domain=="pokemon"?"pokemon":seriesForTitle(a.title)});
    }
    std::sort(entries_.begin(),entries_.end(),[](const auto& a,const auto& b) {
        const int cmp=QString::compare(a.title,b.title,Qt::CaseInsensitive);return cmp ? cmp<0 : a.id<b.id;
    });
    const auto platforms=systems(); bool found=collection_!="multiverse";
    for(int i=0;i<platforms.size() && collection_=="multiverse";++i) if(platforms[i].toMap()["id"].toString()==system_) {systemFocus_=i;found=true;break;}
    if(!found) {route_=collection_=="multiverse"?"systems":"games";systemFocus_=0;if(!platforms.isEmpty())system_=platforms.front().toMap()["id"].toString();}
    const auto list=filtered();
    for(int i=0;i<list.size();++i) if(list[i].id==focused) positions_[system_]=i;
    emit libraryChanged(); emit gamesChanged(); emit changed();
}

bool MultiversePresentation::belongs(const Game& game) const {
    if(collection_=="multiverse")return true;
    if(collection_.startsWith("auto:") || collection_.startsWith("user:")) {
        if(!collections_ || !repository_)return false;
        const auto record=repository_->registration(game.id);
        if(record)return collections_->contains(collection_,record->adventure);
        for(const auto& a:repository_->adventures())if(a.id==game.id)return collections_->contains(collection_,a);
        return false;
    }
    return (game.series.isEmpty()?seriesForTitle(game.title):game.series)==collection_;
}
QString MultiversePresentation::collectionName() const {
    if(collection_=="multiverse")return "All games";
    if(collections_) {
        for(const auto& d:collections_->definitions())if(d.toObject()["id"]==collection_)return d.toObject()["name"].toString();
        for(const auto& d:collections_->automatic())if(d.toMap()["id"]==collection_)return d.toMap()["name"].toString();
    }
    return seriesDefinition(collection_).name;
}
QString MultiversePresentation::collectionArt() const {
    return "qrc:/series/"+(collection_.contains(':')?QString("multiverse"):collection_)+".png";
}
QVariantMap MultiversePresentation::game(const QString& id) const {
    for(const auto& entry:entries_)if(entry.id==id)return present(entry);
    return {};
}
QVariantList MultiversePresentation::collections() const {
    if(!collectionsCache_.isEmpty())return collectionsCache_;
    QHash<QString,int> counts;
    for(const auto& game:entries_)if(game.linked)++counts[game.series.isEmpty()?seriesForTitle(game.title):game.series];
    QVariantList result;
    for(const auto& d:seriesDefinitions()) {
        if(d.id!="pokemon" && d.id!="multiverse" && !counts.value(d.id))continue;
        result.append(QVariantMap{{"id",d.id},{"name",d.id=="multiverse"?QString("All games"):d.name},{"colour",d.colour},
            {"art","qrc:/series/"+d.id+".png"},{"count",d.id=="multiverse"?int(entries_.size()):counts.value(d.id)}});
    }
    if(collections_) {
        auto extra=collections_->automatic();
        for(const auto& d:collections_->definitions())extra.append(d.toObject().toVariantMap());
        for(const auto& v:extra) {
            auto row=v.toMap();int count=0;
            for(const auto& g:entries_)if(g.linked) {
                const auto r=repository_->registration(g.id);
                if(r && collections_->contains(row["id"].toString(),r->adventure))++count;
            }
            row["count"]=count;row["colour"]=row["dynamic"].toBool()||row["id"].toString().startsWith("auto:")?"#9bb9df":"#e9b2cd";
            row["art"]="";result.append(row);
        }
    }
    collectionsCache_=result;return result;
}
QJsonObject MultiversePresentation::navigationState() const {
    auto state=localNavigation(); auto scopes=collectionStates_; scopes[collection_]=localNavigation();
    state["collection"]=collection_; state["collections"]=scopes; return state;
}
void MultiversePresentation::restoreNavigation(const QJsonObject& state) {
    collectionStates_=state["collections"].toObject();
    collection_=state["collection"].toString("multiverse");
    if(collections_ && !collections_->known(collection_))collection_="multiverse";
    // Carry a legacy explicit Home choice into its new series without changing the Adventure.
    if(collectionStates_.isEmpty())for(const auto& game:entries_)if(game.id==state["selected"].toString())
        collectionStates_[seriesForTitle(game.title)]=state;
    restoreLocal(collectionStates_.contains(collection_)?collectionStates_[collection_].toObject():state);
    refresh();
}
void MultiversePresentation::setCollection(const QString& id) {
    const auto next=collections_ && collections_->known(id)?id:seriesDefinition(id).id;
    if(next==collection_)return;
    collectionStates_[collection_]=localNavigation(); collection_=next;
    restoreLocal(collectionStates_.value(next).toObject());
    emit libraryChanged(); emit gamesChanged(); emit changed();
}

QJsonObject MultiversePresentation::localNavigation() const {
    QJsonObject queries,filters,positions;
    for(auto i=queries_.cbegin();i!=queries_.cend();++i)queries[i.key()]=i.value();
    for(auto i=filters_.cbegin();i!=filters_.cend();++i)filters[i.key()]=i.value();
    for(auto i=positions_.cbegin();i!=positions_.cend();++i)positions[i.key()]=i.value();
    return {{"selected",selected_},{"system",system_},{"route",route_},{"queries",queries},{"filters",filters},{"positions",positions},
        {"focused",detail().value("id").toString()}};
}
void MultiversePresentation::showSystems() {
    route_ = collection_=="multiverse" ? "systems" : "games";
    emit changed();
}
void MultiversePresentation::restoreLocal(const QJsonObject& state) {
    selected_=state["selected"].toString();system_=state["system"].toString();
    route_=state["route"].toString();if(route_=="detail")route_="games";
    if(!QStringList{"systems","games"}.contains(route_))route_="systems";
    if(collection_!="multiverse")system_="_series";
    queries_.clear();filters_.clear();positions_.clear();
    const auto queries=state["queries"].toObject(),filters=state["filters"].toObject(),positions=state["positions"].toObject();
    auto keys=multiversePlatforms(); keys.append({"_series","","",""});
    for(const auto& p:keys) {
        if(queries.contains(p.id))queries_[p.id]=queries[p.id].toString().left(48);
        if(filters.contains(p.id))filters_[p.id]=std::clamp(filters[p.id].toInt(),0,2);
        if(positions.contains(p.id))positions_[p.id]=std::max(0,positions[p.id].toInt());
    }
    systemsCached_=false; systemsCache_.clear();
    if(collection_!="multiverse")route_="games";
    const auto list=filtered();for(int i=0;i<list.size();++i)if(list[i].id==state["focused"].toString() && (i || positions.contains(system_)))positions_[system_]=i;
}
QVariantList MultiversePresentation::systems() const {
    if(systemsCached_)return systemsCache_;
    QVariantList result;
    if (repository_ || !sample_) {
        auto platforms=multiversePlatforms();
        // Unknown imported categories remain usable, never silently discarded.
        for(const auto& g:entries_) if(std::none_of(platforms.begin(),platforms.end(),[&](const auto& p){return p.id==g.system;}))
            platforms.append({g.system,g.system,g.system,"console"});
        for(const auto& p:platforms) {
            const auto count=std::count_if(entries_.begin(),entries_.end(),[&](const auto& g){return g.system==p.id && g.linked && belongs(g);});
            if(count)result.append(QVariantMap{{"id",p.id},{"name",p.name},{"shape",p.shape},{"count",int(count)}});
        }
        systemsCache_=result;systemsCached_=true;return result;
    }
    const QList<QStringList> data{{"gb","Game Boy","handheld"},{"snes","Super Nintendo","cartridge"},
        {"ps","PlayStation","disc"},{"dc","Dreamcast","disc"},{"gc","GameCube","cube"},{"psp","PSP","handheld"}};
    for (const auto& system : data) result.append(QVariantMap{{"id",system[0]}, {"name",system[1]}, {"shape",system[2]}});
    systemsCache_=result;systemsCached_=true;return result;
}
QString MultiversePresentation::systemName() const {
    for (const auto& item : systems()) if (item.toMap()["id"] == system_) return item.toMap()["name"].toString();
    return {};
}
QString MultiversePresentation::filterLabel() const { return QStringList{"All titles","Linked only","Missing files"}[filters_.value(system_)]; }
QVariantMap MultiversePresentation::present(const Game& game) const {
    const auto cached=presentations_.constFind(game.id);
    if(cached!=presentations_.cend())return *cached;
    QString system;
    for (const auto& item : systems()) if (item.toMap()["id"] == game.system) system = item.toMap()["name"].toString();
    if(system.isEmpty())system=platformLabel(game.system).name;
    bool playable=false;QString description,preview,time;
    if(repository_ && !sample_) {
        const auto r=repository_->registration(game.id);
        if(r){playable=game.linked && adapter_ && adapter_->capabilities(r->adventure).launch;description=r->adventure.description;}
        const auto media=repository_->exitMedia(game.id);if(media)preview="image://exit-media/"+media->sessionId;
        for(const auto& session:repository_->recentSessions())if(session.adventureId==game.id){time="Last opened "+session.startedAt.toLocalTime().toString("dd MMM · HH:mm");break;}
    }
    const auto artwork=repository_ ? repository_->artwork(game.id) : QVariantMap{};
    if(!artwork.value("desc").toString().isEmpty())description=artwork.value("desc").toString();
    auto year=artwork.value("releasedate").toString().left(4);
    bool validYear=false;const auto number=year.toInt(&validYear);
    if(!validYear || number<1970 || number>2100)year.clear();
    QString screenshot;
    for(const auto& field:QStringList{"screenshot","image","titleshot","thumbnail","cover"})
        if(!artwork.value(field).toString().isEmpty()){screenshot=artwork.value(field).toString();break;}
    const QVariantMap result{{"id",game.id},{"title",game.title},{"system",system},{"linked",game.linked},{"playable",playable},
        {"platformShort",platformLabel(game.system).badge},{"platformShape",platformLabel(game.system).shape},
        {"description",description},{"preview",preview.isEmpty() && repository_ ? repository_->artwork(game.id).value("cover").toString() : preview},{"time",time},
        {"artwork",artwork},{"logo",artwork.value("marquee",artwork.value("wheel"))},{"screenshot",screenshot},
        {"year",year},{"genre",artwork.value("genre")},{"players",artwork.value("players")},{"playInfo",gamePlayers(artwork).presentation()},
        {"developer",artwork.value("developer")},{"publisher",artwork.value("publisher")},{"synopsis",artwork.value("desc")},
        {"action",game.linked?"Start Adventure":"File unavailable"},
        {"status",sample_ ? (game.linked?"Sample linked entry":"Sample missing file") : !game.linked?"File unavailable":playable?"Ready to play":""}};
    presentations_.insert(game.id,result);return result;
}
QList<MultiversePresentation::Game> MultiversePresentation::filtered() const {
    QList<Game> result;
    const int filter = filters_.value(system_);
    for (const auto& game : entries_) if (belongs(game) && (collection_!="multiverse" || game.system == system_) && game.title.contains(query(), Qt::CaseInsensitive)
        && (filter != 1 || game.linked) && (filter != 2 || !game.linked)) result.append(game);
    if(collection_=="auto:recent" && repository_) {
        QHash<QString,int> order;int index=0;for(const auto& session:repository_->recentSessions())
            if(!order.contains(session.adventureId))order[session.adventureId]=index++;
        std::stable_sort(result.begin(),result.end(),[&](const auto& a,const auto& b){return order.value(a.id,INT_MAX)<order.value(b.id,INT_MAX);});
    }
    return result;
}
QVariantList MultiversePresentation::games() const {
    QVariantList result; for (const auto& game : filtered()) result.append(present(game)); return result;
}
int MultiversePresentation::focusIndex() const {
    if (route_ == "systems") return systemFocus_;
    return std::clamp(positions_.value(system_), 0, std::max(0, int(filtered().size()) - 1));
}
QVariantMap MultiversePresentation::detail() const {
    const auto list = filtered(); return list.isEmpty() ? QVariantMap{} : present(list[std::clamp(positions_.value(system_),0,int(list.size())-1)]);
}
QVariantMap MultiversePresentation::selected() const {
    for (const auto& game : entries_) if (belongs(game) && game.id == selected_) return present(game);
    if(!selected_.isEmpty() && std::none_of(entries_.begin(),entries_.end(),[&](const auto& g){return g.id==selected_;}))return {{"id",selected_},{"title","Selected Adventure is unavailable"},{"linked",false},{"playable",false},{"action","Choose Adventure"}};
    if(repository_ && !sample_) for(const auto& session:repository_->recentSessions())
        for(const auto& game:entries_) if(belongs(game) && game.id==session.adventureId)return present(game);
    if(!sample_)for(const auto& game:entries_)if(belongs(game) && game.linked)return present(game);
    return {};
}
QVariantList MultiversePresentation::choices() const {
    QVariantList result;
    for (const auto& game : entries_) if (belongs(game) && game.linked) {
        auto row = present(game);
        row["world"] = row["system"]; row["previewLabel"] = sample_ ? "Development sample" : repository_ && repository_->exitMedia(game.id)?"Last exit":"Adventure";
        row["location"] = ""; row["summary"] = sample_ ? "Select for Home · no launch in preview" : "Choose for Home";
        result.append(row);
    }
    if(repository_ && !sample_) {
        const auto recent=repository_->recentSessions();QHash<QString,int> order;
        for(int i=0;i<recent.size();++i)order.insert(recent[i].adventureId,i);
        std::stable_sort(result.begin(),result.end(),[&](const auto& a,const auto& b){return order.value(a.toMap()["id"].toString(),100000)<order.value(b.toMap()["id"].toString(),100000);});
    }
    return result;
}
void MultiversePresentation::select(const QString& id) {
    for (const auto& game : entries_) if (belongs(game) && game.id == id && game.linked) { selected_ = id; emit changed(); return; }
}
void MultiversePresentation::applySearch(const QString& text) {
    if (route_ != "games") return;
    queries_[system_] = text.trimmed().left(48); positions_[system_] = 0; emit gamesChanged(); emit changed();
}
void MultiversePresentation::activate(int index) {
    if (route_ == "systems") {
        if (index < 0 || index >= systems().size()) return;
        systemFocus_ = index; system_ = systems()[index].toMap()["id"].toString(); route_ = "games";
        emit gamesChanged();
    } else if (route_ == "games") {
        const auto list = filtered();
        if (list.isEmpty()) {
            if (!query().isEmpty() || filters_.value(system_)) { queries_[system_].clear(); filters_[system_] = 0; emit gamesChanged(); }
            else route_ = collection_=="multiverse"?"systems":"games";
        } else {
            if (index < 0 || index >= list.size()) return;
            positions_[system_] = index;
            if(sample_){if(list[index].linked){select(list[index].id);emit homeRequested();}} // Content-free UI rehearsal only.
            else if(repository_ && adapter_){
                const auto record=repository_->registration(list[index].id);
                if(record && !record->removed && record->contentAvailable && adapter_->capabilities(record->adventure).launch){
                    const auto result=adapter_->launch(record->adventure);if(!result.inProgress)emit messageRequested(result.message);
                }else emit setupRequested(list[index].id);
            }
        }
    }
    emit changed();
}
void MultiversePresentation::dispatch(Action action) {
    if (action == Action::Back) {
        if (route_ == "games" && collection_=="multiverse") route_ = "systems";
        emit changed(); return;
    }
    if (action == Action::Confirm) { activate(focusIndex()); return; }
    if (route_ == "games" && action == Action::Secondary) { emit searchRequested(query()); return; }
    if (route_ == "games" && action == Action::ToggleContinue) { filters_[system_] = (filters_.value(system_) + 1) % 3; positions_[system_] = 0; emit gamesChanged(); }
    if (route_ == "systems") {
        const int delta = action == Action::Left ? -1 : action == Action::Right ? 1 : action == Action::Up ? -3 : action == Action::Down ? 3 : 0;
        systemFocus_ = std::clamp(systemFocus_ + delta, 0, std::max(0, int(systems().size()) - 1));
    } else if (route_ == "games") {
        const int delta = action == Action::Up ? -1 : action == Action::Down ? 1 : 0;
        const int count=filtered().size();
        positions_[system_] = count ? (focusIndex() + delta + count) % count : 0;
    }
    emit changed();
}
}
