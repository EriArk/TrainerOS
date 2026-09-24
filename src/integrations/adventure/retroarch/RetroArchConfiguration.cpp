#include "RetroArchConfiguration.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDirIterator>

namespace trainer::retroarch {
QString fileDigest(const QString& path, qint64 limit, const std::atomic_bool& cancelled) {
    QFile f(path);
    if (cancelled || !f.open(QIODevice::ReadOnly) || f.size() <= 0 || f.size() > limit) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 total = 0;
    while (!f.atEnd() && !cancelled) {
        const auto bytes = f.read(65536);
        if (bytes.isEmpty() || (total += bytes.size()) > limit) return {};
        hash.addData(bytes);
    }
    return !cancelled && f.error() == QFile::NoError ? QString::fromLatin1(hash.result().toHex()) : QString();
}
Settings readSettings(const QString& path) {
    QFile f(path); Settings result;
    if (!f.open(QIODevice::ReadOnly) || f.size() > 1024 * 1024) return {};
    const auto lines = QString::fromUtf8(f.readAll()).split('\n');
    const QRegularExpression pattern("^([a-zA-Z0-9_]+)\\s*=\\s*\"([^\"]*)\"\\s*(?:#.*)?$");
    for (auto line : lines) {
        line = line.trimmed();
        if (line.startsWith("#include")) return {}; // Unverified config layering.
        const auto match = pattern.match(line);
        if (match.hasMatch()) {
            if (result.contains(match.captured(1))) return {}; // Ambiguous duplicate precedence is unverified.
            result.insert(match.captured(1), match.captured(2));
        }
    }
    return result;
}
bool safePath(const QString& path) {
    return QDir::isAbsolutePath(path) && !path.contains('"') && !path.contains('|')
        && !path.contains('\n') && !path.contains('\r') && !path.contains(QChar::Null);
}
bool enabled(const Settings& s, const QString& key, bool fallback) {
    return s.value(key, fallback ? "true" : "false") == "true";
}
QString configuredPath(const Settings& s, const QString& key, const QString& fallback) {
    auto path = s.value(key);
    if (path.isEmpty() || path == "default") path = fallback;
    if (path.startsWith("~/")) path = QDir::home().filePath(path.mid(2));
    return path;
}
QString configDirectory(const Settings& settings, const RetroArchInstallation& i) {
    const auto path = configuredPath(settings, "rgui_config_directory");
    return safePath(path) ? path : QFileInfo(i.configFile).dir().filePath("config");
}
QStringList contextFiles(const AdventureRegistration& r, const RetroArchInstallation& i, const Settings& s) {
    const auto config = configDirectory(s, i);
    const QFileInfo content(r.contentPath);
    const auto core = r.integrationConfig.value("core").toString("mgba");
    const QHash<QString, QString> names{{"mgba", "mGBA"}, {"snes9x", "Snes9x"},
        {"genesis_plus_gx", "Genesis Plus GX"}, {"picodrive", "PicoDrive"},
        {"mednafen_ngp", "Beetle NeoPop"}, {"mednafen_pce_fast", "Beetle PCE Fast"}, {"neocd", "NeoCD"}};
    const auto name = names.value(core);
    QStringList paths{i.configFile, i.cores.value(core), i.runtimeFile, r.contentPath};
    paths << configuredPath(s, "core_options_path", QFileInfo(i.configFile).dir().filePath("retroarch-core-options.cfg"));
    for (const auto& scope : {name, content.dir().dirName(), content.completeBaseName()})
        for (const auto& extension : {QString(".cfg"), QString(".opt")})
            paths << QDir(config).filePath(name + "/" + scope + extension);
    const auto system = configuredPath(s, "system_directory");
    if (core == "mgba" && safePath(system)) paths << QDir(system).filePath("gba_bios.bin");
    for (const auto& extension : {QString(".ips"), QString(".bps"), QString(".ups")})
        paths << content.dir().filePath(content.completeBaseName() + extension);
    return paths;
}
bool supportedConfiguration(const AdventureRegistration& r, const RetroArchInstallation& i, const Settings& s) {
    if (s.isEmpty() || (enabled(s, "cheevos_enable") && enabled(s, "cheevos_hardcore_mode_enable", true))
        || enabled(s, "netplay_start_as_server") || enabled(s, "netplay_start_as_client")) return false;
    if (enabled(s, "auto_overrides_enable", true)) {
        const auto files = contextFiles(r, i, s);
        for (int n = 5; n < files.size(); ++n)
            if (files[n].endsWith(".cfg") && QFileInfo::exists(files[n])) return false;
    }
    return true;
}
QString prepareFileMove(const AdventureRegistration& record,LibraryEdit& edit,const RetroArchInstallation& installation) {
    if(record.adventure.adapterId!="retroarch")return {};
    const auto settings=readSettings(installation.configFile);
    if(!supportedConfiguration(record,installation,settings))return "This game's play settings need to be checked before moving it.";
    for(const auto& key:QStringList{"savefiles_in_content_dir","sort_savefiles_enable","sort_savefiles_by_content_enable"})
        if(settings.value(key)!="true" && settings.value(key)!="false")return "This save layout needs to be checked before moving the game.";
    const auto oldFolder=QFileInfo(record.contentPath).dir().dirName(),newFolder=QFileInfo(edit.text).fileName();
    // Core/title overrides can redirect saves too. Keep this first route bounded
    // to the ordinary configuration used by TrainerOS, without changing it.
    QDirIterator configs(configDirectory(settings,installation),{"*.cfg","*.opt","*.rmp"},QDir::Files,QDirIterator::Subdirectories);
    while(configs.hasNext()) {
        const QFileInfo file(configs.next());const auto name=file.completeBaseName();
        if(file.suffix()=="cfg" || name==oldFolder || name==newFolder)
            return "This game uses custom play settings. Move it in Desktop Mode.";
    }
    auto base=record.integrationConfig["librarySaveBase"].toString();
    auto sortCore=record.integrationConfig["librarySaveSortCore"].toBool();
    if(base.isEmpty()) {
        base=enabled(settings,"savefiles_in_content_dir")?QFileInfo(record.contentPath).absolutePath():configuredPath(settings,"savefile_directory");
        if(!safePath(base))return "The save folder needs to be configured before moving this game.";
        if(enabled(settings,"sort_savefiles_by_content_enable"))base=QDir(base).filePath(oldFolder);
        sortCore=enabled(settings,"sort_savefiles_enable");
    }
    if(!safePath(base))return "The saved game folder is unavailable.";
    edit.retainedSaveBase=base;edit.retainedSaveSortCore=sortCore;
    return {};
}
}
