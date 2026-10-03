#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonDocument>
#include <QVariantMap>
#include <QHash>
#include <QTimer>
#include <QSet>
#include <QProcess>
#include <functional>
#include "OnlineLink.h"

class QNetworkAccessManager;
class QNetworkReply;
class QWebSocket;
namespace trainer {
class EncryptedCredentials;
class AltchaProof;
// Lives on its own event loop. Tokens and protocol payloads never cross into QML.
class FluxerSession final : public QObject {
    Q_OBJECT
public:
    struct Reply { int status = 0; QJsonDocument body; int retrySeconds = 0; };
    using Completion = std::function<void(Reply)>;
    using Transport = std::function<void(QByteArray, QString, QJsonObject, Completion, QByteArray)>;
    explicit FluxerSession(QObject* parent = nullptr);
    ~FluxerSession() override;
    // Deterministic transport seam; supplied before the worker starts, tests only.
    void setTransport(Transport transport) { transport_ = std::move(transport); }
public slots:
    void setOwner(QString owner, quint64 generation);
    void command(QString operation, QVariantMap arguments = {});
    void stop();
signals:
    void snapshot(quint64 generation, QVariantMap state);
    void sendFailed(quint64 generation, QString channel, QString text);
    void mutationFinished(quint64 generation, QString operation, QString channel, QString id, bool success);
    void incomingMessage(quint64 generation, QString channel, QString name, QString text);
    void attachmentFinished(quint64 generation, QString channel, int status);
    void reviewsChanged(quint64 generation, QString identity, QVariantMap state);
    void onlineEstablished(quint64 generation, QString self, QString peer, QString name, QString activity, bool initiator);
    void onlineFrame(quint64 generation, QJsonObject frame);
    void onlineEnded(quint64 generation);
    void runtimeProbeFailed(quint64 generation, QString peer, QString message);
private:
    friend class SocialTests;
    void reviewCommand(const QString& operation,const QVariantMap& args);
    void loadReviews(int page=1);
    void publishReviews();
    QString reviewIdentity_,reviewChannel_,reviewStatus_;
    QVariantList reviewRows_;
    QVariantMap ownReview_;
    quint64 reviewRevision_=0;
    int reviewPage_=1;
    bool reviewBusy_=false,reviewMore_=false,reviewFresh_=false;
    OnlineLink online_{this};
    quint64 runtimeProbeRevision_ = 0;
    QTimer onlineSendTimer_{this};
    QList<QPair<QString,QString>> onlineQueue_;
    bool onlineSending_=false;
    quint64 onlineSendRevision_=0;
    void sendOnline();
    void sendAttachment(const QVariantMap& args);
    bool attachmentBusy_ = false, notificationSound_ = true;
    QNetworkReply* attachmentReply_=nullptr;
    QProcess voiceProcess_{this};
    QTimer voiceHeartbeat_{this},voiceDeadline_{this};
    QString voiceChannel_,voiceConnection_,voiceState_,voiceStatus_;
    QByteArray voiceBuffer_;
    QByteArray voiceGrantIdentity_;
    bool voiceReplacing_ = false;
    bool voiceMuted_=true,voiceDeaf_=false;
    QJsonObject profile_;
    QString profileStatus_;
    bool profileBusy_=false;
    QString voiceInput_,voiceOutput_;
    int voiceVolume_=100;
    void updateProfile(const QJsonObject& user);
    void profileCommand(const QVariantMap& args);
    QJsonObject audioConfiguration() const;
    int voiceParticipants_=0;
    QHash<QString,QJsonObject> calls_;
    QHash<QString,QJsonObject> callNotices_;
    void updateCallNotice(const QString& type,const QString& channel,const QJsonObject& event);
    bool voiceAvailable() const;
    void voiceCommand(QString operation,const QVariantMap& args);
    void leaveVoice();
    void voiceStateUpdate(bool leave=false);
    void voiceGrant(const QJsonObject& grant);
    void reconcileVoice();
    void voiceWrite(const QJsonObject& command);
    void bindOnline();
    void request(QByteArray method, QString path, QJsonObject body, Completion done, bool anonymous = false, QByteArray captcha = {});
    void verifiedRequest(QByteArray method, QString path, QJsonObject body, Completion done, int attempt = 0, QByteArray captcha = {});
    AltchaProof* proof_ = nullptr;
    void applyReadState(const QJsonObject& state, bool gateway);
    void updateUnread(const QString& channel);
    void acknowledge(QString channel, QString message);
    QHash<QString,QString> readThrough_;
    QHash<QString,QString> quietThrough_;
    QHash<QString,quint64> readRevision_;
    bool readsReady_ = false, ackBusy_ = false, doNotDisturb_ = false, privatePreviews_ = true;
    QStringList muted_;
    void publish();
    void ensureConversation();
    void checkCommunities();
    void invalidateCommunity(const QString& guild);
    void createCommunity(QString name);
    void markCommunity(QString guild);
    bool communityOnly_ = false, communityChecking_ = false;
    QSet<QString> communityChecked_, communityMarked_;
    QHash<QString,quint64> communityRevision_;
    QHash<QString,QString> communityChannels_;
    quint64 guildListRevision_ = 0;
    QString communityStatus_, communityInvite_;
    void openConversation(const QString& id);
    void search(QString mode, QString text, int offset = 0);
    bool mutate(const QString& operation, const QVariantMap& args);
    bool mutationBusy_ = false, channelsLoading_ = false;
    quint64 channelRevision_ = 0;
    void remember(const QString& key, const QString& value);
    QString channelKind(const QJsonObject& channel) const;
    QString face_ = "chats", navigationKey_;
    QHash<QString,QString> preferred_;
    bool channelsLoaded_ = false, friendsLoaded_ = false, openingDm_ = false, openingGuild_ = false;
    QVariantList searchResults_;
    QString searchMode_ = "communities", searchText_, searchStatus_;
    int searchOffset_ = 0, searchTotal_ = 0;
    quint64 searchRevision_ = 0;
    bool searching_ = false;

