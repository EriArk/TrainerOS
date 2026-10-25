#include "MelonDsSave.h"
#include "integrations/adventure/CartridgeContent.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <optional>

namespace trainer {
namespace {
QString hashFile(const QString& path, qint64 limit) {
    QFile file(path);
    if (!QFileInfo(path).isAbsolute() || !file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > limit) return {};
    const qint64 size = file.size(); qint64 read = 0;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const auto bytes = file.read(65536);
        if (bytes.isEmpty() || (read += bytes.size()) > limit) return {};
        hash.addData(bytes);
    }
    return file.error() == QFile::NoError && read == size ? QString::fromLatin1(hash.result().toHex()) : QString();
}
// A deliberately narrow reader for melonDS 1.1's own serialized configuration.
// Other TOML spellings/layouts must be verified before they can select a save.
std::optional<QString> saveDirectory(const QByteArray& bytes) {
    const QRegularExpression table(R"(^\[([A-Za-z0-9_.]+)\]$)");
    const QRegularExpression value(R"(^([A-Za-z0-9_]+)\s*=\s*(.*)$)");
    const QRegularExpression path(R"toml(^"([^"\\\r\n]*)"$)toml");
    QString section; QHash<QString, QString> settings; QSet<QString> tables;
    for (auto line : QString::fromUtf8(bytes).split('\n')) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        if (line.contains("\"\"\"") || line.contains("'''")) return {};
        if (line.startsWith('[')) {
            const auto match = table.match(line);
            if (!match.hasMatch() || tables.contains(match.captured(1))) return {};
            section = match.captured(1); tables.insert(section);
            for (const auto& scalar : {QString("Instance0.SaveFilePath"), QString("Savestate.RelocSRAM"), QString("Emu.ConsoleType")})
                if (section == scalar || section.startsWith(scalar + '.')) return {};
            if (section.startsWith("Instance") && section != "Instance0" && !section.startsWith("Instance0.")) return {};
            continue;
        }
        // Do not treat an unsupported dotted/inline root spelling as absent.
        if (section.isEmpty()) {
            const auto root = value.match(line);
            if (!root.hasMatch() || root.captured(1).startsWith("Instance") || root.captured(1) == "Savestate" || root.captured(1) == "Emu") return {};
        }
        if (section != "Instance0" && section != "Savestate" && section != "Emu") continue;
        const auto match = value.match(line);
        if (!match.hasMatch()) return {};
        const QString key = section + '.' + match.captured(1);
        if (settings.contains(key)) return {};
        settings.insert(key, match.captured(2));
    }
    if (settings.value("Savestate.RelocSRAM", "false") != "false" || settings.value("Emu.ConsoleType", "0") != "0") return {};
    const auto match = path.match(settings.value("Instance0.SaveFilePath", "\"\""));
    const auto directory = match.captured(1);
    // melonDS 1.1 defaults: DS, no SRAM relocation, and save beside ROM.
    if (!match.hasMatch() || (!directory.isEmpty() && !QDir::isAbsolutePath(directory)) || directory.contains(QChar::Null)) return {};
    return directory;
}
}
bool supportsMelonDsSave(const AdventureRegistration& record, const StandaloneInstallation& installation) {
    return installation.melonDsSaveBackups && installation.platforms.contains("nds")
        && record.adventure.adapterId == "melonds" && record.adventure.platformId == "nds"
        && !record.adventure.collectionOnly && QStringList{"nds","srl","zip","7z"}.contains(QFileInfo(record.contentPath).suffix().toLower());
}
SaveTarget resolveMelonDsSave(const AdventureRegistration& record, const StandaloneInstallation& installation) {
    SaveTarget target; target.adventureId = record.adventure.id; target.title = record.adventure.title;
    if (!supportsMelonDsSave(record, installation)) return target;
#ifdef Q_OS_LINUX
    QDirIterator processes("/proc", QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    while (processes.hasNext()) {
        processes.next(); bool number = false; processes.fileName().toUInt(&number); if (!number) continue;
        QFile comm(QDir(processes.filePath()).filePath("comm"));
        QFile stat(QDir(processes.filePath()).filePath("stat"));
        if (!stat.open(QIODevice::ReadOnly)) continue;
        const auto state = stat.read(4096); const auto end = state.lastIndexOf(')');
        if (end < 0 || state.mid(end + 2, 1) == "Z") continue;
        // AppImage preserves its launcher name (truncated to 15 bytes in comm).
        if (comm.open(QIODevice::ReadOnly) && comm.read(128).trimmed().toLower().startsWith("melonds")) {
            target.error = "Close the running Adventure before checking or changing saves."; return target;
        }
    }
#endif
    QFile config(installation.configFile);
    if (!config.open(QIODevice::ReadOnly) || config.size() > 65536) return target;
    const auto bytes = config.read(65537);
    const auto directory = saveDirectory(bytes);
    if (config.error() != QFile::NoError || bytes.size() > 65536 || !directory.has_value()) {
        target.error = "This save layout needs verification before backups can be used."; return target;
    }
    const bool archived=cartridge::archived(record.contentPath);
    const auto image=archived?cartridge::read(record.contentPath,"nds",std::atomic_bool{false}):cartridge::Image{};
    if(archived&&image.bytes.size()<512)return target;
    target.contentRevision = hashFile(record.contentPath, (512LL+16) * 1024 * 1024);
    const auto runtime = hashFile(installation.runtimeFile, 256LL * 1024 * 1024);
    if (target.contentRevision.isEmpty() || runtime.isEmpty()) {
        target.error = "Game content or play setup couldn't be verified. Check storage and try again."; return target;
    }
    const QJsonArray context{QString("melonds-sav-v1"), installation.program, installation.runtimeFile, runtime,
        installation.configFile, QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()),
        QJsonArray::fromStringList(installation.prefixArguments)};
    target.contextRevision = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(context).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256).toHex());
    target.savePath = QDir(directory->isEmpty() ? QFileInfo(record.contentPath).absolutePath() : *directory).filePath(QFileInfo(archived?image.memberName:record.contentPath).completeBaseName() + ".sav");
    target.supported = true;
    return target;
}
}
