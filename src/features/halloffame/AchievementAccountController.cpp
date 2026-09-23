#include "AchievementAccountController.h"
#include <algorithm>

namespace trainer {
AchievementAccountController::AchievementAccountController(AchievementProvider& provider, QObject* parent)
    : QObject(parent), provider_(provider) {
    connect(&provider_, &AchievementProvider::snapshotChanged, this, [this] {
        focus_ = busy() ? int(rows().size()) - 1 : std::clamp(focus_, 0, int(rows().size()) - 1); emit changed();
    });
}
QString AchievementAccountController::status() const {
    if (confirmSignOut_) return "Sign out of Hall of Fame? Saved records remain private to this account.";
    if (!provider_.accountMessage().isEmpty()) return provider_.accountMessage();
    return connected() ? "Connected as " + provider_.context().accountId
                       : "Connect your RetroAchievements account. Only an access token is saved on this handheld.";
}
QVariantList AchievementAccountController::rows() const {
    if (connected()) return {
        QVariantMap{{"label", "Refresh recent Adventures"}, {"detail", provider_.context().accountId}, {"enabled", !busy()}},
        QVariantMap{{"label", confirmSignOut_ ? "Confirm sign out" : "Sign out of Hall of Fame"}, {"detail", "Your local archive stays available"}, {"enabled", !busy()}},
        QVariantMap{{"label", "Back"}, {"detail", ""}, {"enabled", true}}};
    return {
        QVariantMap{{"label", "Account name"}, {"detail", username_.isEmpty() ? "Enter name" : username_}, {"enabled", !busy()}},
        QVariantMap{{"label", "Password"}, {"detail", password_.isEmpty() ? "Enter password" : QString(password_.size(), QChar(0x2022))}, {"enabled", !busy()}},
        QVariantMap{{"label", busy() ? "Connecting…" : "Connect account"}, {"detail", ""}, {"enabled", !busy()}},
        QVariantMap{{"label", "Back"}, {"detail", ""}, {"enabled", true}}};
}
void AchievementAccountController::begin() {
    if (!available()) return;
    open_ = true; username_ = provider_.context().accountId; password_.clear(); focus_ = 0; confirmSignOut_ = false; emit changed();
}
void AchievementAccountController::close() {
    open_ = false; password_.clear(); username_.clear(); confirmSignOut_ = false; emit changed();
}
void AchievementAccountController::applyText(const QString& value) {
    if (!open_ || busy() || connected()) return;
    if (passwordField_) password_ = value.left(128); else username_ = value.trimmed().left(32);
    emit changed();
}
void AchievementAccountController::activate(int index) {
    if (!open_ || index < 0 || index >= rows().size()) return;
    focus_ = index;
    if (index == rows().size() - 1) { close(); return; }
    if (busy()) return;
    if (connected()) {
        if (index == 0) { confirmSignOut_ = false; provider_.refreshAll(); }
        else if (confirmSignOut_) { provider_.disconnectAccount(); confirmSignOut_ = false; focus_ = 0; }
        else confirmSignOut_ = true;
    } else if (index < 2) {
        passwordField_ = index == 1;
        emit textRequested(passwordField_ ? "RetroAchievements password" : "RetroAchievements account name",
            passwordField_ ? password_ : username_, passwordField_ ? 128 : 32, passwordField_);
    } else {
        const auto password = password_; password_.clear(); provider_.login(username_, password);
    }
    emit changed();
}
void AchievementAccountController::dispatch(Action action) {
    if (!open_) return;
    if (action == Action::Back) {
        if (confirmSignOut_) { confirmSignOut_ = false; emit changed(); } else close();
        return;
    }
    if (action == Action::Confirm) { activate(focus_); return; }
    if (action == Action::Up) focus_ = std::max(0, focus_ - 1);
    if (action == Action::Down) focus_ = std::min(int(rows().size()) - 1, focus_ + 1);
    if (busy()) focus_ = int(rows().size()) - 1;
    confirmSignOut_ = false; emit changed();
}
}
