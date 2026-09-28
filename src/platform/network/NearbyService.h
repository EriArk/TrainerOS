#pragma once
#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QVariantList>
#include <QTimer>
namespace trainer {
class NearbyService final: public QObject {
    Q_OBJECT
public:
    explicit NearbyService(QObject* parent=nullptr);
    ~NearbyService();
    void configure(const QString& id,const QString& name,bool visible);
    void request(const QJsonObject&);
    QVariantList peers() const{return peers_;}
signals:
    void event(const QJsonObject&);
private:
    QProcess process_;QByteArray buffer_;QJsonObject config_;QVariantList peers_;
    QTimer retry_;
};
}
