#pragma once
#include <QObject>
#include <QThread>
#include <QString>
#include <QList>
#include <functional>

namespace trainer {
struct FileEntry { QString path, name; bool directory = false; };
struct DirectoryPage {
    QString path, error;
    QList<FileEntry> entries;
    int page = 0, pages = 1;
};
class FileCatalog : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void list(const QString& path, int page, QObject* context, std::function<void(DirectoryPage)>) = 0;
};
class LocalFileCatalog final : public FileCatalog {
    Q_OBJECT
public:
    explicit LocalFileCatalog(QObject* parent = nullptr);
    ~LocalFileCatalog() override;
    void list(const QString&, int, QObject*, std::function<void(DirectoryPage)>) override;
private:
    QThread thread_;
    QObject* worker_;
};
}
