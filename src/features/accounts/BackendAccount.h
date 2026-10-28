#pragma once
#include "platform/storage/EncryptedCredentials.h"
#include <QJsonObject>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>
#include <QLockFile>
#include <memory>

namespace trainer {
// Owns backend credentials. QML and SDK receive presentation/identity only.
class BackendAccount final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString actionLabel READ actionLabel NOTIFY changed)
    Q_PROPERTY(QString detail READ detail NOTIFY changed)
    Q_PROPERTY(QVariantMap progression READ progression NOTIFY changed)
    Q_PROPERTY(QString rewardStatus READ rewardStatus NOTIFY changed)
    Q_PROPERTY(QString rewardActionLabel READ rewardActionLabel NOTIFY changed)
public:
    using Reply = std::function<void(int,QJsonObject)>;
    struct Services {
        std::function<void(QByteArray,QString,QJsonObject,QByteArray,Reply)> request;
        std::function<void(QString,std::function<void(bool,QByteArray)>)> read;
        std::function<void(QString,QByteArray,std::function<void(bool)>)> write;
        std::function<bool(QUrl)> openBrowser;
    };
    explicit BackendAccount(QObject* parent=nullptr);
    ~BackendAccount() override;
    // Test seams set before configure; production origin is fixed, never SDK-supplied.
    void setServices(Services value) { services_=std::move(value); }
    std::function<QString()> loginGuard;
    void configure(const QString& owner);
    void setSocialSubject(const QString& subject);
    QJsonObject snapshot() const;
    QString actionLabel() const;
    QString detail() const { return detail_; }
    Q_INVOKABLE void activate();
    void cancel();
    // Fixed, host-mediated game API. No caller-supplied URL or credential export.
    void playRequest(const QString& channel,const QString& app,const QString& method,QJsonObject args,Reply done);
    void closePlay(const QString& channel);
    // Composition supplies the current owner's saved SDK recipe; games cannot write it.
    void setPublicAvatar(const QString& owner,const QJsonObject& recipe);
    QVariantMap progression() const;
    QString rewardStatus() const { return rewardStatus_; }
    QString rewardActionLabel() const;
    // Current owner's credential and game locators, never stored in presentation.
    std::function<QJsonObject()> rewardSource;
    void refreshProgression(bool sync=true);
    void activateRewards();
    void copyRewardChallenge();
signals:
    void rewardsEarned(qint64 xp,qint64 sparks);
    void changed();
    void returnRequested();
private:
    void state(QString value,QString detail);
    void reset();
    void begin();
    void accept();
    void tokens(const QJsonObject& result);
    void refresh();
    void syncPublicAvatar();
    void rewardRequest(QString action,QJsonObject body,bool sync=false);
    bool adoptProgression(const QJsonObject&);
    void logout();
    void fail(const QString& message);
    void request(QByteArray method,QString path,QJsonObject body,Reply done,QByteArray access={});
    void read(std::function<void(bool,QByteArray)> done);
    void write(QByteArray bytes,std::function<void(bool)> done);
    QByteArray saved(bool pending=false) const;
    Services services_;
    QNetworkAccessManager network_{this};
    EncryptedCredentials encrypted_{this};
    QTcpServer listener_{this};
    QTimer expiry_{this}, refreshTimer_{this};
    QTimer avatarRetry_{this};
    QTimer progressionTimer_{this};
    QJsonObject progression_;
    QString rewardStatus_;
    bool rewardBusy_=false,progressionStale_=false;
    qint64 lastRewardSync_=0;
    QJsonObject publicAvatar_, publishedAvatar_;
    bool avatarBusy_=false;
    std::shared_ptr<QLockFile> lock_;
    QString owner_,key_,state_="disabled",detail_="Open your Trainer to sign in";
    QString attempt_,verifier_,callbackPath_,socialSubject_,subject_,player_,session_;
    QByteArray access_,refresh_;
    QDateTime accessExpiry_;
    quint64 epoch_=0;
    bool encryptedStore_=false;
    bool logoutPending_=false;
    struct PlayGrant { QByteArray token; QString ruleset; QDateTime expires; };
    QHash<QString,PlayGrant> playGrants_;
};
}
