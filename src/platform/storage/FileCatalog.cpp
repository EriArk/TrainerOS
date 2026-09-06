#include "FileCatalog.h"
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QSet>
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
            const auto add = [&](const QString& entry, const QString& name) {
                if (entry.isEmpty() || seen.contains(entry)) return;
                seen.insert(entry); result.entries.append({entry, name, true});
            };
            add(QDir::homePath(), "Home folder");
            for (const auto& disk : QStorageInfo::mountedVolumes()) if (disk.isValid() && disk.isReady())
                add(disk.rootPath(), disk.displayName().isEmpty() ? disk.rootPath() : disk.displayName() + " · " + disk.rootPath());
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
                for (int i = result.page * PageSize; i < std::min(int(entries.size()), (result.page + 1) * PageSize); ++i)
                    result.entries.append({entries[i].absoluteFilePath(), entries[i].fileName(), entries[i].isDir()});
            }
        }
        QMetaObject::invokeMethod(this, [guard, completed, result] { if (guard) completed(result); }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
}
