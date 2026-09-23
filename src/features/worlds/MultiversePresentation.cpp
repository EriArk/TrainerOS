#include "MultiversePresentation.h"
#include <algorithm>
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
    repository_ = &repository; adapter_ = &adapter; refresh();
}
void MultiversePresentation::refresh() {
    if (sample_ || !repository_) return;
    const auto focused = route_ == "systems" ? QString() : detail().value("id").toString();
    presentations_.clear(); systemsCache_.clear(); systemsCached_=false;
    entries_.clear();
    for (const auto& a : repository_->adventures()) if (a.domain == "multiverse" && !a.collectionOnly) {
        const auto r = repository_->registration(a.id);
        entries_.append({a.id,a.platformId,a.title,r && r->contentAvailable});
    }
    std::sort(entries_.begin(),entries_.end(),[](const auto& a,const auto& b) {
        const int cmp=QString::compare(a.title,b.title,Qt::CaseInsensitive);return cmp ? cmp<0 : a.id<b.id;
    });
    const auto platforms=systems(); bool found=false;
    for(int i=0;i<platforms.size();++i) if(platforms[i].toMap()["id"].toString()==system_) {systemFocus_=i;found=true;break;}
    if(!found) {route_="systems";systemFocus_=0;if(!platforms.isEmpty())system_=platforms.front().toMap()["id"].toString();}
    const auto list=filtered();
    for(int i=0;i<list.size();++i) if(list[i].id==focused) positions_[system_]=i;
    emit libraryChanged(); emit gamesChanged(); emit changed();
}
QJsonObject MultiversePresentation::navigationState() const {
    QJsonObject queries,filters,positions;
    for(auto i=queries_.cbegin();i!=queries_.cend();++i)queries[i.key()]=i.value();
    for(auto i=filters_.cbegin();i!=filters_.cend();++i)filters[i.key()]=i.value();
    for(auto i=positions_.cbegin();i!=positions_.cend();++i)positions[i.key()]=i.value();
    return {{"selected",selected_},{"system",system_},{"route",route_},{"queries",queries},{"filters",filters},{"positions",positions},
        {"focused",detail().value("id").toString()}};
}
void MultiversePresentation::restoreNavigation(const QJsonObject& state) {
    selected_=state["selected"].toString();system_=state["system"].toString();
    route_=state["route"].toString();if(route_=="detail")route_="games";
    if(!QStringList{"systems","games"}.contains(route_))route_="systems";
    queries_.clear();filters_.clear();positions_.clear();
    const auto queries=state["queries"].toObject(),filters=state["filters"].toObject(),positions=state["positions"].toObject();
    for(const auto& p:multiversePlatforms()) {
        if(queries.contains(p.id))queries_[p.id]=queries[p.id].toString().left(48);
        if(filters.contains(p.id))filters_[p.id]=std::clamp(filters[p.id].toInt(),0,2);
        if(positions.contains(p.id))positions_[p.id]=std::max(0,positions[p.id].toInt());
    }
    refresh();
    const auto list=filtered();for(int i=0;i<list.size();++i)if(list[i].id==state["focused"].toString() && (i || positions.contains(system_)))positions_[system_]=i;
    emit gamesChanged(); emit changed();
}
QVariantList MultiversePresentation::systems() const {
    if(systemsCached_)return systemsCache_;
    QVariantList result;
    if (!sample_) {
        auto platforms=multiversePlatforms();
        // Unknown imported categories remain usable, never silently discarded.
        for(const auto& g:entries_) if(std::none_of(platforms.begin(),platforms.end(),[&](const auto& p){return p.id==g.system;}))
            platforms.append({g.system,g.system,g.system,"console"});
        for(const auto& p:platforms) {
            const auto count=std::count_if(entries_.begin(),entries_.end(),[&](const auto& g){return g.system==p.id && g.linked;});
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
        {"description",description},{"preview",preview.isEmpty() && repository_ ? repository_->artwork(game.id).value("cover").toString() : preview},{"time",time},
        {"artwork",artwork},{"logo",artwork.value("marquee",artwork.value("wheel"))},{"screenshot",screenshot},
        {"year",year},{"genre",artwork.value("genre")},{"players",artwork.value("players")},
        {"developer",artwork.value("developer")},{"publisher",artwork.value("publisher")},
        {"action",playable?"Start Adventure":game.linked?"Set up Adventure":"File unavailable"},
        {"status",sample_ ? (game.linked?"Sample linked entry":"Sample missing file") : !game.linked?"File unavailable":playable?"Ready to play":"Needs setup"}};
    presentations_.insert(game.id,result);return result;
}
QList<MultiversePresentation::Game> MultiversePresentation::filtered() const {
    QList<Game> result;
    const int filter = filters_.value(system_);
    for (const auto& game : entries_) if (game.system == system_ && game.title.contains(query(), Qt::CaseInsensitive)
        && (filter != 1 || game.linked) && (filter != 2 || !game.linked)) result.append(game);
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
    for (const auto& game : entries_) if (game.id == selected_) return present(game);
    if(!selected_.isEmpty())return {{"id",selected_},{"title","Selected Adventure is unavailable"},{"linked",false},{"playable",false},{"action","Choose Adventure"}};
    if(repository_ && !sample_) for(const auto& session:repository_->recentSessions())
        for(const auto& game:entries_) if(game.id==session.adventureId)return present(game);
    return {};
}
QVariantList MultiversePresentation::choices() const {
    QVariantList result;
    for (const auto& game : entries_) if (game.linked) {
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
    for (const auto& game : entries_) if (game.id == id && game.linked) { selected_ = id; emit changed(); return; }
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
            else route_ = "systems";
        } else {
            if (index < 0 || index >= list.size()) return;
            positions_[system_] = index;
            if(list[index].linked) {select(list[index].id);emit homeRequested();}
        }
    }
    emit changed();
}
void MultiversePresentation::dispatch(Action action) {
    if (action == Action::Back) {
        if (route_ == "games") route_ = "systems";
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
        positions_[system_] = std::clamp(focusIndex() + delta,0,std::max(0,int(filtered().size())-1));
    }
    emit changed();
}
}
