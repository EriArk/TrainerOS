#pragma once
#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QHash>
#include <QVariantMap>

namespace trainer {
// A bounded, two-person provider-message session. The caller supplies authenticated
// DM authors; history is presentation only. No account credentials or save writes.
class OnlineLink final : public QObject {
    Q_OBJECT
public:
    explicit OnlineLink(QObject* parent=nullptr);
    void bind(QString account, QString endpoint);
    void setCapabilities(QJsonArray capabilities);
    void setAvailable(bool available);
    void probe(QString channel, QString peer);
    void invite(QString activity);
    void answer(bool accept);
    void close(QString reason={});
    void receive(QString channel, QString author, QString name, const QJsonObject& envelope);
    void sendFrame(QJsonObject frame);
    QVariantMap state() const;
    QString peerAccount() const { return peer_; }
    static QString encode(const QJsonObject&);
    static QJsonObject decode(const QString&);
    static QString identity(const QString& account,const QString& endpoint);
signals:
    void changed();
    void outgoing(QString channel, QString content);
    void established(QString selfIdentity, QString peerIdentity, QString name, QString activity, bool initiator);
    void frameReceived(QJsonObject frame);
    void ended();
private:
    friend class OnlineLinkTests;
    void emitPacket(QString kind,QJsonObject fields={});
    void tick();
    bool supports(const QJsonArray&,const QJsonObject&) const;
    QJsonObject activity(QString id) const;
    void activate();
    QString account_,endpoint_,boot_,peer_,peerEndpoint_,peerBoot_,channel_,peerName_;
    QString request_,session_,stage_="idle",status_;
    QJsonArray capabilities_,remoteCapabilities_;
    QJsonObject selected_;
    QTimer timer_;
    bool available_=false,initiator_=false;
    qint64 deadline_=0,lastProbe_=0;
    int sent_=0,received_=0;
    struct Parts { int total=0; QHash<int,QString> data; };
    QHash<int,Parts> parts_;
    QStringList seenProbes_;
};
}
