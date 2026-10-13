#pragma once
#include <QObject>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>
#include <QSet>
#include <QHash>
#include <functional>

namespace trainer {
// Account-local, journalled cooperation between accepted lobby members. Only
// the requester removes a relationship, after the recipient releases its lease.
class TemporaryContacts final : public QObject {
    Q_OBJECT
public:
    struct Reply {int status=0;QJsonDocument body;int retrySeconds=0;};
    using Completion=std::function<void(Reply)>;
    using Request=std::function<void(QByteArray,QString,QJsonObject,Completion)>;
    explicit TemporaryContacts(QObject* parent=nullptr):QObject(parent) {}
    void setRequest(Request request) {request_=std::move(request);}
    void configure(QString file,QString self);
    void context(QJsonObject lobby,bool online);
    void setHeld(QSet<QString> peers) {held_=std::move(peers);}
    void tick(qint64 now);
    void receive(QString peer,const QJsonObject& packet);
    bool keep(const QString& peer);
    // Explicit block/remove/accept overrides automation before the API write.
    bool manual(const QString& peer);
    QVariantMap state() const;
    bool managed(const QString& peer) const;
signals:
    void changed();
    void outgoing(QString peer,QJsonObject packet);
    void observed(QString peer,QJsonObject relationship);
private:
    friend class TemporaryContactsTests;
    bool put(const QString& peer,QJsonObject record);
    bool write(const QJsonObject& records);
    void send(const QString& peer,QString operation);
    void read(const QString& peer,QString phase);
    void mutate(const QString& peer,QString phase,QByteArray method,QJsonObject body,int expectedType);
    void request(const QString& peer,QString phase,QByteArray method,QString path,QJsonObject body,Completion completion);
    void fail(const QString& peer,QString text);
    void release(const QString& peer);
    bool active(const QString& peer) const;
    QString file_,self_,room_,host_,busy_,storageError_;
    QJsonObject records_;
    QSet<QString> members_,held_;
    QHash<QString,qint64> next_;
    Request request_;
    quint64 epoch_=0,flight_=0;
    qint64 now_=0,deadline_=0,nextCreate_=0;
    bool online_=false;
};
}
