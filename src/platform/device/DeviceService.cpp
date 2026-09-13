#include "DeviceService.h"
#include <QDirIterator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStorageInfo>
#include <cmath>
#include <algorithm>

namespace trainer {
namespace {
int number(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return -1;
    const auto text = file.read(32).trimmed();
    static const QRegularExpression valid("^[0-9]{1,9}$");
    return valid.match(QString::fromLatin1(text)).hasMatch() ? text.toInt() : -1;
}
#ifdef Q_OS_LINUX
QByteArray command(const QString& program, const QStringList& arguments) {
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("LC_ALL", "C");
    process.setProcessEnvironment(environment);
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(program, arguments);
    if (!process.waitForStarted(1000) || !process.waitForFinished(1500)) {
        process.kill(); process.waitForFinished(1000); return {};
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) return {};
    return process.read(4096);
}
QString changeCommand(const QStringList& arguments) {
    QProcess process;
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start("/usr/bin/wpctl", arguments);
    if (!process.waitForStarted(1000) || !process.waitForFinished(1500)) {
        process.kill(); process.waitForFinished(1000);
        return "The audio control did not respond. Try again.";
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0 ? QString()
        : "The volume could not be changed. Check the audio device in Desktop Mode.";
}
#endif
void storage(const QString& directory, qint64& free, qint64& total) {
    if (directory.isEmpty() || !QFileInfo(directory).isDir()) return;
    QStorageInfo volume(directory);
    if (volume.isValid() && volume.isReady() && volume.bytesTotal() > 0 && volume.bytesAvailable() >= 0) {
        free = volume.bytesAvailable(); total = volume.bytesTotal();
    }
}
}
BacklightValue readBacklight(const QString& root) {
    QDirIterator entries(root, QDir::Dirs | QDir::NoDotAndDotDot);
    BacklightValue found; int count = 0;
    while (entries.hasNext()) {
        const auto directory = entries.next();
        if (++count > 16) return {};
        const int maximum = number(QDir(directory).filePath("max_brightness"));
        const int current = number(QDir(directory).filePath("brightness"));
        if (maximum < 1 || current < 0 || current > maximum) continue;
        if (!found.directory.isEmpty()) return {};
        found = {directory, current, maximum};
    }
    return found;
}
QString writeBacklight(const QString& root, int percent) {
    if (percent < 5 || percent > 100) return "Brightness must be between 5% and 100%.";
    const auto light = readBacklight(root);
    if (light.directory.isEmpty()) return "The screen brightness control is unavailable.";
    QFile file(QDir(light.directory).filePath("brightness"));
    if (!file.open(QIODevice::WriteOnly)) return "The screen brightness could not be changed.";
    const int value = std::clamp(int(std::lround(double(light.maximum) * percent / 100.0)), 1, light.maximum);
    const auto data = QByteArray::number(value) + '\n';
    if (file.write(data) != data.size() || !file.flush()) return "The screen brightness could not be changed.";
    return {};
}
DeviceSnapshot parseVolume(const QByteArray& output) {
    DeviceSnapshot result;
    static const QRegularExpression pattern("^Volume: ([0-9]+(?:\\.[0-9]+)?)( \\[MUTED\\])?$");
    const auto match = pattern.match(QString::fromUtf8(output.trimmed()));
    if (!match.hasMatch()) return result;
    bool valid = false; const double volume = match.captured(1).toDouble(&valid);
    if (!valid || !std::isfinite(volume) || volume < 0 || volume > 10) return result;
    result.volume = int(std::lround(volume * 100));
    result.muted = !match.captured(2).isEmpty();
    return result;
}
DeviceBackend systemDeviceBackend(const QString& dataDirectory, const QString& libraryDirectory) {
    return {[dataDirectory, libraryDirectory] {
        DeviceSnapshot result;
#ifdef Q_OS_LINUX
        result = parseVolume(command("/usr/bin/wpctl", {"get-volume", "@DEFAULT_AUDIO_SINK@"}));
        const auto light = readBacklight("/sys/class/backlight");
        if (light.maximum > 0) result.brightness = int(std::lround(double(light.current) * 100 / light.maximum));
        const auto network = command("/usr/bin/nmcli", {"-t", "-f", "STATE", "general"}).trimmed();
        result.network = network == "connected" ? "Connected" : network.startsWith("connected") ? "Limited connection"
            : network == "connecting" ? "Connecting" : network == "disconnected" || network == "asleep" ? "Offline" : "Unavailable";
#endif
        storage(dataDirectory, result.internalFree, result.internalTotal);
        storage(libraryDirectory, result.libraryFree, result.libraryTotal);
        return result;
    }, [](const QString& control, int value) -> QString {
#ifdef Q_OS_LINUX
        if (control == "brightness") return writeBacklight("/sys/class/backlight", value);
        if (control == "volume" && value >= 0 && value <= 100)
            return changeCommand({"set-volume", "@DEFAULT_AUDIO_SINK@", QString::number(value / 100.0, 'f', 2)});
        if (control == "mute" && (value == 0 || value == 1))
            return changeCommand({"set-mute", "@DEFAULT_AUDIO_SINK@", QString::number(value)});
#else
        Q_UNUSED(control); Q_UNUSED(value);
#endif
        return "This device control is unavailable.";
    }};
}
DeviceService::DeviceService(DeviceBackend backend, QObject* parent)
    : QObject(parent), backend_(std::move(backend)), worker_(new QObject) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    thread_.start();
}
DeviceService::~DeviceService() { thread_.quit(); thread_.wait(); }
void DeviceService::refresh() { execute({}, 0); }
void DeviceService::setValue(const QString& control, int value) { execute(control, value); }
void DeviceService::execute(const QString& control, int value) {
    if (busy_) return;
    busy_ = true; error_.clear(); emit changed();
    QMetaObject::invokeMethod(worker_, [this, control, value] {
        const auto error = control.isEmpty() ? QString() : backend_.write(control, value);
        const auto result = backend_.read();
        QMetaObject::invokeMethod(this, [this, error, result] {
            snapshot_ = result; error_ = error; busy_ = false; emit changed();
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
