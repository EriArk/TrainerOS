#pragma once
#include <QObject>
#include <QVariantList>
#include <memory>

namespace trainer {
// Owns jobs independently of the visible Files page. Native operations are
// delegated to libfm-qt/GIO; this boundary does not implement recursive copying.
class FilesService final : public QObject {
    Q_OBJECT
public:
    explicit FilesService(QObject* parent = nullptr);
    ~FilesService() override;
    void reset();
    void list(const QString& url);
    QVariantList places() const;
    QVariantList tasks() const;
    bool available() const;
    void start(const QString& operation, const QStringList& sources, const QString& destination = {});
    void answer(const QString& task, const QString& answer);
    void cancel(const QString& task = {});
Q_SIGNALS:
    void listed(QString url, QVariantList entries, QString error);
    void changed();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