    void login();
    void pollLogin();
    void authenticated();
    void refresh();
    void refreshChannels();
    void loadMessages(QString channel);
    void loadOlderMessages();
    void retainHistory();
    void restoreHistory(const QString& channel);
    void loadHistoryCache();
    void saveHistoryCache();
    void clearHistoryCache();
    void sendText(QString channel, QString content, QString nonce);
    QHash<QString,QJsonObject> historyCache_;
    QStringList historyLru_;
    QString historyFile_, historyAnchor_;
    QTimer historySave_{this};
    bool historyBusy_ = false, historyMore_ = false, historyPast_ = false;
    quint64 historyRequest_ = 0;
    void openGateway();
    void gatewayEvent(const QJsonObject& event);
    void gatewaySend(int op, const QJsonValue& data);
    void credential(bool write = false, bool remove = false);
    bool credentialStoreAvailable() const;
    EncryptedCredentials* encryptedCredentials_ = nullptr;
    bool credentialLoading_ = false;
    void disconnected();
    void reset();
    void mergeMessage(const QJsonObject& message);
    void fail(const Reply& reply, QString fallback);
    QString label(const QJsonObject& user) const;
    QNetworkAccessManager* network_ = nullptr;
    QWebSocket* socket_ = nullptr;
    QTimer* poll_ = nullptr;
    QTimer* heartbeat_ = nullptr;
    QTimer* reconnect_ = nullptr;
    Transport transport_;
    QString guild_;
    QHash<QString,QJsonObject> guilds_;
    QString owner_, token_, self_, name_, status_ = "Sign in", code_, pollSecret_, gatewaySession_, channel_;
    QString state_ = "signed-out";
    quint64 generation_ = 0, epoch_ = 0, messageRevision_ = 0;
    qint64 sequence_ = 0, expires_ = 0, blockedUntil_ = 0;
    int reconnectAttempt_ = 0;
    bool awaitingAck_ = false, refreshing_ = false, polling_ = false, remembered_ = false;
    QHash<QString,QJsonObject> relationships_, channels_, messages_;
    QStringList messageOrder_;
    QHash<QString,QString> pendingNonces_;
    QHash<QString,int> unread_;
    QHash<QString,int> mentions_;
};
}
