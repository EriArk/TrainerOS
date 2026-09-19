#pragma once
#include <QObject>
#include <QJsonObject>
#include <QVariantList>

namespace trainer {
// Temporary #60 bootstrap reader. Species/form identity is independent from
// Adventure media and optional animated sprites. Never decodes a raw master.
class ClassicArt final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString coverage READ coverage NOTIFY changed)
public:
    explicit ClassicArt(const QString& directory, QObject* parent = nullptr);
    QString coverage() const;
    QVariantMap image(const QString& target, const QString& profile) const;
    QVariantList choices(const QString& target) const;
    QString select(const QString& target, const QString& id);
signals:
    void changed();
private:
    QString selected(const QString& target) const;
    QVariantMap candidate(const QString& id, const QString& profile) const;
    QString directory_, error_;
    int mapped_ = 0;
    void countMapped();
    QJsonObject targets_, images_, preferences_;
};
}
