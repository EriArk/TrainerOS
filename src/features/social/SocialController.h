#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QThread>
#include <QPointer>
#include <QVariantMap>
#include <QVariantList>
#include <QHash>
#include <QTimer>
#include "SocialMedia.h"
#include "SocialStream.h"
#include "LobbyCoordinator.h"
#include "LobbyReminders.h"
#include "LobbyRuntime.h"
#include "RecentTogether.h"
#include <QJsonArray>

namespace trainer {
class FluxerSession;
class NativeActivityProvider;
class SocialController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool contacts READ contacts NOTIFY changed)
    Q_PROPERTY(QString searchKind READ searchKind NOTIFY changed)
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(int searchFocus READ searchFocus NOTIFY changed)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY changed)
    Q_PROPERTY(QVariantMap account READ account NOTIFY changed)
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY changed)
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY changed)
    Q_PROPERTY(QVariantList hints READ hints NOTIFY changed)
    Q_PROPERTY(QString draft READ draft NOTIFY changed)
    Q_PROPERTY(bool lobbyChatVisible READ lobbyChatVisible NOTIFY changed)
    Q_PROPERTY(QString lobbyChatStatus READ lobbyChatStatus NOTIFY changed)
    Q_PROPERTY(QString lobbyPreparationStatus READ lobbyPreparationStatus NOTIFY changed)
    Q_PROPERTY(QVariantList recentPeople READ recentPeople NOTIFY changed)
    Q_PROPERTY(int recentFocus READ recentFocus NOTIFY changed)
    Q_PROPERTY(QString conversationName READ conversationName NOTIFY changed)
    Q_PROPERTY(bool conversation READ conversation NOTIFY changed)
    Q_PROPERTY(int focusIndex READ focusIndex NOTIFY changed)
    Q_PROPERTY(int messageIndex READ messageIndex NOTIFY changed)
    Q_PROPERTY(bool reading READ reading NOTIFY changed)
    Q_PROPERTY(QVariantList companyParties READ companyParties NOTIFY changed)
    Q_PROPERTY(bool partyFocused READ partyFocused NOTIFY changed)
    Q_PROPERTY(int partyIndex READ partyIndex NOTIFY changed)
    Q_PROPERTY(bool companyAvailable READ companyAvailable NOTIFY changed)
    Q_PROPERTY(QVariantMap gameParty READ gameParty NOTIFY changed)
    Q_PROPERTY(QVariantMap gameActivity READ gameActivity NOTIFY changed)
    Q_PROPERTY(bool togetherAvailable READ togetherAvailable NOTIFY changed)
    Q_PROPERTY(QStringList menu READ menu NOTIFY changed)
    Q_PROPERTY(int menuIndex READ menuIndex NOTIFY changed)
    Q_PROPERTY(QString menuTitle READ menuTitle NOTIFY changed)
    Q_PROPERTY(QString menuDetail READ menuDetail NOTIFY changed)
    Q_PROPERTY(bool fullMenuDetail READ fullMenuDetail NOTIFY changed)
    Q_PROPERTY(bool canCreateGroup READ canCreateGroup NOTIFY changed)
    Q_PROPERTY(bool runtimeSurface READ runtimeSurfaceOpen NOTIFY changed)
    Q_PROPERTY(QVariantList runtimeActions READ runtimeActions NOTIFY changed)
    Q_PROPERTY(bool surfaceAvailable READ surfaceAvailable WRITE setSurfaceAvailable NOTIFY presentationChanged)
    Q_PROPERTY(bool conversationVisible READ conversationVisible WRITE setConversationVisible NOTIFY presentationChanged)
    Q_PROPERTY(bool gameActive READ gameActive WRITE setGameActive NOTIFY presentationChanged)
    Q_PROPERTY(QString toastTitle READ toastTitle NOTIFY presentationChanged)
    Q_PROPERTY(QString toastText READ toastText NOTIFY presentationChanged)
    Q_PROPERTY(QVariantMap online READ online NOTIFY changed)
    Q_PROPERTY(trainer::SocialMedia* media READ media CONSTANT)
    Q_PROPERTY(trainer::SocialStream* stream READ stream CONSTANT)
    Q_PROPERTY(bool mediaPreview READ mediaPreview NOTIFY changed)
    Q_PROPERTY(QVariantMap person READ person NOTIFY changed)
    Q_PROPERTY(bool profileVisible READ profileVisible NOTIFY changed)
    Q_PROPERTY(bool emojiVisible READ emojiVisible NOTIFY changed)
    Q_PROPERTY(QVariantMap conversationInfo READ currentChat NOTIFY changed)
    Q_PROPERTY(QVariantMap lobbyState READ lobbyState NOTIFY changed)
    Q_PROPERTY(QVariantMap lobbyDirectory READ lobbyDirectory NOTIFY changed)
    Q_PROPERTY(QVariantList lobbyRows READ lobbyRows NOTIFY changed)
    Q_PROPERTY(QVariantList lobbyActions READ lobbyActions NOTIFY changed)
    Q_PROPERTY(int lobbyFocus READ lobbyFocus NOTIFY changed)
    Q_PROPERTY(int lobbyPage READ lobbyPage NOTIFY changed)
    Q_PROPERTY(int lobbyPageCount READ lobbyPageCount NOTIFY changed)
    Q_PROPERTY(bool lobbyRailFocused READ lobbyRailFocused NOTIFY changed)
    Q_PROPERTY(int lobbyRailFocus READ lobbyRailFocus NOTIFY changed)
    Q_PROPERTY(int lobbyActionFocus READ lobbyActionFocus NOTIFY changed)
    Q_PROPERTY(bool lobbyDetailFocused READ lobbyDetailFocused NOTIFY changed)
    Q_PROPERTY(QString lobbySearch READ lobbySearch NOTIFY changed)
    Q_PROPERTY(QString lobbyPlatform READ lobbyPlatform NOTIFY changed)
    Q_PROPERTY(QString lobbyMode READ lobbyMode NOTIFY changed)
    Q_PROPERTY(QString lobbyTopic READ lobbyTopic NOTIFY changed)
    Q_PROPERTY(bool lobbyFreeOnly READ lobbyFreeOnly NOTIFY changed)
