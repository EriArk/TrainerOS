#include "RetroArchDisc.h"
#include "RetroArchConfiguration.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace trainer::retroarch {
bool discPlatform(const QString& platform) { return platform == "segacd" || platform == "neogeocd"; }
namespace {
bool relativeFile(const QString& name) {
    return !name.isEmpty() && !QDir::isAbsolutePath(name) && !name.contains('\\')
        && !name.contains(':') && !name.contains(QChar::Null) && !name.split('/').contains("..")
        && !name.contains('\n') && !name.contains('\r');
}
bool containedFile(const QString& directory, const QString& name) {
    if (!relativeFile(name)) return false;
    const QFileInfo file(QDir(directory).filePath(name));
    const auto root = QFileInfo(directory).canonicalFilePath();
    return !root.isEmpty() && file.isFile() && file.isReadable() && file.size() > 0
        && file.canonicalFilePath().startsWith(root + '/');
}
}
bool verifiedDiscFirmware(const RetroArchInstallation& installation, const QString& platform, const std::atomic_bool& cancel) {
    if (!discPlatform(platform) || cancel) return false;
    const auto files = installation.discFirmware.value(platform);
    if (files.isEmpty() || files.size() > 8) return false;
    if (platform == "segacd")
        for (const auto& name : {"bios_CD_U.bin", "bios_CD_E.bin", "bios_CD_J.bin"})
            if (!files.contains(name)) return false;
    const auto directory = configuredPath(readSettings(installation.configFile), "system_directory");
    if (!safePath(directory)) return false;
    const QRegularExpression digest("^[0-9a-f]{64}$");
    for (auto it = files.begin(); it != files.end(); ++it) {
        if (platform == "neogeocd" && !it.key().startsWith("neocd/")) return false;
        if (!digest.match(it.value().toString()).hasMatch() || !containedFile(directory, it.key())
            || fileDigest(QDir(directory).filePath(it.key()), 1024 * 1024, cancel) != it.value().toString()) return false;
    }
    return !cancel;
}
QString validateDiscContent(const QString& path, const std::atomic_bool& cancel) {
    if (cancel) return "Opening was cancelled.";
    QFile file(path); const QFileInfo info(path);
    if (!info.isAbsolute() || !info.isFile() || !file.open(QIODevice::ReadOnly))
        return "The game disc couldn't be opened. Check storage or link the file again.";
    const auto extension = info.suffix().toLower();
    if (extension == "chd")
        return file.read(8) == "MComprHD" ? QString() : QString("This game disc isn't a readable CHD image. Link a valid copy.");
    if (extension != "cue" || file.size() > 65536) return "This game disc format needs play setup.";
    const auto bytes = file.readAll();
    if (file.error() != QFile::NoError || bytes.contains('\0')) return "The game disc's track list couldn't be read.";
    const QRegularExpression entry("^FILE\\s+(?:\"([^\"]+)\"|(\\S+))\\s+\\S+\\s*$", QRegularExpression::CaseInsensitiveOption);
    int files = 0; bool track = false;
    for (const auto& raw : QString::fromUtf8(bytes).split('\n')) {
        if (cancel) return "Opening was cancelled.";
        const auto line = raw.trimmed();
        if (line.startsWith("TRACK ", Qt::CaseInsensitive)) track = true;
        if (!line.startsWith("FILE", Qt::CaseInsensitive)) continue;
        const auto match = entry.match(line);
        const auto name = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        if (!match.hasMatch() || ++files > 99 || !containedFile(info.absolutePath(), name))
            return "A disc track is missing or outside this game's folder. Keep the track list and its files together.";
    }
    return files && track ? QString() : QString("The game disc's track list is incomplete.");
}
}
