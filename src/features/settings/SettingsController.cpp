#include "SettingsController.h"
#include <algorithm>

namespace trainer {
void SettingsController::reload() { if (repository_ && !saving_) { value_ = repository_->preferences(); emit changed(); } }
QVariantList SettingsController::rows() const {
    return {QVariantMap{{"title", "Shell color"}, {"value", value_.theme}},
        QVariantMap{{"title", "Reduced motion"}, {"value", value_.reducedMotion ? "On" : "Off"}},
        QVariantMap{{"title", "Your handheld"}, {"value", "Sound, screen, connection, storage and power"}},
        QVariantMap{{"title", "Credits"}, {"value", "The artists behind your badge collection"}},
        QVariantMap{{"title", "Back to system menu"}, {"value", "Your preferences are saved when changed"}}};
}
void SettingsController::activate(int index) {
    if (credits_) { credits_ = false; emit changed(); return; }
    if (index == 4) { emit closeRequested(); return; }
    if (index == 3) { focus_ = index; credits_ = true; emit changed(); return; }
    if (index == 2) { focus_ = index; emit deviceRequested(); return; }
    if (index < 0 || index > 1 || saving_) return;
    focus_ = index;
    auto candidate = value_;
    if (index == 0) {
        const QStringList themes{"turquoise", "red", "green", "blue", "orange"};
        candidate.theme = themes[(themes.indexOf(value_.theme) + 1) % themes.size()];
    } else candidate.reducedMotion = !candidate.reducedMotion;
    error_.clear(); saving_ = true; emit changed();
    const auto completed = [this, candidate](const QString& error) {
        saving_ = false; error_ = error;
        if (error.isEmpty()) value_ = candidate;
        else emit messageRequested(error);
        emit changed();
    };
    if (repository_) repository_->savePreferences(candidate, this, completed); else completed({});
}
void SettingsController::dispatch(Action action) {
    if (credits_) {
        if (action == Action::Back || action == Action::Confirm) { credits_ = false; emit changed(); }
        return;
    }
    if (action == Action::Back) { emit closeRequested(); return; }
    if (action == Action::Confirm) { activate(focus_); return; }
    if (action == Action::Up) focus_ = std::max(0, focus_ - 1);
    if (action == Action::Down) focus_ = std::min(4, focus_ + 1);
    emit changed();
}
}
