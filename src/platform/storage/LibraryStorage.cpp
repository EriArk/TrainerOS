#include "LibraryStorage.h"
#include "core/repository/RomPlatforms.h"
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>
#include <QTemporaryFile>
#include <QSet>

namespace trainer {
QList<LibraryLocation> libraryLocations(const QString& currentRoot) {
    QList<LibraryLocation> result;
    QSet<QString> seen;
    const auto add = [&](const QString& label, const QString& path, const QString& existingParent) {
        QStorageInfo disk(existingParent);
        if (!disk.isValid() || !disk.isReady() || disk.isReadOnly()) return;
        const auto canonical = QFileInfo(path).canonicalFilePath();
        const auto target = canonical.isEmpty() ? QDir::cleanPath(path) : canonical;
        if (seen.contains(target)) return;
        seen.insert(target);
        result.append({label, target, disk.rootPath(), QString::fromUtf8(disk.device()), disk.bytesAvailable()});
    };
    if (QFileInfo(currentRoot).isDir()) add("Existing library", currentRoot, currentRoot);
    // A pre-existing SD alias must never be presented as internal memory.
    const auto defaultRoot = QDir::home().filePath("Emulation/roms");
    const bool alias = QFileInfo(defaultRoot).isSymLink() || QFileInfo(QDir::home().filePath("Emulation")).isSymLink();
    const auto internal = alias ? QDir::home().filePath(".local/share/TrainerOS/Games/roms") : defaultRoot;
    add("Internal storage", internal, QDir::homePath());
    for (const auto& disk : QStorageInfo::mountedVolumes()) {
        const auto mount = disk.rootPath();
        if (!(mount.startsWith("/run/media/") || mount.startsWith("/media/") || mount.startsWith("/mnt/") || mount.startsWith("/var/mnt/"))) continue;
        QString path;
        for (const auto& relative : {"Emulation/roms", "roms", "batocera/roms"}) {
            const auto candidate = QDir(mount).filePath(relative);
            if (QFileInfo(candidate).isDir()) { path = candidate; break; }
        }
        if (path.isEmpty()) path = QDir(mount).filePath("Emulation/roms");
        add(disk.name().isEmpty() ? "Memory card" : disk.name(), path, mount);
    }
    return result;
}

QString prepareLibraryLocation(const LibraryLocation& location) {
    QStorageInfo disk(location.mount);
    if (!disk.isValid() || !disk.isReady() || disk.isReadOnly()
        || disk.rootPath() != location.mount || QString::fromUtf8(disk.device()) != location.device)
        return "This storage is no longer available. Reconnect it and try again.";
    if (disk.bytesAvailable() < 1024 * 1024) return "This storage is full. Free some space and try again.";
    if (!QDir::isAbsolutePath(location.path)) return "Choose a valid library folder.";
    auto ancestor = QFileInfo(location.path);
    while (!ancestor.exists() && !ancestor.isSymLink()) {
        const auto parent = ancestor.absolutePath();
        if(parent==ancestor.absoluteFilePath())return "Choose a valid library folder.";
        ancestor = QFileInfo(parent);
    }
    const auto resolvedParent = ancestor.canonicalFilePath();
    const QStorageInfo parentDisk(resolvedParent);
    if (resolvedParent.isEmpty() || !ancestor.isDir() || parentDisk.device()!=disk.device()
        || parentDisk.rootPath()!=disk.rootPath()) return "The storage location changed. Choose it again.";
    if (!QDir().mkpath(location.path)) return "Couldn't prepare this folder. Choose another storage location.";
    const auto root = QFileInfo(location.path).canonicalFilePath();
    QStorageInfo actual(root);
    if (root.isEmpty() || actual.device() != disk.device()) return "The storage location changed. Choose it again.";
    QTemporaryFile probe(QDir(root).filePath(".traineros-write-XXXXXX"));
    if (!probe.open()) return "This folder is read-only. Choose another storage location.";
    // Preflight the whole tree before adding missing directories. Existing
    // aliases and media are preserved; unsafe aliases require explicit repair.
    QStringList missing;
    for (const auto& platform : romPlatforms()) {
        QStringList names{platform.folder, platform.id};
        names.append(platform.aliases); names.removeDuplicates();
        bool found = false;
        for (const auto& name : names) {
            const QFileInfo entry(QDir(root).filePath(name));
            if (!entry.exists() && !entry.isSymLink()) continue;
            const auto resolved = entry.canonicalFilePath();
            if (!entry.isDir() || !resolved.startsWith(root + '/'))
                return "Check the existing folder: " + name;
            found = true;
        }
        if (!found) missing.append(platform.folder);
    }
    for (const auto& name : missing)
        if (!QDir(root).mkdir(name)) return "Couldn't prepare the folder: " + name + ". Try again.";
    return {};
}
}
