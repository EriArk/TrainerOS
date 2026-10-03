#pragma once
#include "core/input/Action.h"
#include <QObject>
#include <QThread>
#include <QVariantMap>
#include <QVariantList>
#include <QHash>
#include <QTimer>
#include "SocialMedia.h"

namespace trainer {
class FluxerSession;
class LinkController;
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
    Q_PROPERTY(bool surfaceAvailable READ surfaceAvailable WRITE setSurfaceAvailable NOTIFY presentationChanged)
    Q_PROPERTY(bool conversationVisible READ conversationVisible WRITE setConversationVisible NOTIFY presentationChanged)
    Q_PROPERTY(bool gameActive READ gameActive WRITE setGameActive NOTIFY presentationChanged)
    Q_PROPERTY(QString toastTitle READ toastTitle NOTIFY presentationChanged)
    Q_PROPERTY(QString toastText READ toastText NOTIFY presentationChanged)
    Q_PROPERTY(QVariantMap online READ online NOTIFY changed)
    Q_PROPERTY(trainer::SocialMedia* media READ media CONSTANT)
    Q_PROPERTY(bool mediaPreview READ mediaPreview NOTIFY changed)
public:
    explicit SocialController(QObject* parent = nullptr);
    ~SocialController() override;
    void setOwner(QString owner);
    void setLink(LinkController* link);
    void setOnlineContext(bool available,bool writable);
    QVariantMap online() const;
    Q_INVOKABLE void answerOnline(bool accept);
    void setFace(QString face);
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
    QVariantList rows() const;
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
    SocialMedia* media(){return &media_;}
    bool mediaPreview() const{return menuMode_=="media-preview"||menuMode_=="media-view";}
    Q_INVOKABLE void activate(int index);
    Q_INVOKABLE void compose();
    Q_INVOKABLE void together();
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
    void partyPacket(QString peer,QString name,QJsonObject packet);
    void partyFailed(QString peer);
    void partyReset();
    void partyQuery(QString peer);
    void companyQuery(QString company);
    void companyAccessChanged(QString company,QVariantMap access);
    void partyJoin(QString peer);
    void partyLeave();
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
    friend class SocialTests;
    QVariantMap dismissedNotifications_;
    QVariantList activityNotifications_;
    QString presentedInvitation_;
    QString notificationSettingsKey_;
    void saveNotifications();
    QString notificationStamp(const QVariantMap& row) const;
    void receive(quint64 generation, QVariantMap snapshot);
    void openMenu();
    void openPeople(QString mode);
    QVariantMap currentChat() const;
    void confirmAction(QString title, QString operation, QString id = {});
    void send();
    void mediaMenu();
    SocialMedia media_{this};
    QVariantList mediaChoices_;
    bool mediaSending_=false,mediaUncertain_=false;
    QThread thread_;
    FluxerSession* session_;
    LinkController* link_=nullptr;
    bool onlineAvailable_=false,onlineWritable_=true;
    bool runtimeAvailable_=false,runtimeOnline_=false;
    QVariantList runtimeCapabilities_;
    QHash<QString,QVariantMap> gameActivities_;
    QVariantMap gameParty_;
    QVariantList companyParties_;
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
