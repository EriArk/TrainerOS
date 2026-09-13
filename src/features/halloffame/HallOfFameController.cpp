#include "HallOfFameController.h"
#include <algorithm>

namespace trainer {
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
        archiveId_ = id; route_ = "archive-detail"; zone_ = "actions"; actionFocus_ = 0; refreshArchive();
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
    for (const auto& row : currentRows()) result.append(QVariantMap{{"id", row.id}, {"title", row.title}, {"subtitle", row.subtitle}});
    return result;
}
int HallOfFameController::rowIndex() const {
    const auto rows = currentRows();
    for (int i = 0; i < rows.size(); ++i) if (rows[i].id == selectedRowId()) return i;
    return 0;
}
int HallOfFameController::focusIndex() const { return account_.isOpen() ? account_.focusIndex() : zone_ == "rail" ? railFocus_ : zone_ == "actions" ? actionFocus_ : rowIndex(); }
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
    if (isArchive()) return archiveError_.isEmpty() ? "Your first memory starts here. Press Y to record a completed Adventure." : archiveError_;
    if (route_ == "sets") return provider_.context().accountId.isEmpty()
        ? "X · Connect your RetroAchievements account. Your local archive remains available."
        : "Play a supported Adventure, then press Y to check its achievements. Your local archive remains available.";
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
    if (route_ == "archive-list") return {QVariantMap{{"label", "Refresh archive"}, {"enabled", true}}};
    if (route_ == "archive-detail") return {QVariantMap{{"label", "Back to archive"}, {"enabled", true}}};
    if (route_ == "sets") return {QVariantMap{{"label", "Open local archive"}, {"enabled", true}}};
    const auto state = currentSnapshot().state;
    const bool refresh = !setId_.isEmpty() && selectedSet().supported && !provider_.context().accountId.isEmpty() && state != AchievementState::Loading;
    return {QVariantMap{{"label", route_ == "achievement-detail" ? "Back to achievements" : "Back to Adventures"}, {"enabled", true}},
        QVariantMap{{"label", state == AchievementState::Loading ? "Refreshing…" : "Refresh records"}, {"enabled", refresh}}};
}
void HallOfFameController::normalizeActions() {
    const auto available = actions();
    if (actionFocus_ < 0 || actionFocus_ >= available.size() || !available[actionFocus_].toMap()["enabled"].toBool()) actionFocus_ = 0;
}
void HallOfFameController::reconcile() {
    const auto sets = provider_.sets();
    const bool exists = std::any_of(sets.begin(), sets.end(), [&](const auto& set) { return set.id == setId_; });
    if (!exists) {
        setId_ = sets.isEmpty() ? QString() : sets.first().id;
        if (!isArchive()) route_ = "sets";
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
QJsonObject HallOfFameController::navigationState() const {
    QJsonObject selected;
    for (auto i = achievementIds_.cbegin(); i != achievementIds_.cend(); ++i) selected.insert(i.key(), i.value());
    return {{"archive", archiveId_}, {"set", setId_}, {"achievements", selected}, {"route", route_},
            {"zone", zone_}, {"rail", railFocus_}, {"action", actionFocus_}};
}
void HallOfFameController::restoreNavigation(const QJsonObject& state) {
    archiveId_ = state["archive"].toString(); setId_ = state["set"].toString();
    achievementIds_.clear();
    const auto selected = state["achievements"].toObject();
    for (const auto& set : provider_.sets())
        for (const auto& entry : checkedSnapshot(set).definitions)
            if (selected[set.id].toString() == entry.id) achievementIds_[set.id] = entry.id;
    const auto route = state["route"].toString();
    route_ = QStringList{"archive-list", "archive-detail", "sets", "achievements", "achievement-detail"}.contains(route) ? route : "archive-list";
    const auto zone = state["zone"].toString();
    zone_ = QStringList{"rail", "list", "actions"}.contains(zone) ? zone : "list";
    // Details only expose their action rail; a stale list focus cannot point at hidden rows.
    if (isDetail()) zone_ = "actions";
    railFocus_ = std::clamp(state["rail"].toInt(), 0, 1);
    actionFocus_ = std::max(0, state["action"].toInt());
    reconcile();
}
void HallOfFameController::back() {
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
    if (zone_ == "rail") {
        if (index < 0 || index > 1) return;
        railFocus_ = index;
        route_ = index == 0 ? "archive-list" : "sets";
        zone_ = "list"; actionFocus_ = 0; reconcile();
    } else if (zone_ == "list") {
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
        else if (route_ == "sets") { route_ = "archive-list"; zone_ = "list"; railFocus_ = 0; reconcile(); }
        else if (index == 0) back();
        else { provider_.refresh(setId_); normalizeActions(); }
    }
    emit changed();
}
void HallOfFameController::activateControl(const QString& zone, int index) {
    if (account_.isOpen()) { account_.activate(index); return; }
    if (zone == "achievement-account") { account_.begin(); return; }
    if (editor_.isOpen()) { editor_.activate(index); return; }
    if (zone == "memory-new" || zone == "memory-edit") { beginMemory(zone == "memory-edit"); return; }
    if (zone != "rail" && zone != "actions" && (zone != "list" || isDetail() || currentRows().isEmpty())) return;
    zone_ = zone; activate(index);
}
void HallOfFameController::dispatch(Action action) {
    if (account_.isOpen()) { account_.dispatch(action); return; }
    if (editor_.isOpen()) { editor_.dispatch(action); return; }
    if (!isArchive() && action == Action::Secondary) { account_.begin(); return; }
    if (!isArchive() && action == Action::ToggleContinue) { provider_.refreshAll(); return; }
    if (isArchive() && action == Action::ToggleContinue) { beginMemory(false); return; }
    if (isArchive() && action == Action::Secondary) { beginMemory(true); return; }
    if (action == Action::Confirm) { activate(focusIndex()); return; }
    if (action == Action::Back) { back(); emit changed(); return; }
    if (zone_ == "rail") {
        if (action == Action::Left) railFocus_ = 0;
        if (action == Action::Right) railFocus_ = 1;
        if (action == Action::Down) zone_ = isDetail() || currentRows().isEmpty() ? "actions" : "list";
    } else if (zone_ == "list") {
        const auto rows = currentRows();
        const int index = rowIndex();
        if (action == Action::Up) {
            if (index == 0) { zone_ = "rail"; railFocus_ = isArchive() ? 0 : 1; }
            else selectRow(rows[index - 1].id);
        }
        if (action == Action::Down) {
            if (index + 1 >= rows.size()) { zone_ = "actions"; actionFocus_ = 0; }
            else selectRow(rows[index + 1].id);
        }
    } else if (zone_ == "actions") {
        if (action == Action::Up) {
            zone_ = isDetail() || currentRows().isEmpty() ? "rail" : "list";
            railFocus_ = isArchive() ? 0 : 1;
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
