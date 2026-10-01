#pragma once
#include "core/input/Action.h"
#include "platform/network/LocalLinkPeer.h"
#include "integrations/practice/PracticeSession.h"
#include "PracticeController.h"
#include "BattlePlayback.h"
#include "platform/network/NearbyService.h"
#include <QJsonArray>
namespace trainer {
class LinkController final:public QObject {
    Q_OBJECT
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString partner READ partner NOTIFY changed)
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QString turnSummary READ turnSummary NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantList fighters READ fighters NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool pending READ pending NOTIFY changed)
    Q_PROPERTY(QString priceText READ priceText NOTIFY changed)
    Q_PROPERTY(int battleTurn READ battleTurn NOTIFY changed)
    Q_PROPERTY(bool battleStarted READ battleStarted NOTIFY changed)
    Q_PROPERTY(QString battlePanel READ battlePanel NOTIFY changed)
    Q_PROPERTY(QString collectionName READ collectionName NOTIFY changed)
    Q_PROPERTY(QString stakeText READ stakeText NOTIFY changed)
    Q_PROPERTY(bool canSetTerms READ canSetTerms NOTIFY changed)
    Q_PROPERTY(QVariantList team READ team NOTIFY changed)
    Q_PROPERTY(QVariantList previewTeam READ previewTeam NOTIFY changed)
    Q_PROPERTY(int priceStep READ priceStep NOTIFY changed)
    Q_PROPERTY(bool invitationOpen READ invitationOpen NOTIFY changed)
    Q_PROPERTY(bool invitationIncoming READ invitationIncoming NOTIFY changed)
    Q_PROPERTY(QString invitationText READ invitationText NOTIFY changed)
    Q_PROPERTY(bool visibleNearby READ visibleNearby WRITE setVisibleNearby NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool searching READ searching NOTIFY changed)
    Q_PROPERTY(bool online READ online NOTIFY changed)
    Q_PROPERTY(QStringList activityModes READ activityModes NOTIFY changed)
public:
    using Backend=std::function<void(const QString&,const QJsonObject&,QObject*,std::function<void(QJsonObject)>)>;
    explicit LinkController(QObject* parent=nullptr);
    ~LinkController() override;
    void configure(const QString& root,const QString& id,const QString& name,Backend,
        PracticeController::Verifier,const QJsonObject& pending);
    void setObservation(const PracticeSource&,const GameProgress&,const QVariantList&);
    void setTrainerName(const QString& name);
    QJsonArray onlineCapabilities() const;
    bool beginOnline(QString self,QString peer,QString name,QString activity,bool initiator);
    void receiveOnline(QJsonObject frame);
    void showOnline();
    void endOnline();
    bool online() const{return online_;}
    QStringList activityModes() const{return online_?QStringList{"trade","sale","gift"}:QStringList{"battle","trade","sale","gift"};}
    void setVisibleNearby(bool);
    bool visibleNearby() const{return visibleNearby_;}
    void setInvitationsAllowed(bool);
    bool invitationOpen() const;
    bool invitationIncoming() const;
    QString invitationText() const;
    bool connected() const{return paired_;}
    bool searching() const{return directSearching_ && visibleNearby_ && invitationsAllowed_;}
    Q_INVOKABLE void answerInvitation(bool accept);
    Q_INVOKABLE void disconnectSession();
    void setArtwork(std::function<QVariantMap(QVariantMap)> resolve){artwork_=std::move(resolve);}
    void enter();void leave();void dispatch(Action);
    Q_INVOKABLE void activate(int);
    QString stage() const{return stage_;}QString message() const{return message_;}
    QString partner() const{return peerName_;}QString code() const{return pin_;}QString mode() const{return mode_;}
    QString turnSummary() const;
    QString priceText() const{return QString::number(price_);}
    int battleTurn() const{return playback_.active()?playback_.turn():battleState_["turn"].toInt();}
    bool battleStarted() const{return battleStarting_ || !battleState_.isEmpty();}
    QString battlePanel() const{return bag_?(bagItem_?"target":"bag"):bench_?"team":"moves";}
    QString collectionName() const;
    QString stakeText() const;
    bool canSetTerms() const{return host() && !confirmed_ && mode_=="battle" && stage_=="review";}
    QVariantList team() const;QVariantList previewTeam() const;
    int priceStep() const{return priceStep_;}
    bool isOpen() const{return open_;}
    QVariantList rows() const;QVariantList fighters() const;
    int focusIndex() const{return focus_;}
    bool active() const{return busy_ || pending() || !mode_.isEmpty() || invitationOpen();}
    bool navigationBlocked() const{return busy_ || pending() || !mode_.isEmpty();}
    bool pending() const{return !journal_.isEmpty() && journal_["stage"]!="complete" && journal_["stage"]!="cancelled";}
