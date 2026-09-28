#include "ClockController.h"
#include <QTimeZone>
#include <algorithm>

namespace trainer {
ClockController::ClockController(QObject* parent) : QObject(parent) {
    timer_.setInterval(10000);
    connect(&timer_, &QTimer::timeout, this, [this] {
        if (++ticks_ >= 6 && mode_ == "main" && !busy_) { ticks_ = 0; refresh(); }
        else emit changed();
    });
}
ClockController::~ClockController() { if (worker_) { worker_->wait(); delete worker_; } }
void ClockController::begin(bool onboarding) {
    if (busy_) return;
    onboarding_ = onboarding; mode_ = "main"; focus_ = onboarding ? 3 : 0;
    ticks_ = 0; timer_.start(); refresh();
}
void ClockController::leave() { timer_.stop(); if (!busy_) { mode_ = "main"; error_.clear(); } }
QDateTime ClockController::now() const { return QDateTime::currentDateTimeUtc().toTimeZone(QTimeZone(snapshot_.zone.toUtf8())); }
QString ClockController::time() const { return snapshot_.available ? now().toString("HH:mm") : "—"; }
QString ClockController::date() const { return snapshot_.available ? now().toString("dddd, d MMMM yyyy") : QString(); }
QString ClockController::subtitle() const {
    if (mode_ == "zones") return regions_.value(region_).replace('_', ' ');
    if (mode_ == "manual") return "Set date and time";
    return snapshot_.available ? snapshot_.zone.section('/', -1).replace('_', ' ') + " · "
        + (snapshot_.automatic ? (snapshot_.synchronized ? "Synced" : "Waiting for sync") : "Manual time") : QString();
}
QStringList ClockController::cities() const {
    QStringList result;
    for (const auto& zone : snapshot_.zones) if (zone.section('/', 0, 0) == regions_.value(region_)) result.append(zone);
    return result;
}
QVariantList ClockController::rows() const {
    QVariantList result;
    const auto add = [&](QString label, QString detail = {}, bool enabled = true) {
        result.append(QVariantMap{{"label", label}, {"detail", detail}, {"enabled", enabled}});
    };
    if (mode_ == "zones") {
        for (const auto& zone : cities()) add(zone.contains('/') ? zone.section('/', 1).replace('_', ' ') : zone, zone == snapshot_.zone ? "Current" : "");
    } else if (mode_ == "manual") {
        add("Year", QString::number(draft_.date().year()));
        add("Month", draft_.toString("MMMM")); add("Day", QString::number(draft_.date().day()));
        add("Hour", draft_.toString("HH")); add("Minute", draft_.toString("mm"));
        add("Save date & time");
    } else {
        add("Automatic time", snapshot_.automatic ? "On" : "Off", snapshot_.available && snapshot_.canSync);
        add("Time zone", QString(snapshot_.zone).replace('_', ' '), snapshot_.available);
        add("Date & time", snapshot_.automatic ? "Turn off automatic time to change" : "Set manually", snapshot_.available && !snapshot_.automatic);
        if (onboarding_) add("Continue");
    }
    return result;
}
void ClockController::refresh(const QString& operation, const QString& value) {
    if (busy_) return;
    busy_ = true; error_.clear(); emit changed();
    if (worker_) { worker_->wait(); delete worker_; }
    worker_ = QThread::create([this, operation, value, zone = snapshot_.zone, reader = read, writer = write] {
        QString failure;
        if (operation == "time") {
            const auto current = reader();
            if (!current.available || current.automatic || current.zone != zone)
                failure = "Time settings changed. Return and check them before saving.";
        }
        if (!operation.isEmpty() && failure.isEmpty()) failure = writer(operation, value);
        auto snapshot = reader();
        QMetaObject::invokeMethod(this, [this, operation, failure, snapshot] {
            busy_ = false; snapshot_ = snapshot; error_ = failure.isEmpty() ? snapshot.error : failure;
            if (!operation.isEmpty() && error_.isEmpty()) { mode_ = "main"; focus_ = operation == "zone" ? 1 : operation == "time" ? 2 : 0; }
            emit changed();
        }, Qt::QueuedConnection);
    });
    worker_->start();
}
void ClockController::openZones() {
    regions_.clear();
    for (const auto& zone : snapshot_.zones) {
        const auto region = zone.section('/', 0, 0); if (!regions_.contains(region)) regions_.append(region);
    }
    region_ = std::max(0, int(regions_.indexOf(snapshot_.zone.section('/', 0, 0))));
    focus_ = std::max(0, int(cities().indexOf(snapshot_.zone))); mode_ = "zones";
}
void ClockController::activate(int index) {
    if (busy_ || index < 0 || index >= rows().size()) return;
    focus_ = index;
    if (!rows()[index].toMap()["enabled"].toBool()) { emit changed(); return; }
    if (mode_ == "zones") refresh("zone", cities().value(index));
    else if (mode_ == "manual") {
        if (index == 5) {
            if (!draft_.isValid()) error_ = "This local time does not exist. Choose another time.";
            else refresh("time", QString::number(draft_.toSecsSinceEpoch()));
        } else focus_ = index + 1;
    } else if (index == 0) refresh("automatic", snapshot_.automatic ? "false" : "true");
    else if (index == 1) openZones();
    else if (index == 2) { draft_ = now(); draft_.setTime(QTime(draft_.time().hour(), draft_.time().minute())); mode_ = "manual"; focus_ = 0; }
    else if (index == 3 && onboarding_) emit continueRequested();
    emit changed();
}
void ClockController::adjust(int direction) {
    if (busy_ || !direction) return;
    const int delta = direction < 0 ? -1 : 1;
    if (mode_ == "zones" && !regions_.isEmpty()) { region_ = (region_ + delta + regions_.size()) % regions_.size(); focus_ = 0; }
    else if (mode_ == "manual") {
        if (focus_ == 0) { auto date = draft_.date().addYears(delta); if (date.year() >= 2000 && date.year() <= 2099) draft_.setDate(date); }
        else if (focus_ == 1) draft_.setDate(draft_.date().addMonths(delta));
        else if (focus_ == 2) draft_.setDate(draft_.date().addDays(delta));
        else if (focus_ == 3) draft_ = draft_.addSecs(delta * 3600);
        else if (focus_ == 4) draft_ = draft_.addSecs(delta * 60);
        error_.clear();
    } else if (focus_ == 0 && snapshot_.automatic != (delta > 0)) activate(0);
    emit changed();
}
void ClockController::dispatch(Action action) {
    if (busy_) return;
    if (action == Action::Back) {
        if (mode_ != "main") { focus_ = mode_ == "zones" ? 1 : 2; mode_ = "main"; error_.clear(); emit changed(); }
        else emit backRequested();
    } else if (action == Action::Confirm) activate(focus_);
    else if (action == Action::Left || action == Action::Right) adjust(action == Action::Left ? -1 : 1);
    else if (action == Action::Up || action == Action::Down) { focus_ = std::clamp(focus_ + (action == Action::Up ? -1 : 1), 0, std::max(0, int(rows().size()) - 1)); emit changed(); }
}
}
