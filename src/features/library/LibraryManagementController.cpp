#include "LibraryManagementController.h"
#include <QFileInfo>
#include <QUuid>
#include <algorithm>

namespace trainer {
LibraryManagementController::LibraryManagementController(LibraryRepository& repository, FileCatalog* files, QObject* parent)
    : QObject(parent), repository_(repository), files_(files, this) {
    connect(&files_, &FilePickerController::changed, this, &LibraryManagementController::changed);
    connect(&files_, &FilePickerController::selected, this, [this](const QString& path) {
        if (!open_ || route_ != "edit" || saving_) return;
        draft_.contentPath = path; initialFolder_ = QFileInfo(path).absolutePath();
        if (draft_.adventure.title.isEmpty()) draft_.adventure.title = QFileInfo(path).completeBaseName().left(64);
        error_.clear(); emit changed();
    });
}
void LibraryManagementController::begin(const QString& worldId) {
    open_ = true; preferredWorld_ = worldId; route_ = "list"; error_.clear();
    refresh(); zone_ = records_.isEmpty() ? "actions" : "list"; focus_ = 0; emit changed();
}
void LibraryManagementController::close() {
    open_ = false; files_.cancel(); textField_ = -1; route_ = "list";
    if (!saving_) draft_ = {};
    error_.clear(); emit changed();
}
void LibraryManagementController::refresh() {
    records_ = repository_.adventures();
    if (std::none_of(records_.begin(), records_.end(), [this](const auto& a) { return a.id == selectedId_; }))
        selectedId_ = records_.isEmpty() ? QString() : records_.first().id;
    if (records_.isEmpty() && route_ == "list") zone_ = "actions";
    emit rowsChanged(); emit changed();
}
int LibraryManagementController::rowIndex() const {
    for (int i = 0; i < records_.size(); ++i) if (records_[i].id == selectedId_) return i;
    return 0;
}
QString LibraryManagementController::worldName(const QString& id) const {
    if (draft_.newWorld && draft_.newWorld->id == id) return draft_.newWorld->name;
    for (const auto& world : repository_.worlds()) if (world.id == id) return world.name;
    return "Choose a World";
}
QVariantList LibraryManagementController::rows() const {
    QVariantList result;
    for (const auto& a : records_) result.append(QVariantMap{{"id", a.id}, {"title", a.title}, {"subtitle", worldName(a.worldId) + " · A to edit"}});
    return result;
}
QVariantList LibraryManagementController::fields() const {
    QStringList extra;
    for (const auto& id : draft_.adventure.additionalWorldIds) extra.append(worldName(id));
    return {QVariantMap{{"title", "Title"}, {"value", draft_.adventure.title.isEmpty() ? "Name your Adventure" : draft_.adventure.title}},
        QVariantMap{{"title", "Primary World"}, {"value", worldName(draft_.adventure.worldId)}},
        QVariantMap{{"title", "Also in Worlds"}, {"value", extra.isEmpty() ? "None" : extra.join(" · ")}},
        QVariantMap{{"title", "Edition"}, {"value", QStringList{"Original", "Remake", "ROM hack"}.value(int(draft_.adventure.kind), "Original")}},
        QVariantMap{{"title", "Local file"}, {"value", draft_.contentPath.isEmpty() ? "Choose a file" : QFileInfo(draft_.contentPath).fileName()}},
        QVariantMap{{"title", "Notes"}, {"value", draft_.adventure.description.isEmpty() ? "Optional description" : draft_.adventure.description}}};
}
QString LibraryManagementController::title() const {
    if (route_ == "world") return "Choose a primary World";
    if (route_ == "extras") return "Additional Worlds";
    if (route_ == "edit") return draft_.revision == 0 ? "Add Adventure" : "Edit Adventure";
    return "Manage Adventures";
}
QVariantList LibraryManagementController::choices() const {
    QVariantList result;
    for (const auto& world : repository_.worlds()) {
        if (route_ == "extras" && world.id == draft_.adventure.worldId) continue;
        result.append(QVariantMap{{"id", world.id}, {"title", world.name}, {"selected", route_ == "extras" ? extraDraft_.contains(world.id) : draft_.adventure.worldId == world.id}});
    }
    result.append(QVariantMap{{"id", route_ == "extras" ? "apply" : "new"}, {"title", route_ == "extras" ? "Apply choices" : "Create a World"}, {"selected", false}});
    result.append(QVariantMap{{"id", "cancel"}, {"title", "Cancel"}, {"selected", false}});
    return result;
}
void LibraryManagementController::edit(const QString& id) {
    if (saving_) return;
    if (!repository_.editable()) { emit messageRequested("Library editing isn't available in the sample preview."); return; }
    if (id.isEmpty()) {
        draft_ = {}; draft_.adventure.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        draft_.adventure.adapterId = "unconfigured";
        const auto worlds = repository_.worlds();
        draft_.adventure.worldId = worlds.isEmpty() ? QString() : worlds.first().id;
        for (const auto& world : worlds) if (world.id == preferredWorld_) draft_.adventure.worldId = world.id;
    } else {
        const auto loaded = repository_.registration(id);
        if (!loaded) { refresh(); error_ = "This Adventure is no longer available."; emit changed(); return; }
        draft_ = *loaded;
    }
    route_ = "edit"; focus_ = 0; error_.clear(); emit changed();
}
void LibraryManagementController::save() {
    if (saving_) return;
    if (draft_.adventure.title.trimmed().isEmpty()) { error_ = "Give your Adventure a title."; focus_ = 0; return; }
    if (draft_.contentPath.isEmpty()) { error_ = "Choose the Adventure's local file."; focus_ = 4; return; }
    saving_ = true; error_.clear();
    const auto id = draft_.adventure.id;
    repository_.saveAdventureAsync(draft_, this, [this, id](const LibraryWriteResult& result) {
        saving_ = false;
        if (result.success) {
            selectedId_ = id; draft_ = {}; route_ = "list"; zone_ = "list";
            refresh(); emit saved();
        } else {
            error_ = result.error;
            if (!open_ || route_ != "edit") emit messageRequested(error_);
        }
        emit changed();
    });
}
void LibraryManagementController::applyText(const QString& text) {
    if (!open_ || saving_ || textField_ < 0) return;
    if (textField_ == 0) draft_.adventure.title = text;
    else if (textField_ == 5) draft_.adventure.description = text;
    else if (textField_ == 6) {
        if (text.trimmed().isEmpty()) error_ = "A new World needs a name.";
        else {
            draft_.newWorld = World{QUuid::createUuid().toString(QUuid::WithoutBraces), text.trimmed(), {}};
            draft_.adventure.worldId = draft_.newWorld->id; route_ = "edit"; focus_ = 1; error_.clear();
        }
    }
    textField_ = -1; emit changed();
}
void LibraryManagementController::back() {
    if (route_ == "world" || route_ == "extras") { focus_ = route_ == "world" ? 1 : 2; route_ = "edit"; }
    else if (route_ == "edit") { route_ = "list"; zone_ = records_.isEmpty() ? "actions" : "list"; focus_ = 0; if (!saving_) draft_ = {}; error_.clear(); }
    else { close(); emit closeRequested(); }
    emit changed();
}
void LibraryManagementController::activate(int index, const QString& area) {
    if (!open_) return;
    if (files_.isOpen()) { files_.activate(index, area); return; }
    if (route_ == "list") {
        const auto zone = area.isEmpty() ? zone_ : area;
        if (zone == "actions") { if (index == 0) edit(); else if (index == 1) back(); }
        else if (index >= 0 && index < records_.size()) { selectedId_ = records_[index].id; edit(selectedId_); }
    } else if (route_ == "edit") {
        if (index == 7) { back(); return; }
        if (saving_ || index < 0 || index > 6) return;
        focus_ = index;
        if (index == 0 || index == 5) {
            textField_ = index; emit textRequested(index == 0 ? "Adventure title" : "Adventure notes", index == 0 ? draft_.adventure.title : draft_.adventure.description, index == 0 ? 64 : 160);
        } else if (index == 1 || index == 2) {
            route_ = index == 1 ? "world" : "extras"; extraDraft_ = draft_.adventure.additionalWorldIds; focus_ = 0;
        } else if (index == 3) draft_.adventure.kind = AdventureKind((int(draft_.adventure.kind) + 1) % 3);
        else if (index == 4) files_.begin(draft_.contentPath.isEmpty() ? initialFolder_ : QFileInfo(draft_.contentPath).absolutePath());
        else if (index == 6) save();
    } else {
        const auto all = choices();
        if (index < 0 || index >= all.size()) return;
        focus_ = index; const auto id = all[index].toMap()["id"].toString();
        if (id == "cancel") back();
        else if (id == "new") { textField_ = 6; emit textRequested("New World name", "", 32); }
        else if (id == "apply") { draft_.adventure.additionalWorldIds = extraDraft_; back(); }
        else if (route_ == "extras") { if (extraDraft_.contains(id)) extraDraft_.removeAll(id); else extraDraft_.append(id); }
        else { draft_.adventure.worldId = id; draft_.newWorld.reset(); draft_.adventure.additionalWorldIds.removeAll(id); back(); }
    }
    emit changed();
}
void LibraryManagementController::dispatch(Action action) {
    if (files_.isOpen()) { files_.dispatch(action); return; }
    if (action == Action::Back) { back(); return; }
    if (action == Action::Confirm) { activate(focusIndex()); return; }
    if (route_ == "list") {
        if (zone_ == "list") {
            const int index = rowIndex();
            if (action == Action::Up) { if (index > 0) selectedId_ = records_[index - 1].id; else { zone_ = "actions"; focus_ = 0; } }
            if (action == Action::Down) { if (index + 1 < records_.size()) selectedId_ = records_[index + 1].id; else { zone_ = "actions"; focus_ = 0; } }
        } else {
            if (action == Action::Left) focus_ = 0;
            if (action == Action::Right) focus_ = 1;
            if ((action == Action::Up || action == Action::Down) && !records_.isEmpty()) zone_ = "list";
        }
    } else {
        const int columns = route_ == "edit" ? 2 : 1;
        const int count = route_ == "edit" ? 8 : choices().size();
        if (action == Action::Left && focus_ % columns > 0) --focus_;
        if (action == Action::Right && focus_ % columns + 1 < columns && focus_ + 1 < count) ++focus_;
        if (action == Action::Up && focus_ >= columns) focus_ -= columns;
        if (action == Action::Down && focus_ + columns < count) focus_ += columns;
    }
    emit changed();
}
}