public:
    SocialStream* stream() {return &stream_;}
    void setGameProcess(qint64 pid);
    Q_INVOKABLE void streamAction(QString action);
    Q_INVOKABLE void streamsMenu();
    quint64 accountGeneration() const { return generation_; }
    explicit SocialController(QObject* parent = nullptr);
    QVariantMap lobbyState() const {return lobby_.state().toVariantMap();}
    QVariantMap lobbyDirectory() const {return snapshot_["lobbyDirectory"].toMap();}
    QVariantList lobbyRows() const;
    QVariantList lobbyActions() const;
    int lobbyFocus() const;
    int lobbyPage() const;
    bool lobbyRailFocused() const {return lobbyRailFocused_;}
    int lobbyRailFocus() const {return lobbyRailFocus_;}
    Q_INVOKABLE void activateLobbyRail(int index);
    int lobbyPageCount() const {return qMax(1,(int(lobbyRows().size())-1+5)/6);}
    Q_INVOKABLE void turnLobbyPage(int delta);
    int lobbyActionFocus() const {return lobbyActionFocus_;}
    bool lobbyDetailFocused() const {return lobbyDetailFocused_;}
    QString lobbySearch() const {return lobbySearch_;}
    QString lobbyPlatform() const {return lobbyPlatform_;}
    QString lobbyMode() const {return lobbyMode_;}
    QString lobbyTopic() const {return lobbyTopic_;}
    bool lobbyFreeOnly() const {return lobbyFreeOnly_;}
    Q_INVOKABLE void selectLobby(int index);
    Q_INVOKABLE void lobbyAction(QString action);
    Q_INVOKABLE void editLobbySearch();
    Q_INVOKABLE void openLobby();
    void openLobbyForGame(const QString& key);
    void openLobbyForPeople(QString peer={},QString conversation={});
    QVariantList recentPeople() const;
    int recentFocus() const;
    Q_INVOKABLE void selectRecent(int index);
    Q_INVOKABLE void recentAction(QString action);
    void setLobbyGames(QVariantList games);
    LobbyCoordinator* lobbyCoordinator() {return &lobby_;}
    LobbyRuntime* lobbyRuntime() {return &lobbyRuntime_;}
    QString lobbyPreparationStatus() const {return lobbyRuntime_.status();}
    bool lobbyChatVisible() const;
    QString lobbyChatStatus() const {return lobbyChatStatus_;}
    Q_INVOKABLE void focusLobbyChat();
    ~SocialController() override;
    void setOwner(QString owner);
    void setLink(NativeActivityProvider* link);
    void setOnlineContext(bool available,bool writable);
    QVariantMap online() const;
    Q_INVOKABLE void answerOnline(bool accept);
    void setFace(QString face);
    QString face() const {return face_;}
    void dispatch(Action action);
    void showContacts();
    bool contacts() const { return contacts_; }
    QString searchKind() const { return searchKind_; }
    QString query() const { return query_; }
    int searchFocus() const { return searchFocus_; }
    QVariantList searchResults() const { return snapshot_.value("searchResults").toList(); }
    Q_INVOKABLE void setSearchKind(QString kind);
    Q_INVOKABLE void editSearch();
    Q_INVOKABLE void activateSearch(int index);

    void applyText(QString text);
    void preserveText(QString text);
    void closeMenu();
    QVariantMap account() const;
    QVariantList activePlayers() const;
    QVariantMap directConversation(const QString& peer) const;
    bool openDirectConversation(const QString& peer);
    void releaseDirectConversation(const QString& peer);
    QString trainerIdentity() const {return owner_;}
    QVariantList rows() const;
    QVariantList quickPeople() const;
    bool openQuickConversation(const QString& id);
    QVariantList messages() const { return snapshot_.value("messages").toList(); }
    QVariantList hints() const;
    QString draft() const { return drafts_.value(snapshot_.value("channel").toString()); }
    QString conversationName() const;
    bool conversation() const { return !snapshot_.value("channel").toString().isEmpty(); }
    int focusIndex() const {return focus_;}
    int messageIndex() const {return messageFocus_;}
    bool reading() const {return reading_;}
    QStringList menu() const {return menu_;}
    int menuIndex() const {return menuFocus_;}
    QString menuTitle() const {return menuTitle_;}
    QString menuDetail() const {return menuDetail_;}
    bool fullMenuDetail() const {
        if (runtimeSurfaceOpen()) return true;
        for (const auto& choice : joinChoices_) if (choice["game"].toMap()["privateProgress"].toBool()||choice["game"].toMap()["temporaryProgress"].toBool()) return true;
        return false;
    }
    bool canCreateGroup() const { return menuMode_=="create-group"&&!pickedPeople_.isEmpty()&&!snapshot_["mutationBusy"].toBool(); }
    Q_INVOKABLE void createSelectedGroup() { if(canCreateGroup())dispatch(Action::ToggleContinue); }
    SocialMedia* media(){return &media_;}
    bool mediaPreview() const{return menuMode_=="media-preview"||menuMode_=="media-view";}
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void compose();
    Q_INVOKABLE void sendDraft();
    Q_INVOKABLE void attachPicture();
    Q_INVOKABLE void recordVoice();
    Q_INVOKABLE void call();
    Q_INVOKABLE void contextRow(int index = -1);
    Q_INVOKABLE void collectionOptions();
    Q_INVOKABLE void emoji();
    Q_INVOKABLE void profile(QString id = {});
    Q_INVOKABLE void conversationProfile();
    QVariantMap person() const;
    bool profileVisible() const { return menuMode_=="profile"; }
    bool emojiVisible() const { return menuMode_=="emoji"; }
    Q_INVOKABLE void options() { if(menu_.isEmpty())openMenu(); }
    Q_INVOKABLE void selectMessage(int index);
    Q_INVOKABLE void back() { dispatch(Action::Back); }
    Q_INVOKABLE void retainMessagePosition(int index, bool latest);
    Q_INVOKABLE void together();
    Q_INVOKABLE void showFriends();
    Q_INVOKABLE void latest();
    Q_INVOKABLE void earlierMessages();
    void setRuntimeSurface(QString title, QString detail, QVariantList actions);
    bool runtimeSurfaceOpen() const { return menuMode_=="runtime" && !menu_.isEmpty(); }
    QVariantList runtimeActions() const { return runtimeActions_; }
    void setRuntimeInvitation(QVariantMap request);
    bool togetherAvailable() const;
    QString textSubmitLabel() const { return textPurpose_ == "message" ? "Send" : "Apply"; }
    bool textAllowsEmoji() const { return textPurpose_ == "message" || textPurpose_ == "edit-message"; }
    Q_INVOKABLE void login();
    Q_INVOKABLE void selectMenu(int index);
    bool surfaceAvailable() const { return surfaceAvailable_; }
    bool conversationVisible() const { return conversationVisible_; }
    bool gameActive() const{return gameActive_;}
    void setGameActive(bool active){if(gameActive_!=active){gameActive_=active;emit presentationChanged();emit changed();}}
    void controlCall(const QString& operation){emit commandRequested(operation,{});}
    QVariantMap gameParty() const{return gameParty_;}
    void setGameParty(QVariantMap state);
    QString runtimePeer() const;
    QVariantList runtimeCompanies() const;
    QVariantList companyParties() const;
    void setCompanyParties(QVariantList rows);
    QVariantMap companyAccess(const QString& company) const;
    bool companyAvailable() const;
    bool partyFocused() const{return partyFocus_;}
    int partyIndex() const{return partyIndex_;}
    Q_INVOKABLE void joinCompanyParty(int index);
    Q_INVOKABLE void editCompanyAccess();
    void refreshPartyBrowse();
    QVariantMap gameActivity() const;
    void setGameActivity(const QString& peer, const QVariantMap& offer);
    Q_INVOKABLE void joinGame();
    void setRuntimeContext(bool available, QVariantList capabilities);
    QVariantList runtimeFriends() const;
    void runtimeCommand(const QString& op,const QVariantMap& args={}){emit commandRequested(op,args);}
    bool answerCall(const QString& channel,bool accept,bool allowOngoing=false);
    QVariantList incomingCallActions(const QString& retainedChannel={}) const;
    void reviewCommand(const QString& operation,const QVariantMap& args){emit commandRequested(operation,args);}
    void setSurfaceAvailable(bool available);
    void setConversationVisible(bool visible);
    QString toastTitle() const { return toastTitle_; }
    QString toastText() const { return toastText_; }
    Q_INVOKABLE void presented(QString channel, QString message);
    QString notificationFace() const;
    void openNotification();
    QVariantList notifications() const;
    QString notificationFaceAt(int index) const;
    void openNotificationAt(int index);
    void dismissNotificationAt(int index);
    void dismissNotifications();
