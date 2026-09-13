#include "DeviceController.h"
#include <algorithm>

namespace trainer {
namespace {
QString capacity(qint64 free, qint64 total) {
    if (free < 0 || total <= 0) return "Unavailable";
    constexpr double gib = 1024.0 * 1024 * 1024;
    return QString("%1 GiB free / %2 GiB").arg(free / gib, 0, 'f', 1).arg(total / gib, 0, 'f', 1);
}
}
void DeviceController::configure(DeviceService* service, bool powerAvailable) {
    service_ = service; powerAvailable_ = powerAvailable;
    if (service_) connect(service_, &DeviceService::changed, this, &DeviceController::changed);
}
void DeviceController::begin() { focus_ = 0; if (service_) service_->refresh(); emit changed(); }
QVariantList DeviceController::rows() const {
    const auto value = service_ ? service_->snapshot() : DeviceSnapshot{};
    const QString volume = value.volume < 0 ? "Unavailable" : QString::number(value.volume) + "%" + (value.muted ? " · Muted" : "");
    return {
        QVariantMap{{"title", "Volume"}, {"value", volume + "   ·   Left / Right adjust · A mute"}},
        QVariantMap{{"title", "Screen brightness"}, {"value", (value.brightness < 0 ? QString("Unavailable") : QString::number(value.brightness) + "%") + "   ·   Left / Right adjust"}},
        QVariantMap{{"title", "Refresh status"}, {"value", "Y also refreshes these readings"}},
        QVariantMap{{"title", "Restart device"}, {"value", powerAvailable_ ? "Save your place, then restart" : "Available in the ArmadaOS installation"}},
        QVariantMap{{"title", "Power off"}, {"value", powerAvailable_ ? "Save your place, then turn off" : "Available in the ArmadaOS installation"}},
        QVariantMap{{"title", "Back to Settings"}, {"value", "B also returns to Settings"}}
    };
}
QVariantList DeviceController::status() const {
    const auto value = service_ ? service_->snapshot() : DeviceSnapshot{};
    return {QVariantMap{{"title", "Network"}, {"value", value.network}},
            QVariantMap{{"title", "Trainer storage"}, {"value", capacity(value.internalFree, value.internalTotal)}},
            QVariantMap{{"title", "Adventure storage"}, {"value", capacity(value.libraryFree, value.libraryTotal)}}};
}
void DeviceController::activate(int index) {
    if (index < 0 || index > 5) return;
    focus_ = index;
    if (index == 5) { emit closeRequested(); return; }
    if (index >= 3) {
        if (powerAvailable_) emit powerRequested(index == 3 ? "reboot" : "poweroff");
        else emit messageRequested("Power controls are available in the ArmadaOS installation.");
    } else if (service_ && !busy()) {
        if (index == 2) service_->refresh();
        else if (index == 0 && service_->snapshot().volume >= 0) service_->setValue("mute", !service_->snapshot().muted);
    }
    emit changed();
}
void DeviceController::dispatch(Action action) {
    if (action == Action::Back) { emit closeRequested(); return; }
    if (action == Action::Confirm) { activate(focus_); return; }
    if (action == Action::ToggleContinue) { if (service_) service_->refresh(); return; }
    if (action == Action::Up) focus_ = std::max(0, focus_ - 1);
    if (action == Action::Down) focus_ = std::min(5, focus_ + 1);
    if ((action == Action::Left || action == Action::Right) && service_ && !busy() && focus_ < 2) {
        const int current = focus_ == 0 ? service_->snapshot().volume : service_->snapshot().brightness;
        if (current >= 0) service_->setValue(focus_ == 0 ? "volume" : "brightness",
            std::clamp(current + (action == Action::Right ? 5 : -5), focus_ == 0 ? 0 : 5, 100));
    }
    emit changed();
}
}
