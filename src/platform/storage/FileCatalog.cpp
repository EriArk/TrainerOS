#include "FileCatalog.h"
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QSet>
#include <QMimeDatabase>
#include <QLocale>
#include <QDateTime>
#include <algorithm>

namespace trainer {
LocalFileCatalog::LocalFileCatalog(QObject* parent) : FileCatalog(parent), worker_(new QObject) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    thread_.start();
}
LocalFileCatalog::~LocalFileCatalog() { thread_.quit(); thread_.wait(); }
void LocalFileCatalog::list(const QString& path, int page, QObject* context, std::function<void(DirectoryPage)> completed) {
    QMetaObject::invokeMethod(worker_, [this, path, page, guard = QPointer<QObject>(context), completed] {
        DirectoryPage result; result.path = path;
        if (path.isEmpty()) {
            QSet<QString> seen;
            const auto add = [&](const QString& entry, const QString& name, const QString& icon) {
                if (entry.isEmpty() || seen.contains(entry)) return;
                seen.insert(entry); result.entries.append({entry, name, true, icon, {}, {}});
            };
            add(QDir::homePath(), "Home", "user-home");
            add(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), "Downloads", "folder-download");
            add(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), "Documents", "folder-documents");
            add(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), "Pictures", "folder-pictures");
            for (const auto& disk : QStorageInfo::mountedVolumes()) {
                const auto mount=disk.rootPath();
                if(!disk.isValid() || !disk.isReady() || !QFileInfo(mount).isDir())continue;
#ifdef Q_OS_LINUX
                // User volumes, not the OS's bind mounts and pseudo-filesystems.
                if(mount!="/" && !(mount.startsWith("/run/media/") || mount.startsWith("/media/") || mount.startsWith("/mnt/") || mount.startsWith("/var/mnt/") || mount.startsWith(QDir::homePath()+'/')))continue;
#endif
                add(mount,mount=="/"?"Internal storage":disk.displayName().isEmpty()?QFileInfo(mount).fileName():disk.displayName(),"drive-harddisk");
            }
        } else {
            const QFileInfo folder(path);
            if (!folder.isDir() || !folder.isReadable()) result.error = "This folder isn't available. Go back or retry.";
            else {
                result.path = folder.absoluteFilePath();
                const auto entries = QDir(result.path).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                                   QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
                constexpr int PageSize = 80;
                result.pages = std::max(1, int((entries.size() + PageSize - 1) / PageSize));
                result.page = std::clamp(page, 0, result.pages - 1);
                QMimeDatabase mime;
                for (int i = result.page * PageSize; i < std::min(int(entries.size()), (result.page + 1) * PageSize); ++i) {
                    const auto& entry=entries[i];
                    result.entries.append({entry.absoluteFilePath(), entry.fileName(), entry.isDir(),
                        entry.isDir()?"folder":mime.mimeTypeForFile(entry,QMimeDatabase::MatchExtension).iconName(),
                        entry.isDir()?QString():QLocale().formattedDataSize(entry.size()),entry.lastModified().toString("dd MMM yyyy")});
                }
            }
        }
        QMetaObject::invokeMethod(this, [guard, completed, result] { if (guard) completed(result); }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