signals:
    void messageQueued(QString channel);
    void partyPacket(QString peer,QString name,QJsonObject packet);
    void partyFailed(QString peer);
    void partyReset();
    void partyQuery(QString peer);
    void companyQuery(QString company);
    void companyAccessChanged(QString company,QVariantMap access);
    void partyJoin(QString peer,QJsonObject expectedOffer);
    void partyLeave();
    void runtimeAction(QString action);
    void communicationSettingsRequested();
    void conversationsRequested();
    void lobbyRequested();
    void lobbyDeferred();
    void runtimeEstablished(QString activity,bool host);
    void runtimeFrame(QJsonObject frame);
    void runtimeProbeFailed(QString peer, QString message);
    void runtimeEnded();
    void changed();
    void presentationChanged();
    void textRequested(QString title, QString initial, int limit);
    void commandRequested(QString operation, QVariantMap args);
    void ownerRequested(QString owner, quint64 generation);
    void backgroundNotification(QString title,QString text);
    void reviewsChanged(QString identity,QVariantMap state);
private:
    void dispatchRecent(Action action);
    RecentTogether recentTogether_;
    QString recentSelection_;
    void dispatchLobby(Action action);
    void publishLobby();
    void syncLobbyChat();
    void openLobbyChat();
    QString lobbyChatKey_,lobbyChatRoom_,lobbyChatChannel_,lobbyChatStatus_;
    bool lobbyChatBusy_=false,lobbyChatFocused_=false;
    LobbyCoordinator lobby_{this};
    LobbyReminders lobbyReminders_;
    QTimer lobbyReminderTimer_;
    QJsonObject pendingLobbyReminder_;
    void syncLobbyReminder();
    void tickLobbyReminders();
    LobbyRuntime lobbyRuntime_{lobby_,this};
    QTimer lobbyPublishTimer_{this},lobbyHeartbeat_{this};
    QVariantList lobbyGames_;
    QString lobbySearch_,lobbyTopic_,publishedLobby_,presentedLobbyInvite_,lobbySelection_,lobbyPublicationTopic_="other";
    QString lobbyPlatform_,lobbyMode_;
    QString lobbyLastRoom_;
    bool lobbyRailFocused_=false;
    int lobbyRailFocus_=1;
    int rememberedLobbyRoom() const;
    void focusLobbyCard(int index);
    void dispatchLobbyCatalogue(Action action);
    int lobbyActionFocus_=0;
    bool lobbyFreeOnly_=false,lobbyDetailFocused_=false;
    bool lobbyPublicationVisible_=false;
    friend class SocialTests;
    friend class HomeSocialFixture;
    friend class RuntimeMultiplayerTests;
    friend class LobbySmokeScenario;
    friend class PlayerOverlaySmokeScenario;
    QVariantMap dismissedNotifications_;
    QVariantList activityNotifications_;
    QString presentedInvitation_;
    QString notificationSettingsKey_;
    void saveNotifications();
    QString notificationStamp(const QVariantMap& row) const;
    void receive(quint64 generation, QVariantMap snapshot);
    void openMenu();
    void nativeActivities();
    void groupDetails();
    QVariantList runtimeActions_;
    QVariantMap runtimeInvitation_;
    void openPeople(QString mode);
    QVariantMap currentChat() const;
    QVariantMap menuChat() const;
    QString profileTarget_, profileRequest_, menuGuild_;
    void confirmAction(QString title, QString operation, QString id = {});
    void send();
    void mediaMenu();
    SocialMedia media_{this};
    SocialStream stream_{this};
    qint64 streamGamePid_=0;
    QVariantList mediaChoices_;
    bool mediaSending_=false,mediaUncertain_=false;
    QThread thread_;
    FluxerSession* session_;
    QPointer<NativeActivityProvider> link_;
    bool onlineAvailable_=false,onlineWritable_=true;
    bool runtimeAvailable_=false,runtimeOnline_=false;
    QVariantList runtimeCapabilities_;
    QHash<QString,QVariantMap> gameActivities_;
    QVariantMap gameParty_;
    QVariantList companyParties_;
    QVariantMap joinOffer(const QString& peer) const;
    void addJoinChoice(QString label,const QString& command,const QString& peer,const QVariantMap& offer);
    void directJoin(const QString& peer,const QVariantMap& offer);
    QHash<QString,QVariantMap> joinChoices_;
    int partyIndex_=0;
    bool partyFocus_=false;
    QString companySettingsKey(const QString&) const;
    void companyAccessMenu();
    QTimer partyBrowse_{this};
    void publishOnlineContext();
    QVariantList onlineCapabilities_;
    QVariantMap snapshot_;
    QHash<QString,QString> drafts_;
    QString owner_, face_ = "chats", textPurpose_, textChannel_;
    QString directConversationLease_;
    QStringList menu_, menuCommands_;
    QString menuSubject_, query_;
    QString menuTitle_, menuDetail_, menuMode_, menuChannel_, textId_;
    QStringList pickedPeople_;
    QHash<QString,QString> editDrafts_;
    quint64 generation_ = 0;
    int focus_ = 0, messageFocus_ = 0, menuFocus_ = 0;
    bool reading_ = false, contacts_ = false, searchStarted_ = false;
    QString searchKind_ = "communities";
    int searchFocus_ = -1;
    QTimer selection_;
    QTimer draftSave_;
    QTimer toastTimer_;
    bool surfaceAvailable_ = false, conversationVisible_ = false;
    bool gameActive_=false;
    QString toastTitle_, toastText_, toastChannel_;
    QString draftFile_;
    void saveDrafts();
    void bindDrafts(const QString& accountId);
    void preview();
    void runSearch(int offset = 0);

};
}
