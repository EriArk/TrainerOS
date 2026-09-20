#include "SettingsController.h"
#include <algorithm>

namespace trainer {
void SettingsController::reload() { if (repository_ && !saving_) { value_ = repository_->preferences(); emit changed(); } }
QVariantList SettingsController::rows() const {
    if (media_) return {
        QVariantMap{{"title", "Pictures & video"}, {"value", "Static pictures supported"}},
        QVariantMap{{"title", "Music & interface sounds"}, {"value", "Playback not available yet"}},
        QVariantMap{{"title", "Charger vibration"}, {"value", "Not enabled by TrainerOS"}},
        QVariantMap{{"title", "Device lighting"}, {"value", "Support not verified"}},
        QVariantMap{{"title", "Reduced motion"}, {"value", value_.reducedMotion ? "On" : "Off"}}};
    return {QVariantMap{{"title", "Shell color"}, {"value", value_.theme}},
        QVariantMap{{"title", "Reduced motion"}, {"value", value_.reducedMotion ? "On" : "Off"}},
        QVariantMap{{"title", "Your handheld"}, {"value", "Sound, screen, connection, storage and power"}},
        QVariantMap{{"title", "Credits"}, {"value", "The artists behind your badge collection"}},
        QVariantMap{{"title", "Trainer & accounts"}, {"value", "Your profile and RetroAchievements"}},
        QVariantMap{{"title", "Media & feedback"}, {"value", "Pictures, sound, vibration, lighting and motion"}},
        QVariantMap{{"title", "Back to system menu"}, {"value", "Your preferences are saved when changed"}}};
}
QString SettingsController::mediaDescription() const {
    switch (focus_) {
    case 0: return "Home and Choose Adventure use clean exit pictures where available. Pokédex illustrations have their own sources and credits.\n\nVideo previews and installing media packs are not available here yet.";
    case 1: return "Music, interface sound packs and listening previews are not available yet.\n\nSystem volume and mute already work in Start or Your handheld. Game audio is unchanged.";
    case 2: return "TrainerOS does not send vibration when a charger is connected or removed yet.\n\nPatterns and intensity need verification on this handheld. Game rumble is unchanged.";
    case 3: return "Controllable lighting has not been verified on this handheld. No color or brightness is being changed.\n\nA supported device service is needed before lighting controls become available.";
    default: return "Reduce interface movement using the same saved preference as the main Settings page.\n\nBackground parallax and gyroscope-driven motion are not available yet. A toggles reduced motion.";
    }
}
void SettingsController::activate(int index) {
    if (mediaDetail_) { mediaDetail_ = false; emit changed(); return; }
    if (media_) {
        if (index < 0 || index > 4) return;
        focus_ = index;
        if (index != 4) { mediaDetail_ = true; emit changed(); return; }
        index = 1; // Reuse the existing durable Reduced Motion write path.
    } else if (index == 5) { media_ = true; focus_ = 0; emit changed(); return; }
    if (credits_) { credits_ = false; emit changed(); return; }
    if (index == 6) { emit closeRequested(); return; }
    if (index == 4) { focus_ = index; emit trainerRequested(); return; }
    if (index == 3) { focus_ = index; credits_ = true; emit changed(); return; }
    if (index == 2) { focus_ = index; emit deviceRequested(); return; }
    if (index < 0 || index > 1 || saving_) return;
    if (!media_) focus_ = index;
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
    if (mediaDetail_) {
        if (action == Action::Back || action == Action::Confirm) { mediaDetail_ = false; emit changed(); }
        return;
    }
    if (media_ && action == Action::Back) { media_ = false; focus_ = 5; emit changed(); return; }
    if (credits_) {
        if (action == Action::Back || action == Action::Confirm) { credits_ = false; emit changed(); }
        return;
    }
    if (action == Action::Back) { emit closeRequested(); return; }
    if (action == Action::Confirm) { activate(focus_); return; }
    if (action == Action::Up) focus_ = std::max(0, focus_ - 1);
    if (action == Action::Down) focus_ = std::min(media_ ? 4 : 6, focus_ + 1);
    emit changed();
}
}