signals:
    void changed();void closeRequested();void saveChanged();void workspaceRequested();
    void connectionFailed(const QString&);
    void onlineSend(QJsonObject frame);
    void onlineClosed();
private:
    void receive(const QJsonObject&);void send(const QString&,QJsonObject={});
    void fail(const QString&);void pairReady();void startMode(const QString&);
    void choose(int);void review();void confirm();void advanceTrade();void battleChanged();
    void operation(const QString&,QJsonObject,std::function<void(QJsonObject)>);
    void verify(std::function<void()>);void resetChoice();void recover(const QString&);
    QVariantMap display(const QJsonObject&) const;QJsonObject partyMember(int) const;
    QString proposal() const;bool host() const{return localId()<peerId_;}
    bool sale() const{return mode_=="sale" || mode_=="gift";}
    bool seller() const{return sellerId_==localId();}
    bool transportConnected() const{return online_||peer_.connected();}
    QString localId() const{return online_?onlineSelf_:peer_.id();}
    void disconnectTransport();
    bool online_=false;QString onlineSelf_;
    void publishOffer();void beginBattle();void showBattle(const QJsonObject&);
    void finishBattle(const QJsonObject&);void battleRules(const QString&,int);
    void finishPlayback();void tryBattleTurn();
    void submitMove(int);void concede(int);QVariantList savedMembers() const;
    void updatePresence();void directEvent(const QJsonObject&);void inviteActivity(const QString&);
    void endConnectionAttempt(const QString&);
    QString battleResult() const;
    LocalLinkPeer peer_;PracticeSession battle_;QTimer heartbeat_;
    NearbyService nearby_;QTimer invitationTimer_,connectionTimer_;
    QString inviteId_,inviteMode_,inviteOwner_,directPeer_,directName_,directInterface_;
    bool visibleNearby_=true,invitationsAllowed_=true,directIncoming_=false,directAccepted_=false;
    bool directSearching_=false;
    bool directConnecting_=false;
    BattlePlayback playback_;QTimer playbackTimer_;bool turnSubmitted_=false;
    Backend backend_;PracticeController::Verifier verify_;std::function<QVariantMap(QVariantMap)> artwork_;
    PracticeSource source_;GameProgress progress_;QVariantList actors_;
    QString runtime_,stage_="browse",message_,mode_,peerId_,peerName_,pin_,nonce_,peerNonce_,peerPending_,transaction_,trainerName_;
    QJsonObject localChoice_,remoteChoice_,journal_,remoteJournal_,battleState_;
    bool open_=false,accepted_=false,peerAccepted_=false,paired_=false,busy_=false,confirmed_=false,remoteConfirmed_=false,moveSent_=false;
    int focus_=0,localMove_=-1,remoteMove_=-1;quint64 generation_=0;
    qint64 lastMessage_=0;
    QString sellerId_;int price_=1000,priceStep_=100;
    bool bench_=false,bag_=false,recovering_=false,battleStarting_=false;
    int forfeitSide_=-1,bagItem_=0,collection_=-1;QString stake_="none";int stakeAmount_=1000;
};
}
