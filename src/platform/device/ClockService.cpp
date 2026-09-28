#include "ClockService.h"
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimeZone>

namespace trainer {
namespace {
QString command(const QStringList& arguments, QString& output) {
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("LC_ALL", "C"); environment.insert("TZ", "UTC");
    process.setProcessEnvironment(environment);
    process.start("/usr/bin/timedatectl", QStringList{"--no-ask-password", "--no-pager"} + arguments);
    if (!process.waitForStarted(2000)) return "Date and time service is unavailable.";
    if (!process.waitForFinished(10000)) { process.kill(); process.waitForFinished(); return "Date and time service did not respond."; }
    output = QString::fromUtf8(process.readAllStandardOutput());
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode()) {
        const auto detail = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return detail.isEmpty() ? QString("Couldn't change date and time.") : detail.left(350);
    }
    return {};
}
}
ClockSnapshot readClock() {
    ClockSnapshot result; QString output;
    result.error = command({"show"}, output);
    if (!result.error.isEmpty()) return result;
    for (const auto& line : output.split('\n')) {
        const auto key = line.section('=', 0, 0), value = line.section('=', 1);
        if (key == "Timezone") result.zone = value.trimmed();
        if (key == "NTP") result.automatic = value.trimmed() == "yes";
        if (key == "CanNTP") result.canSync = value.trimmed() == "yes";
        if (key == "NTPSynchronized") result.synchronized = value.trimmed() == "yes";
    }
    result.available = QTimeZone(result.zone.toUtf8()).isValid();
    if (!result.available) { result.error = "The system time zone could not be read."; return result; }
    QString zones;
    result.error = command({"list-timezones"}, zones);
    if (result.error.isEmpty()) result.zones = zones.split('\n', Qt::SkipEmptyParts);
    if (!result.zones.contains(result.zone)) result.zones.append(result.zone);
    result.zones.sort();
    return result;
}
QString changeClock(const QString& operation, const QString& value) {
    QString output;
    if (operation == "zone" && QTimeZone::isTimeZoneIdAvailable(value.toUtf8()))
        return command({"set-timezone", value}, output);
    if (operation == "automatic" && (value == "true" || value == "false"))
        return command({"set-ntp", value}, output);
    if (operation == "time") {
        bool ok = false; const auto seconds = value.toLongLong(&ok);
        const auto date = QDateTime::fromSecsSinceEpoch(seconds, QTimeZone("UTC"));
        if (ok && date.date().year() >= 2000 && date.date().year() <= 2099)
            return command({"set-time", date.toString("yyyy-MM-dd HH:mm:ss") + " UTC"}, output);
    }
    return "Choose a valid date, time or time zone.";
}
}
