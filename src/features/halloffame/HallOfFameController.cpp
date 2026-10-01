#include "HallOfFameController.h"
#include <algorithm>

namespace trainer {
QVariantMap HallOfFameController::championPreview() const {
    const auto records=championRecords();
    if(!records.isEmpty()) {
        int index=0;for(int i=0;i<records.size();++i)if(records[i].id==championId_)index=i;
        const auto& r=records[index];QVariantList team;
        for(const auto& m:r.team)team.append(QVariantMap{{"name",m.name},{"nickname",m.nickname},{"number",m.number},
            {"speciesId",m.speciesId},{"level",QString("Lv. %1").arg(m.level)},{"shiny",m.shiny},{"art",art_?art_->image(m.speciesId+'/'+QString::number(m.number),"pokedexDetailArt"):QVariantMap{}}});
        return {{"title",QString("Hoenn Champion #%1").arg(r.victory)},{"team",team},{"sample",false},
            {"position",QString("%1 / %2").arg(index+1).arg(records.size())},
            {"observed","Added "+r.observedAt.toLocalTime().toString("dd MMM yyyy")},{"victory",QString()}};
    }
    if (!sampleJourney_) return {};
    return {{"title", "Sample Hoenn run"}, {"build", "Development fixture · independent historical snapshot"},
        {"victory", "Victory date unknown"}, {"observed", "Observed 20 Sep 2026 · not the victory date"},
        {"team", QVariantList{QVariantMap{{"name", "Sceptile"}, {"level", "Lv. 52"}},
            QVariantMap{{"name", "Gardevoir"}, {"level", "Level unknown"}},
            QVariantMap{{"name", "Not recorded"}, {"level", "—"}}}}};
}
QList<ChampionRecord> HallOfFameController::championRecords() const {
    QList<ChampionRecord> out;for(const auto& r:repository_.champions())if(r.adventureId==currentAdventure_)out.append(r);return out;
}
QVariantMap HallOfFameController::journey() const {
    QVariantMap out{{"time",QString::fromUtf8("—")},{"seen",QString::fromUtf8("—")},{"milestones",QVariantList{}},
        {"championCount",championRecords().size()},{"error",QString()}};
    if(progress_.availability!=ProgressAvailability::Available)return out;
    if(progress_.pokedex && progress_.pokedex->error.isEmpty())out["seen"]=QString::number(progress_.pokedex->seen.size());
    if(progress_.journey) {
        const auto& j=*progress_.journey;
        if(j.playtimeMinutes)out["time"]=QString("%1h %2m").arg(*j.playtimeMinutes/60).arg(*j.playtimeMinutes%60,2,10,QChar('0'));
        QVariantList milestones;
        for(const auto& m:j.milestones)milestones.append(QVariantMap{{"title",m.title},{"known",m.achieved.has_value()},{"earned",m.achieved.value_or(false)}});
        out["milestones"]=milestones;out["error"]=j.championError;
    }
    return out;
}
void HallOfFameController::setProgress(const QString& adventure,const GameProgress& progress) {
    progress_=adventure==currentAdventure_?progress:GameProgress{};
    if(progress_.availability==ProgressAvailability::Available && progress_.journey && repository_.archiveEditable()
        && !currentAdventure_.isEmpty() && !progress_.contentRevision.isEmpty() && !progress_.saveRevision.isEmpty()) {
        QList<ChampionRecord> candidates;const auto existing=repository_.champions();
        for(auto record:progress_.journey->champions) {
            record.adventureId=currentAdventure_;record.build=progress_.contentRevision;record.saveRevision=progress_.saveRevision;
            record.observedAt=progress_.observedAt;
            const auto key=record.adventureId+':'+record.build+':'+record.id;
            const bool found=std::any_of(existing.begin(),existing.end(),[&](const auto& r){return r.adventureId==record.adventureId && r.build==record.build && r.id==record.id;});
            if(!found && !preserving_.contains(key)){candidates.append(record);preserving_.insert(key);}
        }
        if(!candidates.isEmpty())repository_.preserveChampionsAsync(candidates,this,[this,candidates](const QString& error){
            for(const auto& r:candidates)preserving_.remove(r.adventureId+':'+r.build+':'+r.id);
            if(!error.isEmpty())emit messageRequested(error);
            emit changed();
        });
    }
    emit changed();
}

namespace {
QString dateLabel(const QDateTime& date) { return date.isValid() ? date.toUTC().toString("dd MMM yyyy · HH:mm 'UTC'") : "Date not recorded"; }
QString memoryDate(const QDateTime& date) { return date.isValid() ? date.toUTC().toString("dd MMM yyyy") : "Date not recorded"; }
QString modeLabel(const std::optional<AchievementMode>& mode) {
    if (!mode) return "Mode not recorded";
    return *mode == AchievementMode::Hardcore ? "Hardcore" : "Standard";
}
AchievementUnlock unlockFor(const AchievementSnapshot& snapshot, const QString& id) {
    for (const auto& unlock : snapshot.unlocks) if (unlock.achievementId == id) return unlock;
    return {id, {}, {}, {}};
}
QString unlockLabel(const AchievementUnlock& unlock) {
    if (!unlock.unlocked) return "Not recorded";
    return *unlock.unlocked ? "Unlocked · " + modeLabel(unlock.mode) : "Locked";
}
QString snapshotStatus(const AchievementSnapshot& snapshot) {
    const bool cached = !snapshot.definitions.isEmpty();
    switch (snapshot.state) {
    case AchievementState::Disconnected: return "No account connected";
    case AchievementState::Unsupported: return "No supported set confirmed";
    case AchievementState::Loading: return cached ? "Refreshing · saved records remain available" : "Loading achievement records…";
    case AchievementState::Ready: return snapshot.context.providerId.endsWith("-mock") ? "RetroAchievements · sample records" : "RetroAchievements · confirmed records";
    case AchievementState::Offline: return cached ? "Offline · showing saved records" : "Offline · no saved records";
    case AchievementState::Error: return cached ? "Refresh failed · showing saved records" : "Achievements could not be loaded";
    }
    return "Achievements unavailable";
}
}
HallOfFameController::HallOfFameController(HallOfFameRepository& repository, AchievementProvider& provider, QObject* parent)
    : QObject(parent), repository_(repository), provider_(provider), editor_(repository, this), account_(provider, this) {
    connect(&account_, &AchievementAccountController::changed, this, &HallOfFameController::changed);
    connect(&editor_, &ArchiveEditor::changed, this, &HallOfFameController::changed);
    connect(&editor_, &ArchiveEditor::messageRequested, this, &HallOfFameController::messageRequested);
    connect(&editor_, &ArchiveEditor::saved, this, [this](const QString& id) {
        archiveId_ = id; archiveView_ = {"archive-detail", "actions"};
        if (isArchive()) { route_ = "archive-detail"; zone_ = "actions"; actionFocus_ = 0; }
        refreshArchive();
    });
    const auto sets = provider_.sets();
    if (!sets.isEmpty()) setId_ = sets.first().id;
    connect(&provider_, &AchievementProvider::snapshotChanged, this, [this] { reconcile(); });
    refreshArchive();
}
AchievementSet HallOfFameController::selectedSet() const {
    for (const auto& set : provider_.sets()) if (set.id == setId_) return set;
    return {};
}
AchievementSnapshot HallOfFameController::checkedSnapshot(const AchievementSet& set) const {
    const auto context = provider_.context();
    AchievementSnapshot unavailable{context, set.id, AchievementState::Disconnected, {}, {}, {}};
    if (!set.supported) { unavailable.state = AchievementState::Unsupported; return unavailable; }
    if (context.accountId.isEmpty()) return unavailable;
    const auto snapshot = provider_.snapshot(set.id);
    if (snapshot.context != context || snapshot.setId != set.id) {
        unavailable.state = AchievementState::Error;
        return unavailable;
    }
    if (snapshot.state == AchievementState::Disconnected || snapshot.state == AchievementState::Unsupported) {
        unavailable.state = snapshot.state;
        return unavailable; // Never expose stale account records in these states.
    }
    return snapshot;
}
AchievementSnapshot HallOfFameController::currentSnapshot() const { return checkedSnapshot(selectedSet()); }
QString HallOfFameController::selectedRowId() const {
    if (isArchive()) return archiveId_;
    if (route_ == "sets") return setId_;
    return achievementIds_.value(setId_);
}
void HallOfFameController::selectRow(const QString& id) {
    if (isArchive()) archiveId_ = id;
    else if (route_ == "sets") setId_ = id;
    else achievementIds_[setId_] = id;
}
QList<HallOfFameController::Row> HallOfFameController::currentRows() const {
    QList<Row> result;
    if (isArchive()) {
        for (const auto& entry : archive_) result.append({entry.id, entry.adventureTitle, entry.world + " · " + memoryDate(entry.completedAt)});
    } else if (route_ == "sets") {
        for (const auto& set : provider_.sets()) result.append({set.id, set.title, set.world + " · " + snapshotStatus(checkedSnapshot(set))});
    } else {
        const auto snapshot = currentSnapshot();
        for (const auto& definition : snapshot.definitions)
            result.append({definition.id, definition.title, unlockLabel(unlockFor(snapshot, definition.id))});
    }
    return result;
}
QVariantList HallOfFameController::rows() const {
    QVariantList result;
    for (const auto& row : currentRows()) {
        QVariantMap item{{"id", row.id}, {"title", row.title}, {"subtitle", row.subtitle}};
        if (!isArchive() && route_ != "sets") {
            const auto unlock = unlockFor(currentSnapshot(), row.id);
            item["tint"] = !unlock.unlocked ? "#d9cfdf" : *unlock.unlocked ? "#f5d886" : "#c8d3d0";
            item["earnedState"] = !unlock.unlocked ? "unknown" : *unlock.unlocked ? "earned" : "locked";
        }
        result.append(item);
    }
    return result;
}
int HallOfFameController::rowIndex() const {
    const auto rows = currentRows();
    for (int i = 0; i < rows.size(); ++i) if (rows[i].id == selectedRowId()) return i;
    return 0;
}
int HallOfFameController::focusIndex() const { return account_.isOpen() ? account_.focusIndex() : zone_ == "actions" ? actionFocus_ : rowIndex(); }
QVariantMap HallOfFameController::detail() const {
    if (isArchive()) {
        for (const auto& entry : archive_) if (entry.id == archiveId_) {
            const auto time = entry.playtimeMinutes ? QString("%1h %2m").arg(*entry.playtimeMinutes / 60).arg(*entry.playtimeMinutes % 60) : "Time not recorded";
            return {{"title", entry.adventureTitle}, {"world", entry.world}, {"date", memoryDate(entry.completedAt)},
                {"time", time}, {"description", entry.notes}, {"source", entry.source == ArchiveSource::Manual ? "Manually recorded" : "Imported record"},
                {"summary", entry.world + " · " + time}};
        }
        return {{"title", "Your journeys belong here"}, {"world", ""}, {"date", "Date not recorded"},
            {"time", "Time not recorded"}, {"description", "Completed Adventures will become part of your archive."},
            {"source", "Local archive"}, {"summary", "A place for your memories"}};
    }
    const auto set = selectedSet();
    const auto snapshot = currentSnapshot();
    int unlocked = 0, unknown = 0;
    for (const auto& definition : snapshot.definitions) {
        const auto record = unlockFor(snapshot, definition.id);
        if (!record.unlocked) ++unknown;
        else if (*record.unlocked) ++unlocked;
    }
    const bool sample = provider_.context().providerId.endsWith("-mock");
    QVariantMap result{{"title", set.title.isEmpty() ? "RetroAchievements" : set.title}, {"world", set.world}, {"source", sample ? "RetroAchievements · fictional sample" : "RetroAchievements"},
        {"summary", snapshot.definitions.isEmpty() ? "Unlocks unavailable" : unknown == snapshot.definitions.size()
            ? "Unlock status not recorded" : QString("%1 unlocked · %2 not recorded").arg(unlocked).arg(unknown)},
        {"description", sample ? "Sample achievement sets demonstrate browsing. No real account or game coverage is claimed."
            : "Confirmed file matches and account unlocks. These records remain separate from current-save progress."},
        {"date", "Saved: " + dateLabel(snapshot.fetchedAt)}, {"time", ""}};
    if (route_ != "sets") for (const auto& definition : snapshot.definitions) if (definition.id == achievementIds_.value(setId_)) {
        const auto unlock = unlockFor(snapshot, definition.id);
        result["title"] = definition.title;
        result["world"] = set.title;
        result["description"] = definition.description;
        result["summary"] = unlockLabel(unlock);
        result["earnedState"] = !unlock.unlocked ? "unknown" : *unlock.unlocked ? "earned" : "locked";
        result["time"] = unlock.unlocked.value_or(false) ? "Earned: " + dateLabel(unlock.earnedAt) : "Unlock date unavailable";
    }
    return result;
}
QVariantList HallOfFameController::team() const {
    QList<HallOfFameMember> members;
    for (const auto& entry : archive_) if (entry.id == archiveId_) members = entry.team;
    QVariantList result;
    for (int i = 0; i < 6; ++i) {
        const bool known = i < members.size() && !members[i].name.isEmpty();
        result.append(QVariantMap{{"name", known ? members[i].name : "Not recorded"},
            {"level", known && members[i].level ? "Lv " + QString::number(*members[i].level) : "Lv —"}, {"known", known}});
    }
    return result;
}
QString HallOfFameController::status() const {
    if (isArchive()) return archiveError_.isEmpty() ? QString("Local archive · %1 memories").arg(archive_.size()) : "Archive refresh failed · saved records kept";
    if (provider_.context().accountId.isEmpty()) return "No account connected";
    return snapshotStatus(currentSnapshot());
}
QString HallOfFameController::emptyMessage() const {
    if (isArchive()) return archiveError_.isEmpty() ? "Your first memory starts here. Record a completed Adventure." : archiveError_;
    if (route_ == "sets") return provider_.context().accountId.isEmpty()
        ? "Connect your RetroAchievements account. Your local archive remains available."
        : "Play a supported Adventure to start your achievement collection. Your local archive remains available.";
    switch (currentSnapshot().state) {
    case AchievementState::Disconnected: return "No account is connected. Your local Hall of Fame remains available.";
    case AchievementState::Unsupported: return "No supported achievement set has been confirmed. This Adventure can still have its own archive memories.";
    case AchievementState::Loading: return "Loading records. You can return to Adventures or the archive while this finishes.";
    case AchievementState::Offline: return "Offline, with no saved achievement records. Return to your archive or retry later.";
    case AchievementState::Error: return "Achievement records couldn't be loaded. Retry or return to your archive.";
    case AchievementState::Ready: return "This set has no core achievement definitions. No unlock total is available.";
    }
    return {};
}
QVariantList HallOfFameController::actions() const {
    if (overview()) return {QVariantMap{{"label", route_ == "archive-journey" ? "Champion records" : "Journey Record"}, {"enabled", true}}};
    if (route_ == "archive-list") return {QVariantMap{{"label", "Refresh archive"}, {"enabled", true}}};
    if (route_ == "archive-detail") return {QVariantMap{{"label", "Back to archive"}, {"enabled", true}}};
    if (route_ == "sets") return {QVariantMap{{"label", "Refresh records"}, {"enabled", true}}};
    const auto state = currentSnapshot().state;
    const bool refresh = !setId_.isEmpty() && selectedSet().supported && !provider_.context().accountId.isEmpty() && state != AchievementState::Loading;
    return {QVariantMap{{"label", route_ == "achievement-detail" ? "Back to achievements" : "Back to Adventures"}, {"enabled", true}},
        QVariantMap{{"label", state == AchievementState::Loading ? "Refreshing…" : "Refresh records"}, {"enabled", refresh}}};
}
void HallOfFameController::normalizeActions() {
    const auto available = actions();
    if (actionFocus_ < 0 || actionFocus_ >= available.size() || !available[actionFocus_].toMap()["enabled"].toBool()) actionFocus_ = 0;
}
void HallOfFameController::setCurrentAdventure(const QString& id) {
    if(currentAdventure_==id)return;
    currentAdventure_=id;progress_={};preferCurrent_=true;reconcile();
}
void HallOfFameController::reconcile() {
    const auto sets = provider_.sets();
    if(preferCurrent_)for(const auto& set:sets)if(set.adventureId==currentAdventure_){
        setId_=set.id;preferCurrent_=false;achievementView_={"sets","list"};
        if(!isArchive()){route_="sets";zone_="list";actionFocus_=0;}break;
    }
    const bool exists = std::any_of(sets.begin(), sets.end(), [&](const auto& set) { return set.id == setId_; });
    if (!exists) {
        setId_ = sets.isEmpty() ? QString() : sets.first().id;
        achievementView_ = {"sets", "list"};
        if (!isArchive()) { route_ = "sets"; zone_ = "list"; actionFocus_ = 0; }
    }
    const auto rows = currentRows();
    const bool selected = std::any_of(rows.begin(), rows.end(), [&](const auto& row) { return row.id == selectedRowId(); });
    if (!selected) {
        selectRow(rows.isEmpty() ? QString() : rows.first().id);
        if (isDetail()) { route_ = isArchive() ? "archive-list" : "achievements"; zone_ = rows.isEmpty() ? "actions" : "list"; }
    }
    if (rows.isEmpty() && zone_ == "list") zone_ = "actions";
    normalizeActions();
    emit rowsChanged(); emit changed();
}
void HallOfFameController::refreshArchive() {
    const auto result = repository_.loadArchive();
    archiveError_ = result.success ? QString() : result.error;
    if (!result.success && archiveError_.isEmpty()) archiveError_ = "Your archive couldn't be loaded. Try again.";
    if (result.success) {
        archive_ = result.entries;
        std::stable_sort(archive_.begin(), archive_.end(), [](const auto& a, const auto& b) {
            if (a.completedAt.isValid() != b.completedAt.isValid()) return a.completedAt.isValid();
            return a.completedAt > b.completedAt;
        });
    }
    reconcile();
    if (!result.success && !archive_.isEmpty()) emit messageRequested(archiveError_);
}
void HallOfFameController::switchFace() {
    if (editor_.isOpen() || account_.isOpen()) return;
    const bool fromArchive = isArchive();
    (fromArchive ? archiveView_ : achievementView_) = {route_, zone_, actionFocus_};
    const auto view = fromArchive ? achievementView_ : archiveView_;
    route_ = view.route; zone_ = view.zone; actionFocus_ = view.action;
    reconcile(); // Missing rows and changed account data must never leave hidden focus.
}
void HallOfFameController::cycleFace(int delta) {
    showFace((faceIndex()+(delta<0?2:1))%3);
}
void HallOfFameController::showFace(int index) {
    if(editor_.isOpen() || account_.isOpen())return;
    if(index<0 || index>2 || index==faceIndex())return;
    FaceView* views[]{&journeyView_,&archiveView_,&achievementView_};
    const int from=faceIndex();*views[from]={route_,zone_,actionFocus_};
    const auto view=*views[index];
    route_=view.route;zone_=view.zone;actionFocus_=view.action;reconcile();
}
QJsonObject HallOfFameController::navigationState() const {
    QJsonObject selected;
    for (auto i = achievementIds_.cbegin(); i != achievementIds_.cend(); ++i) selected.insert(i.key(), i.value());
    const auto encode = [](const FaceView& view) {
        return QJsonObject{{"route", view.route}, {"zone", view.zone}, {"action", view.action}};
    };
    const FaceView current{route_, zone_, actionFocus_};
    return {{"champion",championId_}, {"archive", archiveId_}, {"set", setId_}, {"achievements", selected}, {"route", route_},
            {"zone", zone_}, {"action", actionFocus_},
            {"journeyView", encode(overview() ? current : journeyView_)},
            {"archiveView", encode(isArchive() && !overview() ? current : archiveView_)},
            {"achievementView", encode(isArchive() ? achievementView_ : current)}};
}
void HallOfFameController::restoreNavigation(const QJsonObject& state) {
    championId_=state["champion"].toString();
    archiveId_ = state["archive"].toString(); setId_ = state["set"].toString();
    achievementIds_.clear();
    const auto selected = state["achievements"].toObject();
    for (const auto& set : provider_.sets())
        for (const auto& entry : checkedSnapshot(set).definitions)
            if (selected[set.id].toString() == entry.id) achievementIds_[set.id] = entry.id;
    const auto decode = [this](const QJsonObject& value, bool archive) {
        const QStringList routes = archive ? QStringList{"archive-journey", "archive-list", "archive-detail", "archive-champions", "archive-champion-detail"}
            : QStringList{"sets", "achievements", "achievement-detail"};
        FaceView view{value["route"].toString(), value["zone"].toString(), std::max(0, value["action"].toInt())};
        if (!routes.contains(view.route)) view.route = routes.first();
        if (view.route == "archive-champion-detail" && !sampleJourney_ && championRecords().isEmpty()) view.route = "archive-champions";
        // Old rail focus migrates to the visible list; details only expose actions.
        if (view.zone != "actions") view.zone = "list";
        if (view.route.endsWith("detail") || view.route == "archive-journey" || view.route == "archive-champions") view.zone = "actions";
        return view;
    };
    const bool archive = !QStringList{"sets", "achievements", "achievement-detail"}.contains(state["route"].toString());
    archiveView_ = decode(state["archiveView"].toObject(), true);
    if(archiveView_.route!="archive-list" && archiveView_.route!="archive-detail")archiveView_={"archive-list","list"};
    journeyView_=decode(state["journeyView"].toObject(),true);
    if(!QStringList{"archive-journey","archive-champions","archive-champion-detail"}.contains(journeyView_.route))journeyView_={"archive-journey","actions"};
    achievementView_ = decode(state["achievementView"].toObject(), false);
    // Keep the old active-route fields readable without a database migration.
    const auto active = decode(state, archive);
    (active.route=="archive-list"||active.route=="archive-detail" ? archiveView_ : archive ? journeyView_ : achievementView_) = active;
    route_ = active.route; zone_ = active.zone; actionFocus_ = active.action;
    if (route_ == "archive-champion-detail" && !sampleJourney_ && championRecords().isEmpty()) route_ = "archive-champions";
    reconcile();
}
void HallOfFameController::showJourney() {
    route_ = "archive-journey"; zone_ = "actions"; actionFocus_ = 0;
    emit rowsChanged(); emit changed();
}
void HallOfFameController::back() {
    if (route_ == "archive-champion-detail") { route_ = "archive-champions"; emit changed(); return; }
    if (route_ == "archive-champions") { showJourney(); return; }
    if (route_ == "archive-detail") route_ = "archive-list";
    else if (route_ == "achievement-detail") route_ = "achievements";
    else if (route_ == "achievements") route_ = "sets";
    else return;
    zone_ = currentRows().isEmpty() ? "actions" : "list";
    actionFocus_ = 0;
    emit rowsChanged();
}
void HallOfFameController::activate(int index) {
    if (account_.isOpen()) { account_.activate(index); return; }
    if (overview()) {
        if(route_!="archive-journey")showJourney();
        else {route_="archive-champions";zone_="actions";emit changed();}
        return;
    }
    if (zone_ == "list") {
        const auto rows = currentRows();
        if (index < 0 || index >= rows.size()) return;
        selectRow(rows[index].id);
        if (route_ == "archive-list") { route_ = "archive-detail"; zone_ = "actions"; }
        else if (route_ == "sets") { route_ = "achievements"; reconcile(); }
        else if (route_ == "achievements") { route_ = "achievement-detail"; zone_ = "actions"; }
        actionFocus_ = 0;
    } else if (zone_ == "actions") {
        const auto available = actions();
        if (index < 0 || index >= available.size() || !available[index].toMap()["enabled"].toBool()) return;
        actionFocus_ = index;
        if (route_ == "archive-list") { zone_ = "list"; refreshArchive(); }
        else if (route_ == "sets") currentAdventure_.isEmpty()?provider_.refreshAll():provider_.refreshAdventure(currentAdventure_);
        else if (index == 0) back();
        else { provider_.refresh(setId_); normalizeActions(); }
    }
    emit changed();
}
void HallOfFameController::activateControl(const QString& zone, int index) {
    if (account_.isOpen()) { account_.activate(index); return; }
    if (zone == "journey-champions" && overview()) { route_ = "archive-champions"; emit changed(); return; }
    if (zone == "achievement-account") { account_.begin(); return; }
    if (editor_.isOpen()) { editor_.activate(index); return; }
    if (zone == "memory-new" || zone == "memory-edit") { beginMemory(zone == "memory-edit"); return; }
    if (zone != "actions" && (zone != "list" || isDetail() || currentRows().isEmpty())) return;
    zone_ = zone; activate(index);
}
void HallOfFameController::dispatch(Action action) {
    if (account_.isOpen()) { account_.dispatch(action); return; }
    if (editor_.isOpen()) { editor_.dispatch(action); return; }
    if (overview()) {
        if(route_!="archive-journey" && (action==Action::Left || action==Action::Right)) {
            const auto records=championRecords();if(records.isEmpty())return;
            int index=0;for(int i=0;i<records.size();++i)if(records[i].id==championId_)index=i;
            index=(index+(action==Action::Left?-1:1)+records.size())%records.size();championId_=records[index].id;emit changed();return;
        }
        if (action == Action::Secondary) { route_ = route_ == "archive-journey" ? "archive-champions" : "archive-journey"; emit changed(); }
        else if (action == Action::Confirm) activate(0);
        else if (action == Action::Back) { back(); emit changed(); }
        else if (action == Action::ToggleContinue) beginMemory(false);
        return;
    }
    if (!isArchive() && action == Action::Secondary) { account_.begin(); return; }
    if (!isArchive() && action == Action::ToggleContinue) { currentAdventure_.isEmpty()?provider_.refreshAll():provider_.refreshAdventure(currentAdventure_); return; }
    if (isArchive() && action == Action::ToggleContinue) { beginMemory(false); return; }
    if (isArchive() && action == Action::Secondary) { beginMemory(true); return; }
    if (action == Action::Confirm) { activate(focusIndex()); return; }
    if (action == Action::Back) { back(); emit changed(); return; }
    if (zone_ == "list") {
        const auto rows = currentRows();
        const int index = rowIndex();
        if (action == Action::Up) {
            if (index > 0) selectRow(rows[index - 1].id);
        }
        if (action == Action::Down) {
            if (index + 1 >= rows.size()) { zone_ = "actions"; actionFocus_ = 0; }
            else selectRow(rows[index + 1].id);
        }
    } else if (zone_ == "actions") {
        if (action == Action::Up) {
            if (!isDetail() && !currentRows().isEmpty()) zone_ = "list";
        }
        if (action == Action::Left) actionFocus_ = 0;
        if (action == Action::Right && actions().size() > 1 && actions()[1].toMap()["enabled"].toBool()) actionFocus_ = 1;
    }
    emit changed();
}
void HallOfFameController::beginMemory(bool edit) {
    if (!isArchive() || !editable()) return;
    if (!edit) { editor_.begin(); return; }
    for (const auto& entry : archive_) if (entry.id == archiveId_) { editor_.begin(entry); return; }
}
}
