#include "SpeciesPicker.h"
#include <algorithm>

namespace trainer {
namespace {
QString normalized(const QString& text) {
    QString result;
    for (const auto c : text.normalized(QString::NormalizationForm_D))
        if (c.isLetterOrNumber()) result += c.toLower();
    return result;
}
}
QVariantList SpeciesPicker::entries() const {
    QVariantList rows;
    for (const auto& e : filtered_)
        rows.append(QVariantMap{{"id", e.id}, {"name", e.name},
            {"number", QString("#%1").arg(e.number, 3, 10, QChar('0'))}});
    return rows;
}
void SpeciesPicker::begin(const QString& selected) {
    query_.clear(); error_.clear(); catalog_.clear();
    if (reference_) {
        const auto loaded = reference_->load();
        if (loaded.success) catalog_ = loaded.entries;
        else error_ = "The field guide couldn't be opened. Your current favorite is unchanged.";
    } else error_ = "The field guide isn't available. Your current favorite is unchanged.";
    std::sort(catalog_.begin(), catalog_.end(), [](const auto& a, const auto& b) { return a.number < b.number; });
    open_ = true; rebuild();
    for (int i = 0; i < filtered_.size(); ++i) if (filtered_[i].id == selected) focus_ = i;
    emit changed();
}
void SpeciesPicker::cancel() { open_ = false; emit changed(); }
void SpeciesPicker::rebuild() {
    filtered_.clear(); focus_ = 0;
    QString numberQuery = query_; if (numberQuery.startsWith('#')) numberQuery.remove(0, 1);
    bool numeric = false; const int number = numberQuery.toInt(&numeric);
    const auto text = normalized(query_);
    for (const auto& entry : catalog_)
        if (query_.isEmpty() || (numeric ? entry.number == number : normalized(entry.name).contains(text))) filtered_.append(entry);
    emit rowsChanged(); emit changed();
}
void SpeciesPicker::applySearch(const QString& text) { if (open_) { query_ = text.trimmed(); rebuild(); } }
void SpeciesPicker::activate(int index) {
    if (!open_ || index < 0 || index >= filtered_.size()) return;
    const auto id = filtered_[index].id; cancel(); emit selected(id);
}
void SpeciesPicker::dispatch(Action action) {
    if (!open_) return;
    if (action == Action::Back) { cancel(); return; }
    if (action == Action::Secondary) { requestSearch(); return; }
    if (action == Action::ToggleContinue) { clearChoice(); return; }
    if (action == Action::Confirm) { if (filtered_.isEmpty()) requestSearch(); else activate(focus_); return; }
    const int step = action == Action::Left ? -8 : action == Action::Right ? 8
        : action == Action::Up ? -1 : action == Action::Down ? 1 : 0;
    focus_ = std::clamp(focus_ + step, 0, std::max(0, int(filtered_.size()) - 1));
    emit changed();
}
}
