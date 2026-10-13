#pragma once
#include <QJsonObject>
#include <QVariantList>

namespace trainer {
// Device-local encounter history, scoped to Trainer and communication account.
class RecentTogether {
public:
    void configure(QString file,QString self);
    bool observe(const QJsonObject& lobby,qint64 now);
    QVariantList rows() const;
    bool contains(const QString& peer) const {return people_.contains(peer);}
    bool forget(const QString& peer);
private:
    void save() const;
    QString file_,self_,lastRoster_;
    QJsonObject people_;
};
}
