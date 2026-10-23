#pragma once
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace trainer {
// Only renderer caches may escape a temporary NetPlay profile. Each runtime
// build gets its own cache; saves, configuration and firmware stay session-owned.
inline bool prepareNetplayCache(const QString& sessionPath, const QString& family,
                                const QString& runtime) {
#ifdef Q_OS_UNIX
    const auto key = QString::fromLatin1(QCryptographicHash::hash(
        (family + ':' + runtime).toUtf8(), QCryptographicHash::Sha256).toHex());
    const auto base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (!base.isEmpty() && !runtime.isEmpty()) {
        const auto cache = base + "/TrainerOS/netplay/" + key;
        if (QDir().mkpath(cache) && QDir().mkpath(QFileInfo(sessionPath).absolutePath()) &&
            QFile::link(cache, sessionPath)) return true;
    }
#endif
    // Caching is optional: retain the previous private-directory behavior on
    // unsupported filesystems or when the cache location is not writable.
    return QDir().mkpath(sessionPath);
}
}
