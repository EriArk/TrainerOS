#include "FilePickerController.h"
#include <QDir>
#include <algorithm>

namespace trainer {
QVariantList FilePickerController::rows() const {
    QVariantList rows;
    for (const auto& entry : current_.entries) rows.append(QVariantMap{{"id", entry.path}, {"title", entry.name},
        {"subtitle", entry.directory ? "Folder" : "File"}});
    return rows;
}
QVariantList FilePickerController::actions() const {
    return {QVariantMap{{"title", "Locations"}}, QVariantMap{{"title", "Parent folder"}},
        QVariantMap{{"title", "Previous batch"}}, QVariantMap{{"title", "Next batch"}},
        QVariantMap{{"title", current_.error.isEmpty() ? QString("Refresh · %1/%2").arg(current_.page + 1).arg(current_.pages) : "Retry"}},
        QVariantMap{{"title", "Cancel"}}};
}
void FilePickerController::begin(const QString& folder) { open_ = true; history_.clear(); load(folder); }
void FilePickerController::cancel() { ++generation_; open_ = false; busy_ = false; emit changed(); }
void FilePickerController::load(const QString& path, int page, const QString& selection) {
    const int token = ++generation_;
    busy_ = true; zone_ = "actions"; action_ = 5; current_.error.clear(); emit changed();
    if (!catalog_) { busy_ = false; current_.error = "Local file browsing isn't available in this preview."; emit changed(); return; }
    catalog_->list(path, page, this, [this, token, selection](const DirectoryPage& result) {
        if (!open_ || token != generation_) return;
        current_ = result; busy_ = false; row_ = 0;
        for (int i = 0; i < current_.entries.size(); ++i) if (current_.entries[i].path == selection) row_ = i;
        zone_ = current_.entries.isEmpty() ? "actions" : "list"; action_ = current_.error.isEmpty() ? 0 : 4;
        emit rowsChanged(); emit changed();
    });
}
void FilePickerController::back() {
    if (busy_ || history_.isEmpty()) { cancel(); return; }
    const auto previous = history_.takeLast(); load(previous.path, previous.page, previous.selection);
}
void FilePickerController::activate(int index, const QString& zone) {
    if (!open_) return;
    if (zone == "actions") {
        if (index == 5) { cancel(); return; }
        if (busy_) return;
        if (index == 0) { history_.clear(); load({}); }
        else if (index == 1) {
            const auto parent = QDir::cleanPath(current_.path + "/..");
            if (current_.path.isEmpty() || parent == QDir::cleanPath(current_.path)) load({}); else load(parent, 0, current_.path);
        } else if (index == 2 && current_.page > 0) load(current_.path, current_.page - 1);
        else if (index == 3 && current_.page + 1 < current_.pages) load(current_.path, current_.page + 1);
        else if (index == 4) load(current_.path, current_.page);
    } else if (zone == "list" && !busy_ && index >= 0 && index < current_.entries.size()) {
        row_ = index;
        const auto entry = current_.entries[index];
        if (entry.directory) {
            history_.append({current_.path, current_.page, entry.path}); load(entry.path);
        } else { cancel(); emit selected(entry.path); }
    }
    emit changed();
}
void FilePickerController::dispatch(Action action) {
    if (action == Action::Back) { back(); return; }
    if (action == Action::Confirm) { activate(focusIndex(), zone_); return; }
    if (zone_ == "list") {
        if (action == Action::Up) { if (row_ > 0) --row_; else { zone_ = "actions"; action_ = 0; } }
        if (action == Action::Down) {
            if (row_ + 1 < current_.entries.size()) ++row_;
            else { zone_ = "actions"; action_ = 0; }
        }
    } else {
        if (action == Action::Left) action_ = std::max(0, action_ - 1);
        if (action == Action::Right) action_ = std::min(5, action_ + 1);
        if ((action == Action::Up || action == Action::Down) && !busy_ && !current_.entries.isEmpty()) zone_ = "list";
    }
    emit changed();
}
}
