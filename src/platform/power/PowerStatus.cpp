#include "PowerStatus.h"
#include <QDirIterator>
#include <QFile>
#include <QDir>
#include <QRegularExpression>

namespace trainer {
namespace {
QString attribute(const QString& directory, const QString& name) {
    QFile file(QDir(directory).filePath(name));
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto bytes = file.read(257);
    return file.error() == QFile::NoError && bytes.size() <= 256 ? QString::fromUtf8(bytes).trimmed() : QString();
}
}
BatterySnapshot readBatteryStatus(const QString& root) {
    QDirIterator supplies(root, QDir::Dirs | QDir::NoDotAndDotDot);
    QString battery; int count = 0;
    while (supplies.hasNext()) {
        const auto directory = supplies.next();
        if (++count > 64) return {};
        if (attribute(directory, "type") != "Battery" || attribute(directory, "scope") == "Device"
            || attribute(directory, "present") == "0") continue;
        // Do not average multiple packs or mistake a peripheral battery for the handheld.
        if (!battery.isEmpty()) return {};
        battery = directory;
    }
    if (battery.isEmpty()) return {};
    const auto capacity = attribute(battery, "capacity");
    static const QRegularExpression valid("^(?:[0-9]|[1-9][0-9]|100)$");
    if (!valid.match(capacity).hasMatch()) return {};
    const auto state = attribute(battery, "status");
    return {capacity.toInt(), QStringList{"Charging", "Discharging", "Full", "Not charging"}.contains(state) ? state : QString("Unknown")};
}
BatterySnapshot systemBatteryStatus() {
#ifdef Q_OS_LINUX
    return readBatteryStatus("/sys/class/power_supply");
#else
    return {};
#endif
}
PowerStatus::PowerStatus(std::function<BatterySnapshot()> reader, QObject* parent)
    : QObject(parent), reader_(std::move(reader)), worker_(new QObject) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    timer_.setInterval(30000);
    connect(&timer_, &QTimer::timeout, this, &PowerStatus::refresh);
    thread_.start();
}
PowerStatus::~PowerStatus() { timer_.stop(); thread_.quit(); thread_.wait(); }
void PowerStatus::start() { timer_.start(); refresh(); }
void PowerStatus::refresh() {
    if (busy_) return;
    busy_ = true;
    QMetaObject::invokeMethod(worker_, [this] {
        auto result = reader_();
        if (result.percent < 0 || result.percent > 100) result = {};
        QMetaObject::invokeMethod(this, [this, result] {
            busy_ = false;
            if (snapshot_ != result) { snapshot_ = result; emit changed(); }
            emit refreshed();
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
