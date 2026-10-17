#pragma once
#include "core/input/Action.h"
#include "GameParty.h"
#include "HotseatHandoff.h"
#include "LobbyGamePreparation.h"
#include "features/social/LobbyRuntime.h"
#include "core/model/GamePlayers.h"
#include "integrations/adventure/retroarch/RetroArchNetplay.h"
#include "integrations/adventure/retroarch/RetroArchNetplayClient.h"
#include "integrations/adventure/retroarch/LinkedSavePreparation.h"
#include "integrations/adventure/standalone/PpssppNetplay.h"
#include "integrations/adventure/standalone/DolphinNetplay.h"
#include "integrations/adventure/standalone/RmgNetplay.h"
#include "integrations/adventure/standalone/FlycastNetplay.h"
#include "integrations/adventure/standalone/MelonDsNetplay.h"
#include "integrations/adventure/standalone/AzaharNetplay.h"
#include "integrations/adventure/standalone/VitaNetplay.h"
#include "platform/network/LocalLinkPeer.h"
#include <QFutureWatcher>
#include <QNetworkAccessManager>

namespace trainer {
class SocialController;
class AdventureLaunchController;
class AdventureExitPresentation;
// One runtime session, composed with existing consent, process and Home services.
class RuntimeMultiplayer final : public LobbyRuntimeService {
    Q_OBJECT
    Q_PROPERTY(QVariantMap party READ party NOTIFY changed)
    Q_PROPERTY(bool incoming READ incoming NOTIFY changed)
    Q_PROPERTY(QString invitation READ invitation NOTIFY changed)
    Q_PROPERTY(QString invitationId READ invitationId NOTIFY changed)
    Q_PROPERTY(bool invitationBadge READ invitationBadge NOTIFY changed)
    Q_PROPERTY(QVariantMap gameModes READ gameModes NOTIFY availabilityChanged)
    Q_PROPERTY(QStringList onlineGames READ onlineGames NOTIFY availabilityChanged)
    Q_PROPERTY(QStringList nearbyGames READ nearbyGames NOTIFY availabilityChanged)
public:
    QString preparationProblem() const override {return lobbyProblem_;}
    using ImportCheck=std::function<void(QJsonObject,QString)>;
    void verifyImport(AdventureRegistration,QJsonObject,std::shared_ptr<std::atomic_bool>,ImportCheck);
    bool opening() const;
    QString openingGame() const {return game_;}
    QString openingStage() const {return status_;}
    void cancelOpening();
    RuntimeMultiplayer(LibraryRepository&, RetroArchAdapter&, StandaloneAdapter&, StandaloneAdapter&, SocialController&,
                       ProcessService&, AdventureLaunchController&, AdventureExitPresentation&);
    ~RuntimeMultiplayer() override;
    LobbyGamePreparation& gamePreparation() {return preparation_;}
    std::function<QVariantList(const QMap<QString,QJsonObject>&)> additionalLobbyGames;
    void refresh(const RetroArchInstallation&,QString trainer,bool allowed);
    void prepare(QString operation,QJsonObject context,Completion done) override;
    void launch(QString operation,QString ticket,QJsonObject context,Completion done) override;
    void cancel(QString operation) override;
    bool supportsLive(const QJsonObject& game) const override;
    bool admitLive(QString ticket,QString peer,QString operation,int seat) override;
    void releaseLive(QString peer,QString operation) override;
    QVariantMap party() const {return party_.state().toVariantMap();}
    Q_INVOKABLE void leaveParty();
    QString menuLabel() const;
    QVariantList gameMenuActions() const;
    bool canInvite() const;
    QVariantMap gameModes() const {return gameModes_;}
    QStringList onlineGames() const { return onlineGames_; }
    QStringList nearbyGames() const { return nearbyGames_; }
    QStringList multiplayerGames() const { return multiplayerGames_; }
    bool incoming() const;
    QString invitation() const;
    QString invitationId() const;
    bool invitationBadge() const;
    Q_INVOKABLE void openInvitation(const QString& request);
    Q_INVOKABLE void answer(bool accept);
    bool dispatch(Action);
    bool action(const QString&, bool fromSocial=false);
    void beginTogether(QString game={},QString peer={},bool company=false);
    bool contextAction(const QString&);
    void resumeLiveInvocation();
    void cancelInvocation() { liveInvocation_=false;++invocationGeneration_; }
    // Optional trusted runtime integration. Both checks are LOCAL capabilities;
    // a peer descriptor or matching title never grants an in-place join.
    struct RunningJoin {
        std::function<bool(const QString&,const QJsonObject&)> available;
        std::function<bool(const QString&,const QJsonObject&,const QJsonObject&)> join;
    } runningJoin;
    // Shared application launch barrier for a packaged alternate runtime.
    std::function<AdventureResult(const ProcessCommand&,const QString&)> requestRuntimeLaunch;
    QString saveBackupRoot;
    SaveTargetResolver resolveSaveTarget;
signals:
    void liveOptionsRequested();
    void invitationOptionsRequested();
    void invitationSurfaceRequested();
    void changed();
    void availabilityChanged();
    void notice(QString text);
private:
    friend class RuntimeMultiplayerTests;
    void startParty(bool host,const QJsonObject& endpoint);
    void setupHotseat();
    QJsonObject selectedControllerProfile(const QJsonObject&) const;
    QString controllerModeLabel() const;
    QString hotseatPlayerName(int slot) const;
    bool hotseat() const { return usesSharedController(descriptor_); }
    void update();
    void show(QString panel);
    void prepareHost();
    void resolveRelay();
    void resolvePspRelay();
    void prepareRmg();
    bool rmg() const { return descriptor_["id"]=="runtime.rmg.n64.v1"; }
    bool flycast() const { return descriptor_["id"]=="runtime.flycast-dojo.dc.v1"; }
    bool azahar() const { return descriptor_["id"]=="runtime.azahar.3ds.v1"; }
    bool vita() const { return descriptor_["id"]=="runtime.vita3k.lan.v1"; }
    bool ds() const { return descriptor_["id"]=="runtime.melonds.nds.v1"; }
    bool psp() const { return descriptor_["id"].toString().startsWith("runtime.ppsspp."); }
    bool dolphin() const { return descriptor_["id"].toString().startsWith("runtime.dolphin."); }
    void frame(const QJsonObject&);
    void send(QJsonObject);
    void launch();
    void prepareLinked(const QJsonObject& endpoint = {});
    void stopLinkedTransfers();
    void linkedTransferCompleted(int slot,QByteArray bytes);
    bool linkedRosterMatches() const;
    QMap<int,QString> linkedPeers_;
    QMap<int,retroarch::LinkedSavePreparation*> linkedTransfers_;
    bool linked() const { return requiresLinkedSavePreparation(descriptor_); }
    void fail(QString);
    void pollRelay();
    void output(const QByteArray&);
    LibraryRepository& library_;RetroArchAdapter& adapter_;SocialController& social_;
    StandaloneAdapter& ppsspp_;StandaloneAdapter& dolphin_;
    dolphin::NetplayRequest dolphinRequest_;
    flycast::Request flycastRequest_;
    melonds::Request dsRequest_;
    azahar::Request azaharRequest_;
    vita::Request vitaRequest_;
    rmg::Lobby rmgLobby_{this};
    rmg::Lobby::Request rmgRequest_;
    bool rmgPreparing_=false;
    ProcessService& process_;AdventureLaunchController& lifecycle_;AdventureExitPresentation& overlay_;
    LobbyGamePreparation preparation_;
    LocalLinkPeer nearby_{this,47855,47856,"trainerosRuntime",true};
    GameParty party_{this};
    HotseatHandoff handoff_{this};
    QString controllerMode_="separate";
    bool partySession_=false,companyOverride_=false;
    using ScannedGame = QPair<QString,QJsonObject>;
    QFutureWatcher<ScannedGame> scan_{this};
    QNetworkAccessManager network_{this};QTimer timer_{this};
    QString trainer_,identity_,game_,status_,transportPeer_,lastRequest_,scanPriority_;
    QString lobbyProblem_;
    QJsonObject descriptor_;
    QMap<QString,QJsonObject> profiles_,games_;
    QVariantMap gameModes_;
    QStringList onlineGames_,nearbyGames_,multiplayerGames_;
    retroarch::NetplayRequest request_;
    retroarch::NetplayClient client_{this};
    retroarch::LinkedSavePreparation linkedSave_{this};
    bool linkedPreparing_=false,linkedPrepared_=false;
    QJsonObject linkedEndpoint_;
    RetroArchInstallation installation_;
    bool allowed_=false,online_=false,host_=false,active_=false,restarting_=false,launchPending_=false,query_=false,relaySent_=false;
    qint64 deadline_=0;quint64 scanRevision_=0,sessionGeneration_=0;
    QByteArray output_;
    bool socialSurface_=false;
    QString socialPanel_;
    QString invocationGame_,invocationPeer_,invocationOwner_;
    bool invocationCompany_=false;
    bool liveInvocation_=false;
    qint64 invocationProcess_=0,invocationExpires_=0;
    quint64 invocationGeneration_=0;
    quint64 invocationAccountGeneration_=0;
    int invocationRevision_=0;
    QJsonObject invocationDescriptor_;
    void showInvocation();
    void resumeInvitation();
    void cancelInvitationExit();
    bool canJoinRunning(const QJsonObject&) const;
    QString openingInvitation_, exitingInvitation_;
    quint64 invitationAccount_ = 0;
    qint64 invitationProcess_ = 0;
    QTimer invitationOpenTimer_{this};
    QString lobbyOperation_,lobbyTicket_,lobbyGame_;
    QJsonObject lobbyContext_,lobbyDescriptor_;
    Completion lobbyCompletion_;
    int lobbyRevision_=0;
    QString lobbyPath_;
};
}
