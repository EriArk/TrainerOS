#include "WorldsController.h"
#include "core/repository/CollectionRepository.h"
#include "core/navigation/ResumePresentation.h"
#include <algorithm>
#include <QSet>
#include <QRandomGenerator>
#include <random>

namespace trainer {
namespace {
QString statusLabel(const std::optional<JourneyStatus>& status) {
    if (!status) return "Not recorded";
    switch (*status) {
    case JourneyStatus::NotStarted: return "Not started";
    case JourneyStatus::InProgress: return "In progress";
    case JourneyStatus::Completed: return "Completed";
    }
    return "Not recorded";
}
QString kindLabel(AdventureKind kind) {
    switch (kind) {
    case AdventureKind::Original: return "Original";
    case AdventureKind::Remake: return "Remake";
    case AdventureKind::RomHack: return "ROM hack";
    }
    return "Adventure";
}
QString countLabel(const std::optional<int>& number) { return number ? QString::number(*number) : "—"; }
QString searchKey(const QString& text) {
    QString result;
    for (const auto c : text.normalized(QString::NormalizationForm_D).toCaseFolded())
        if (c.category() != QChar::Mark_NonSpacing && c.category() != QChar::Mark_SpacingCombining && c.category() != QChar::Mark_Enclosing) result += c;
    return result.simplified();
}
}
WorldsController::WorldsController(LibraryRepository& repository, AdventureAdapter& adapter, QObject* parent)
    : QObject(parent), repository_(repository), adapter_(adapter), pairingSeed_(QRandomGenerator::global()->generate()) { refresh(); }
QString WorldsController::route() const {
    switch (route_) {
    case Route::Regions: return "regions";
    case Route::Adventures: return "adventures";
    case Route::Detail: return "detail";
    }
    return "regions";
}
int WorldsController::regionIndex() const {
    for (int i = 0; i < worlds_.size(); ++i) if (worlds_[i].id == worldId_) return i;
    return 0;
}
QList<Adventure> WorldsController::currentAdventures() const {
    QList<Adventure> result;
    const auto words = searchKey(query()).split(' ', Qt::SkipEmptyParts);
    const int filter = filters_.value(worldId_);
    for (const auto& adventure : adventures_) {
        if (adventure.worldId != worldId_ && !adventure.additionalWorldIds.contains(worldId_)) continue;
        if ((filter == 1 && adventure.collectionOnly) || (filter == 2 && !adventure.collectionOnly)) continue;
        const auto text = searchText_.value(adventure.id);
        if (std::all_of(words.cbegin(), words.cend(), [&](const auto& word) { return text.contains(word); })) result.append(adventure);
    }
    return result;
}
QString WorldsController::filterLabel() const {
    return QStringList{"All", "Linked", "Missing"}.value(filters_.value(worldId_), "All");
}
void WorldsController::updateFilter() {
    const auto entries = currentAdventures();
    const auto selected = rememberedAdventures_.value(worldId_);
    if (std::none_of(entries.cbegin(), entries.cend(), [&](const auto& a) { return a.id == selected; }))
        rememberedAdventures_[worldId_] = entries.isEmpty() ? QString() : entries.first().id;
    backFocused_ = entries.isEmpty();
    emit contentChanged();
    emit changed();
}
void WorldsController::applySearch(const QString& text) {
    if (route_ != Route::Adventures) return;
    queries_[worldId_] = text.left(48).simplified();
    updateFilter();
}
int WorldsController::adventureIndex() const {
    const auto entries = currentAdventures();
    const auto remembered = rememberedAdventures_.value(worldId_);
    for (int i = 0; i < entries.size(); ++i) if (entries[i].id == remembered) return i;
    return 0;
}
int WorldsController::focusIndex() const {
    if (route_ == Route::Regions) return regionIndex();
    if (route_ == Route::Detail) return actionFocus_;
    return backFocused_ ? currentAdventures().size() : adventureIndex();
}
QVariantList WorldsController::regions() const {
    QVariantList result;
    for (const auto& world : worlds_) {
        const auto count = std::count_if(adventures_.begin(), adventures_.end(), [&](const auto& a) { return a.worldId == world.id || a.additionalWorldIds.contains(world.id); });
        result.append(QVariantMap{{"id", world.id}, {"name", world.name}, {"count", int(count)},
                                  {"owned", int(std::count_if(adventures_.begin(), adventures_.end(), [&](const auto& a) { return !a.collectionOnly && (a.worldId == world.id || a.additionalWorldIds.contains(world.id)); }))},
                                  {"status", statusLabel(world.status)}});
    }
    return result;
}
QList<QList<int>> WorldsController::regionGroups() const {
    // Stable for this controller session, including refresh and page/face changes.
    // Each half is a complete World presentation, never a pre-composed pair asset.
    static const QSet<QString> major{"kanto","johto","hoenn","sinnoh","unova","kalos","alola","galar","paldea","hisui"};
    QSet<QString> known;
    for(const auto& world:collectionWorlds())known.insert(world.id);
    const auto small=[&](const QString& id){
        QSet<QString> titles;
        for(const auto& a:adventures_)if(a.worldId==id || a.additionalWorldIds.contains(id))
            titles.insert(a.catalogueId.isEmpty()?a.id:a.catalogueId);
        return titles.size()<=4;
    };
    QStringList candidates;
    for(const auto& world:worlds_)if(known.contains(world.id) && !major.contains(world.id) && small(world.id))candidates.append(world.id);
    candidates.sort();
    std::mt19937 random(pairingSeed_);
    std::shuffle(candidates.begin(),candidates.end(),random);
    QHash<QString,QString> pairs;
    for(int i=0;i+1<candidates.size();i+=2){pairs.insert(candidates[i],candidates[i+1]);pairs.insert(candidates[i+1],candidates[i]);}
    QList<QList<int>> result;QSet<int> used;
    for(int i=0;i<worlds_.size();++i) {
        if(used.contains(i))continue;
        QList<int> group{i};used.insert(i);
        const auto partner=pairs.value(worlds_[i].id);
        if(!partner.isEmpty() && small(worlds_[i].id) && small(partner))
            for(int j=i+1;j<worlds_.size();++j)if(!used.contains(j) && worlds_[j].id==partner){group.append(j);used.insert(j);break;}
        result.append(group);
    }
    return result;
}
int WorldsController::regionTileIndex() const {
    const auto groups=regionGroups();
    for(int i=0;i<groups.size();++i)if(groups[i].contains(regionIndex()))return i;
    return 0;
}
QVariantList WorldsController::regionTiles() const {
    const auto entries=regions();QVariantList result;
    for(const auto& group:regionGroups()) {
        QVariantList members;
        for(const int index:group){auto item=entries[index].toMap();item["index"]=index;members.append(item);}
        result.append(QVariantMap{{"members",members}});
    }
    return result;
}
QVariantList WorldsController::adventures() const {
    QVariantList result;
    for (const auto& adventure : currentAdventures()) {
        const auto art=repository_.artwork(adventure.id);
        result.append(QVariantMap{{"id", adventure.id}, {"title", adventure.title},
            {"logo",art.value("marquee",art.value("wheel"))},
            {"kind", kindLabel(adventure.kind)}, {"status", adventure.collectionOnly ? "Missing from collection" : statusLabel(adventure.status)},
            {"missing", adventure.collectionOnly}, {"platform", platformLabel(adventure.platformId).badge},
            {"platformShape", platformLabel(adventure.platformId).shape}, {"variant", adventure.variant}});
    }
    return result;
}
QVariantMap WorldsController::region() const {
    if (worlds_.isEmpty()) return {{"name", "Worlds"}, {"count", 0}};
    const auto& world = worlds_.at(regionIndex());
    return {{"id", world.id}, {"name", world.name}, {"count", int(currentAdventures().size())},
            {"status", statusLabel(world.status)}};
}
std::optional<Adventure> WorldsController::currentAdventure() const {
    const auto entries = currentAdventures();
    if (entries.isEmpty()) return {};
    return entries.at(adventureIndex());
}
std::optional<ResumePoint> WorldsController::latestResume(const Adventure& adventure) const {
    std::optional<ResumePoint> result;
    for (const auto& point : resumePoints_) {
        if (point.adventureId != adventure.id || point.id.isEmpty()) continue;
        if (!result || (point.savedAt.isValid() && (!result->savedAt.isValid() || point.savedAt > result->savedAt))) result = point;
    }
    return result;
}
QVariantMap WorldsController::detail() const {
    const auto adventure = currentAdventure();
    if (!adventure) return {{"title", "No matching Adventures"}, {"kind", ""},
        {"status", ""}, {"description", "Your Adventures for this World will appear here."},
        {"badges", "—"}, {"caught", "—"}, {"availability", "Try another search, filter or World."},
        {"resume", "No recent trail recorded"}};
    const auto caps = adapter_.capabilities(*adventure);
    const auto point = latestResume(*adventure);
    const bool canResume = point && adapter_.resumeAvailability(*adventure, *point) == ResumeAvailability::Exact;
    QString availability = "Open the Adventure and choose your save there.";
    if (canResume) availability = "A recent trail is ready to continue.";
    else if (!caps.launch) availability = "The file is linked. Play setup is still needed.";
    if (adventure->collectionOnly) availability = "Missing from your collection. Link a local file to add this edition.";
    const auto art=repository_.artwork(adventure->id);
    auto year=art.value("releasedate").toString().left(4);
    bool numeric=false;const auto number=year.toInt(&numeric);
    if(!numeric || number<1970 || number>2100) {
        year.clear();
        static const auto years=[] { QHash<QString,int> result;for(const auto& e:collectionChronology())if(e.releaseYear)result.insert(e.id,*e.releaseYear);return result; }();
        if(years.contains(adventure->catalogueId))year=QString::number(years.value(adventure->catalogueId));
    }
    QString screenshot;
    for(const auto& field:QStringList{"screenshot","image","titleshot","thumbnail","cover"})
        if(!art.value(field).toString().isEmpty()){screenshot=art.value(field).toString();break;}
    return {{"id", adventure->id}, {"title", adventure->title}, {"kind", kindLabel(adventure->kind)},
        {"system",platformLabel(adventure->platformId).name},{"year",year},{"screenshot",screenshot},
        {"platformShort",platformLabel(adventure->platformId).badge},{"platformShape",platformLabel(adventure->platformId).shape},
        {"genre",art.value("genre")},{"players",art.value("players")},{"developer",art.value("developer")},{"publisher",art.value("publisher")},{"synopsis",art.value("desc")},
        {"playable",caps.launch},{"status", adventure->collectionOnly ? "File unavailable" : statusLabel(adventure->status)},
        {"description",art.value("desc").toString().isEmpty()?adventure->description:art.value("desc").toString()},
        {"platform", platformLabel(adventure->platformId).name}, {"limitation", adventure->limitation}, {"variant", adventure->variant},
        {"badges", countLabel(adventure->badges)}, {"caught", countLabel(adventure->caught)},
        {"availability", availability}, {"artwork", art},
        {"resume", point ? (canResume ? (point->location.isEmpty() ? "Recent trail" : point->location) : resumeLabel(adapter_.resumeAvailability(*adventure, *point))) : "No recent trail recorded"}};
}
QList<WorldsController::DetailAction> WorldsController::detailActions() const {
    QList<DetailAction> result;
    if (const auto adventure = currentAdventure()) {
        const auto caps = adapter_.capabilities(*adventure);
        if (const auto point = latestResume(*adventure); point && caps.directResume)
            result.append({"resume", "Resume Adventure", adapter_.resumeAvailability(*adventure, *point) == ResumeAvailability::Exact});
        if (!adventure->collectionOnly) result.append({"launch", caps.launch ? "Start Adventure" : "Needs setup", caps.launch});
        if (repository_.editable()) result.append({"setup", adventure->collectionOnly ? "Link a file" : "Edit / change file", true});
    }
    result.append({"back", "Back to Adventures", true});
    return result;
}
QVariantList WorldsController::actions() const {
    QVariantList result;
    for (const auto& action : detailActions())
        result.append(QVariantMap{{"id", action.id}, {"label", action.label}, {"enabled", action.enabled}});
    return result;
}
void WorldsController::normalizeActionFocus() {
    const auto available = detailActions();
    if (actionFocus_ >= 0 && actionFocus_ < available.size() && available[actionFocus_].enabled) return;
    for (int i = 0; i < available.size(); ++i) if (available[i].enabled) { actionFocus_ = i; return; }
}
void WorldsController::refresh() {
    const auto oldWorld = worldId_;
    const auto oldAdventure = rememberedAdventures_.value(worldId_);
    worlds_ = repository_.worlds();
    adventures_ = repository_.adventures();
    adventures_.erase(std::remove_if(adventures_.begin(),adventures_.end(),[](const auto& a){return a.domain!="pokemon";}),adventures_.end());
    resumePoints_ = repository_.resumePoints();
    searchText_.clear();
    for (const auto& a : adventures_) {
        const auto platform = platformLabel(a.platformId);
        searchText_.insert(a.id, searchKey(a.title + ' ' + a.variant + ' ' + platform.name + ' ' + platform.badge));
    }
    const bool worldExists = std::any_of(worlds_.begin(), worlds_.end(), [&](const auto& w) { return w.id == worldId_; });
    if (!worldExists) {
        worldId_ = worlds_.isEmpty() ? QString() : worlds_.first().id;
        route_ = Route::Regions;
    }
    const auto entries = currentAdventures();
    const bool adventureExists = std::any_of(entries.begin(), entries.end(), [&](const auto& a) { return a.id == oldAdventure; });
    if (!adventureExists) {
        rememberedAdventures_[worldId_] = entries.isEmpty() ? QString() : entries.first().id;
        if (route_ == Route::Detail) route_ = Route::Adventures;
    }
    if (oldWorld != worldId_) backFocused_ = false;
    if (entries.isEmpty()) backFocused_ = true;
    normalizeActionFocus();
    emit contentChanged();
    emit changed();
}
QJsonObject WorldsController::navigationState() const {
    QJsonObject remembered;
    for (auto i = rememberedAdventures_.cbegin(); i != rememberedAdventures_.cend(); ++i) remembered.insert(i.key(), i.value());
    QJsonObject browsing;
    for (const auto& world : worlds_) if (!queries_.value(world.id).isEmpty() || filters_.value(world.id) != 0)
        browsing.insert(world.id, QJsonObject{{"query", queries_.value(world.id)}, {"filter", filters_.value(world.id)}});
    const auto available = detailActions();
    return {{"world", worldId_}, {"adventures", remembered}, {"browsing", browsing}, {"route", route()}, {"back", backFocused_},
            {"action", actionFocus_ >= 0 && actionFocus_ < available.size() ? available[actionFocus_].id : QString()}};
}
void WorldsController::restoreNavigation(const QJsonObject& state) {
    worldId_ = state["world"].toString();
    queries_.clear(); filters_.clear();
    const auto browsing = state["browsing"].toObject();
    for (const auto& world : repository_.worlds()) {
        const auto value = browsing[world.id].toObject();
        queries_[world.id] = value["query"].toString().left(48).simplified();
        filters_[world.id] = std::clamp(value["filter"].toInt(), 0, 2);
    }
    rememberedAdventures_.clear();
    const auto remembered = state["adventures"].toObject();
    // Accept only existing relationships, not arbitrary stale IDs or row offsets.
    for (const auto& adventure : repository_.adventures()) {
        const auto relationships = QStringList{adventure.worldId} + adventure.additionalWorldIds;
        for (const auto& world : relationships) if (remembered[world].toString() == adventure.id) rememberedAdventures_[world] = adventure.id;
    }
    const auto route = state["route"].toString();
    route_ = route == "detail" ? Route::Detail : route == "adventures" ? Route::Adventures : Route::Regions;
    backFocused_ = state["back"].toBool(); actionFocus_ = 0;
    refresh();
    const auto available = detailActions();
    for (int i = 0; i < available.size(); ++i) if (available[i].id == state["action"].toString()) actionFocus_ = i;
    normalizeActionFocus();
    emit changed();
}
void WorldsController::chooseAdventure(int index) {
    const auto entries = currentAdventures();
    if (index < 0 || index >= entries.size()) return;
    rememberedAdventures_[worldId_] = entries[index].id;
    backFocused_ = false;
}
void WorldsController::openRegion() {
    if (worlds_.isEmpty()) { emit homeRequested(); return; }
    route_ = Route::Adventures;
    chooseAdventure(adventureIndex());
    backFocused_ = currentAdventures().isEmpty();
    emit contentChanged();
}
void WorldsController::openDetail() {
    if (!currentAdventure()) return;
    repository_.refreshResumePoints(currentAdventure()->id);
    route_ = Route::Detail;
    actionFocus_ = 0;
    normalizeActionFocus();
}
void WorldsController::back() {
    if (route_ == Route::Detail) { route_ = Route::Adventures; backFocused_ = false; }
    else if (route_ == Route::Adventures) { route_ = Route::Regions; backFocused_ = false; }
}
void WorldsController::executeAction(int index) {
    const auto available = detailActions();
    if (index < 0 || index >= available.size() || !available[index].enabled) return;
    actionFocus_ = index;
    if (available[index].id == "back") { back(); return; }
    // Re-read before acting: a cached row must not launch a removed/moved record.
    const auto id = rememberedAdventures_.value(worldId_);
    const auto current = repository_.adventures();
    const auto found = std::find_if(current.begin(), current.end(), [&](const auto& a) { return a.id == id && (a.worldId == worldId_ || a.additionalWorldIds.contains(worldId_)); });
    if (found == current.end()) {
        refresh();
        emit messageRequested("This Adventure is no longer available in this World. Choose another trail.");
        return;
    }
    const auto caps = adapter_.capabilities(*found);
    if (available[index].id == "setup") { emit setupRequested(found->id); return; }
    AdventureResult result{false, "This action is no longer available. Your Adventure record has been kept."};
    if (available[index].id == "launch" && caps.launch) result = adapter_.launch(*found);
    else if (available[index].id == "resume" && caps.directResume) {
        if (const auto selected = latestResume(*found)) {
            const auto currentPoints = repository_.resumePoints();
            const auto point = std::find_if(currentPoints.cbegin(), currentPoints.cend(), [&](const auto& p) {
                return p.id == selected->id && p.adventureId == found->id && p.source == selected->source
                    && adapter_.resumeAvailability(*found, p) == ResumeAvailability::Exact;
            });
            if (point != currentPoints.cend()) result = adapter_.resume(*found, *point);
            else {
                refresh();
                result.message = "That saved moment changed. Review the updated Adventure before continuing.";
            }
        }
    }
    if (!result.success || !result.inProgress)
        emit messageRequested(result.message.isEmpty() ? (result.success ? "Adventure request complete." : "Couldn't open this Adventure. Try again.") : result.message);
}
void WorldsController::activate(int index) {
    if (route_ == Route::Regions) {
        if (!worlds_.isEmpty()) {
            if (index < 0 || index >= worlds_.size()) return;
            worldId_ = worlds_[index].id;
        }
        openRegion();
    } else if (route_ == Route::Adventures) {
        const auto count = currentAdventures().size();
        if (index == count) back();
        else if (index >= 0 && index < count) { chooseAdventure(index); openDetail(); }
    } else executeAction(index);
    emit changed();
}
void WorldsController::dispatch(Action action) {
    if (route_ == Route::Adventures && action == Action::Secondary) { emit searchRequested(query()); return; }
    if (route_ == Route::Adventures && action == Action::ToggleContinue) {
        filters_[worldId_] = (filters_.value(worldId_) + 1) % 3;
        updateFilter(); return;
    }
    if (action == Action::Confirm) { activate(focusIndex()); return; }
    if (action == Action::Back) { back(); emit changed(); return; }
    if (route_ == Route::Regions && !worlds_.isEmpty()) {
        const auto groups=regionGroups();int tile=regionTileIndex(),part=groups[tile].indexOf(regionIndex());
        if(action==Action::Left && tile%3>0)--tile;
        if(action==Action::Right && tile%3<2 && tile+1<groups.size())++tile;
        if(action==Action::Up) {if(part>0)--part;else if(tile>=3){tile-=3;part=groups[tile].size()-1;}}
        if(action==Action::Down) {if(part+1<groups[tile].size())++part;else if(tile+3<groups.size()){tile+=3;part=0;}}
        worldId_ = worlds_[groups[tile][std::min(part,int(groups[tile].size())-1)]].id;
    } else if (route_ == Route::Adventures) {
        const auto count = currentAdventures().size();
        const int index = adventureIndex();
        if ((action == Action::Left || action == Action::Right) && count > 0)
            chooseAdventure(std::clamp(index + (action == Action::Right ? 8 : -8), 0, int(count) - 1));
        if (action == Action::Up && count > 0) {
            if (backFocused_) backFocused_ = false;
            else chooseAdventure((index + count - 1) % count);
        }
        if (action == Action::Down && !backFocused_) {
            if (count > 0) chooseAdventure((index + 1) % count);
        }
    } else if (route_ == Route::Detail) {
        normalizeActionFocus();
        const auto available = detailActions();
        const int delta = action == Action::Left ? -1 : action == Action::Right ? 1 : 0;
        for (int i = actionFocus_ + delta; delta != 0 && i >= 0 && i < available.size(); i += delta)
            if (available[i].enabled) { actionFocus_ = i; break; }
    }
    emit changed();
}
}
