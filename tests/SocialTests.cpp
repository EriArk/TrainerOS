#include "adapters/pokemon/PokemonExperience.h"
#include <QtTest>
#include "integrations/social/LobbyDirectory.h"
#include "features/adventure/GameParty.h"
#include "integrations/social/AdventureReviews.h"
#include "core/model/AdventureCompletion.h"
#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QSettings>
#include "integrations/social/FluxerSession.h"
#include "integrations/social/CommunityIdentity.h"
#include "integrations/social/LobbyChatIdentity.h"
#include "features/social/SocialController.h"
#include "features/social/SocialMedia.h"
#include "features/social/PlayerOverlayController.h"
#include "features/adventure/AdventureExitPresentation.h"
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QtEndian>
#include <QVideoFrame>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWebSocket>
#include "platform/storage/EncryptedCredentials.h"

namespace trainer {
// Held replies reproduce a dead pooled connection without public network access.
class HeldSocialReply final : public QNetworkReply {
public:
    explicit HeldSocialReply(QObject* parent):QNetworkReply(parent){open(QIODevice::ReadOnly);}
    void fail(int status=0) {
        if(isFinished())return;
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute,status);
        setError(QNetworkReply::TimeoutError,"Simulated route loss");
        setFinished(true);close();emit finished();
    }
    void abort() override {fail();}
    void succeed() {setAttribute(QNetworkRequest::HttpStatusCodeAttribute,200);setFinished(true);emit finished();}
protected:
    qint64 readData(char*,qint64) override {return -1;}
};
class HeldSocialNetwork final : public QNetworkAccessManager {
public:
    QList<HeldSocialReply*> replies;
    explicit HeldSocialNetwork(QObject* parent):QNetworkAccessManager(parent){}
protected:
    QNetworkReply* createRequest(Operation,const QNetworkRequest&,QIODevice*) override {
        auto* reply=new HeldSocialReply(this);replies.append(reply);return reply;
    }
};
class SocialTests : public QObject {
    Q_OBJECT
    using Reply=FluxerSession::Reply;
    using Completion=FluxerSession::Completion;
    static constexpr auto channel="1501314428688998182";
    static constexpr auto remote="1501314428688998183";
    static QJsonObject playedRoom(QString self="1",QString peer="2") {
        return {{"id","room"},{"phase","playing"},{"ticket","first-session"},{"game",QJsonObject{{"id","pong"},{"title","Pong"}}},
            {"members",QJsonArray{QJsonObject{{"id",self},{"name","Self"},{"connected",true},{"seat",1}},
                QJsonObject{{"id",peer},{"name","Guest"},{"connected",true},{"seat",2}}}}};
    }
    static void bind(FluxerSession& s) {
        s.setOwner("trainer-a",1);s.self_="1501314428688998181";s.token_="synthetic-test-session";s.state_="connected";
        s.channel_=channel;s.channels_[channel]={{"id",channel},{"type",1}};
    }
private slots:
    void nativeActivitiesSurviveRuntimeCatalogueRefresh() {
        SocialController c;
        const QVariantMap native{{"key","sdk:counter"},{"id","sdk:counter"},{"title","Counter"},{"platform","Apps"},{"mode","sdk.records.v1"},{"seats",2}};
        const QVariantMap emulator{{"key","nes"},{"id","nes"},{"title","NES game"},{"platform","nes"},{"mode","netplay"},{"seats",2}};
        c.setLobbyActivities({native});c.setLobbyGames({emulator});
        QCOMPARE(c.lobbyPickerRows().size(),2);
        c.setLobbyGames({emulator});QCOMPARE(c.lobbyPickerRows().size(),2);
        c.setLobbyActivities({});QCOMPARE(c.lobbyPickerRows().size(),1);
        QCOMPARE(c.lobbyPickerRows().first().toMap()["id"].toString(),"nes");
    }
    void directCallEndsWhenLastRemoteAccountLeaves() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,auto){});bind(s);
        s.voiceChannel_=channel;s.voiceState_="connected";s.voiceMuted_=false;
        s.voiceConnection_="local";s.voiceParticipants_=2;s.voicePeople_={s.self_,remote};
        auto update=[&](QJsonArray roster){s.gatewayEvent({{"op",0},{"t","CALL_UPDATE"},{"d",QJsonObject{
            {"channel_id",channel},{"message_id","123"},{"voice_states",roster}}}});};
        const QJsonObject self{{"user_id",s.self_},{"connection_id","local"}};
        const QJsonObject peer{{"user_id",remote},{"connection_id","peer"}};
        update({self,peer});QVERIFY(s.voicePeerSeen_);
        // Muting, leaving the selected DM and other-device self presence do not
        // change the lifetime. Only loss of the last remote account does.
        s.channel_=remote;update({self,peer});QCOMPARE(s.voiceChannel_,QString(channel));
        update({self,QJsonObject{{"user_id",s.self_},{"connection_id","other-self"}}});
        QVERIFY(s.voiceChannel_.isEmpty());QVERIFY(s.voiceConnection_.isEmpty());
        QVERIFY(s.voicePeople_.isEmpty());QVERIFY(s.voiceMuted_);QCOMPARE(s.voiceParticipants_,0);
        QCOMPARE(s.voiceStatus_,QString("Call ended"));QVERIFY(!s.voicePeerSeen_);QVERIFY(s.voiceCallMessage_.isEmpty());
    }
    void directCallWaitsForAnswerAndRetainsOtherPeerConnections() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,auto){});bind(s);
        s.voiceChannel_=channel;s.voiceState_="connecting";
        auto update=[&](QJsonArray roster){s.reconcileDirectCall(channel,{{"message_id","123"},{"voice_states",roster}});};
        const QJsonObject self{{"user_id",s.self_}};
        const QJsonObject peer{{"user_id",remote},{"connection_id","peer-one"}};
        update({self});QCOMPARE(s.voiceChannel_,QString(channel));QVERIFY(!s.voicePeerSeen_);
        update({self,peer,QJsonObject{{"user_id",remote},{"connection_id","peer-two"}}});
        update({self,peer});QCOMPARE(s.voiceChannel_,QString(channel));
        update({self});QVERIFY(s.voiceChannel_.isEmpty());
        // A second outgoing call in the same DM must not inherit answered state.
        s.voiceChannel_=channel;update({self});QCOMPARE(s.voiceChannel_,QString(channel));
    }
    void directCallIgnoresPartialStaleUnavailableAndMediaReconnectEvents() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,auto){});bind(s);
        s.voiceChannel_=channel;s.voiceState_="connected";
        auto event=[&](QString type,QJsonObject d){s.gatewayEvent({{"op",0},{"t",type},{"d",d}});};
        event("CALL_CREATE",{{"channel_id",channel},{"message_id","123"},
            {"voice_states",QJsonArray{QJsonObject{{"user_id",s.self_}},QJsonObject{{"user_id",remote}}}}});
        event("CALL_UPDATE",{{"channel_id",channel},{"ringing",QJsonArray{}}});
        event("CALL_UPDATE",{{"channel_id",channel},{"message_id","122"},{"voice_states",QJsonArray{}}});
        event("CALL_UPDATE",{{"channel_id",channel},{"message_id","123"},{"voice_states",QJsonArray{QJsonObject{}}}});
        event("CALL_DELETE",{{"channel_id",remote}});
        event("CALL_DELETE",{{"channel_id",channel},{"unavailable",true}});
        s.voiceState_="reconnecting";s.voicePresence({{"count",1},{"people",QJsonArray{}}});s.disconnected();
        QCOMPARE(s.voiceChannel_,QString(channel));QVERIFY(s.voicePeerSeen_);
        // Fresh Gateway roster after recovery still ends a call whose peer left.
        s.calls_.clear();event("CALL_CREATE",{{"channel_id",channel},{"message_id","123"},
            {"voice_states",QJsonArray{QJsonObject{{"user_id",s.self_}}}}});
        QVERIFY(s.voiceChannel_.isEmpty());
    }
    void directCallDeletionEndsEvenPendingAnswerButGroupsRemainIndependent() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,auto){});bind(s);
        s.voiceChannel_=channel;s.voiceState_="connecting";
        s.gatewayEvent({{"op",0},{"t","CALL_DELETE"},{"d",QJsonObject{{"channel_id",channel}}}});
        QVERIFY(s.voiceChannel_.isEmpty());
        for(const int type:{3,2}) {
            s.channels_[channel]["type"]=type;s.voiceChannel_=channel;s.voiceState_="connected";
            s.reconcileDirectCall(channel,{{"voice_states",QJsonArray{QJsonObject{{"user_id",remote}}}}});
            s.reconcileDirectCall(channel,{{"voice_states",QJsonArray{}}});
            QCOMPARE(s.voiceChannel_,QString(channel));QVERIFY(!s.voicePeerSeen_);
        }
    }
    void incomingCallPromptSurvivesVisibleDmAndKeepsNavigationAndDraft() {
        MockLibraryRepository library;MockTrainerRepository trainers;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository hall;MockAchievementProvider achievements;
        ShellController shell(library,trainers,adapter,platform,builtinExperiences(dex,dex),hall,achievements);
        auto* c=shell.social();shell.goToPage(3);const auto origin=shell.navigationState();
        c->setSurfaceAvailable(true);c->setConversationVisible(true);c->drafts_[channel]="unsent";
        QVariantMap row{{"id",channel},{"name","Friend"},{"kind","chats"},{"ringing",true},{"call",true}};
        QVariantMap state{{"state","connected"},{"userId","self"},{"channel",channel},{"notificationSound",false},
            {"voice",QVariantMap{{"available",true}}},{"chats",QVariantList{row}}};
        c->receive(0,state);QSignalSpy commands(c,&SocialController::commandRequested);
        QCOMPARE(c->incomingCall()["id"].toString(),QString(channel));
        shell.dispatch(Action::NextPage);QCOMPARE(shell.navigationState(),origin);QVERIFY(commands.isEmpty());
        c->respondToIncomingCall("stale",true);QVERIFY(commands.isEmpty());
        shell.dispatch(Action::Confirm);QCOMPARE(commands.last()[0].toString(),QString("voice-join"));
        QCOMPARE(c->drafts_[channel],QString("unsent"));QCOMPARE(shell.navigationState(),origin);
        row["callBusy"]=true;state["chats"]=QVariantList{row};c->receive(0,state);commands.clear();
        shell.dispatch(Action::Back);QVERIFY(commands.isEmpty());
        row["callBusy"]=false;state["chats"]=QVariantList{row};c->receive(0,state);
        shell.dispatch(Action::Back);QCOMPARE(commands.last()[0].toString(),QString("voice-decline"));
        row["ringing"]=false;state["chats"]=QVariantList{row};c->receive(0,state);QVERIFY(c->incomingCall().isEmpty());
        commands.clear();c->respondToIncomingCall(channel,true);QVERIFY(commands.isEmpty());
        row["ringing"]=true;state["chats"]=QVariantList{row};state["callsSuppressed"]=true;c->receive(0,state);QVERIFY(c->incomingCall().isEmpty());
        state["callsSuppressed"]=false;row["muted"]=true;state["chats"]=QVariantList{row};c->receive(0,state);QVERIFY(c->incomingCall().isEmpty());
        row["muted"]=false;row["socialHidden"]=true;state["chats"]=QVariantList{row};c->receive(0,state);QVERIFY(c->incomingCall().isEmpty());
        state["state"]="signed-out";c->receive(0,state);QVERIFY(c->incomingCall().isEmpty());
    }
    void declineCallWaitsForProviderAndAllowsRetryWithoutParallelRequests() {
        FluxerSession s;bind(s);Completion pending;int requests=0;
        s.setTransport([&](auto method,auto path,auto,Completion done,QByteArray){QCOMPARE(method,QByteArray("POST"));QVERIFY(path.endsWith("/call/stop-ringing"));++requests;pending=done;});
        s.calls_[channel]={{"ringing",QJsonArray{s.self_}}};
        s.voiceCommand("voice-decline",{{"channel",channel}});QVERIFY(s.decliningCalls_.contains(channel));
        s.voiceCommand("voice-decline",{{"channel",channel}});QCOMPARE(requests,1);
        pending({0,{}});QVERIFY(!s.decliningCalls_.contains(channel));QVERIFY(s.calls_[channel]["ringing"].toArray().contains(s.self_));
        s.voiceCommand("voice-decline",{{"channel",channel}});QCOMPARE(requests,2);pending({204,{}});
        QVERIFY(!s.decliningCalls_.contains(channel));QVERIFY(s.calls_[channel]["ringing"].toArray().isEmpty());
    }
    void clearingOldNotificationsKeepsLiveInvitationsAndNewMessagesReturn() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        QVariantMap old{{"id",channel},{"kind","chats"},{"name","Friend"},{"unread",1},{"last","123"}};
        QVariantMap ringing{{"id",remote},{"kind","chats"},{"ringing",true}};
        QVariantMap state{{"state","connected"},{"userId","self"},{"notificationSound",false},{"chats",QVariantList{old,ringing}}};
        c.receive(0,state);c.dismissNotifications();QCOMPARE(c.notifications().size(),1);QVERIFY(commands.isEmpty());
        c.receive(0,state);QCOMPARE(c.notifications().size(),1);
        old["last"]="124";state["chats"]=QVariantList{old,ringing};c.receive(0,state);QCOMPARE(c.notifications().size(),2);
    }
    void callBarControllerActionsUseTheExistingSession() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.receive(0,{{"state","connected"},{"userId","self"},{"channel",channel},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"kind","chats"},{"name","Friend"}}}},
            {"voice",QVariantMap{{"available",true},{"channel",channel},{"state","connected"}}}});
        c.reading_=true;c.conversationControl_=1;commands.clear();
        c.dispatch(Action::Down);QCOMPARE(c.conversationControl_,3);c.dispatch(Action::Confirm);
        QCOMPARE(commands.last()[0].toString(),QString("voice-mute"));
        c.dispatch(Action::Right);c.dispatch(Action::Confirm);QCOMPARE(commands.last()[0].toString(),QString("voice-output"));
        c.dispatch(Action::Right);c.dispatch(Action::Confirm);QCOMPARE(commands.last()[0].toString(),QString("voice-leave"));
    }
    void homeInboxSelectClearsOldNotificationsAndKeepsPendingInvites() {
        MockLibraryRepository library;MockTrainerRepository trainers;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository hall;MockAchievementProvider achievements;
        ShellController shell(library,trainers,adapter,platform,builtinExperiences(dex,dex),hall,achievements);
        auto* c=shell.social();c->receive(0,{{"state","connected"},{"userId","self"},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"name","Friend"},{"kind","chats"},{"unread",1},{"last","123"}}}}});
        c->activityNotifications_.append(QVariantMap{{"id","pending"},{"session","invite"},{"runtime",true}});
        c->runtimeInvitation_={{"request","invite"}};
        shell.dispatch(Action::Home);shell.dispatch(Action::Right);
        QVERIFY(shell.notificationsOpen());QCOMPARE(shell.notifications().size(),2);
        QSignalSpy commands(c,&SocialController::commandRequested);shell.dispatch(Action::LocalAction);
        QCOMPARE(shell.notifications().size(),1);QVERIFY(shell.notifications().first().toMap()["pending"].toBool());QVERIFY(commands.isEmpty());
        shell.dismissNotificationById("pending");QCOMPARE(shell.notifications().size(),1);
    }
    void switchingCallsRequiresExplicitConfirmationAndKeepsOldCallOnCancel() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","self"},{"channel",remote},
            {"chats",QVariantList{QVariantMap{{"id",remote},{"name","New friend"},{"kind","chats"}}}},
            {"voice",QVariantMap{{"available",true},{"channel",channel},{"name","Old friend"}}}});
        QSignalSpy commands(&c,&SocialController::commandRequested);c.call();
        QVERIFY(c.menuTitle().contains("Switch"));QVERIFY(commands.isEmpty());
        c.dispatch(Action::Back);QVERIFY(commands.isEmpty());QCOMPARE(c.account()["voice"].toMap()["channel"].toString(),QString(channel));
        c.call();c.selectMenu(1);QCOMPARE(commands.last()[0].toString(),QString("voice-switch"));
        QCOMPARE(commands.last()[1].toMap()["previous"].toString(),QString(channel));
        QCOMPARE(commands.last()[1].toMap()["channel"].toString(),QString(remote));
    }
    void viewedUpdatesDisappearOnlyAfterTheActualTailIsPresented() {
        SocialController c;c.setSurfaceAvailable(true);c.setConversationVisible(true);
        QVariantMap chat{{"id",channel},{"name","Friend"},{"kind","chats"},{"last","10"},{"unread",1},{"missedCall","missed"}};
        QVariantMap state{{"state","connected"},{"userId","self"},{"channel",channel},{"readTail","10"},{"chats",QVariantList{chat}}};
        c.receive(0,state);QCOMPARE(c.notifications().size(),1);
        c.presented(channel,"9");QCOMPARE(c.notifications().size(),1);
        c.presented(channel,"10");QVERIFY(c.notifications().isEmpty());
        c.receive(0,state);QVERIFY(c.notifications().isEmpty());
        chat["last"]="11";state["chats"]=QVariantList{chat};state["readTail"]="11";c.receive(0,state);QCOMPARE(c.notifications().size(),1);
        c.setConversationVisible(false);c.presented(channel,"11");QCOMPARE(c.notifications().size(),1);
        c.setConversationVisible(true);c.presented(channel,"11");QVERIFY(c.notifications().isEmpty());
    }
    void clearUpdatesPreservesFriendRequestsAndViewedRequestStaysInPeople() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","self"},
            {"friends",QVariantList{QVariantMap{{"id",remote},{"name","Friend"},{"type",3}}}}});
        QSignalSpy commands(&c,&SocialController::commandRequested);c.dismissNotifications();
        QCOMPARE(c.notifications().size(),1);QVERIFY(commands.isEmpty());
        c.openNotificationAt(0);QVERIFY(c.notifications().isEmpty());QCOMPARE(c.peopleRows().size(),1);
        for(const auto& args:commands)QVERIFY(args[0]!="remove");
    }
    void peopleFilterCanBeClearedAndRetryUsesOnlyCurrentFailedMessage() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","self"},{"channel",channel},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"kind","chats"},{"name","Friend"}}}},
            {"messages",QVariantList{QVariantMap{{"id","failed"},{"retryable",true}},QVariantMap{{"id","sent"},{"retryable",false}}}}});
        c.peopleFilter_="missing";c.dispatch(Action::Back);QVERIFY(c.peopleFilter_.isEmpty());
        QSignalSpy commands(&c,&SocialController::commandRequested);
        c.retryMessage("stale");c.retryMessage("sent");QVERIFY(commands.isEmpty());
        c.retryMessage("failed");QCOMPARE(commands.size(),1);QCOMPARE(commands.last()[0].toString(),QString("retry-message"));
        QCOMPARE(commands.last()[1].toMap()["channel"].toString(),QString(channel));
    }
    void routeLossRetiresPoolCompletesBusyRequestsAndNeverReplaysMutation() {
        FluxerSession s;bind(s);const auto token=s.token_;s.gatewaySession_="resume-session";
        auto* manager=new HeldSocialNetwork(&s);s.network_=manager;
        int gets=0,posts=0;
        s.request("GET","/v1/test",{},[&](Reply r){++gets;QCOMPARE(r.status,0);});
        s.request("POST","/v1/test",{},[&](Reply r){++posts;QCOMPARE(r.status,0);});
        manager->replies.first()->fail(200); // Partial HTTP 200 is not a success.
        QCOMPARE(gets,1);QCOMPARE(posts,1);QVERIFY(!s.network_);
        QCOMPARE(s.token_,token);QCOMPARE(s.gatewaySession_,QString("resume-session"));
        QVERIFY(s.networkRecovery_.isActive());
        QStringList methods;
        s.setTransport([&](auto method,auto,auto,Completion done,QByteArray){methods.append(method);done({0,{}});});
        s.networkRecovery_.stop();QMetaObject::invokeMethod(&s.networkRecovery_,"timeout");
        QVERIFY(!methods.isEmpty());for(const auto& method:methods)QCOMPARE(method,QString("GET"));
        QCOMPARE(posts,1);
    }
    void queuedRestDeadlineReleasesRequestAndRetiredPoolCannotKillReplacement() {
        FluxerSession s;bind(s);auto* manager=new HeldSocialNetwork(&s);s.network_=manager;
        int finished=0;
        s.request("GET","/v1/test",{},[&](Reply r){++finished;QCOMPARE(r.status,0);});
        auto* deadline=manager->replies.first()->findChild<QTimer*>();QVERIFY(deadline);
        QMetaObject::invokeMethod(deadline,"timeout");QCOMPARE(finished,1);QVERIFY(!s.network_);
        auto* replacement=new HeldSocialNetwork(&s);s.network_=replacement;
        s.retireNetwork(manager);QCOMPARE(s.network_,replacement);
        s.status_="Connection lost. Try again when online";
        s.request("GET","/v1/test",{},[](Reply r){QCOMPARE(r.status,200);});
        replacement->replies.first()->succeed();QCOMPARE(s.status_,QString("Connected"));
        s.reset();QVERIFY(!s.networkRecovery_.isActive());QVERIFY(!s.gatewayDeadline_.isActive());
    }
    void gatewayHandshakeDeadlineRetriesWithoutLosingVoiceConsent() {
        FluxerSession s;bind(s);
        s.voiceChannel_=channel;s.voiceMuted_=true;s.voiceState_="reconnecting";
        s.socket_=new QWebSocket(QString(),QWebSocketProtocol::VersionLatest,&s);
        s.gatewayDeadline_.start(1);
        QTRY_VERIFY(!s.socket_);QVERIFY(s.reconnect_&&s.reconnect_->isActive());
        QCOMPARE(s.voiceChannel_,QString(channel));QVERIFY(s.voiceMuted_);
        s.setTransport([](auto,auto,auto,Completion done,QByteArray){done({200,QJsonDocument(QJsonArray{})});});
        s.gatewayEvent({{"op",0},{"t","RESUMED"},{"d",QJsonObject{}}});
        QVERIFY(!s.reconnect_->isActive());QVERIFY(!s.gatewayDeadline_.isActive());
    }
    void discoverPeopleAreProfilesAndUnknownTagsRequireConfirmation() {
        FluxerSession s;bind(s);QStringList requests;
        s.setTransport([&](auto method,auto path,auto,Completion done,QByteArray){requests<<method+" "+path;if(method=="GET")done({200,QJsonDocument(QJsonObject{{"id",remote},{"username","Mika"}})});});
        s.relationships_[remote]={{"type",1},{"user",QJsonObject{{"id",remote},{"username","Mika"},{"discriminator","1234"}}}};
        s.search("people","Mik");QCOMPARE(s.searchResults_.size(),1);QCOMPARE(s.searchResults_[0].toMap()["kind"].toString(),QString("person"));QVERIFY(requests.isEmpty());
        s.command("search-action",{{"id",remote}});QVERIFY(requests.isEmpty());
        s.search("people","Unknown#1234");QCOMPARE(s.searchResults_.first().toMap()["kind"].toString(),QString("request-tag"));QVERIFY(requests.isEmpty());
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.receive(0,{{"state","connected"},{"searchResults",s.searchResults_}});commands.clear();c.activateSearch(0);
        QVERIFY(commands.isEmpty());QVERIFY(c.menuCommands_.contains("request-discovered-tag"));c.selectMenu(0);QVERIFY(commands.isEmpty());
        c.activateSearch(0);c.selectMenu(1);QCOMPARE(commands.last()[0].toString(),QString("search-action"));
        s.relationships_.clear();s.search("people",remote);QCOMPARE(requests.last(),QString("GET /v1/users/")+remote);QCOMPARE(s.searchResults_.size(),1);
        s.relationships_[remote]={{"type",2}};requests.clear();s.search("people",remote);QVERIFY(s.searchResults_.isEmpty());QVERIFY(requests.isEmpty());
    }
    void discoverBareInviteAndUntrustedLinkNeverAutoJoin() {
        FluxerSession s;bind(s);QStringList paths;
        s.setTransport([&](auto method,auto path,auto,Completion done,QByteArray){QCOMPARE(method,QByteArray("GET"));paths<<path;
            if(path.startsWith("/v1/discovery"))done({200,QJsonDocument(QJsonObject{{"guilds",QJsonArray{}},{"total",0}})});
            else done({200,QJsonDocument(QJsonObject{{"code","Ab123"},{"guild",QJsonObject{{"id","123"},{"name","Club"}}}})});
        });
        s.search("communities","Ab123");QCOMPARE(paths.size(),2);QCOMPARE(paths.last(),QString("/v1/invites/Ab123"));QCOMPARE(s.searchResults_.first().toMap()["kind"].toString(),QString("invite"));
        paths.clear();s.search("invite","https://other.example/Ab123");QVERIFY(paths.isEmpty());QVERIFY(s.searchResults_.isEmpty());
        s.search("invite","https://user@fluxer.gg/Ab123");QVERIFY(paths.isEmpty());
    }
    void communityLeaveChecksFreshOwnershipAndPreservesOtherConversations() {
        FluxerSession s;bind(s);s.guilds_["123"]={{"id","123"},{"owner_id",remote}};
        s.channels_["124"]={{"id","124"},{"guild_id","123"},{"type",0}};s.channels_["125"]={{"id","125"},{"guild_id","126"},{"type",0}};
        bool own=true,denied=false;int mutations=0;QSignalSpy done(&s,&FluxerSession::mutationFinished);
        s.setTransport([&](auto method,auto path,auto body,Completion reply,QByteArray){
            if(method=="GET"&&path=="/v1/guilds/123"){reply({200,QJsonDocument(QJsonObject{{"id","123"},{"owner_id",own?s.self_:remote}})});return;}
            if(method=="DELETE"){++mutations;QCOMPARE(path,QString("/v1/users/@me/guilds/123"));QVERIFY(body.isEmpty());reply({denied?403:204,{}});return;}
            reply({200,QJsonDocument(QJsonArray{})});
        });
        s.communityMembership("leave-community",{{"id","123"}});QCOMPARE(mutations,0);QVERIFY(s.guilds_.contains("123"));
        own=false;denied=true;s.communityMembership("leave-community",{{"id","123"}});QVERIFY(s.guilds_.contains("123"));QVERIFY(!done.last()[4].toBool());
        denied=false;s.communityMembership("leave-community",{{"id","123"}});QCOMPARE(mutations,2);QVERIFY(done.last()[4].toBool());
        QVERIFY(!s.guilds_.contains("123"));QVERIFY(!s.channels_.contains("124"));QVERIFY(s.channels_.contains("125"));QVERIFY(s.channels_.contains(channel));
    }
    void communityDeleteRequiresProviderIdentityAndNeverRetriesUncertainMutation() {
        FluxerSession s;bind(s);s.guilds_["123"]={{"id","123"},{"owner_id",s.self_}};
        int posts=0;bool mfa=false,timeout=false;QSignalSpy verify(&s,&FluxerSession::communityVerificationRequired);
        s.setTransport([&](auto method,auto path,auto body,Completion reply,QByteArray){
            if(method=="GET"){reply({200,QJsonDocument(QJsonObject{{"id","123"},{"owner_id",s.self_}})});return;}
            QCOMPARE(method,QByteArray("POST"));QCOMPARE(path,QString("/v1/guilds/123/delete"));++posts;
            if(body.isEmpty())reply({403,QJsonDocument(QJsonObject{{"code","SUDO_MODE_REQUIRED"},{"has_mfa",mfa},{"methods",QJsonObject{{"totp",mfa}}}})});
            else {QVERIFY(body.contains("password")||(body["mfa_method"]=="totp"&&body.contains("mfa_code")));reply({timeout?0:204,{}});}
        });
        s.communityMembership("delete-community",{{"id","123"}});QCOMPARE(posts,1);QCOMPARE(verify.last()[2].toString(),QString("password"));QVERIFY(s.guilds_.contains("123"));
        mfa=true;s.communityMembership("delete-community",{{"id","123"}});QCOMPARE(verify.last()[2].toString(),QString("totp"));
        timeout=true;s.communityMembership("delete-community",{{"id","123"},{"method","totp"},{"secret","123456"}});QCOMPARE(posts,3);QVERIFY(s.guilds_.contains("123"));
        timeout=false;s.communityMembership("delete-community",{{"id","123"},{"method","totp"},{"secret","123456"}});QCOMPARE(posts,4);QVERIFY(!s.guilds_.contains("123"));
    }
    void communityConfirmationCapturesIdentityAndSecretIsNotAChatDraft() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","1"},{"communities",QVariantList{QVariantMap{{"id","123"},{"name","Club"},{"kind","community"},{"owner","1"}}}}});
        c.setFace("communities");c.contextRow(0);QVERIFY(c.menuCommands_.contains("ask-delete-community"));QVERIFY(!c.menuCommands_.contains("ask-leave-community"));
        QSignalSpy commands(&c,&SocialController::commandRequested);c.selectMenu(c.menuCommands_.indexOf("ask-delete-community"));QVERIFY(commands.isEmpty());
        c.communityId_="different";c.selectMenu(1);QCOMPARE(commands.last()[0].toString(),QString("delete-community"));QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString("123"));
        c.textPurpose_="community-secret:password";c.textId_="123";QVERIFY(c.textSecret());c.applyText("synthetic-password");QVERIFY(!c.textSecret());QVERIFY(c.draft().isEmpty());
        QCOMPARE(commands.last()[1].toMap()["secret"].toString(),QString("synthetic-password"));QVERIFY(c.textId_.isEmpty());
        c.textPurpose_="community-secret:totp";c.setOwner("different-owner");QVERIFY(!c.textSecret());
    }
    void discoverPreviewAndControllerPreserveSearchAndRequireExplicitJoin() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","1"}});c.setFace("friends");
        c.query_="Club";c.receive(0,{{"state","connected"},{"userId","1"},{"searchResults",QVariantList{QVariantMap{{"id","123"},{"name","Club"},{"kind","community"},{"action","Join"}}}}});
        QSignalSpy commands(&c,&SocialController::commandRequested);c.dispatch(Action::Down);c.dispatch(Action::Confirm);
        QCOMPARE(c.discoverPreview()["id"].toString(),QString("123"));QVERIFY(commands.isEmpty());c.dispatch(Action::Back);QVERIFY(c.discoverPreview().isEmpty());QCOMPARE(c.query_,QString("Club"));
        c.activateSearch(0);c.discoverAction("join");QCOMPARE(commands.last()[0].toString(),QString("search-action"));c.discoverAction("back");
        c.setFace("chats");c.setFace("friends");QCOMPARE(c.query_,QString("Club"));
    }
    void lobbyGroupIdentitySurvivesRenameAndIsNotATitleHeuristic() {
        FluxerSession s;bind(s);s.channels_.clear();s.face_="communities";s.channelsLoaded_=true;
        const auto room=QUuid::createUuid().toString(QUuid::WithoutBraces);
        const auto marker=QJsonObject{{"id","88"},{"channel_id",channel},{"pinned",true},{"author",QJsonObject{{"id",remote}}},{"content",lobbyChatIdentity::content(room,channel,remote)}};
        s.channels_[channel]={{"id",channel},{"type",3},{"name","Weekend"}};
        s.channels_["123"]={{"id","123"},{"type",3},{"name","Weekend"}};
        Completion delayed;s.setTransport([&](auto,QString path,auto,Completion done,QByteArray){
            if(path.contains(QString(channel)))delayed=done;else done({200,QJsonDocument(QJsonObject{{"items",QJsonArray{}}})});
        });
        QVERIFY(s.hiddenSocialGroup(channel));s.checkLobbyGroups();QVERIFY(s.hiddenSocialGroup(channel));QVERIFY(!s.hiddenSocialGroup("123"));
        delayed({200,QJsonDocument(QJsonObject{{"items",QJsonArray{QJsonObject{{"message",marker}}}}})});
        QVERIFY(s.hiddenSocialGroup(channel));s.channels_[channel]["name"]="Renamed";QVERIFY(s.hiddenSocialGroup(channel));
        s.ensureConversation();QCOMPARE(s.channel_,QString("123"));
        QVERIFY(lobbyChatIdentity::identity(marker,"123").isEmpty());auto forged=marker;forged["author"]=QJsonObject{{"id","99"}};QVERIFY(lobbyChatIdentity::identity(forged,channel).isEmpty());
        s.navigationKey_="social-test/"+QUuid::createUuid().toString()+"/";s.saveLobbyChats();
        FluxerSession restored;restored.navigationKey_=s.navigationKey_;restored.restoreLobbyChats();QCOMPARE(restored.lobbyChatRecords_,s.lobbyChatRecords_);QSettings().remove(s.navigationKey_);
    }
    void closedLobbyRemovesOfflineMembersBeforeOwnerAndRetriesPartialFailure() {
        FluxerSession s;bind(s);const auto room=QUuid::createUuid().toString();
        s.lobbyChatRecords_[channel]={{"lobby",room},{"managed",true}};
        QJsonArray members{QJsonObject{{"id",remote}},QJsonObject{{"id","123"}}};QStringList deleted;bool fail=true;
        s.setTransport([&](auto method,QString path,auto,Completion done,QByteArray){
            if(method=="GET"){done({200,QJsonDocument(QJsonObject{{"id",channel},{"type",3},{"owner_id",s.self_},{"recipients",members}})});return;}
            QVERIFY(method=="DELETE");deleted<<path;
            if(path.endsWith("/recipients/123")&&fail){fail=false;done({503,{}});return;}
            if(path.contains("/recipients/")){members.removeFirst();done({204,{}});}else done({204,{}});
        });
        QSignalSpy finished(&s,&FluxerSession::mutationFinished);
        s.closeLobbyChat({{"lobby",room},{"channel",channel},{"close",true}});QVERIFY(finished.isEmpty());QCOMPARE(members.size(),1);
        QCOMPARE(s.lobbyChatRecords_[channel]["cleanup"].toString(),QString("close"));
        s.cleanupLobbyChat(channel);QCOMPARE(finished.size(),1);QVERIFY(members.isEmpty());QCOMPARE(deleted.last(),QString("/v1/channels/")+channel);QVERIFY(!s.channels_.contains(channel));
        QVERIFY(!s.lobbyChatRecords_[channel].contains("cleanup"));
    }
    void unknownOrReusedOrdinaryConversationCannotBeMassDeleted() {
        FluxerSession s;bind(s);int calls=0;const auto room=QUuid::createUuid().toString();
        s.setTransport([&](auto method,QString path,auto,Completion done,QByteArray){++calls;
            if(method=="GET")done({200,QJsonDocument(QJsonObject{{"id",channel},{"type",3},{"owner_id",s.self_},{"recipients",QJsonArray{QJsonObject{{"id",remote}}}}})});
            else {QVERIFY(!path.contains("/recipients/"));done({204,{}});}
        });
        s.closeLobbyChat({{"lobby",room},{"channel",channel},{"close",true}});QCOMPARE(calls,0);
        s.lobbyChatRecords_[channel]={{"lobby",room}};s.ordinaryGroups_.insert(channel);s.closeLobbyChat({{"lobby",room},{"channel",channel},{"close",true}});QCOMPARE(calls,2);
    }
    void newOwnerResolvesLobbyIdentityBeforeClosingAllMembers() {
        FluxerSession s;bind(s);const auto room=QUuid::createUuid().toString();
        s.channels_[channel]={{"id",channel},{"type",3}};
        s.lobbyChatRecords_[channel]={{"lobby",room}}; // Accepted as guest before the pin probe.
        const auto marker=QJsonObject{{"channel_id",channel},{"pinned",true},{"author",QJsonObject{{"id",remote}}},{"content",lobbyChatIdentity::content(room,channel,remote)}};
        QStringList deleted;QJsonArray members{QJsonObject{{"id",remote}}};bool fail=true;
        s.setTransport([&](auto method,QString path,auto,Completion done,QByteArray){
            if(path.contains("/messages/pins")) {
                if(fail){fail=false;done({503,{}});return;}
                done({200,QJsonDocument(QJsonObject{{"items",QJsonArray{QJsonObject{{"message",marker}}}}})});return;
            }
            if(method=="GET"){done({200,QJsonDocument(QJsonObject{{"id",channel},{"type",3},{"owner_id",s.self_},{"recipients",members}})});return;}
            QCOMPARE(method,QByteArray("DELETE"));deleted<<path;members={};done({204,{}});
        });
        s.closeLobbyChat({{"lobby",room},{"channel",channel},{"close",true}});QVERIFY(deleted.isEmpty());
        QCOMPARE(s.lobbyChatRecords_[channel]["cleanup"].toString(),QString("classify-close"));
        s.checkLobbyGroups();QCOMPARE(deleted,(QStringList{QString("/v1/channels/")+channel+"/recipients/"+remote,QString("/v1/channels/")+channel}));
        QVERIFY(s.lobbyChatRecords_[channel]["managed"].toBool());
    }
    void controllerReachesHeaderMessagesComposerAndSafeRemoval() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);
        s.snapshot_={{"state","connected"},{"userId","self"},{"channel","dm"},{"friends",QVariantList{QVariantMap{{"id","friend"},{"type",1},{"name","Mika"}}}},
            {"chats",QVariantList{QVariantMap{{"id","dm"},{"name","Mika"},{"kind","chats"},{"members",QVariantList{QVariantMap{{"id","friend"}}}}}}}};
        QSignalSpy commands(&s,&SocialController::commandRequested),text(&s,&SocialController::textRequested);
        s.dispatch(Action::Confirm);QVERIFY(s.reading());QCOMPARE(s.conversationControl(),11);
        s.dispatch(Action::Up);QCOMPARE(s.conversationControl(),0);s.dispatch(Action::Down);QCOMPARE(s.conversationControl(),11);
        s.dispatch(Action::ChangeView);QCOMPARE(text.size(),1);s.dispatch(Action::Back);QVERIFY(!s.reading());
        s.dispatch(Action::ContextMenu);QVERIFY(s.menu().contains("Remove friend"));s.selectMenu(s.menu().indexOf("Remove friend"));QCOMPARE(s.menuMode_,QString("confirm"));
        for(const auto& cmd:commands)QVERIFY(cmd[0]!="remove");s.selectMenu(1);QCOMPARE(commands.last()[0].toString(),QString("remove"));QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString("friend"));
        s.snapshot_["chats"]=QVariantList{QVariantMap{{"id","hidden"},{"kind","groups"},{"socialHidden",true},{"unread",2}},QVariantMap{{"id","ordinary"},{"name","Weekend"},{"kind","groups"}}};
        s.face_="communities";QCOMPARE(s.rows().size(),1);QVERIFY(s.notifications().isEmpty());s.contextRow(0);QVERIFY(s.menu().contains("Leave group"));
    }
    void peopleAreIdentitiesIncludingRequestsWithoutMixingGroups() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);
        s.snapshot_={{"state","connected"},{"userId","self"},{"friends",QVariantList{
            QVariantMap{{"id","friend"},{"name","Mika"},{"type",1}},QVariantMap{{"id","incoming"},{"name","Ren"},{"type",3}},
            QVariantMap{{"id","outgoing"},{"name","Sol"},{"type",4}},QVariantMap{{"id","blocked"},{"name","Blocked"},{"type",2}}}},
            {"chats",QVariantList{QVariantMap{{"id","dm"},{"name","Mika"},{"kind","chats"},{"members",QVariantList{QVariantMap{{"id","friend"}}}}},
                QVariantMap{{"id","service-dm"},{"name","Directory"},{"service",true},{"kind","chats"},{"members",QVariantList{QVariantMap{{"id","service"}}}}},
                QVariantMap{{"id","stranger-dm"},{"name","Alex"},{"kind","chats"},{"members",QVariantList{QVariantMap{{"id","stranger"}}}}},
                QVariantMap{{"id","group"},{"name","Weekend"},{"kind","groups"}},
                QVariantMap{{"id","blocked-dm"},{"name","Blocked"},{"kind","chats"},{"members",QVariantList{QVariantMap{{"id","blocked"}}}}}}}};
        QCOMPARE(s.rows().size(),4);
        for(const auto& v:s.rows()){const auto row=v.toMap();QVERIFY(!row["id"].toString().isEmpty());QVERIFY(!row["name"].toString().isEmpty());}
        QSignalSpy commands(&s,&SocialController::commandRequested);
        int request=-1;for(int i=0;i<s.rows().size();++i)if(s.rows()[i].toMap()["id"]=="incoming")request=i;
        QVERIFY(request>=0);s.activate(request);QCOMPARE(commands.last()[0].toString(),QString("dm"));
        for(const auto& cmd:commands)QVERIFY(cmd[0]!="accept");
        s.contextRow(request);QCOMPARE(s.menu(),(QStringList{"Pin","Profile"}));s.selectMenu(0);QCOMPARE(s.rows().first().toMap()["id"].toString(),QString("incoming"));
        s.filterPeople();s.applyText("Mik");QCOMPARE(s.rows().size(),1);QCOMPARE(s.rows().first().toMap()["channel"].toString(),QString("dm"));
    }
    void communityDrilldownReplacesListAndMenusStayBasic() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);s.face_="communities";
        s.snapshot_={{"state","connected"},{"userId","self"},{"channel","group"},{"chats",QVariantList{
            QVariantMap{{"id","group"},{"name","Weekend"},{"kind","groups"},{"owner","self"}},
            QVariantMap{{"id","text"},{"name","general"},{"guild","guild"},{"kind","community-channel"}}}},
            {"communities",QVariantList{QVariantMap{{"id","guild"},{"name","Good company"},{"kind","community"}}}}};
        QCOMPARE(s.rows().size(),2);s.contextRow(0);QCOMPARE(s.menu(),(QStringList{"Pin","Mute conversation","Group info","Leave group"}));s.closeMenu();
        s.activate(1);QVERIFY(s.communityOpen());QCOMPARE(s.rows().size(),1);QCOMPARE(s.rows().first().toMap()["id"].toString(),QString("text"));
        s.communityBack();QVERIFY(!s.communityOpen());QCOMPARE(s.rows().size(),2);QCOMPARE(s.focusIndex(),1);
        s.conversationProfile();QVERIFY(s.menu().contains("Members"));QVERIFY(s.menu().contains("Leave group"));QVERIFY(!s.menu().contains("Who can join my games"));
    }
    void conversationSelectionNeverCreatesAnUnrequestedDm() {
        FluxerSession s;bind(s);s.channels_.clear();s.channel_.clear();s.channelsLoaded_=s.friendsLoaded_=true;
        s.relationships_[remote]={{"type",1},{"user",QJsonObject{{"id",remote},{"username","Ren"}}}};
        int requests=0;s.setTransport([&](QByteArray,QString,QJsonObject,Completion done,QByteArray){++requests;done({403,{}});});
        s.ensureConversation();QCOMPARE(requests,0);QVERIFY(s.channel_.isEmpty());
        s.relationships_[remote]["type"]=3;s.command("dm",{{"id",remote}});QCOMPARE(requests,1);QCOMPARE(s.relationships_[remote]["type"].toInt(),3);
    }
    void lateDmCreationCannotReplaceTheConversationChosenAfterIt() {
        FluxerSession s;bind(s);Completion created;
        s.relationships_[remote]={{"type",1},{"user",QJsonObject{{"id",remote},{"username","Ren"}}}};
        s.setTransport([&](QByteArray method,QString,QJsonObject,Completion done,QByteArray){if(method=="POST")created=done;else done({200,QJsonDocument(QJsonArray{})});});
        s.command("dm",{{"id",remote}});QVERIFY(s.channel_.isEmpty());QVERIFY(bool(created));
        s.command("open",{{"id",channel}});QCOMPARE(s.channel_,QString(channel));
        created({200,QJsonDocument(QJsonObject{{"id","1501314428688998199"},{"type",1},{"recipients",QJsonArray{QJsonObject{{"id",remote}}}}})});
        QCOMPARE(s.channel_,QString(channel));QVERIFY(s.channels_.contains("1501314428688998199"));
    }
    void pinsAreScopedToTrainerAndCommunicationAccount() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);s.owner_=QUuid::createUuid().toString();s.snapshot_["userId"]="one";
        s.restoreSocialPreferences();const auto key=s.socialSettingsKey_;s.pinnedPeople_={"friend"};s.saveSocialPreferences();
        s.snapshot_["userId"]="two";s.restoreSocialPreferences();QVERIFY(s.pinnedPeople_.isEmpty());
        s.snapshot_["userId"]="one";s.restoreSocialPreferences();QCOMPARE(s.pinnedPeople_,QStringList{"friend"});QSettings().remove(key);
    }

    void filesPreserveNamesAndSupportBeyondLegacyMediaLimit() {
        FluxerSession worker;bind(worker);QTemporaryDir dir;QFile file(dir.filePath("backup.zip"));
        QVERIFY(file.open(QIODevice::WriteOnly));QVERIFY(file.resize(20*1024*1024));file.close();const QFileInfo info(file);
        QJsonObject payload;worker.setTransport([&](QByteArray method,QString path,QJsonObject body,Completion done,QByteArray){
            QCOMPARE(method,QByteArray("POST"));QCOMPARE(path,QString("/v1/channels/")+channel+"/messages");payload=body;done({200,QJsonDocument(QJsonObject{{"id","123"},{"channel_id",channel}})});
        });
        QSignalSpy events(&worker,&FluxerSession::fileEvent);
        worker.command("files-send",{{"channel",channel},{"nonce","stable-nonce"},{"text","Backup"},{"files",QVariantList{QVariantMap{{"path",info.filePath()},{"name",info.fileName()},{"size",info.size()},{"modified",info.lastModified().toMSecsSinceEpoch()}}}}});
        QCOMPARE(payload["attachments"].toArray()[0].toObject()["filename"].toString(),QString("backup.zip"));
        QCOMPARE(payload["nonce"].toString(),QString("stable-nonce"));QVERIFY(events.last()[1].toMap()["ok"].toBool());
        QCOMPARE(info.size(),qint64(20*1024*1024));
    }
    void fileSelectionSurvivesChannelSwitchAndUnknownSendReusesNonce() {
        ChatFiles files;QTemporaryDir dir;QFile file(dir.filePath("a.bin"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("unchanged");file.close();
        files.context("account","channel-a");files.picker()->selected(file.fileName());QCOMPARE(files.selected().size(),1);
        files.context("account","channel-b");QVERIFY(files.selected().isEmpty());files.context("account","channel-a");QCOMPARE(files.selected().size(),1);
        QSignalSpy requests(&files,&ChatFiles::request);QVERIFY(files.send("hello"));const auto nonce=requests.last()[1].toMap()["nonce"].toString();
        files.receive({{"nonce",nonce},{"finished",true},{"uncertain",true},{"status","Unknown"}});
        QVERIFY(files.send("hello"));QCOMPARE(requests.last()[1].toMap()["nonce"].toString(),nonce);
        files.receive({{"nonce",nonce},{"finished",true},{"ok",true}});QVERIFY(files.selected().isEmpty());
        files.picker()->selected(file.fileName());files.context("other-account","channel-a");QVERIFY(files.selected().isEmpty());
    }
    void fileDownloadRejectsUnrelatedUrlsAndMissingMessageAccess() {
        FluxerSession worker;bind(worker);int status=200;QString url="https://example.com/attachments/secret";
        worker.setTransport([&](QByteArray method,QString path,QJsonObject,Completion done,QByteArray){
            QCOMPARE(method,QByteArray("GET"));QVERIFY(path.contains("/messages/"));done({status,QJsonDocument(QJsonObject{{"attachments",QJsonArray{QJsonObject{{"id","5"},{"filename","backup.zip"},{"size",4},{"url",url}}}}})});
        });
        worker.command("files-download",{{"channel",channel},{"message","4"},{"attachment","5"}});
        QCOMPARE(worker.fileJobs_.value(QString(channel)+":4:5")["state"].toString(),QString("error"));QVERIFY(worker.fileReplies_.isEmpty());
        status=403;worker.command("files-control",{{"id",QString(channel)+":4:5"},{"action","retry"}});
        QCOMPARE(worker.fileJobs_.value(QString(channel)+":4:5")["state"].toString(),QString("error"));
        worker.reset();QVERIFY(worker.fileJobs_.isEmpty());
    }
    void myLobbiesBrowseIsIndependentAndHomeDisappearsAfterProviderLeave() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);QObject::disconnect(s.session_,nullptr,&s,nullptr);
        s.receive(0,{{"state","connected"},{"userId","1"},{"name","Mika"}});s.createAnotherLobby();s.submitLobbyDraft();
        const auto first=s.lobbyState()["id"].toString();s.createAnotherLobby();s.submitLobbyDraft();const auto second=s.lobbyState()["id"].toString();
        QVERIFY(first!=second);QCOMPARE(s.myLobbies().size(),2);QCOMPARE(s.homeLobby()["count"].toInt(),2);
        QCOMPARE(s.lobby_.state()["id"].toString(),first); // browsing never switches runtime
        s.showMyLobbies();s.dispatch(Action::Down);s.dispatch(Action::Confirm);
        QVERIFY(!s.myLobbiesOpen());QCOMPARE(s.lobbyState()["id"].toString(),second);
        QCOMPARE(s.lobby_.state()["id"].toString(),first);
        s.lobby_.room(second)->setConversation("300");s.lobbyChatPending_.clear();
        QSignalSpy commands(&s,&SocialController::commandRequested);s.lobbyAction("leave");
        QCOMPARE(commands.last()[0].toString(),QString("lobby-chat-close"));QCOMPARE(s.myLobbies().size(),2);
        s.lobby_.room(second)->forgetMembership();QCOMPARE(s.myLobbies().size(),1);
        s.lobby_.room(first)->forgetMembership();QVERIFY(s.homeLobby().isEmpty());
    }
    void lobbyRelayMenuUsesTheExistingOwnerOptions() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);QObject::disconnect(s.session_,nullptr,&s,nullptr);
        s.receive(0,{{"state","connected"},{"userId","1"},{"name","Mika"}});s.createAnotherLobby();s.submitLobbyDraft();
        const QJsonObject runtime{{"id","runtime.retroarch.nes.fceumm.v1"},{"content","same"},{"players",2}};
        QVERIFY(s.viewedLobby().selectGame({"game","Game","nes","runtime.retroarch.nes.fceumm.v1",2,runtime}));
        s.lobbyAction("relay");QCOMPARE(s.menuCommands_.size(),5);QVERIFY(s.menuCommands_.contains("lobby:relay:auto"));
        s.selectMenu(s.menuCommands_.indexOf("lobby:relay:nyc"));QCOMPARE(s.viewedLobby().state()["relay"].toString(),"nyc");
        s.lobbyAction("relay");QVERIFY(s.menu_.join(' ').contains("New York · Selected"));
        s.closeMenu();s.lobbyAction("relay:arbitrary-address");QCOMPARE(s.viewedLobby().state()["relay"].toString(),"nyc");
    }
    void playersCannotBecomeSpectatorsFromTheRoom() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);QObject::disconnect(s.session_,nullptr,&s,nullptr);
        s.receive(0,{{"state","connected"},{"userId","1"},{"name","Mika"}});s.createAnotherLobby();s.submitLobbyDraft();
        for(const auto& v:s.lobbyRoomActions())QVERIFY(v.toMap()["command"]!="spectate");
        s.lobbyAction("spectate");QCOMPARE(s.lobbySpectators().size(),0);
    }
    void creationIsTransientAndValidatesBeforeAnyRoomExists() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);QObject::disconnect(s.session_,nullptr,&s,nullptr);
        s.receive(0,{{"state","connected"},{"userId","1"},{"name","Mika"}});
        QSignalSpy commands(&s,&SocialController::commandRequested);
        s.createAnotherLobby();QVERIFY(s.myLobbies().isEmpty());
        s.lobbyDraft_["title"]="Discard me";s.cancelLobbyDraft();s.createAnotherLobby();
        QCOMPARE(s.lobbyDraft()["title"].toString(),QString("Mika's lobby"));QVERIFY(s.myLobbies().isEmpty());
        s.lobbyDraft_["title"]="";s.submitLobbyDraft();QVERIFY(s.myLobbies().isEmpty());QVERIFY(!s.lobbyDraft()["error"].toString().isEmpty());
        s.lobbyDraft_["title"]="Friday games";s.lobbyDraftAction("category:party-games");s.lobbyDraftAction("access:approval");s.lobbyDraft_["startsAt"]=QDateTime::currentSecsSinceEpoch()+3600;
        s.submitLobbyDraft();QVERIFY(!s.lobbyCreateOpen());QCOMPARE(s.myLobbies().size(),1);QCOMPARE(s.lobbyState()["title"].toString(),QString("Friday games"));
        QCOMPARE(s.lobbyState()["access"].toString(),QString("approval"));QCOMPARE(s.viewedLobby().publicationTopic(),QString("party-games"));
        s.createAnotherLobby();s.lobbyChoices_={QVariantMap{{"key","removed"},{"title","Gone"}}};s.submitLobbyDraft();QCOMPARE(s.myLobbies().size(),1);QVERIFY(s.lobbyCreateOpen());
    }
    void lobbyRoomUsesIdentitySeatsAndVerifiedVoice() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);QObject::disconnect(s.session_,nullptr,&s,nullptr);
        s.receive(0,{{"state","connected"},{"userId","1"},{"name","Mika"},{"profile",QVariantMap{{"avatar","self.png"}}},
            {"friends",QVariantList{QVariantMap{{"id","2"},{"name","Ren"},{"avatar","ren.png"},{"type",1}}}}});
        s.openLobbyForPeople();if(s.lobbyCreateOpen())s.submitLobbyDraft();QCOMPARE(s.lobbyPlayers().size(),2);QCOMPARE(s.lobbyPlayers()[0].toMap()["seat"].toInt(),0);
        LobbyCoordinator guest;guest.configure("2","Ren",true);
        connect(&s.lobby_,&LobbyCoordinator::outgoing,&guest,[&](QString peer,QJsonObject p){if(peer=="2")guest.receive("1","Mika",p);});
        connect(&guest,&LobbyCoordinator::outgoing,&s,[&](QString,QJsonObject p){s.lobby_.receive("2","Ren",p);});
        QVERIFY(s.lobby_.invite("2"));guest.answer(true);
        QVERIFY(s.lobby_.selectGame({"game","A real game","snes","netplay",3,{}}));
        QCOMPARE(s.lobbyPlayers().size(),3);QCOMPARE(s.lobbyPlayers()[1].toMap()["avatar"].toString(),QString("ren.png"));
        QVERIFY(s.lobbyPlayers()[2].toMap()["empty"].toBool());
        s.lobby_.setConversation("300");s.lobbyChatChannel_="300";s.snapshot_["voice"]=QVariantMap{{"state","connected"},{"channel","999"},{"speakers",QStringList{"2"}}};
        QVERIFY(!s.lobbyPlayers()[1].toMap()["speaking"].toBool());
        auto voice=s.snapshot_["voice"].toMap();voice["channel"]="300";s.snapshot_["voice"]=voice;
        QVERIFY(s.lobbyPlayers()[1].toMap()["speaking"].toBool());
        guest.spectate(true);QCOMPARE(s.lobbySpectators().size(),1);QVERIFY(s.lobbyPlayers()[1].toMap()["empty"].toBool());
        QCOMPARE(s.lobbySpectators()[0].toMap()["id"].toString(),QString("2"));
        s.snapshot_["channel"]="300";s.snapshot_["messages"]=QVariantList{
            QVariantMap{{"id","10"},{"author","1"},{"name","Mika"},{"text","Player message"}},
            QVariantMap{{"id","11"},{"author","2"},{"name","Ren"},{"text","Spectator message"}}};
        s.drafts_["300"]="Unsent player draft";
        QCOMPARE(s.lobbyRoomMessages().size(),1);QCOMPARE(s.lobbyRoomMessages()[0].toMap()["id"].toString(),QString("10"));
        s.dispatch(Action::ToggleFavorite);QVERIFY(s.lobbySpectatorChat());
        QCOMPARE(s.lobbyRoomMessages().size(),1);QCOMPARE(s.lobbyRoomMessages()[0].toMap()["sourceIndex"].toInt(),1);
        QSignalSpy writes(&s,&SocialController::textRequested),commands(&s,&SocialController::commandRequested);
        s.compose();s.sendDraft();s.attachPicture();s.recordVoice();s.emoji();s.dispatch(Action::Secondary);s.dispatch(Action::Confirm);
        QCOMPARE(writes.size(),0);QVERIFY(s.menu_.isEmpty());QCOMPARE(s.draft(),QString("Unsent player draft"));
        for(const auto& command:commands)QVERIFY(command[0]!="send");
        s.toggleLobbySpectatorChat();QVERIFY(!s.lobbySpectatorChat());QCOMPARE(s.draft(),QString("Unsent player draft"));
        const auto room=s.lobbyState()["id"];QVERIFY(s.lobby_.selectGame({"other","Another game","gba","netplay",1,{}}));
        QCOMPARE(s.lobbyPlayers().size(),1);QCOMPARE(s.lobbyState()["id"],room);
    }
    void lobbyRoomNavigationRetainsChatAndMembership() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);QObject::disconnect(s.session_,nullptr,&s,nullptr);
        s.receive(0,{{"state","connected"},{"userId","1"},{"name","Mika"}});s.openLobbyForPeople();if(s.lobbyCreateOpen())s.submitLobbyDraft();
        const auto room=s.lobbyState()["id"];s.lobbyChatChannel_="300";s.snapshot_["channel"]="300";s.drafts_["300"]="Keep my draft";
        s.dispatch(Action::Down);QCOMPARE(s.lobbyRoomFocus(),QString("person:1"));
        s.dispatch(Action::Right);QCOMPARE(s.lobbyRoomFocus(),QString("invite"));
        s.dispatch(Action::Right);QVERIFY(s.lobbyChatFocused());
        s.dispatch(Action::ChangeView);QVERIFY(s.lobbyChatExpanded());s.dispatch(Action::Back);QVERIFY(!s.lobbyChatExpanded());QVERIFY(s.lobbyChatFocused());
        s.dispatch(Action::Back);QVERIFY(!s.lobbyChatFocused());QCOMPARE(s.lobbyRoomFocus(),QString("invite"));
        s.dispatch(Action::Down);QCOMPARE(s.lobbyRoomFocus(),QString("action:call"));
        s.dispatch(Action::Back);QVERIFY(!s.lobbyDetailFocused());QCOMPARE(s.lobbyState()["id"],room);
        s.selectLobby(0);QCOMPARE(s.lobbyRoomFocus(),QString("action:call"));QCOMPARE(s.draft(),QString("Keep my draft"));
        s.closeMyLobbies();s.lobbyAction("room-menu");QVERIFY(s.menuCommands_.contains("lobby:leave-confirm"));
    }
    void lobbyReadyIsNeverQueuedBeforeVerification() {
        class Runtime final : public LobbyRuntimeService { public: Completion pending;int launches=0;
            void prepare(QString,QJsonObject,Completion done) override {pending=done;}
            void launch(QString,QString,QJsonObject,Completion) override {++launches;}
            void cancel(QString) override {} } runtime;
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);QObject::disconnect(s.session_,nullptr,&s,nullptr);
        s.receive(0,{{"state","connected"},{"userId","1"},{"name","Mika"}});s.openLobbyForPeople();s.submitLobbyDraft();
        s.lobbyRuntime_.registerService("netplay",&runtime);
        QVERIFY(s.lobby_.selectGame({"game","Game","snes","netplay",2,{}}));
        s.reconcileLobbyPreparation();QVERIFY(bool(runtime.pending));
        s.requestLobbyReady();runtime.pending(true,{});s.reconcileLobbyPreparation();
        QCOMPARE(runtime.launches,0);QCOMPARE(s.lobbyState()["phase"].toString(),QString("gathering"));
        s.requestLobbyReady();QCOMPARE(runtime.launches,0);QVERIFY(s.lobbyRoomGame()["canStart"].toBool());
        s.requestLobbyReady();QVERIFY(!s.lobbyRoomGame()["canStart"].toBool());
        s.activateLobbyRoom("start");QCOMPARE(runtime.launches,0);
        s.requestLobbyReady();s.lobbyRoomFocus_="game";s.dispatch(Action::Right);
        QCOMPARE(s.lobbyRoomFocus(),QString("start"));s.dispatch(Action::Confirm);
        QCOMPARE(runtime.launches,1);QVERIFY(!s.lobbyRoomGame()["canStart"].toBool());
        s.activateLobbyRoom("start");QCOMPARE(runtime.launches,1);
    }
    void lobbyAutomaticPreparationIsBoundedAndNeverInterruptsGame() {
        class Runtime final : public LobbyRuntimeService { public: int attempts=0; void prepare(QString,QJsonObject,Completion done) override {++attempts;done(false,"Missing local game");}
            void launch(QString,QString,QJsonObject,Completion done) override {done(false,"unused");} void cancel(QString) override {} } runtime;
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);QObject::disconnect(s.session_,nullptr,&s,nullptr);
        s.receive(0,{{"state","connected"},{"userId","1"},{"name","Mika"}});s.openLobbyForPeople();if(s.lobbyCreateOpen())s.submitLobbyDraft();
        s.lobbyRuntime_.registerService("netplay",&runtime);s.setGameActive(true);
        QVERIFY(s.lobby_.selectGame({"game","Game","snes","netplay",2,{}}));s.reconcileLobbyPreparation();QCOMPARE(runtime.attempts,0);
        s.setGameActive(false);s.reconcileLobbyPreparation();QCOMPARE(runtime.attempts,1);
        s.reconcileLobbyPreparation();QCOMPARE(runtime.attempts,1);
        s.activateLobbyRoom("action:prepare");QCOMPARE(runtime.attempts,2);
        s.reconcileLobbyPreparation();QCOMPARE(runtime.attempts,2);
        QVERIFY(s.lobby_.selectGame({"another","Another","snes","netplay",2,{}}));s.reconcileLobbyPreparation();QCOMPARE(runtime.attempts,3);
        for(const auto& v:s.lobbyState()["members"].toList())QVERIFY(!v.toMap()["ready"].toBool());
    }
    void lobbyCatalogueControllerTraversesRailGridAndCreate() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);QObject::disconnect(s.session_,nullptr,&s,nullptr);
        QVariantList rows;const auto now=QDateTime::currentSecsSinceEpoch();
        for(int i=0;i<14;++i)rows.append(QVariantMap{{"lobby",QString::number(40+i)},{"owner",QString::number(40+i)},
            {"title",QString("Room %1").arg(i)},{"expires",now+180},{"topic","racing"},{"status","choosing"},{"access","public"}});
        s.receive(0,{{"state","connected"},{"userId","1"},{"lobbyDirectory",QVariantMap{{"rows",rows}}}});
        s.face_="lobby";s.lobbyDetailFocused_=true; // A restored detail flag must not trap a game-free catalogue.
        s.dispatch(Action::Back);QVERIFY(s.lobbyRailFocused());QCOMPARE(s.lobbyRailFocus(),1);
        s.dispatch(Action::Down);QCOMPARE(s.lobbyRailFocus(),2);QVERIFY(s.lobbyTopic().isEmpty());
        s.dispatch(Action::Confirm);QCOMPARE(s.lobbyTopic(),"racing");QVERIFY(!s.lobbyRailFocused());QCOMPARE(s.lobbyFocus(),1);
        s.dispatch(Action::Right);QCOMPARE(s.lobbyFocus(),2);s.dispatch(Action::Down);QCOMPARE(s.lobbyFocus(),4);
        s.dispatch(Action::Right);QCOMPARE(s.lobbyPage(),1);QCOMPARE(s.lobbyFocus(),9);
        s.dispatch(Action::Left);QCOMPARE(s.lobbyFocus(),4);s.dispatch(Action::Up);QCOMPARE(s.lobbyFocus(),2);
        s.dispatch(Action::Up);QCOMPARE(s.lobbyFocus(),0);s.dispatch(Action::Down);QCOMPARE(s.lobbyFocus(),2);
        s.turnLobbyPage(1);s.dispatch(Action::Up);QCOMPARE(s.lobbyFocus(),0);QCOMPARE(s.lobbyPage(),1);
        s.dispatch(Action::Down);QCOMPARE(s.lobbyFocus(),7);
        const auto remembered=s.lobbySelection_;s.dispatch(Action::Back);QCOMPARE(s.lobbyRailFocus(),2);
        s.dispatch(Action::Confirm);QCOMPARE(s.lobbySelection_,remembered);
        // A refresh may reorder rows, but never changes the remembered room identity.
        std::swap(rows[6],rows[8]);s.snapshot_["lobbyDirectory"]=QVariantMap{{"rows",rows}};
        QCOMPARE(s.lobbyFocus(),9);s.dispatch(Action::Back);s.dispatch(Action::Confirm);QCOMPARE(s.lobbyFocus(),9);
        s.activateLobbyRail(3);QCOMPARE(s.lobbyTopic(),"fighting");QCOMPARE(s.lobbyFocus(),0);QVERIFY(!s.lobbyRailFocused());
        s.dispatch(Action::Down);QCOMPARE(s.lobbyFocus(),0);s.dispatch(Action::Back);QCOMPARE(s.lobbyRailFocus(),3);
        // The sidebar exposes search and filters without needing a touch screen.
        QSignalSpy text(&s,&SocialController::textRequested);s.activateLobbyRail(0);QCOMPARE(text.size(),1);
        s.activateLobbyRail(8);QVERIFY(s.lobbyFreeOnly_);QVERIFY(s.lobbyRailFocused());
        s.activateLobbyRail(9);QVERIFY(!s.menu_.isEmpty());
    }
    void scheduledLobbyInvitationArmsLocallyOnlyAfterAcceptance() {
        QTemporaryDir dir;SocialController guest;
        QObject::disconnect(&guest,nullptr,guest.session_,nullptr);QObject::disconnect(guest.session_,nullptr,&guest,nullptr);
        guest.owner_="test-owner";guest.snapshot_={{"state","connected"},{"userId","2"},{"name","Guest"},{"doNotDisturb",true},{"notificationSound",false}};
        guest.lobby_.configure("2","Guest",true);QVERIFY(guest.lobbyReminders_.configure(dir.filePath("reminders.json")));
        LobbyCoordinator host;host.configure("1","Host",true);QVERIFY(host.create());
        const auto now=QDateTime::currentSecsSinceEpoch();QVERIFY(host.setDetails("Friday friends",now+7200));
        connect(&host,&LobbyCoordinator::outgoing,&guest,[&](QString,QJsonObject p){guest.lobby_.receive("1","Host",p);});
        connect(&guest.lobby_,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
        QVERIFY(host.invite("2"));const auto id=host.state()["id"].toString();
        QVERIFY(!guest.lobbyReminders_.contains(id));guest.selectLobby(1);
        QVERIFY(guest.menuDetail_.contains("15 minutes"));guest.lobbyAction("accept");QVERIFY(guest.lobbyReminders_.contains(id));
        QVERIFY(host.setDetails("Friday friends",now+60));
        QSignalSpy outbound(&guest.lobby_,&LobbyCoordinator::outgoing);guest.tickLobbyReminders();
        QCOMPARE(outbound.size(),0);QCOMPARE(guest.activityNotifications_.size(),1);QVERIFY(guest.activityNotifications_.first().toMap()["lobbyReminder"].toBool());
        guest.tickLobbyReminders();QCOMPARE(guest.activityNotifications_.size(),1);
        QVERIFY(host.setDetails("Friday friends",now+7200));host.leave();QVERIFY(!guest.lobbyReminders_.contains(id));
        guest.owner_.clear(); // Keep this isolated test out of account persistence teardown.
    }
    void lobbyReminderConsentPersistenceRescheduleResumeAndIsolation() {
        QTemporaryDir dir;LobbyReminders reminders;
        const auto file=dir.filePath("account-a.json");QVERIFY(reminders.configure(file));
        QJsonObject room{{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"owner","42"},{"title","Friday racing"},{"startsAt",10000}};
        QVERIFY(reminders.observe(room));QVERIFY(reminders.due(9200).isEmpty()); // Presence is not consent.
        QVERIFY(reminders.accept(room,8000));QVERIFY(reminders.due(9099).isEmpty());
        LobbyReminders restarted;QVERIFY(restarted.configure(file));
        QCOMPARE(restarted.due(9400).size(),1);QVERIFY(restarted.due(9401).isEmpty()); // Resume once.
        LobbyReminders again;QVERIFY(again.configure(file));QVERIFY(again.due(9402).isEmpty());
        auto spoof=room;spoof["owner"]="99";spoof["startsAt"]=12000;QVERIFY(again.observe(spoof));
        room["startsAt"]=12000;QVERIFY(again.observe(room));QVERIFY(again.due(11099).isEmpty());QCOMPARE(again.due(11100).size(),1);
        room["startsAt"]=14000;QVERIFY(again.observe(room));QVERIFY(again.due(14000).isEmpty()); // No stale alarm.
        room["startsAt"]=16000;QVERIFY(again.accept(room,14001));
        QVERIFY(again.configure(dir.filePath("account-b.json")));QVERIFY(again.due(15100).isEmpty());
        QVERIFY(again.configure(file));QVERIFY(again.contains(room["id"].toString()));
        room["startsAt"]=0;QVERIFY(again.observe(room));QVERIFY(again.due(15100).isEmpty());
        QVERIFY(!again.accept(room,14000));
        LobbyReminders unavailable;QVERIFY(!unavailable.accept(QJsonObject{{"id",room["id"]},{"owner","42"},{"title","Room"},{"startsAt",20000}},14000));
    }
    void lobbyCataloguePagesFiltersAndScheduleConsent() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);QObject::disconnect(s.session_,nullptr,&s,nullptr);
        QVariantList rows;const auto now=QDateTime::currentSecsSinceEpoch();
        for(int i=0;i<14;++i)rows.append(QVariantMap{{"lobby",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"owner",QString::number(40+i)},
            {"title",QString("Room %1").arg(i)},{"name","Host"},{"expires",now+180},{"topic",i%2?"racing":"adventure"},
            {"seats",4},{"available",i==0?0:3},{"streams",i==0?1:0},{"status","gathering"},{"access","public"},
            {"game",QVariantMap{{"title","Shared game"},{"platform","snes"},{"mode","netplay"}}}});
        s.receive(0,{{"state","connected"},{"userId","1"},{"name","Host"},{"lobbyDirectory",QVariantMap{{"rows",rows}}}});
        QCOMPARE(s.lobbyPageCount(),3);s.turnLobbyPage(1);QCOMPARE(s.lobbyPage(),1);QCOMPARE(s.lobbyFocus(),7);
        s.turnLobbyPage(10);QCOMPARE(s.lobbyPage(),2);s.turnLobbyPage(1);QCOMPARE(s.lobbyPage(),2);
        s.lobbyAction("category:racing");QCOMPARE(s.lobbyPage(),0);QCOMPARE(s.lobbyRows().size(),8);
        s.lobbyAction("clear-filters");s.lobbyAction("free");QCOMPARE(s.lobbyRows().size(),14); // Stream != free player seat.
        s.lobbyAction("platform:gba");QCOMPARE(s.lobbyRows().size(),1);
        s.lobbyAction("clear-filters");s.textPurpose_="lobby-search";s.applyText("Room 13");QCOMPARE(s.lobbyRows().size(),2);
        s.lobbyAction("clear-filters");s.selectLobby(0);s.submitLobbyDraft();QCOMPARE(s.lobbyState()["phase"].toString(),QString("choosing"));
    }

    void streamFramesAreBoundedAndExitClearsTheVideoSink() {
        SocialStream stream;QVideoSink sink;stream.setVideoSink(&sink);
        QSignalSpy frames(&sink,&QVideoSink::videoFrameChanged),ended(&stream,&SocialStream::ended);
        const auto endpoint=stream.open();QVERIFY(!endpoint.isEmpty());
        QLocalSocket writer;writer.connectToServer(endpoint);QVERIFY(writer.waitForConnected());
        QByteArray frame(20,'\0');auto* p=reinterpret_cast<uchar*>(frame.data());
        qToLittleEndian<quint32>(0x31565254,p);qToLittleEndian<quint32>(1,p+4);qToLittleEndian<quint32>(1,p+8);qToLittleEndian<quint32>(4,p+12);
        frame[16]=char(221);frame[17]=char(34);frame[18]=char(17);
        writer.write(frame.left(7));writer.flush();QTest::qWait(10);QVERIFY(!stream.hasFrame());
        writer.write(frame.mid(7));writer.flush();QTRY_VERIFY(stream.hasFrame());QVERIFY(sink.videoFrame().isValid());
        qToLittleEndian<quint32>(0xffffffff,p+4);writer.write(frame);writer.flush();
        QTRY_COMPARE(ended.size(),1);QVERIFY(!stream.active());QVERIFY(!sink.videoFrame().isValid());
    }

    void screenMetadataAndWatchLifecycleNeverReplaceTheVoiceCall() {
        FluxerSession s;bind(s);s.voiceChannel_=channel;s.voiceState_="connected";
        const QString identity=QString("user_")+remote+"_connection";
        s.mediaEvent({{"event","streams"},{"streams",QJsonArray{QJsonObject{{"identity",identity}},QJsonObject{{"identity","unknown"}}}}});
        QCOMPARE(s.voiceStreams_.size(),1);
        QVERIFY(s.mediaCommand("voice-watch",{{"identity",identity},{"socket","private-frame-socket"}}));
        QCOMPARE(s.voiceWatching_,identity);QCOMPARE(s.voiceChannel_,QString(channel));QVERIFY(s.voiceMuted_);
        QVERIFY(s.mediaCommand("voice-watch-stop",{}));QVERIFY(s.voiceWatching_.isEmpty());QCOMPARE(s.voiceState_,QString("connected"));
        s.mediaEvent({{"event","sharing"},{"active",true}});QVERIFY(s.voiceSharing_);
        s.leaveVoice();QVERIFY(!s.voiceSharing_);QVERIFY(s.voiceStreams_.isEmpty());
    }

    void voiceSpeakersRequireCurrentMediaRosterAndProjectOntoActualPlayers() {
        FluxerSession s;bind(s);s.voiceChannel_=channel;s.voiceState_="connected";
        s.voicePresence({{"count",2},{"people",QJsonArray{QString(remote),s.self_}},
            {"speakers",QJsonArray{QString(remote),s.self_,"not-an-account","1234567890123456789"}}});
        QCOMPARE(s.voiceSpeakers_,(QSet<QString>{remote,s.self_}));
        s.voiceState_="reconnecting";s.voicePresence({{"people",QJsonArray{QString(remote)}},{"speakers",QJsonArray{QString(remote)}}});
        QVERIFY(s.voiceSpeakers_.isEmpty());
        SocialController c;disconnect(c.session_,nullptr,&c,nullptr);disconnect(&c,nullptr,c.session_,nullptr);
        c.snapshot_={{"state","connected"},{"userId","1"},{"voice",QVariantMap{{"state","connected"},{"speakers",QStringList{"2"}}}}};
        c.gameParty_={{"party","session"},{"running",true},{"host",true},{"members",QVariantList{
            QVariantMap{{"name","Same name"},{"peer","online:2"},{"slot",2},{"ready",true}},
            QVariantMap{{"name","Same name"},{"peer","nearby-device"},{"slot",3},{"ready",true}}}}};
        c.setGameActive(true);QVERIFY(c.activePlayers()[0].toMap()["speaking"].toBool());QVERIFY(!c.activePlayers()[1].toMap()["speaking"].toBool());
        c.snapshot_["friends"]=QVariantList{QVariantMap{{"id","2"},{"type",2}}};QVERIFY(!c.activePlayers()[0].toMap()["speaking"].toBool());
        c.snapshot_["friends"]=QVariantList{};c.snapshot_["voice"]=QVariantMap{{"state","reconnecting"},{"speakers",QStringList{"2"}}};
        QVERIFY(!c.activePlayers()[0].toMap()["speaking"].toBool());
        s.leaveVoice();QVERIFY(s.voicePeople_.isEmpty());QVERIFY(s.voiceSpeakers_.isEmpty());
    }

    void playerProjectionUsesRunningSeatsAndProviderUnreadOnly() {
        SocialController c;disconnect(c.session_,nullptr,&c,nullptr);disconnect(&c,nullptr,c.session_,nullptr);
        c.snapshot_={{"state","connected"},{"userId","1"},{"profile",QVariantMap{{"avatar","self-avatar"}}},
            {"chats",QVariantList{QVariantMap{{"id","50"},{"kind","groups"},{"unread",99},{"members",QVariantList{QVariantMap{{"id","2"}}}}},
                QVariantMap{{"id","51"},{"kind","chats"},{"unread",3},{"avatar","friend-avatar"},{"members",QVariantList{QVariantMap{{"id","2"}}}}}}}};
        c.gameParty_={{"party","session"},{"running",true},{"host",true},{"members",QVariantList{
            QVariantMap{{"name","Friend"},{"peer","online:2"},{"slot",2},{"ready",true}},
            QVariantMap{{"name","Me"},{"slot",1},{"ready",true}},
            QVariantMap{{"name","Pending"},{"peer","online:3"},{"slot",3},{"ready",false}},
            QVariantMap{{"name","Friend"},{"peer","nearby-device"},{"slot",4},{"ready",true}}}}};
        QVERIFY(c.activePlayers().isEmpty());c.setGameActive(true);auto rows=c.activePlayers();QCOMPARE(rows.size(),3);
        QCOMPARE(rows[0].toMap()["id"].toString(),QString("1"));QCOMPARE(rows[0].toMap()["avatar"].toString(),QString("self-avatar"));
        QCOMPARE(rows[1].toMap()["unread"].toInt(),3);QCOMPARE(rows[1].toMap()["avatar"].toString(),QString("friend-avatar"));
        QVERIFY(rows[2].toMap()["id"].toString().isEmpty());QVERIFY(!rows[2].toMap()["canMessage"].toBool());
        QSignalSpy commands(&c,&SocialController::commandRequested);QVERIFY(!c.openDirectConversation("3"));QVERIFY(c.openDirectConversation("2"));
        QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString("2"));
        c.lobby_.configure("1","Me",true);c.lobby_.create();c.face_="lobby";c.lobbyChatChannel_="70";
        commands.clear();c.openLobbyChat();QVERIFY(commands.isEmpty());
        c.releaseDirectConversation("3");c.openLobbyChat();QVERIFY(commands.isEmpty());
        c.releaseDirectConversation("2");QCOMPARE(commands.last()[0].toString(),QString("open"));QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString("70"));
        c.snapshot_["friends"]=QVariantList{QVariantMap{{"id","2"},{"type",2}}};QVERIFY(!c.openDirectConversation("2"));QCOMPARE(c.activePlayers()[1].toMap()["unread"].toInt(),0);
        c.snapshot_["friends"]=QVariantList{};c.gameParty_["host"]=false;c.gameParty_["hostPeer"]="online:8";
        QCOMPARE(c.activePlayers()[0].toMap()["id"].toString(),QString("8"));
        c.gameParty_["running"]=false;QVERIFY(c.activePlayers().isEmpty());
    }
    void miniChatWaitsForCorrectRecipientAndPreservesDraftOnReturn() {
        SocialController c;disconnect(c.session_,nullptr,&c,nullptr);disconnect(&c,nullptr,c.session_,nullptr);
        c.snapshot_={{"state","connected"},{"userId","1"},{"channel","old"},
            {"chats",QVariantList{QVariantMap{{"id","51"},{"kind","chats"},{"members",QVariantList{QVariantMap{{"id","2"}}}}}}}};
        c.gameParty_={{"party","session"},{"running",true},{"host",true},{"members",QVariantList{QVariantMap{{"name","Friend"},{"peer","online:2"},{"slot",2},{"ready",true}}}}};c.setGameActive(true);
        disconnect(&c.lobby_,nullptr,&c.lobbyRuntime_,nullptr);
        c.lobby_.configure("1","Host",true);QVERIFY(c.lobby_.create());
        auto* room=c.lobby_.activeRoom();room->allowRuntime(true);QVERIFY(room->selectGame({"fixture","Fixture","nes","netplay",1}));
        QVERIFY(room->setConversation("51"));QVERIFY(room->prepared(room->state()["generation"].toInteger(),true));QVERIFY(room->ready(true));
        QString ticket;connect(room,&LobbyRoomCoordinator::launchRequested,room,[&](QString t,QJsonObject){ticket=t;});
        QVERIFY(room->start());QVERIFY(room->acknowledgeLaunch(ticket,true));
        AdventureExitController exit;exit.beginSession(AdventureSavePolicy::ManualConfirm);exit.setAvailable(true);
        AdventureExitPresentation view(exit);TextEntryController keyboard;PlayerOverlayController hud(c,keyboard,view);hud.setGame(42);
        connect(&view,&AdventureExitPresentation::menuCaptureRequested,&view,[&](quint64 token){view.menuCaptureCompleted(token,{});});
        connect(&c,&SocialController::textRequested,&keyboard,[&](QString title,QString initial,int limit){keyboard.begin(title,initial,limit);});
        connect(&keyboard,&TextEntryController::accepted,&c,&SocialController::applyText);
        QSignalSpy requests(&hud,&PlayerOverlayController::optionsRequested),dismissed(&view,&AdventureExitPresentation::menuDismissed),commands(&c,&SocialController::commandRequested);
        const auto neutral=[&]{view.updateInput(view.inputGeneration(),{true,true});};
        hud.openPlayer("2");QCOMPARE(requests.size(),1);QVERIFY(!hud.chatOpen());QVERIFY(view.requestMenu());
        view.setInputIsolated(true);view.setWindowFocused(true);neutral();QVERIFY(hud.chatOpen());QVERIFY(!view.ready());neutral();
        QVERIFY(!hud.conversationReady());hud.compose();QVERIFY(!keyboard.isOpen());
        c.snapshot_["channel"]="51";emit c.changed();QVERIFY(hud.conversationReady());
        hud.dispatch(Action::Confirm);QVERIFY(keyboard.isOpen());keyboard.insertText("Keep this draft");hud.dispatch(Action::Back);
        QVERIFY(!keyboard.isOpen());QCOMPARE(c.draft(),QString("Keep this draft"));QVERIFY(hud.chatOpen());
        hud.dispatch(Action::Back);QVERIFY(!hud.chatOpen());QCOMPARE(dismissed.size(),1);QCOMPARE(exit.phase(),AdventureExitController::Phase::Idle);
        hud.openPlayer("2");QVERIFY(view.requestMenu());neutral();neutral();QVERIFY(hud.conversationReady());
        hud.send();QVERIFY(hud.chatOpen());QCOMPARE(dismissed.size(),1);QVERIFY(c.draft().isEmpty());
        QVERIFY(std::any_of(commands.begin(),commands.end(),[](const auto& row){return row[0]=="send"&&row[1].toMap()["channel"]=="51";}));
        hud.compose();keyboard.insertText("Unsent");
        hud.setGame(43);QVERIFY(!hud.chatOpen());QVERIFY(!keyboard.isOpen());QCOMPARE(c.draft(),QString("Unsent"));
        hud.openPlayer("2");QVERIFY(view.requestMenu());neutral();neutral();QVERIFY(hud.chatOpen());
        hud.compose();keyboard.insertText("Account-private text");++c.generation_;c.snapshot_["userId"]="other-account";c.drafts_.clear();emit c.changed();
        QVERIFY(!hud.chatOpen());QVERIFY(c.drafts_.isEmpty());QCOMPARE(exit.phase(),AdventureExitController::Phase::Idle);
    }
    void playerPositionsAreClampedAndIsolatedByTrainer() {
        SocialController c;disconnect(c.session_,nullptr,&c,nullptr);disconnect(&c,nullptr,c.session_,nullptr);
        AdventureExitController exit;AdventureExitPresentation view(exit);TextEntryController keyboard;
        PlayerOverlayController hud(c,keyboard,view);c.owner_=QUuid::createUuid().toString();emit c.changed();
        hud.setPosition({-3,8});QCOMPARE(hud.position(),QPointF(0,1));hud.setPosition({0.25,0.75});
        PlayerOverlayController restored(c,keyboard,view);emit c.changed();QCOMPARE(restored.position(),QPointF(0.25,0.75));
        c.owner_=QUuid::createUuid().toString();emit c.changed();QCOMPARE(restored.position(),QPointF());
        restored.setPosition({qQNaN(),0.5});QCOMPARE(restored.position(),QPointF());
    }
    void peopleFirstLobbyPreservesFriendAndGroupContext() {
        const QVariantList members{QVariantMap{{"id","1"}},QVariantMap{{"id","2"}},QVariantMap{{"id","3"}}};
        const QVariantMap group{{"id","50"},{"kind","groups"},{"members",members}};
        SocialController c;c.snapshot_={{"state","connected"},{"userId","1"},{"name","Host"},
            {"chats",QVariantList{group}}};
        c.lobby_.configure("1","Host",true);QSignalSpy packets(&c.lobby_,&LobbyCoordinator::outgoing);
        c.openLobbyForPeople("2");c.submitLobbyDraft();QVERIFY(c.lobby_.state()["game"].toObject()["id"].toString().isEmpty());
        QCOMPARE(c.face(),QString("lobby"));QCOMPARE(packets.last()[0].toString(),QString("2"));
        const auto room=c.lobby_.state()["id"].toString();packets.clear();c.openLobbyForPeople({},"50");
        QCOMPARE(c.lobby_.state()["id"].toString(),room);QCOMPARE(c.lobby_.state()["conversation"].toString(),QString("50"));
        QSet<QString> invited;for(const auto& p:packets)if(p[1].toJsonObject()["kind"]=="invite")invited.insert(p[0].toString());
        QCOMPARE(invited,(QSet<QString>{"2","3"}));
        c.lobby_.leave();c.openLobbyForPeople({},"missing");QVERIFY(c.lobby_.state().isEmpty());
    }
    void lobbyInvitationBackDefersAndPublicProjectionExcludesRuntime() {
        SocialController c;c.snapshot_={{"state","connected"},{"userId","2"},{"name","Guest"}};c.lobby_.configure("2","Guest",true);
        LobbyCoordinator host;host.configure("1","Host",true);QVERIFY(host.create());
        QVERIFY(host.selectGame({"game","Game","test","runtime.test",2,{{"content","private-digest"}}}));
        connect(&host,&LobbyCoordinator::outgoing,&c,[&](QString,QJsonObject p){if(p["kind"]=="invite")QVERIFY(!p["data"].toObject()["game"].toObject().contains("runtime"));c.lobby_.receive("1","Host",p);});
        QVERIFY(host.invite("2"));const auto offer=c.lobby_.invitation();QVERIFY(offer["expires"].toInteger()>QDateTime::currentSecsSinceEpoch()+250);
        c.openLobby();c.selectLobby(1);QSignalSpy deferred(&c,&SocialController::lobbyDeferred);c.dispatch(Action::Back);
        QCOMPARE(deferred.size(),1);QCOMPARE(c.lobby_.invitation(),offer);QVERIFY(c.lobby_.state().isEmpty());
        c.dispatch(Action::Back);QVERIFY(c.lobbyRailFocused());QCOMPARE(deferred.size(),1);
        SocialController own;own.snapshot_={{"state","connected"},{"userId","1"},{"name","Host"}};own.lobby_.configure("1","Host",true);own.lobby_.create("public");
        own.lobby_.selectGame({"game","Game","test","runtime.test",2,{{"content","private-digest"}}});
        QSignalSpy commands(&own,&SocialController::commandRequested);own.publishLobby();
        for(const auto& cmd:commands)if(cmd[0]=="lobby-directory-publish")QVERIFY(!cmd[1].toMap()["game"].toMap().contains("runtime"));
    }
    void lobbyJoinFromActivityKeepsRunningGameAndRejectsChangedRoom() {
        SocialController c;c.snapshot_={{"state","connected"},{"userId","2"}};c.gameActive_=true;
        const auto room=QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QVariantMap offer{{"lobby",room},{"party",room},{"joinable",true},{"game",QVariantMap{{"label","Choosing a game"}}}};
        c.gameActivities_["1"]=offer;QSignalSpy joins(&c,&SocialController::partyJoin);
        c.directJoin("1",offer);QCOMPARE(joins.size(),1);QCOMPARE(joins.last()[1].toJsonObject()["lobby"].toString(),room);
        c.addJoinChoice("Join lobby","join-reviewed","1",offer);c.gameActivities_["1"]["lobby"]="changed";c.selectMenu(0);
        QCOMPARE(joins.size(),1);
    }
    void pendingContactDoesNotBlockAnotherMembersGroupAdmission() {
        FluxerSession s;bind(s);s.relationships_[remote]={{"type",1}};s.relationships_["4"]={{"type",1}};
        const QJsonObject group{{"id",channel},{"type",3},{"recipients",QJsonArray{QJsonObject{{"id",remote}}}}};int adds=0;
        s.setTransport([&](auto method,auto path,auto,Completion done,QByteArray){
            if(method=="GET")done({200,QJsonDocument(group)});
            else {QCOMPARE(method,QByteArray("PUT"));QVERIFY(path.endsWith("/recipients/4"));++adds;done({204,{}});}
        });
        QSignalSpy results(&s,&FluxerSession::lobbyChatFinished);
        s.command("lobby-chat",{{"lobby",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"channel",channel},{"host",true},{"peers",QStringList{remote,"3","4"}}});
        QCOMPARE(adds,1);QCOMPARE(results.last()[2].toString(),channel);
    }
    void lobbyChatStartsWithReadyFriendsWhileAnotherMemberConnects() {
        FluxerSession s;bind(s);s.relationships_[remote]={{"type",1}};QStringList steps;
        s.setTransport([&](auto method,QString path,auto body,Completion done,QByteArray){
            if(path=="/v1/users/@me/channels"){QCOMPARE(body["recipients"].toArray(),QJsonArray{});steps<<"create";done({200,QJsonDocument(QJsonObject{{"id",channel},{"type",3}})});}
            else if(path.endsWith("/messages")){steps<<"mark";QVERIFY(!lobbyChatIdentity::decode(body["content"].toString()).isEmpty());done({200,QJsonDocument(QJsonObject{{"id","555"}})});}
            else if(path.endsWith("/pins/555")){steps<<"pin";done({204,{}});}
            else {QCOMPARE(method,QByteArray("PUT"));QVERIFY(path.endsWith(QString("/recipients/")+remote));steps<<"add";done({204,{}});}
        });
        QSignalSpy results(&s,&FluxerSession::lobbyChatFinished);
        s.command("lobby-chat",{{"lobby",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"host",true},{"peers",QStringList{remote,"3"}}});
        QCOMPARE(steps,(QStringList{"create","mark","pin","add"}));QCOMPARE(results.size(),1);QCOMPARE(results.first()[2].toString(),channel);
    }
    void lateGroupAdmissionRetriesTheGuestConversationAutomatically() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","2"}});
        LobbyCoordinator host;host.configure("1","Host",true);host.create("public");host.setConversation(channel);
        connect(&host,&LobbyCoordinator::outgoing,&c,[&](QString,QJsonObject p){c.lobby_.receive("1","Host",p);});
        connect(&c.lobby_,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
        c.lobby_.join("1",host.state()["id"].toString());c.lobbyChatBusy_=false;
        QSignalSpy commands(&c,&SocialController::commandRequested);auto snapshot=c.snapshot_;
        snapshot["chats"]=QVariantList{QVariantMap{{"id",channel},{"kind","groups"},{"name","Lobby"}}};c.receive(0,snapshot);
        bool retried=false;for(const auto& command:commands)if(command[0]=="lobby-chat"&&command[1].toMap()["channel"]==channel)retried=true;
        QVERIFY(retried);
    }
    void recentTemporaryFriendUsesKeepActionAndHidesOnlyItsAutomaticRequest() {
        SocialController c;
        const auto lobby=playedRoom();
        QVariantMap s{{"state","connected"},{"userId","1"},{"friends",QVariantList{QVariantMap{{"id","2"},{"type",1}}}},
            {"temporaryContacts",QVariantMap{{"peers",QVariantMap{{"2",QVariantMap{{"temporary",true},{"canKeep",true},{"status","Temporary lobby friend"}}}}}}}};
        c.receive(0,s);c.recentTogether_.observe(lobby,100);c.setFace("recent");
        QCOMPARE(c.recentPeople().first().toMap()["friendLabel"].toString(),"Keep as friend");
        QSignalSpy commands(&c,&SocialController::commandRequested);c.dispatch(Action::Secondary);
        QCOMPARE(commands.last()[0].toString(),"recent-add");
        c.profile("2");QVERIFY(c.menuCommands_.contains("contact-keep"));c.closeMenu();
        s["friends"]=QVariantList{QVariantMap{{"id","2"},{"type",3}},QVariantMap{{"id","3"},{"type",3}}};c.receive(0,s);
        QCOMPARE(c.notifications().size(),1);QCOMPARE(c.notifications().first().toMap()["id"].toString(),"3");
    }
    void keepingTemporaryFriendPersistsWithoutAnotherFriendRequestOrVoiceChange() {
        QTemporaryDir dir;const auto room=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QFile file(dir.path()+"/contacts.json");QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(QJsonObject{{"v",1},{"self","1501314428688998181"},{"contacts",QJsonObject{{remote,QJsonObject{
            {"room",room},{"lease",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"host",true},{"phase","friend"},{"type",1},{"since","2026-10-13T10:00:00.000Z"}}}}}}).toJson());file.close();
        FluxerSession s;bind(s);int friendships=0;
        s.setTransport([&](auto,auto path,auto,Completion done,QByteArray){if(path.contains("/relationships"))++friendships;done({200,QJsonDocument(QJsonObject{{"id","999"}})});});
        s.channels_[channel]={{"id",channel},{"type",1},{"recipients",QJsonArray{QJsonObject{{"id",remote}}}}};
        s.relationships_[remote]={{"id",remote},{"type",1}};s.voiceChannel_="ongoing-call";s.voiceMuted_=true;
        s.temporaryContacts_.configure(file.fileName(),s.self_);s.temporaryContacts_.context({},true);
        s.command("recent-add",{{"id",remote}});QCOMPARE(friendships,0);
        QVERIFY(!s.temporaryContacts_.state()["peers"].toMap().contains(remote));
        QCOMPARE(s.voiceChannel_,"ongoing-call");QVERIFY(s.voiceMuted_);
        QVERIFY(file.open(QIODevice::ReadOnly));const auto journal=QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(journal["contacts"].toObject()[remote].toObject()["phase"].toString(),"kept");
    }
    void recentTogetherPersistsOnlyActualMeetingsAndIsolatesAccounts() {
        QTemporaryDir dir;const auto path=dir.path()+"/recent.json";
        RecentTogether history;history.configure(path,"1");
        auto room=playedRoom();room["phase"]="waiting";
        QVERIFY(!history.observe(room,100));QVERIFY(history.rows().isEmpty());
        room["phase"]="launching";QVERIFY(!history.observe(room,100));room["phase"]="playing";
        auto roster=room["members"].toArray();
        roster.append(QJsonObject{{"id","3"},{"name","Offline"},{"seat",3},{"connected",false}});
        roster.append(QJsonObject{{"id","4"},{"name","Watcher"},{"seat",4},{"connected",true},{"spectator",true}});
        roster.append(QJsonObject{{"id","5"},{"name","Preparing"},{"seat",5},{"connected",true},{"admission","pending"}});room["members"]=roster;
        QVERIFY(history.observe(room,101));QCOMPARE(history.rows().size(),1);
        QVERIFY(!history.observe(room,102));QCOMPARE(history.rows().first().toMap()["last"].toLongLong(),101);
        RecentTogether reopened;reopened.configure(path,"1");QCOMPARE(reopened.rows(),history.rows());
        reopened.configure(path,"other-account");QVERIFY(reopened.rows().isEmpty());
        reopened.configure(path,"1");QVERIFY(!reopened.observe(room,102));QCOMPARE(reopened.rows().first().toMap()["game"].toString(),QString("Pong"));QVERIFY(reopened.forget("2"));
        RecentTogether forgotten;forgotten.configure(path,"1");QVERIFY(forgotten.rows().isEmpty());
        QVERIFY(!reopened.observe(room,103)); // A heartbeat must not undo Forget.
        reopened.observe({},104);QVERIFY(!reopened.observe(room,105));
        room["ticket"]="second-session";QVERIFY(reopened.observe(room,106)); // A new successful game may.
    }
    void recentTogetherRecordsAcceptedLobbyAndKeepsOfflineActionsLocal() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","2"},{"name","Guest"}});
        LobbyCoordinator host;host.configure("1","Host",true);QVERIFY(host.create());
        connect(&host,&LobbyCoordinator::outgoing,&c,[&](QString,QJsonObject p){c.lobby_.receive("1","Host",p);});
        connect(&c.lobby_,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
        QVERIFY(host.invite("2"));QVERIFY(c.recentPeople().isEmpty());c.lobby_.answer(true);
        QVERIFY(c.recentPeople().isEmpty()); // Joining a room alone is not co-play.
        c.recentTogether_.observe(playedRoom("2","1"),100);
        QCOMPARE(c.recentPeople().size(),1);QCOMPARE(c.recentPeople().first().toMap()["id"].toString(),"1");
        c.lobby_.leave();QCOMPARE(c.recentPeople().size(),1);c.setFace("recent");
        QSignalSpy commands(&c,&SocialController::commandRequested);c.dispatch(Action::Secondary);
        QCOMPARE(commands.last()[0].toString(),"recent-add");QCOMPARE(commands.last()[1].toMap()["id"].toString(),"1");
        c.snapshot_["state"]="disconnected";commands.clear();c.recentAction("profile");c.recentAction("friend");QVERIFY(commands.isEmpty());
        c.dispatch(Action::ContextMenu);c.selectMenu(0);QVERIFY(c.recentPeople().isEmpty());QVERIFY(commands.isEmpty());
    }
    void recentFriendActionPreservesFriendsRequestsAndBlocks() {
        FluxerSession s;bind(s);int posts=0;
        s.setTransport([&](auto method,auto path,auto,Completion done,QByteArray){
            if(method=="POST") {++posts;QVERIFY(path.endsWith(QString("/relationships/")+remote));done({403,{}});}
            else done({200,QJsonDocument(QJsonArray{})});
        });
        for(int type:{1,2,4}) {s.relationships_[remote]={{"type",type}};s.command("recent-add",{{"id",remote}});}
        s.command("recent-add",{{"id","../invalid"}});s.command("recent-add",{{"id",s.self_}});QCOMPARE(posts,0);
        s.relationships_.remove(remote);s.command("recent-add",{{"id",remote}});QCOMPARE(posts,1);
        s.relationships_[remote]={{"type",3}};s.command("recent-add",{{"id",remote}});QCOMPARE(posts,2);
    }
    void gameContextKeepsRoomAndRequiresDeliberateGameReplacement() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","1"},{"name","Host"}});
        c.setLobbyGames({QVariantMap{{"key","a"},{"id","a"},{"title","First"},{"platform","nes"},{"mode","netplay"},{"seats",2}},
            QVariantMap{{"key","b"},{"id","b"},{"title","Second"},{"platform","nes"},{"mode","netplay"},{"seats",2}}});
        c.openLobbyForGame("a");QVERIFY(c.lobbyPicker()["modeOpen"].toBool());
        c.lobbyPickerAction("mode:a");c.lobbyPickerAction("apply");c.submitLobbyDraft();const auto room=c.lobby_.state()["id"].toString();
        QCOMPARE(c.lobby_.state()["game"].toObject()["id"].toString(),"a");
        c.openLobbyForGame("b");QCOMPARE(c.lobby_.state()["id"].toString(),room);QCOMPARE(c.lobby_.state()["game"].toObject()["id"].toString(),"a");
        QVERIFY(c.menuCommands_.isEmpty());QVERIFY(c.lobbyPicker()["modeOpen"].toBool());
        c.lobbyPickerAction("mode:b");c.lobbyPickerAction("clear");c.lobbyPickerAction("game:b");c.lobbyPickerAction("mode:b");c.lobbyPickerAction("apply");
        QCOMPARE(c.lobby_.state()["game"].toObject()["id"].toString(),"b");
        c.openLobbyForGame("missing");QCOMPARE(c.lobby_.state()["game"].toObject()["id"].toString(),"b");
    }
    void gameContextOffersNativeAndRemoteModesWithoutChangingTheRoom() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","1"},{"name","Host"}});
        c.setLobbyGames({QVariantMap{{"key","a"},{"id","native"},{"title","Game"},{"platform","nes"},{"mode","netplay"},{"seats",2}},
            QVariantMap{{"key","a:remoteplay"},{"adventure","a"},{"id","remote"},{"title","Game"},{"platform","nes"},{"mode","remoteplay.v1"},{"modeLabel","Remote Play"},{"seats",2}}});
        c.openLobbyForGame("a");QVERIFY(c.lobbyPicker()["open"].toBool());QVERIFY(c.lobby_.state().isEmpty());
        c.lobbyPickerAction("mode:a:remoteplay");c.lobbyPickerAction("apply");c.submitLobbyDraft();const auto room=c.lobby_.state()["id"].toString();
        QCOMPARE(c.lobby_.state()["game"].toObject()["id"].toString(),QString("remote"));
        c.openLobbyForGame("a");QCOMPARE(c.lobby_.state()["id"].toString(),room);QCOMPARE(c.lobby_.state()["game"].toObject()["id"].toString(),"remote");
    }
    void pickerKeepsIdentityAcrossHeartbeatReorderRenameAndRemoval() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","1"},{"name","Host"}});
        QVariantList games;
        for(int i=0;i<120;++i)games.append(QVariantMap{{"key",QString::number(i)},{"id","net:"+QString::number(i)},{"title","Game "+QString::number(i)},{"platform","nes"},{"mode","netplay"},{"seats",2}});
        c.setLobbyGames(games);c.createAnotherLobby();c.openLobbyPicker();c.lobbyPickerAction("focus:85");
        QSignalSpy resets(c.lobbyPickerModel(),&QAbstractItemModel::modelReset);
        QSignalSpy navigations(&c,&SocialController::lobbyPickerNavigate);
        QSignalSpy refreshes(&c.lobbyPickerModel_,&LobbyPickerModel::refreshed);
        for(int i=0;i<180;++i){emit c.changed();c.setLobbyGames(games);}
        QCOMPARE(refreshes.size(),0);QCOMPARE(c.lobbyPicker()["gameKey"].toString(),"85");
        games.move(85,91);auto renamed=games[91].toMap();renamed["title"]="Renamed";games[91]=renamed;c.setLobbyGames(games);
        QCOMPARE(c.lobbyPicker()["gameKey"].toString(),"85");QCOMPARE(c.lobbyPicker()["focus"].toInt(),91);
        games.removeAt(91);c.setLobbyGames(games);
        QCOMPARE(c.lobbyPicker()["focus"].toInt(),91);QCOMPARE(c.lobbyPicker()["gameKey"],games[91].toMap()["key"]);
        QCOMPARE(resets.size(),0);QCOMPARE(navigations.size(),0);
    }
    void pickerGroupsExactModesPinsAcrossFiltersAndCancelsWithoutMutation() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","1"},{"name","Host"}});
        const QVariantMap a{{"key","a"},{"id","native-a"},{"title","Same title"},{"platform","nes"},{"mode","netplay"},{"seats",2}};
        const QVariantMap b{{"key","a:remote"},{"adventure","a"},{"id","remote-a"},{"title","Same title"},{"platform","nes"},{"mode","remoteplay.v1"},{"seats",2}};
        const QVariantMap other{{"key","b"},{"id","native-b"},{"title","Same title"},{"platform","snes"},{"mode","netplay"},{"seats",2}};
        c.setLobbyGames({a,b,other});c.createAnotherLobby();c.openLobbyPicker();
        QCOMPARE(c.lobbyPickerRows().size(),2);
        c.lobbyPickerAction("game:a");QCOMPARE(c.lobbyPicker()["modes"].toList().size(),2);
        c.dispatch(Action::Back);QVERIFY(c.lobbyChoices_.isEmpty());QVERIFY(c.lobbyPickerOpen());QVERIFY(!c.lobbyPicker()["modeOpen"].toBool());
        c.lobbyPickerAction("game:a");c.lobbyPickerAction("mode:a");
        c.lobbyPickerAction("game:a");c.lobbyPickerAction("mode:a:remote");
        QCOMPARE(c.lobbyChoices_.size(),2);QCOMPARE(c.lobbyPickerPins().size(),1);
        QCOMPARE(c.lobbyPickerPins()[0].toMap()["modes"].toList().size(),2);
        c.lobbyPickerAction("category:snes");QCOMPARE(c.lobbyPickerRows().size(),1);QCOMPARE(c.lobbyPickerPins().size(),1);
        c.lobbyPickerAction("apply");c.submitLobbyDraft();QCOMPARE(c.lobby_.state()["vote"].toObject()["choices"].toArray().size(),2);
        c.openLobbyPicker();QCOMPARE(c.lobbyChoices_.size(),2);QCOMPARE(c.lobbyPickerPins().size(),1);
        c.lobbyPickerAction("clear");c.lobbyPickerAction("cancel");c.openLobbyPicker();QCOMPARE(c.lobbyChoices_.size(),2);
    }
    void pickerModeRefreshNeverChangesTheFocusedModeSilently() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","1"},{"name","Host"}});
        const QVariantMap a{{"key","a"},{"id","native-a"},{"title","Game"},{"platform","nes"},{"mode","netplay"},{"seats",2}};
        const QVariantMap b{{"key","a:remote"},{"adventure","a"},{"id","remote-a"},{"title","Game"},{"platform","nes"},{"mode","remoteplay.v1"},{"seats",2}};
        c.setLobbyGames({a,b});c.createAnotherLobby();c.openLobbyPicker();c.lobbyPickerAction("game:a");
        c.dispatch(Action::Down);QCOMPARE(c.lobbyPicker()["modeFocus"].toInt(),1);
        c.setLobbyGames({b,a});QCOMPARE(c.lobbyPicker()["modeFocus"].toInt(),0);
        c.setLobbyGames({a});QVERIFY(!c.lobbyPicker()["modeOpen"].toBool());
        QVERIFY(!c.lobbyPicker()["error"].toString().isEmpty());QVERIFY(c.lobbyChoices_.isEmpty());
        QCOMPARE(c.lobbyPicker()["gameKey"].toString(),"a");
    }
    void pickerMetadataRefreshIsAllowedButChangedRuntimeNeedsReselection() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","1"},{"name","Host"}});
        QVariantMap g{{"key","a"},{"id","native-a"},{"title","Game"},{"platform","nes"},{"mode","netplay"},{"seats",2},{"runtime",QVariantMap{{"build","one"}}}};
        c.setLobbyGames({g});c.createAnotherLobby();c.openLobbyPicker();c.lobbyPickerAction("game:a");
        QVERIFY(c.lobbyPicker()["modeOpen"].toBool());c.lobbyPickerAction("mode:a");
        g["title"]="New title";g["cover"]="new-cover";c.setLobbyGames({g});QVERIFY(c.validateLobbyChoices());
        QCOMPARE(c.lobbyChoices_[0].toMap()["title"].toString(),"New title");
        g["runtime"]=QVariantMap{{"build","two"}};c.setLobbyGames({g});QVERIFY(!c.validateLobbyChoices());
        c.lobbyPickerAction("apply");QVERIFY(c.lobbyPickerOpen());QVERIFY(c.lobby_.state().isEmpty());
    }
    void lobbyConversationCreatesOnceAndAddsMembersWithoutTouchingVoice() {
        FluxerSession s;int creates=0,adds=0;const auto room=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QJsonObject group{{"id",channel},{"type",3},{"recipients",QJsonArray{QJsonObject{{"id",remote}}}}};
        s.setTransport([&](auto method,auto path,auto,Completion done,QByteArray){
            if(path.endsWith("/messages")){done({200,QJsonDocument(QJsonObject{{"id","555"}})});return;}
            if(path.endsWith("/pins/555")){done({204,{}});return;}
            if(method=="POST") {QCOMPARE(path,QString("/v1/users/@me/channels"));++creates;done({200,QJsonDocument(group)});}
            else if(method=="GET")done({200,QJsonDocument(group)});
            else {QCOMPARE(method,QByteArray("PUT"));QVERIFY(path.endsWith("/recipients/3"));++adds;done({204,{}});}
        });bind(s);s.relationships_[remote]={{"type",1}};s.relationships_["3"]={{"type",1}};s.voiceChannel_="existing-call";s.voiceMuted_=false;
        QSignalSpy result(&s,&FluxerSession::lobbyChatFinished);
        QVariantMap args{{"lobby",room},{"host",true},{"peers",QStringList{remote}}};
        s.command("lobby-chat",args);QCOMPARE(creates,1);QCOMPARE(result.last()[2].toString(),QString(channel));
        args["peers"]=QStringList{remote,"3"};s.command("lobby-chat",args);QCOMPARE(creates,1);QCOMPARE(adds,1);
        QCOMPARE(s.voiceChannel_,"existing-call");QVERIFY(!s.voiceMuted_);
    }
    void uncertainLobbyConversationDoesNotDuplicateAndGuestAccessIsChecked() {
        FluxerSession s;int posts=0;int status=0;
        s.setTransport([&](auto method,auto,auto,Completion done,QByteArray){if(method=="POST")++posts;done({status,{}});});bind(s);s.relationships_[remote]={{"type",1}};s.relationships_["3"]={{"type",1}};
        QSignalSpy result(&s,&FluxerSession::lobbyChatFinished);
        const auto room=QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVariantMap args{{"lobby",room},{"host",true},{"peers",QStringList{remote}}};
        s.command("lobby-chat",args);s.command("lobby-chat",args);QCOMPARE(posts,1);QVERIFY(!result.last()[3].toString().isEmpty());
        status=403;args["host"]=false;args["channel"]="555";args["peers"]=QStringList{};
        QSignalSpy lost(&s,&FluxerSession::partyFailed);
        s.command("lobby-chat",args);QVERIFY(result.last()[2].toString().isEmpty());QVERIFY(!s.channels_.contains("555"));QVERIFY(lost.isEmpty());
        s.lobbyChatChannels_[room]="555";s.command("lobby-chat",args);QCOMPARE(lost.size(),1);
    }
    void lateLobbyConversationCannotCrossOwnerOrReplaceNewRoom() {
        FluxerSession s;Completion late;s.setTransport([&](auto,auto,auto,Completion done,QByteArray){late=done;});bind(s);s.relationships_[remote]={{"type",1}};s.relationships_["3"]={{"type",1}};
        QSignalSpy result(&s,&FluxerSession::lobbyChatFinished);
        s.command("lobby-chat",{{"lobby",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"host",true},{"peers",QStringList{remote}}});
        s.setOwner("other",2);late({200,QJsonDocument(QJsonObject{{"id",channel},{"type",3}})});
        QVERIFY(result.isEmpty());QVERIFY(s.lobbyChatChannels_.isEmpty());QVERIFY(s.lobbyChatBusy_.isEmpty());
        SocialController c;c.receive(0,{{"state","connected"},{"userId","1"},{"name","Host"}});c.lobby_.create();
        const auto old=c.lobby_.state()["id"].toString();c.lobby_.leave();c.lobby_.create();
        emit c.session_->lobbyChatFinished(0,old,channel,{});QCoreApplication::processEvents();QVERIFY(c.lobbyChatChannel_.isEmpty());
    }
    void lobbyChatReusesDraftAndExplicitVoiceActions() {
        SocialController c;QVariantMap snapshot{{"state","connected"},{"userId","1"},{"name","Host"},{"channel",channel},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"kind","groups"},{"name","Our people"}}}},{"voice",QVariantMap{{"available",true}}}};
        c.receive(0,snapshot);c.compose();c.preserveText("same draft");c.lobby_.create();c.lobby_.setConversation(channel);
        c.lobbyChatChannel_=channel;c.openLobby();QVERIFY(c.lobbyChatVisible());QCOMPARE(c.draft(),"same draft");
        QSignalSpy commands(&c,&SocialController::commandRequested);c.focusLobbyChat();c.dispatch(Action::ToggleContinue);
        QCOMPARE(commands.last().first().toString(),"voice-join");commands.clear();
        c.lobbyAction("leave");for(const auto& command:commands)QVERIFY(!command.first().toString().startsWith("voice-"));
        QVERIFY(c.lobbyChatVisible());c.lobby_.activeRoom()->forgetMembership();QVERIFY(!c.lobbyChatVisible());QCOMPARE(c.draft(),"same draft");
    }
    void lobbyCatalogueRefreshKeepsTheSelectedIdentity() {
        SocialController c;const auto expiry=QDateTime::currentSecsSinceEpoch()+100;
        const QVariantMap first{{"lobby","first"},{"expires",expiry},{"name","One"}},second{{"lobby","second"},{"expires",expiry},{"name","Two"}};
        QVariantMap snapshot{{"state","connected"},{"userId","1"},{"name","Host"},{"lobbyDirectory",QVariantMap{{"rows",QVariantList{first,second}}}}};
        c.receive(0,snapshot);c.selectLobby(2);
        snapshot["lobbyDirectory"]=QVariantMap{{"rows",QVariantList{second,first}}};c.receive(0,snapshot);
        QCOMPARE(c.lobbyRows()[c.lobbyFocus()].toMap()["lobby"].toString(),"second");
        snapshot["lobbyDirectory"]=QVariantMap{{"rows",QVariantList{first}}};c.receive(0,snapshot);
        QCOMPARE(c.lobbyFocus(),0);QCOMPARE(c.lobbyActions().first().toMap()["id"].toString(),"create");
    }
    void lobbyInvitationUsesSharedInboxAndDoesNotLaunchOnOpen() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","2"},{"name","Guest"}});
        LobbyCoordinator host;host.configure("1","Host",true);QVERIFY(host.create());
        connect(&host,&LobbyCoordinator::outgoing,&c,[&](QString,QJsonObject p){c.lobby_.receive("1","Host",p);});
        connect(&c.lobby_,&LobbyCoordinator::outgoing,&host,[&](QString,QJsonObject p){host.receive("2","Guest",p);});
        QSignalSpy launch(&c.lobby_,&LobbyCoordinator::launchRequested);
        QVERIFY(host.invite("2"));QCOMPARE(c.notifications().size(),1);QCOMPARE(c.notificationFaceAt(0),"lobby");
        c.openNotificationAt(0);QCOMPARE(c.face_,"lobby");QVERIFY(c.lobby_.state().isEmpty());QCOMPARE(launch.size(),0);
        c.lobbyAction("accept");QCOMPARE(c.lobbyState()["members"].toList().size(),2);QVERIFY(c.notifications().isEmpty());
        c.selectLobby(0);c.lobbyAction("leave");QVERIFY(c.lobbyState().isEmpty());QCOMPARE(launch.size(),0);
    }
    void lobbyVisibilityCanWithdrawAndRepublishSameIdentity() {
        SocialController c;c.receive(0,{{"state","connected"},{"userId","1"},{"name","Host"}});c.setFace("lobby");
        QSignalSpy commands(&c,&SocialController::commandRequested);
        c.lobbyAction("create");c.submitLobbyDraft();const auto id=c.lobbyState()["id"].toString();QVERIFY(!id.isEmpty());
        c.publishLobby();
        auto publications=[&]{QVariantList rows;for(const auto& command:commands)if(command[0]=="lobby-directory-publish")rows.append(command[1]);return rows;};
        QVERIFY(publications().isEmpty());c.lobbyAction("access");c.publishLobby();
        QVERIFY(publications().isEmpty());QCOMPARE(c.lobbyState()["access"].toString(),"private");
        QVERIFY(c.menuDetail().contains("shared conversation"));c.selectMenu(0);c.publishLobby();
        QVERIFY(!publications().isEmpty());QCOMPARE(publications().last().toMap()["op"].toString(),"upsert");
        c.lobbyAction("access");c.selectMenu(0);c.lobbyAction("access");c.publishLobby();QCOMPARE(publications().last().toMap()["op"].toString(),"withdraw");
        c.lobbyAction("access");c.selectMenu(0);c.publishLobby();QCOMPARE(publications().last().toMap()["room"].toString(),id);
        QCOMPARE(publications().last().toMap()["op"].toString(),"upsert");
        c.lobbyChatPending_.clear();c.lobbyAction("leave");c.publishLobby();QCOMPARE(publications().last().toMap()["op"].toString(),"withdraw");
    }
    void lobbyDirectoryAggregatesChannelsAndRequiresBotAuthorship() {
        FluxerSession s;int requests=0;
        const auto lobby=QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QJsonObject entry{{"v",1},{"lobby",lobby},{"owner",remote},{"name","Friend"},{"revision",1},
            {"expires",QDateTime::currentSecsSinceEpoch()+180},{"status","choosing"},{"access","public"},{"seats",0},{"available",0}};
        s.setTransport([&](QByteArray method,QString path,QJsonObject,Completion done,QByteArray) {
            if(!path.contains("/messages?limit=100")) {done({200,QJsonDocument(QJsonObject{})});return;}
            QCOMPARE(method,QByteArray("GET"));++requests;
            const auto author=requests==1?lobbyDirectory::bot:QString(remote);
            done({200,QJsonDocument(QJsonArray{QJsonObject{{"id",QString::number(requests)},
                {"author",QJsonObject{{"id",author}}},{"content",lobbyDirectory::encode(entry)}}})});
        });
        bind(s);s.command("lobby-directory-refresh");
        QCOMPARE(requests,lobbyDirectory::channels.size());const auto state=s.lobbyDirectoryState();
        QVERIFY(!state["busy"].toBool());QCOMPARE(state["rows"].toList().size(),1);
        QCOMPARE(state["rows"].toList().first().toMap()["lobby"].toString(),lobby);
    }
    void lobbyDirectoryAutomaticallyJoinsCommunityOnce() {
        FluxerSession s;int posts=0;
        s.setTransport([&](QByteArray method,QString path,QJsonObject,Completion done,QByteArray) {
            if(method=="POST")++posts;
            if(path.contains("/messages?"))done({posts?200:403,QJsonDocument(QJsonArray{})});
            else done({200,QJsonDocument(QJsonObject{})});
        });
        bind(s);s.command("lobby-directory-refresh");
        QVERIFY(!s.lobbyDirectoryState()["needsJoin"].toBool());QCOMPARE(posts,1);
        s.command("lobby-directory-ensure");s.command("lobby-directory-refresh");QCOMPARE(posts,1);
    }
    void lobbyCommunityMembershipStartsAfterSignInWithoutOpeningLobby() {
        FluxerSession s;int joins=0;
        s.setTransport([&](QByteArray method,QString path,QJsonObject,Completion done,QByteArray) {
            if(path=="/v1/users/@me")done({200,QJsonDocument(QJsonObject{{"id","1501314428688998181"},{"username","Host"}})});
            else if(method=="POST"&&path.startsWith("/v1/invites/")){++joins;done({200,QJsonDocument(QJsonObject{})});}
            else done({200,QJsonDocument(QJsonArray{})});
        });
        bind(s);s.authenticated();QCOMPARE(joins,1);QVERIFY(s.lobbyMembershipReady_);
        s.authenticated();QCOMPARE(joins,1);
    }
    void lobbyCommunityExistingMembershipAndRetryIsolation() {
        FluxerSession s;int posts=0;
        s.setTransport([&](QByteArray method,QString,QJsonObject,Completion done,QByteArray) {
            if(method=="POST"){++posts;done({429,QJsonDocument(QJsonObject{}),120});}
            else done({200,QJsonDocument(QJsonArray{})});
        });
        bind(s);s.guilds_[lobbyDirectory::guild]={{"id",lobbyDirectory::guild}};
        s.command("lobby-directory-ensure");QCOMPARE(posts,0);QVERIFY(s.lobbyMembershipReady_);
        s.lobbyMembershipReady_=false;s.guilds_.clear();s.command("lobby-directory-ensure");
        QCOMPARE(posts,1);QVERIFY(s.lobbyMembershipRetry_.isActive());QCOMPARE(s.lobbyMembershipRetry_.interval(),120000);
        s.command("lobby-directory-ensure");QCOMPARE(posts,1);
        s.setOwner("trainer-b",2);QVERIFY(!s.lobbyMembershipRetry_.isActive());QVERIFY(!s.lobbyMembershipReady_);
    }
    void lobbyCommunityJoinFailureDoesNotLoopOrCrossAccount() {
        FluxerSession s;Completion pending;int posts=0;
        s.setTransport([&](QByteArray method,QString,QJsonObject,Completion done,QByteArray) {
            if(method=="POST"){++posts;pending=done;}else done({403,QJsonDocument(QJsonObject{})});
        });
        bind(s);s.command("lobby-directory-ensure");QCOMPARE(posts,1);
        pending({403,QJsonDocument(QJsonObject{})});QCOMPARE(posts,1);QVERIFY(!s.lobbyMembershipRetry_.isActive());
        s.command("lobby-directory-ensure");QCOMPARE(posts,2);s.setOwner("trainer-b",2);
        pending({200,QJsonDocument(QJsonObject{})});QVERIFY(!s.lobbyMembershipReady_);QVERIFY(!s.lobbyDirectoryBusy_);
    }
    void lobbyPublicationCategoryIsChosenByHostWithoutAGame() {
        SocialController c;QObject::disconnect(&c,nullptr,c.session_,nullptr);
        c.snapshot_={{"state","connected"},{"userId","1"},{"name","Host"}};
        c.lobby_.configure("1","Host",true);QVERIFY(c.lobby_.create("public"));
        QSignalSpy commands(&c,&SocialController::commandRequested);
        for(const auto& topic:QStringList{"party-games","competitive"}) {
            c.lobbyAction("publish-topic:"+topic);c.publishLobby();
            QCOMPARE(c.lobby_.publicationTopic(),topic);
            QCOMPARE(commands.last()[1].toMap()["topic"].toString(),topic);
            QCOMPARE(commands.last()[1].toMap()["status"].toString(),"choosing");
        }
        c.lobby_.selectGame({"game","Game","test","runtime.test",2,{{"content","private"}}});c.publishLobby();
        QCOMPARE(commands.last()[1].toMap()["topic"].toString(),"competitive");
        c.lobbyAction("publish-topic:unknown");QCOMPARE(c.lobby_.publicationTopic(),"competitive");
        c.lobbyAction("publish-topic:choosing");QCOMPARE(c.lobby_.publicationTopic(),"competitive");
        QVERIFY(!lobbyDirectory::publicationTopics().contains("choosing"));
        QCOMPARE(lobbyDirectory::publicationTopics().last(),"other");
    }
    void lobbyDirectoryLateReplyCannotCrossTrainerChange() {
        FluxerSession s;Completion pending;
        s.setTransport([&](QByteArray,QString path,QJsonObject,Completion done,QByteArray) {
            if(path.contains("/messages?"))pending=done;else done({200,QJsonDocument(QJsonObject{})});
        });
        bind(s);s.command("lobby-directory-refresh");QVERIFY(bool(pending));
        s.setOwner("trainer-b",2);pending({200,QJsonDocument(QJsonArray{})});
        QVERIFY(!s.lobbyDirectoryState()["busy"].toBool());QVERIFY(s.lobbyDirectoryState()["rows"].toList().isEmpty());
    }
    void runtimeTransportWorksWithoutAnyGameAdapter() {
        SocialController controller;QSignalSpy started(&controller,&SocialController::runtimeEstablished);
        QSignalSpy frames(&controller,&SocialController::runtimeFrame),ended(&controller,&SocialController::runtimeEnded);
        emit controller.session_->onlineEstablished(controller.generation_,"self","peer","Peer","runtime.fixture",true);
        QCOMPARE(started.size(),1);
        emit controller.session_->onlineFrame(controller.generation_,{{"fixture",true}});QCOMPARE(frames.size(),1);
        emit controller.session_->onlineEnded(controller.generation_);QCOMPARE(ended.size(),1);
        emit controller.session_->onlineEstablished(controller.generation_+1,"self","peer","Peer","runtime.stale",true);
        QCOMPARE(started.size(),1);
    }
    void nativeActivityProviderCanBeReplacedWithoutDuplicateRouting() {
        struct Provider final:NativeActivityProvider {
            int started=0,received=0,ended=0;
            QJsonArray onlineCapabilities()const override{return {};}
            bool beginOnline(QString,QString,QString,QString,bool)override{++started;return true;}
            void receiveOnline(QJsonObject)override{++received;}
            void showOnline()override{}
            void endOnline()override{++ended;}
            bool visibleNearby()const override{return false;}
            void setVisibleNearby(bool)override{}
        } first;
        SocialController controller;controller.onlineWritable_=true;
        controller.setLink(&first);controller.setLink(&first);
        emit controller.session_->onlineEstablished(controller.generation_,"self","peer","Peer","fixture.native",true);
        QCOMPARE(first.started,1);
        auto second=std::make_unique<Provider>();controller.setLink(second.get());QCOMPARE(first.ended,1);
        emit controller.session_->onlineFrame(controller.generation_,{});QCOMPARE(first.received,0);QCOMPARE(second->received,1);
        second.reset();QVERIFY(controller.link_.isNull());
        emit controller.session_->onlineEnded(controller.generation_);
    }
    void overflowTargetsTheChosenGroupAndNotTheOpenConversation() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        const QString other="1501314428688998189";
        c.snapshot_={{"state","connected"},{"userId","self"},{"channel",channel},{"chats",QVariantList{
            QVariantMap{{"id",channel},{"kind","chats"},{"name","Open chat"}},
            QVariantMap{{"id",other},{"kind","groups"},{"name","Other group"},{"owner","self"},{"members",QVariantList{QVariantMap{{"id",remote},{"username","Friend"}}}}}}}};
        c.face_="communities";c.contextRow(0);QCOMPARE(c.menuTitle(),QString("Other group"));
        QVERIFY(!c.menuCommands_.contains("runtime:together-company:"+other));c.closeMenu();c.groupDetails(0);
        c.selectMenu(c.menuCommands_.indexOf("rename-group"));QSignalSpy text(&c,&SocialController::textRequested);
        c.applyText("Renamed");QCOMPARE(commands.last()[1].toMap()["channel"].toString(),other);
        c.closeMenu();c.groupDetails(0);c.selectMenu(c.menuCommands_.indexOf("remove-member"));
        QCOMPARE(c.menuChannel_,other);c.selectMenu(0);c.selectMenu(1);
        QCOMPARE(commands.last()[0].toString(),QString("remove-member"));QCOMPARE(commands.last()[1].toMap()["channel"].toString(),other);
        QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString(remote));
        c.closeMenu();c.groupDetails(0);c.selectMenu(c.menuCommands_.indexOf("members"));c.selectMenu(1);
        QVERIFY(c.profileVisible());QCOMPARE(commands.last()[0].toString(),QString("profile-read"));QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString(remote));
        QVERIFY(!c.person().contains("bio"));
    }
    void profileReadsRejectStaleRepliesAndRespectPrivacy() {
        FluxerSession s;QList<Completion> replies;QStringList paths;
        s.setTransport([&](auto method,auto path,auto,Completion done,auto){QCOMPARE(method,QByteArray("GET"));paths<<path;replies<<done;});bind(s);
        s.command("profile-read",{{"id",remote},{"request","first"}});
        s.command("profile-read",{{"id",s.self_},{"request","second"}});
        QVERIFY(paths.first().endsWith("/profile?with_mutual_friends=true&with_mutual_guilds=true"));
        replies[0]({200,QJsonDocument(QJsonObject{{"user",QJsonObject{{"id",remote},{"username","Stale"}}}})});
        QCOMPARE(s.person_["id"].toString(),s.self_);QVERIFY(!s.person_.contains("name"));
        replies[1]({200,QJsonDocument(QJsonObject{{"user",QJsonObject{{"id",s.self_},{"username","Me"},{"email","private@example.test"}}},{"profile_limited",true},{"user_profile",QJsonObject{{"bio","Hidden"},{"pronouns","Hidden"}}}})});
        QVERIFY(s.person_["loaded"].toBool());QVERIFY(s.person_["bio"].toString().isEmpty());QVERIFY(!s.person_.contains("email"));
        s.command("profile-read",{{"id",s.self_},{"request","third"}});QVERIFY(!s.person_.contains("name"));
        replies[2]({403,{}});QVERIFY(!s.person_["loaded"].toBool());QVERIFY(!s.person_.contains("bio"));
        s.command("profile-read",{{"id",remote},{"request","fourth"}});s.reset();replies[3]({200,QJsonDocument(QJsonObject{{"user",QJsonObject{{"id",remote}}}})});QVERIFY(s.person_.isEmpty());
    }
    void profileCardAndEmojiKeepFreshIdentityAndDraftUntilExplicitSend() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.snapshot_={{"state","connected"},{"channel",channel},{"userId","self"}};
        c.profile(remote);const auto oldRequest=c.profileRequest_;
        c.snapshot_["person"]=QVariantMap{{"id",remote},{"request",oldRequest},{"bio","Old bio"}};
        QCOMPARE(c.person()["bio"].toString(),QString("Old bio"));
        c.closeMenu();c.profile(remote);QVERIFY(!c.person().contains("bio"));
        c.closeMenu();c.drafts_[channel]="Draft ";commands.clear();c.emoji();c.selectMenu(0);
        QVERIFY(c.draft().startsWith("Draft "));QVERIFY(c.draft().size()>6);QVERIFY(commands.isEmpty());
        c.sendDraft();QCOMPARE(commands.count(),1);QCOMPARE(commands[0][0].toString(),QString("send"));
        QCOMPARE(commands[0][1].toMap()["channel"].toString(),QString(channel));
        c.closeMenu();c.setFace("communities");c.collectionOptions();QVERIFY(c.menuCommands_.contains("create-group"));QVERIFY(!c.menuCommands_.contains("profile:self"));
        c.closeMenu();c.snapshot_["chats"]=QVariantList{QVariantMap{{"id",channel},{"kind","chats"}}};
        c.composerTools();
        QVERIFY(c.menuCommands_.contains("picture"));QVERIFY(c.menuCommands_.contains("record"));
        c.selectMenu(c.menuCommands_.indexOf("emoji-picker"));QVERIFY(c.emojiVisible());
        c.dispatch(Action::Right);QCOMPARE(c.menuIndex(),1);c.dispatch(Action::Down);QCOMPARE(c.menuIndex(),5);c.dispatch(Action::Up);QCOMPARE(c.menuIndex(),0);
    }
    void profileFriendRequestUsesPostAndProviderDenialStaysAnError() {
        FluxerSession s;QByteArray method;QString path;QJsonObject body;Completion pending;
        s.setTransport([&](auto m,auto p,auto b,Completion done,auto){method=m;path=p;body=b;pending=done;});bind(s);
        s.person_={{"id",remote},{"loaded",true}};
        s.command("add-id",{{"id",remote}});QCOMPARE(method,QByteArray("POST"));
        QCOMPARE(path,QString("/v1/users/@me/relationships/")+remote);QVERIFY(body.isEmpty());
        pending({403,{}});QVERIFY(s.status_!="Connected");QVERIFY(!s.relationships_.contains(remote));
        s.command("block",{{"id",remote}});QCOMPARE(method,QByteArray("PUT"));QCOMPARE(body["type"].toInt(),2);
        s.command("dm",{{"id",remote}});QCOMPARE(path,QString("/v1/users/@me/channels"));QCOMPARE(body["recipient_id"].toString(),QString(remote));
        SocialController c;c.face_="communities";c.snapshot_={{"userId","self"}};QSignalSpy route(&c,&SocialController::conversationsRequested);QSignalSpy commands(&c,&SocialController::commandRequested);
        c.profile(remote);c.selectMenu(c.menuCommands_.indexOf("dm"));QCOMPARE(route.count(),1);QCOMPARE(c.face_,QString("chats"));QCOMPARE(commands.last()[0].toString(),QString("dm"));
    }
    void touchSendAndCallUseCapturedConversationWithoutSwitchingAnExistingCall() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.snapshot_={{"state","connected"},{"channel",channel},{"voice",QVariantMap{{"available",true}}},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"kind","chats"}}}}};
        c.drafts_[channel]="Touch draft";c.sendDraft();QCOMPARE(commands.count(),1);
        QCOMPARE(commands.last()[0].toString(),QString("send"));QCOMPARE(commands.last()[1].toMap()["channel"].toString(),QString(channel));
        c.sendDraft();QCOMPARE(commands.count(),1);
        c.call();QCOMPARE(commands.last()[0].toString(),QString("voice-join"));
        commands.clear();c.snapshot_["voice"]=QVariantMap{{"channel","other-call"},{"available",true},{"muted",true}};
        c.call();QVERIFY(commands.isEmpty());QCOMPARE(c.menuTitle(),QString("Switch call?"));QCOMPARE(c.menu().size(),2);QVERIFY(!c.menuCommands_.contains("open-lobby"));
        c.closeMenu();c.snapshot_["messages"]=QVariantList{QVariantMap{{"id","older"}},QVariantMap{{"id","newer"}}};
        c.retainMessagePosition(0,false);QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString("older"));QVERIFY(c.reading());
    }
    void companyTransportAdmitsMembersNotStrangersAndNeverBroadcastsEndpoint() {
        FluxerSession s;bind(s);const QString group="1501314428688998189";
        const QJsonArray members{QJsonObject{{"id",remote}}};
        s.channels_[group]={{"id",group},{"type",3},{"recipients",members}};
        s.channels_[channel]["recipients"]=members;
        QJsonObject p{{"ns","org.traineros.party"},{"v",1},{"kind","company-offer"},{"company",group},{"target",""}};
        QCOMPARE(s.partyRoute(group,remote,p),"company:"+group+":"+remote);
        QVERIFY(s.partyRoute(channel,remote,p).isEmpty());
        p["endpoint"]=QJsonObject{{"password","secret"}};QVERIFY(s.partyRoute(group,remote,p).isEmpty());p.remove("endpoint");
        p["kind"]="launch";QVERIFY(s.partyRoute(group,remote,p).isEmpty());
        QCOMPARE(s.partyRoute(channel,remote,p),"company:"+group+":"+remote); // not friends: membership suffices
        s.relationships_[remote]={{"type",2}};QVERIFY(s.partyRoute(channel,remote,p).isEmpty());s.relationships_.clear();
        s.setTransport([](auto,auto,auto,Completion,auto){});
        s.gatewayEvent({{"op",0},{"t","CHANNEL_RECIPIENT_REMOVE"},{"d",QJsonObject{{"channel_id",group},{"user",QJsonObject{{"id",remote}}}}}});
        QVERIFY(s.partyRoute(channel,remote,p).isEmpty()); // immediate, before refresh completes
    }
    void companyAdmissionUsesPrivateMessagesAndRechecksQueuedMembership() {
        FluxerSession s;bind(s);const QString group="1501314428688998189";
        s.channels_[group]={{"id",group},{"type",3},{"recipients",QJsonArray{QJsonObject{{"id",remote}}}}};
        s.channels_[channel]["recipients"]=QJsonArray{QJsonObject{{"id",remote}}};
        QStringList paths;QList<Completion> replies;
        s.setTransport([&](auto,auto path,auto,Completion done,auto){paths.append(path);replies.append(done);});
        QJsonObject p{{"ns","org.traineros.party"},{"v",1},{"kind","launch"},{"company",group},{"expires",QDateTime::currentSecsSinceEpoch()+60}};
        s.sendParty("company:"+group+":"+remote,GameParty::encode(p));
        QCOMPARE(paths,QStringList{"/v1/channels/"+QString(channel)+"/messages"});
        s.sendParty("company:"+group+":"+remote,GameParty::encode(p));
        s.channels_.remove(group);replies.takeFirst()({200,{}});s.onlineSendTimer_.stop();s.sendOnline();
        QCOMPARE(paths.size(),1);QVERIFY(s.onlineQueue_.isEmpty());
    }
    void companyPartySelectionIsInlineAndPreferencesAreOwnerScoped() {
        SocialController c;c.owner_="company-"+QUuid::createUuid().toString();c.face_="groups";c.runtimeAvailable_=true;
        c.snapshot_={{"state","connected"},{"userId","self"},{"channel",channel},{"chats",QVariantList{QVariantMap{{"id",channel},{"kind","groups"},{"name","Our group"},{"members",QVariantList{QVariantMap{{"id",remote},{"username","Friend"}}}}}}}};
        c.setCompanyParties({QVariantMap{{"company",channel},{"peer","company:group:a"},{"joinable",true}},QVariantMap{{"company",channel},{"peer","company:group:b"},{"joinable",true}}});
        QSignalSpy join(&c,&SocialController::partyJoin);c.together();
        QCOMPARE(c.menuTitle(),QString("Together"));
        c.selectMenu(c.menuCommands_.indexOf("company-party:company:group:b"));
        QCOMPARE(join.count(),1);QCOMPARE(join[0][0].toString(),QString("company:group:b"));QVERIFY(c.menu().isEmpty());
        c.dispatch(Action::Right);QVERIFY(c.reading());
        c.messageFocus_=0;c.snapshot_["historyMore"]=true;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.dispatch(Action::Up);QCOMPARE(commands.last()[0].toString(),QString("older"));QVERIFY(!c.partyFocused());
        c.setCompanyParties({QVariantMap{{"company",channel},{"peer","company:group:a"},{"party","joined-party"},{"joinable",true}}});
        c.setGameParty({{"party","joined-party"}});QVERIFY(c.companyParties().isEmpty());QVERIFY(!c.partyFocused());
        c.setGameParty({});
        c.editCompanyAccess();c.selectMenu(1);c.selectMenu(3);
        QCOMPARE(c.companyAccess(channel)["policy"].toString(),QString("selected"));QVERIFY(c.companyAccess(channel)["allowed"].toStringList().contains(remote));
        const auto key=c.companySettingsKey(channel);c.owner_+="other";
        QCOMPARE(c.companyAccess(channel)["policy"].toString(),QString("request"));QVERIFY(c.companyAccess(channel)["allowed"].toStringList().isEmpty());
        QSettings().remove(key);
    }
    void gameCoordinationDoesNotDisplaceConversationHistory() {
        FluxerSession s;bind(s);QStringList paths;QList<Completion> replies;
        s.setTransport([&](auto,auto path,auto,Completion done,auto){paths.append(path);replies.append(done);});
        const auto content=GameParty::encode({{"ns","org.traineros.party"},{"v",1}});
        QJsonArray hidden;for(int i=149;i>=100;--i)hidden.append(QJsonObject{{"id",QString::number(i)},{"channel_id",channel},{"content",content}});
        s.loadMessages(channel);replies.takeFirst()({200,QJsonDocument(hidden)});
        QVERIFY(paths.last().endsWith("&before=100"));
        replies.takeFirst()({200,QJsonDocument(QJsonArray{QJsonObject{{"id","99"},{"channel_id",channel},{"content","Hello friend"}}})});
        QVERIFY(s.messages_.contains("99"));QVERIFY(!s.historyPast_);
        for(int i=200;i<350;++i)s.gatewayEvent({{"op",0},{"t","MESSAGE_CREATE"},{"d",QJsonObject{{"id",QString::number(i)},{"channel_id",channel},{"content",content}}}});
        QVERIFY(s.messages_.contains("99"));QVERIFY(!s.messages_.contains("349"));
    }
    void unifiedInboxRestoresLegacyGroupWithoutLosingDraftOrCall() {
        FluxerSession s;bind(s);s.channelsLoaded_=false;
        s.setTransport([](auto,auto,auto,Completion done,auto){done({200,QJsonDocument(QJsonArray{})});});
        s.channels_[remote]={{"id",remote},{"type",3},{"last_message_id","200"}};
        s.ordinaryGroups_.insert(remote); // Already classified ordinary group, not a lobby channel.
        s.preferred_["groups"]=remote;s.channel_.clear();s.voiceChannel_=channel;s.voiceMuted_=true;
        s.command("face",{{"face","groups"}});QVERIFY(s.channel_.isEmpty());
        s.channelsLoaded_=true;s.ensureConversation();QCOMPARE(s.channel_,QString(remote));QCOMPARE(s.face_,QString("communities"));
        QCOMPARE(s.voiceChannel_,QString(channel));QVERIFY(s.voiceMuted_);
        s.command("face",{{"face","friends"}});s.command("face",{{"face","communities"}});QCOMPARE(s.channel_,QString(remote));
    }
    void togetherNeverSendsDraftAndHistoryDoesNotChangeButtonMeaning() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested),text(&c,&SocialController::textRequested);
        c.snapshot_={{"state","connected"},{"channel",channel},{"historyPast",true}};
        c.drafts_[channel]="Keep my draft";c.reading_=true;
        c.together();QCOMPARE(c.menuTitle(),QString("Together"));QVERIFY(commands.isEmpty());QCOMPARE(c.draft(),QString("Keep my draft"));
        c.dispatch(Action::Back);c.dispatch(Action::Secondary);QCOMPARE(text.count(),1);QVERIFY(commands.isEmpty());
        c.latest();QCOMPARE(commands.last()[0].toString(),QString("latest"));QVERIFY(c.reading());
    }
    void partySurfaceRetainsActionIdentityWhenRosterChanges() {
        SocialController c;QSignalSpy actions(&c,&SocialController::runtimeAction);
        c.snapshot_["state"]="connected";
        c.setRuntimeSurface("Party","Game",{QVariantMap{{"id","multiplayer-start"},{"label","Start"}},QVariantMap{{"id","multiplayer-leave"},{"label","Leave"}}});
        c.setRuntimeSurface("Party","Game",{QVariantMap{{"id","multiplayer-leave"},{"label","Leave"}}});
        c.dispatch(Action::Confirm);QVERIFY(actions.isEmpty());
        c.dispatch(Action::Up);c.dispatch(Action::Confirm);QCOMPARE(actions.last()[0].toString(),QString("multiplayer-leave"));
        c.setRuntimeSurface("People","Choose",{QVariantMap{{"id","person"},{"label","Friend"}}});
        QCOMPARE(c.menuIndex(),0);QCOMPARE(c.menu().size(),1);
    }
    void runtimeMissedInvitationCannotAcceptReplacement() {
        SocialController c;c.snapshot_={{"state","connected"},{"userId","test-self"}};
        QSignalSpy actions(&c,&SocialController::runtimeAction);
        c.setRuntimeInvitation({{"request","first"},{"name","Friend"},{"game",QVariantMap{{"label","Game"}}}});
        QCOMPARE(c.notifications().size(),1);QVERIFY(c.notifications().first().toMap()["pending"].toBool());
        c.setRuntimeInvitation({});c.openNotificationAt(0);QVERIFY(actions.isEmpty());QVERIFY(c.menuDetail().contains("expired"));
        c.closeMenu();c.setRuntimeInvitation({{"request","second"},{"name","Other"}});
        c.openNotificationAt(1);QVERIFY(actions.isEmpty());
        c.closeMenu();c.openNotificationAt(0);QCOMPARE(actions.last()[0].toString(),QString("multiplayer-request:second"));
    }
    void initTestCase() { QStandardPaths::setTestModeEnabled(true);QCoreApplication::setOrganizationName("TrainerOSTests");QCoreApplication::setApplicationName("SocialTests"); }
    void partyPacketsUseFriendDmWithoutChangingSavedLinkOrCalls() {
        FluxerSession s;bind(s);s.setTransport([](auto,auto,auto,Completion done,auto){done({200,{}});});
        s.relationships_[remote]={{"type",1}};s.channels_[channel]["recipients"]=QJsonArray{QJsonObject{{"id",remote}}};
        s.voiceChannel_=channel;s.voiceState_="connected";s.voiceMuted_=true;
        QSignalSpy received(&s,&FluxerSession::partyPacket),notices(&s,&FluxerSession::incomingMessage),snapshots(&s,&FluxerSession::snapshot);
        const auto content=GameParty::encode({{"ns","org.traineros.party"},{"v",1},{"kind","query"}});
        auto event=[&](QString id){s.gatewayEvent({{"op",0},{"t","MESSAGE_CREATE"},{"d",QJsonObject{{"id",id},{"channel_id",channel},{"type",0},
            {"author",QJsonObject{{"id",remote},{"username","Friend"}}},{"content",content}}}});};
        event("100");QCOMPARE(received.count(),1);QCOMPARE(notices.count(),0);QVERIFY(snapshots.last()[1].toMap()["messages"].toList().isEmpty());
        QCOMPARE(s.online_.state()["stage"].toString(),QString("idle"));QCOMPARE(s.voiceChannel_,QString(channel));QVERIFY(s.voiceMuted_);
        s.channels_[channel]["type"]=3;event("101");QCOMPARE(received.count(),1);
        s.channels_[channel]["type"]=1;s.relationships_[remote]["type"]=2;event("102");QCOMPARE(received.count(),1);
    }
    void partyOfferUsesStableTogetherEntryAndExplicitJoin() {
        SocialController c;c.receive(0,{{"state","connected"},{"channel",channel},{"chats",QVariantList{QVariantMap{{"id",channel},{"kind","chats"},{"friend",true},{"members",QVariantList{QVariantMap{{"id",remote}}}}}}}});
        c.setGameActivity(remote,{{"joinable",true},{"free",1},{"game",QVariantMap{{"label","A game"}}}});
        QSignalSpy joined(&c,&SocialController::partyJoin);c.together();
        QVERIFY(joined.isEmpty());QCOMPARE(c.menuTitle(),QString("Together"));c.selectMenu(c.menuCommands_.indexOf("party-join"));
        QCOMPARE(joined.count(),1);QCOMPARE(joined.first()[0].toString(),QString(remote));QVERIFY(c.menu().isEmpty());
        c.setGameActivity(remote,{});c.joinGame();QCOMPARE(joined.count(),1);
    }
    void privateProgressJoinDisclosesCopiesAndPinsTheDisplayedGame() {
        SocialController c;c.receive(0,{{"state","connected"},{"channel",channel},{"chats",QVariantList{QVariantMap{{"id",channel},{"kind","chats"},{"friend",true},{"members",QVariantList{QVariantMap{{"id",remote}}}}}}}});
        const QVariantMap game{{"label","A linked game"},{"content","first"},{"privateProgress",true}};
        QVariantMap offer{{"joinable",true},{"party","party-one"},{"game",game}};c.setGameActivity(remote,offer);
        QSignalSpy joined(&c,&SocialController::partyJoin);c.together();
        QVERIFY(c.menuDetail().contains("Temporary save copies"));QVERIFY(c.fullMenuDetail());
        const int index=c.menuCommands_.indexOf("party-join");QVERIFY(index>=0);QVERIFY(c.menu()[index].contains("save copies"));
        c.selectMenu(index);QCOMPARE(joined.count(),1);QVERIFY(c.menu().isEmpty());
        QCOMPARE(joined.first()[1].toJsonObject()["game"].toObject(),QJsonObject::fromVariantMap(game));
        c.together();auto changed=game;changed["content"]="second";offer["game"]=changed;c.setGameActivity(remote,offer);
        c.selectMenu(c.menuCommands_.indexOf("party-join"));QCOMPARE(joined.count(),1);QCOMPARE(c.menuTitle(),QString("Game changed"));
        c.closeMenu();c.contextRow(-1);QVERIFY(!c.menuCommands_.contains("join-peer:"+QString(remote)));
        offer["party"]="replacement-party";c.setGameActivity(remote,offer);
        c.selectMenu(c.menuCommands_.indexOf("join-peer:"+QString(remote)));QCOMPARE(joined.count(),1);
    }
    void temporaryProgressJoinDisclosesFreshSessionWithoutClaimingSaveReturn() {
        SocialController c;
        const QVariantMap game{{"label","A shared console"},{"content","first"},{"temporaryProgress",true}};
        const QVariantMap offer{{"joinable",true},{"party","temporary-party"},{"game",game}};
        c.setGameActivity(remote,offer);QSignalSpy joined(&c,&SocialController::partyJoin);
        c.directJoin(remote,offer);
        QVERIFY(joined.isEmpty());QVERIFY(c.fullMenuDetail());
        QVERIFY(c.menuDetail().contains("starts fresh"));QVERIFY(c.menuDetail().contains("isn't kept"));
        QVERIFY(!c.menuDetail().contains("save copies"));
        c.selectMenu(c.menuCommands_.indexOf("join-reviewed"));QCOMPARE(joined.count(),1);
    }
    void privateCompanyJoinUsesInlineConsentAndLegacyEntryCannotBypassIt() {
        SocialController c;c.owner_="company-private";c.face_="groups";c.runtimeAvailable_=true;
        c.snapshot_={{"state","connected"},{"userId","self"},{"channel",channel},{"chats",QVariantList{QVariantMap{{"id",channel},{"kind","groups"},{"name","Our group"}}}}};
        const QVariantMap game{{"label","Linked game"},{"content","first"},{"privateProgress",true}};
        c.setCompanyParties({QVariantMap{{"company",channel},{"peer","company:group:a"},{"party","one"},{"joinable",true},{"game",game}}});
        QSignalSpy joined(&c,&SocialController::partyJoin);c.together();
        QVERIFY(c.menuDetail().contains("Only your own progress"));
        c.selectMenu(c.menuCommands_.indexOf("company-party:company:group:a"));QCOMPARE(joined.count(),1);
        c.joinCompanyParty(0);QCOMPARE(joined.count(),1);QCOMPARE(c.menuTitle(),QString("Join game"));
        c.selectMenu(c.menuCommands_.indexOf("join-reviewed"));QCOMPARE(joined.count(),2);
        c.contextRow(-1);QVERIFY(!c.menuCommands_.contains("context-party:company:group:a"));
        ++c.generation_;c.selectMenu(c.menuCommands_.indexOf("context-party:company:group:a"));QCOMPARE(joined.count(),2);
    }
    void runtimeInvitationUsesDmContractAndReusesConversation() {
        FluxerSession s;Completion pending;int requests=0;
        QSignalSpy failed(&s,&FluxerSession::runtimeProbeFailed);
        s.setTransport([&](QByteArray method,QString path,QJsonObject body,Completion done,QByteArray){
            ++requests;QCOMPARE(method,QByteArray("POST"));QCOMPARE(path,QString("/v1/users/@me/channels"));
            QCOMPARE(body,QJsonObject({{"recipient_id",remote}}));pending=done;
        });bind(s);s.relationships_[remote]={{"type",1}};
        s.command("runtime-probe-person",{{"id",remote}});QCOMPARE(requests,1);
        pending({400,{}});QVERIFY(s.online_.state()["status"].toString().contains("Couldn't open"));
        QCOMPARE(failed.count(),1);QCOMPARE(failed.first()[1].toString(),QString(remote));
        s.command("runtime-probe-person",{{"id",remote}});
        pending({200,QJsonDocument(QJsonObject{{"id",channel},{"type",1},{"recipients",QJsonArray{QJsonObject{{"id",remote}}}}})});
        s.command("runtime-probe-person",{{"id",remote}});QCOMPARE(requests,2);
        QCOMPARE(s.channels_[channel]["recipients"].toArray().size(),1);
    }
    void cancelledRuntimeInvitationIgnoresLateDmReply() {
        FluxerSession s;Completion pending;
        s.setTransport([&](auto,auto,auto,Completion done,QByteArray){pending=done;});
        bind(s);s.channels_.clear();s.relationships_[remote]={{"type",1}};
        QSignalSpy failed(&s,&FluxerSession::runtimeProbeFailed);
        s.command("runtime-probe-person",{{"id",remote}});
        s.command("online-close");pending({400,{}});QCOMPARE(failed.count(),0);
        s.command("runtime-probe-person",{{"id",remote}});
        s.command("online-close");
        pending({200,QJsonDocument(QJsonObject{{"id",channel},{"type",1},{"recipients",QJsonArray{QJsonObject{{"id",remote}}}}})});
        QVERIFY(!s.channels_.contains(channel));QCOMPARE(s.online_.state()["stage"].toString(),QString("idle"));
    }
    void profileUpdatesUseProviderResponseAndDiscardOldOwnerReply() {
        FluxerSession s;Completion pending;QJsonObject sent;int requests=0;
        s.setTransport([&](QByteArray method,QString path,QJsonObject body,Completion done,QByteArray){
            ++requests;QCOMPARE(method,QByteArray("PATCH"));QCOMPARE(path,QString("/v1/users/@me"));sent=body;pending=done;
        });bind(s);
        s.updateProfile({{"id",s.self_},{"username","test"},{"discriminator","0042"},{"global_name","Before"},{"email","private@example.invalid"}});
        QVERIFY(!s.profile_.contains("email"));
        s.command("profile-update",{{"field","email"},{"value","no@example.invalid"}});QCOMPARE(requests,0);
        s.command("profile-update",{{"field","global_name"},{"value","New name"}});
        QCOMPARE(sent.size(),1);QCOMPARE(s.name_,QString("Before"));QVERIFY(s.profileBusy_);
        pending({200,QJsonDocument(QJsonObject{{"id",s.self_},{"global_name","New name"},{"username","test"},{"discriminator","0042"}})});
        QCOMPARE(s.name_,QString("New name"));QVERIFY(!s.profileBusy_);
        s.command("profile-update",{{"field","bio"},{"value",""}});QVERIFY(sent["bio"].isNull());
        pending({403,{}});QCOMPARE(s.name_,QString("New name"));QVERIFY(s.profileStatus_.contains("Couldn't"));
        s.command("profile-update",{{"field","global_name"},{"value","Late"}});auto old=pending;
        s.setOwner("trainer-b",2);old({200,QJsonDocument(QJsonObject{{"id","1501314428688998181"},{"global_name","Late"}})});
        QVERIFY(s.profile_.isEmpty());QVERIFY(s.name_.isEmpty());
    }
    void callSettingsDoNotRejoinOrUnmuteAndResetAcrossOwners() {
        FluxerSession s;int requests=0;s.setTransport([&](auto,auto,auto,Completion,QByteArray){++requests;});bind(s);
        s.voiceChannel_=channel;s.voiceState_="connected";s.voiceMuted_=true;
        s.command("audio-settings",{{"input","headset"},{"output","speakers"},{"volume",65}});
        QCOMPARE(s.voiceChannel_,QString(channel));QVERIFY(s.voiceMuted_);QCOMPARE(requests,0);
        QCOMPARE(s.audioConfiguration()["volume"].toInt(),65);
        s.command("audio-settings",{{"volume",105}});QCOMPARE(s.voiceVolume_,100);
        QCOMPARE(s.voiceInput_,QString("headset"));
        s.setOwner("trainer-b",2);QVERIFY(s.voiceInput_.isEmpty());QVERIFY(s.voiceOutput_.isEmpty());QCOMPARE(s.voiceVolume_,100);
    }
    void builtInAvatarPreparationIsBoundedAndDoesNotSend() {
        SocialMedia media;
        QCOMPARE(SocialMedia::profileAvatars().size(),6);
        media.chooseProfileAvatar("../../profile");QCOMPARE(media.state(),QString("error"));
        media.chooseProfileAvatar("fox");
        QTRY_COMPARE(media.state(),QString("picture"));
        QVERIFY(!media.bytes().isEmpty());
        QVERIFY(media.bytes().size()<1024*1024);
        media.clear();QVERIFY(media.bytes().isEmpty());
    }
    void profileAvatarAcceptsShortProviderHashesButNotPaths() {
        FluxerSession s;bind(s);
        s.updateProfile({{"id",s.self_},{"avatar","9038e4f6"}});
        QCOMPARE(s.profile_["avatar"].toString(),"https://fluxerusercontent.com/avatars/"+s.self_+"/9038e4f6.png?size=64");
        s.updateProfile({{"id",s.self_},{"avatar","../other"}});
        QVERIFY(s.profile_["avatar"].toString().isEmpty());
    }
    void profileCardReusesEditorWithoutAudioOrAccountLeak() {
        SocialController social;CommunicationSettings flow;flow.configure(&social);
        QVariantMap state{{"userId","self"},{"state","connected"},{"presence","idle"},{"customStatus","Playing"},{"profile",QVariantMap{{"name","Mika"}}}};
        social.receive(0,state);QSignalSpy commands(&social,&SocialController::commandRequested);
        QSignalSpy text(&flow,&CommunicationSettings::textRequested);QSignalSpy back(&flow,&CommunicationSettings::backRequested);
        flow.beginProfile();QCOMPARE(commands.last()[0].toString(),QString("profile-refresh"));QVERIFY(!flow.refresh_.isActive());
        QCOMPARE(flow.rows().size(),7);QVERIFY(flow.rows()[1].toMap()["selected"].toBool());
        flow.activate(3);QCOMPARE(commands.last()[1].toMap()["value"].toString(),QString("invisible"));
        flow.activate(4);QCOMPARE(text.last()[1].toString(),QString("Playing"));flow.applyText("New status");
        QCOMPARE(commands.last()[1].toMap()["field"].toString(),QString("custom_status"));
        flow.editProfile();QVERIFY(flow.profileEditing());QCOMPARE(flow.rows().size(),4);
        flow.activate(0);QCOMPARE(text.last()[1].toString(),QString("Mika"));flow.applyText("Ren");
        QCOMPARE(commands.last()[1].toMap()["field"].toString(),QString("global_name"));
        flow.activate(1);QVERIFY(flow.choosingAvatar());flow.dispatch(Action::Back);QVERIFY(!flow.choosingAvatar());QVERIFY(flow.profileEditing());
        flow.activate(2);state["userId"]="other";social.receive(0,state);auto before=commands.size();flow.applyText("Old account draft");QCOMPARE(commands.size(),before);QVERIFY(!flow.profileEditing());
        flow.dispatch(Action::Back);QCOMPARE(back.size(),1);flow.leave();
    }
    void providerStatusIsConfirmedAndRejectsStaleReads() {
        FluxerSession session;QList<Completion> replies;QStringList paths;QList<QJsonObject> bodies;
        session.setTransport([&](QByteArray,QString path,QJsonObject body,Completion done,QByteArray){paths<<path;bodies<<body;replies<<done;});bind(session);
        session.updateUserSettings({{"status","idle"},{"custom_status",QJsonObject{{"text","Old"}}}});
        session.refreshProfile();QCOMPARE(paths.last(),QString("/v1/users/@me/settings"));auto stale=replies.last();
        session.profileCommand({{"field","status"},{"value","invisible"}});QCOMPARE(paths.last(),QString("/v1/users/@me/settings"));
        QCOMPARE(session.userSettings_["status"].toString(),QString("idle"));
        replies.last()({500,{}});QVERIFY(!session.profileBusy_);QCOMPARE(session.userSettings_["status"].toString(),QString("idle"));
        session.profileCommand({{"field","status"},{"value","invisible"}});replies.last()({200,QJsonDocument(QJsonObject{{"status","invisible"}})});
        stale({200,QJsonDocument(QJsonObject{{"status","online"}})});QCOMPARE(session.userSettings_["status"].toString(),QString("invisible"));
        session.updateUserSettings({{"status","dnd"}});QVERIFY(session.notificationsSuppressed());session.updateUserSettings({{"status","online"}});QVERIFY(!session.notificationsSuppressed());
        const auto count=paths.size();session.profileCommand({{"field","status"},{"value","offline"}});QCOMPARE(paths.size(),count);
        session.profileCommand({{"field","custom_status"},{"value","New"}});QCOMPARE(bodies.last()["custom_status"].toObject()["text"].toString(),QString("New"));
        replies.last()({200,QJsonDocument(QJsonObject{{"custom_status",QJsonObject{{"text","New"}}}})});QCOMPARE(session.customStatus(),QString("New"));
        session.profileCommand({{"field","custom_status"},{"value",""}});QVERIFY(bodies.last()["custom_status"].isNull());auto pending=replies.last();
        session.setOwner("trainer-b",2);pending({200,QJsonDocument(QJsonObject{{"status","online"}})});QVERIFY(session.userSettings_.isEmpty());
        session.updateUserSettings({{"custom_status",QJsonObject{{"text","Expired"},{"expires_at","2000-01-01T00:00:00Z"}}}});QVERIFY(session.customStatus().isEmpty());
    }
    void communicationSettingsKeepSelectionAndUseSharedPreferences() {
        SocialController social;CommunicationSettings settings;settings.configure(&social);
        QVariantMap state{{"userId","self"},{"state","connected"},{"audio",QVariantMap{{"volume",60}}},{"profile",QVariantMap{{"name","Friend"}}}};
        social.receive(0,state);QSignalSpy commands(&social,&SocialController::commandRequested);
        QSignalSpy text(&settings,&CommunicationSettings::textRequested);
        settings.activate(0);QCOMPARE(text.last()[1].toString(),QString("Friend"));
        settings.applyText("Updated");QCOMPARE(commands.last()[0].toString(),QString("profile-update"));
        settings.activate(5);settings.dispatch(Action::Left);QCOMPARE(commands.last()[1].toMap()["volume"].toInt(),55);
        state["status"]="Reconnected";social.receive(0,state);QCOMPARE(settings.focusIndex(),5);
        settings.activate(7);QCOMPARE(commands.last()[0].toString(),QString("dnd"));
        settings.inputs_={QVariantMap{{"id","headset"},{"name","Headset"}}};settings.activate(3);
        QCOMPARE(commands.last()[1].toMap()["input"].toString(),QString("headset"));
        settings.activate(2);state["userId"]="other";social.receive(0,state);const auto count=commands.size();settings.applyText("Old draft");QCOMPARE(commands.size(),count);
        QCOMPARE(settings.focusIndex(),0);QVERIFY(!settings.testing());
    }
    void missedCallRequiresObservedRingAndRealEnd() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,QByteArray){});bind(s);
        auto event=[&](QString type,QJsonObject data){s.gatewayEvent({{"op",0},{"t",type},{"d",data}});};
        QJsonObject call{{"channel_id",channel},{"message_id","123"},{"ringing",QJsonArray{s.self_}},{"voice_states",QJsonArray{}}};
        event("CALL_CREATE",call);QVERIFY(!s.callNotices_[channel]["missed"].toBool());
        event("CALL_DELETE",{{"channel_id",channel},{"unavailable",true}});
        QVERIFY(!s.callNotices_[channel]["missed"].toBool());
        s.disconnected();QVERIFY(!s.callNotices_[channel]["missed"].toBool());
        event("CALL_CREATE",call);event("CALL_DELETE",{{"channel_id",channel}});
        QVERIFY(s.callNotices_[channel]["missed"].toBool());
        QSignalSpy snapshots(&s,&FluxerSession::snapshot);s.publish();
        auto state=snapshots.last()[1].toMap();QCOMPARE(state["unreadCount"].toInt(),1);
        QCOMPARE(state["chats"].toList().first().toMap()["missedCall"].toString(),QString("123"));
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);c.receive(0,state);
        QCOMPARE(c.notifications().first().toMap()["detail"].toString(),QString("Missed call"));
        c.openNotificationAt(0);QCOMPARE(commands.last()[0].toString(),QString("conversation"));
        for(const auto& command:commands)QVERIFY(!command[0].toString().startsWith("voice-"));
        c.dismissNotificationAt(0);QVERIFY(c.notifications().isEmpty());
        s.applyReadState({{"channel_id",channel},{"message_id","123"}},true);QVERIFY(s.callNotices_.isEmpty());
        int requests=0;s.setTransport([&](auto,auto,auto,Completion,QByteArray){++requests;});
        s.readsReady_=true;s.messages_["123"]={{"id","123"},{"type",3}};
        s.callNotices_[channel]={{"message","123"},{"missed",true}};
        s.acknowledge(channel,"123");QVERIFY(s.callNotices_.isEmpty());QCOMPARE(requests,0);
    }
    void answeringElsewhereOrDecliningDoesNotCreateMissedCall() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion done,QByteArray){done({204,{}});});bind(s);
        auto event=[&](QString type,QJsonObject data){s.gatewayEvent({{"op",0},{"t",type},{"d",data}});};
        QJsonObject call{{"channel_id",channel},{"message_id","123"},{"ringing",QJsonArray{s.self_}}};
        event("CALL_CREATE",call);
        event("CALL_UPDATE",{{"channel_id",channel},{"ringing",QJsonArray{}},{"voice_states",QJsonArray{QJsonObject{{"user_id",s.self_},{"connection_id","other-device"}}}}});
        event("CALL_DELETE",{{"channel_id",channel}});QVERIFY(s.callNotices_.isEmpty());
        event("CALL_CREATE",call);s.voiceCommand("voice-decline",{{"channel",channel}});
        event("CALL_DELETE",{{"channel_id",channel}});QVERIFY(s.callNotices_.isEmpty());
        event("CALL_DELETE",{{"channel_id",remote}});QVERIFY(s.callNotices_.isEmpty());
    }
    void missedCallCacheIsBoundedAndAccountIsolated() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,QByteArray){});bind(s);
        s.owner_="missed-test-"+QUuid::createUuid().toString(QUuid::Id128);s.setTransport({});s.loadHistoryCache();
        for(int i=0;i<40;++i){
            const auto id=QString::number(1000+i);s.calls_[id]={{"message_id",QString::number(2000+i)},{"ringing",QJsonArray{s.self_}}};
            s.updateCallNotice("CALL_CREATE",id,{});s.updateCallNotice("CALL_DELETE",id,{});
        }
        QCOMPARE(s.callNotices_.size(),32);QVERIFY(!s.callNotices_.contains("1000"));
        const auto owner=s.owner_,file=s.historyFile_;s.saveHistoryCache();
        s.historyFile_.clear();s.callNotices_.clear();s.self_.clear();s.loadHistoryCache();
        QCOMPARE(s.callNotices_.size(),32);QVERIFY(s.callNotices_["1039"]["missed"].toBool());
        s.clearHistoryCache();QVERIFY(!QFileInfo::exists(file));QVERIFY(s.callNotices_.isEmpty());
        QSettings().remove("social/cached-account/"+QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex()));
        s.setTransport([](auto,auto,auto,Completion,QByteArray){});
        s.setOwner("other",2);QVERIFY(s.callNotices_.isEmpty());
    }
    void callHistoryUsesAuthorAndServerTimesWithoutInventingMissedCalls() {
        FluxerSession s;bind(s);QSignalSpy snapshots(&s,&FluxerSession::snapshot);
        QJsonObject message{{"id","42"},{"type",3},{"author",QJsonObject{{"id",remote},{"global_name","Friend"}}},
            {"timestamp","2026-10-02T10:00:00.000Z"},{"call",QJsonObject{{"ended_timestamp","2026-10-02T10:04:08.000Z"}}}};
        auto row=[&](){s.messages_["42"]=message;s.messageOrder_={"42"};s.publish();return snapshots.last()[1].toMap()["messages"].toList().first().toMap();};
        auto shown=row();QVERIFY(shown["callEvent"].toBool());QVERIFY(shown["system"].toBool());
        QCOMPARE(shown["text"].toString(),QString("Call from Friend"));
        QVERIFY(shown["callDetail"].toString().contains("2026"));QVERIFY(shown["callDetail"].toString().endsWith("4 min 8 s"));
        QVERIFY(!shown["missedCall"].toBool());QVERIFY(!shown["editable"].toBool());
        s.callNotices_[channel]={{"message","42"},{"missed",true}};
        shown=row();QVERIFY(shown["missedCall"].toBool());QCOMPARE(shown["text"].toString(),QString("Missed call from Friend"));
        s.callNotices_.clear();message["author"]=QJsonObject{{"id",s.self_}};
        message["call"]=QJsonObject{{"ended_timestamp",QJsonValue::Null}};
        shown=row();QCOMPARE(shown["text"].toString(),QString("You started a call"));QVERIFY(shown["callDetail"].toString().endsWith("Ongoing"));
        message["call"]=QJsonObject{{"ended_timestamp","2026-10-02T09:59:00Z"}};
        shown=row();QVERIFY(shown["callDetail"].toString().endsWith("Call ended"));QVERIFY(!shown["callDetail"].toString().contains("-60"));
        message["timestamp"]="invalid";message.remove("call");shown=row();QVERIFY(shown["callDetail"].toString().isEmpty());
    }
    void backgroundCallNamesItsOwnConversation() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,QByteArray){});bind(s);
        s.channels_[channel]["name"]="Our group";s.voiceChannel_=channel;s.voiceStatus_="Connected";
        s.channel_=remote;s.channels_[remote]={{"id",remote},{"name","Different chat"},{"type",1}};
        QSignalSpy snapshots(&s,&FluxerSession::snapshot);s.publish();const auto voice=snapshots.last()[1].toMap()["voice"].toMap();
        QCOMPARE(voice["name"].toString(),QString("Our group"));
        QCOMPARE(voice["summary"].toString(),QString("Our group · Connected"));
    }
    void invitationNotificationNeverTakesFocusAndExpiredActionCannotAccept() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        QVariantMap state{{"state","connected"},{"userId","self"},{"channel",channel},
            {"online",QVariantMap{{"session","invitation-one"},{"channel",channel},{"name","Friend"},{"incoming",true},{"open",true}}}};
        c.receive(0,state);QVERIFY(!c.online()["open"].toBool());QCOMPARE(c.notifications().size(),1);
        c.receive(0,state);QCOMPARE(c.notifications().size(),1);QVERIFY(commands.isEmpty());
        c.openNotificationAt(0);QVERIFY(c.online()["open"].toBool());QVERIFY(commands.isEmpty());
        c.answerOnline(false);QCOMPARE(commands.last()[0].toString(),QString("online-answer"));
        QVERIFY(!commands.last()[1].toMap()["accept"].toBool());
        c.receive(0,state);QVERIFY(c.notifications().isEmpty());
        state["online"]=QVariantMap{{"session","invitation-two"},{"channel",channel},{"incoming",true},{"open",true}};
        c.receive(0,state);QVERIFY(!c.online()["open"].toBool());
        state["online"]=QVariantMap{{"stage","idle"}};c.receive(0,state);
        QVERIFY(!c.notifications().last().toMap()["pending"].toBool());
        commands.clear();c.openNotificationAt(c.notifications().size()-1);
        QCOMPARE(commands.last()[0].toString(),QString("conversation"));
        QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString(channel));
        for(const auto& command:commands)QVERIFY(command[0].toString()!="online-answer");
        c.setOwner("different");QVERIFY(c.notifications().isEmpty());
    }
    void repeatedFriendRequestProducesOneQuietDestination() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,QByteArray){});bind(s);
        QSignalSpy notices(&s,&FluxerSession::incomingMessage);
        QJsonObject event{{"op",0},{"t","RELATIONSHIP_ADD"},{"d",QJsonObject{{"id",remote},{"type",3},{"user",QJsonObject{{"id",remote},{"username","Friend"}}}}}};
        s.gatewayEvent(event);s.gatewayEvent(event);QCOMPARE(notices.size(),1);
        QCOMPARE(notices.first()[1].toString(),"request:"+QString(remote));
        SocialController c;c.toastChannel_=notices.first()[1].toString();
        c.receive(0,{{"state","connected"},{"userId","self"},{"friends",QVariantList{QVariantMap{{"id",remote},{"type",3},{"name","Friend"}}}}});
        QCOMPARE(c.notificationFace(),QString("chats"));c.openNotification();QVERIFY(!c.contacts());
        s.relationships_.clear();s.doNotDisturb_=true;s.gatewayEvent(event);QCOMPARE(notices.size(),1);
    }
    void unavailableCallRetainsProviderEntryUntilRecovery() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,QByteArray){});bind(s);
        auto event=[&](QString type,QJsonObject payload){s.gatewayEvent({{"op",0},{"t",type},{"d",payload}});};
        event("CALL_CREATE",{{"channel_id",channel},{"ringing",QJsonArray{s.self_}}});
        event("CALL_DELETE",{{"channel_id",channel},{"unavailable",true}});
        QVERIFY(s.calls_.contains(channel));QVERIFY(s.calls_[channel]["unavailable"].toBool());
        QVERIFY(s.calls_[channel]["ringing"].toArray().isEmpty());
        event("CALL_CREATE",{{"channel_id",channel},{"ringing",QJsonArray{}}});
        QVERIFY(!s.calls_[channel]["unavailable"].toBool());
        event("CALL_DELETE",{{"channel_id",channel}});QVERIFY(!s.calls_.contains(channel));
    }
    void incomingCallAnswersFromHomeWithoutChangingPage() {
        MockLibraryRepository library;MockTrainerRepository trainers;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository hall;MockAchievementProvider achievements;
        ShellController shell(library,trainers,adapter,platform,builtinExperiences(dex,dex),hall,achievements);
        auto* social=shell.social();QSignalSpy commands(social,&SocialController::commandRequested);
        shell.goToPage(1);const auto origin=shell.navigationState();commands.clear();
        social->receive(0,{{"callsSuppressed",true},{"state","connected"},{"userId","self"},{"voice",QVariantMap{{"available",true}}},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"name","Friend"},{"kind","chats"},{"ringing",true}}}}});
        QVERIFY(!shell.homeMenuOpen());QVERIFY(commands.isEmpty());
        shell.dispatch(Action::Home);
        QCOMPARE(shell.homeMenuActions()[shell.homeMenuFocus()].toMap()["id"].toString(),QString("answer-call:")+channel);
        shell.dispatch(Action::Confirm);QVERIFY(!shell.homeMenuOpen());QCOMPARE(shell.navigationState(),origin);
        QCOMPARE(commands.size(),1);QCOMPARE(commands.last()[0].toString(),QString("voice-join"));
        QCOMPARE(commands.last()[1].toMap()["channel"].toString(),QString(channel));
    }
    void homeSelectionFollowsActionWhenCallRowsChange() {
        MockLibraryRepository library;MockTrainerRepository trainers;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository hall;MockAchievementProvider achievements;
        ShellController shell(library,trainers,adapter,platform,builtinExperiences(dex,dex),hall,achievements);
        auto* social=shell.social();QSignalSpy commands(social,&SocialController::commandRequested);
        QVariantMap chat{{"id",channel},{"name","Friend"},{"kind","chats"},{"ringing",true}};
        QVariantMap state{{"callsSuppressed",true},{"state","connected"},{"userId","self"},{"voice",QVariantMap{{"available",true}}},{"chats",QVariantList{chat}}};
        social->receive(0,state);shell.dispatch(Action::Home);shell.dispatch(Action::Down);commands.clear();
        const QString decline="decline-call:"+QString(channel);
        QCOMPARE(shell.homeMenuActions()[shell.homeMenuFocus()].toMap()["id"].toString(),decline);
        // Losing audio availability removes Answer. Keep Decline selected,
        // rather than using the previous numeric position.
        state["voice"]=QVariantMap{{"available",false}};
        social->receive(0,state);
        QCOMPARE(shell.homeMenuActions()[shell.homeMenuFocus()].toMap()["id"].toString(),decline);
        chat["ringing"]=false;state["chats"]=QVariantList{chat};social->receive(0,state);
        QCOMPARE(shell.homeMenuActions()[shell.homeMenuFocus()].toMap()["id"].toString(),decline);
        QVERIFY(!shell.homeMenuActions()[shell.homeMenuFocus()].toMap()["enabled"].toBool());
        shell.dispatch(Action::Confirm);QVERIFY(shell.homeMenuOpen());QVERIFY(commands.isEmpty());
        // No generic Home shortcut remains. Explicit navigation leaves the stale action for the inbox.
        shell.dispatch(Action::Up);QVERIFY(shell.notificationsOpen());
        chat["ringing"]=true;state["chats"]=QVariantList{chat};social->receive(0,state);
        QVERIFY(shell.notificationsOpen());QVERIFY(commands.isEmpty());
    }
    void expiredRingKeepsJoinUntilCallEndsWithoutRedirectingConfirm() {
        MockLibraryRepository library;MockTrainerRepository trainers;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository hall;MockAchievementProvider achievements;
        ShellController shell(library,trainers,adapter,platform,builtinExperiences(dex,dex),hall,achievements);
        auto* social=shell.social();QSignalSpy commands(social,&SocialController::commandRequested);
        shell.goToPage(3);const auto origin=shell.navigationState();
        QVariantMap chat{{"id",channel},{"name","Friend"},{"kind","chats"},{"ringing",true},{"call",true}};
        QVariantMap state{{"callsSuppressed",true},{"state","connected"},{"userId","self"},{"voice",QVariantMap{{"available",true}}},{"chats",QVariantList{chat}}};
        social->receive(0,state);shell.dispatch(Action::Home);commands.clear();
        chat["ringing"]=false;state["chats"]=QVariantList{chat};social->receive(0,state);
        QCOMPARE(shell.homeMenuActions()[shell.homeMenuFocus()].toMap()["label"].toString(),QString("Join call"));
        shell.dispatch(Action::Confirm);
        QVERIFY(!shell.homeMenuOpen());QCOMPARE(shell.navigationState(),origin);
        QCOMPARE(commands.size(),1);QCOMPARE(commands.last()[0].toString(),QString("voice-join"));
        // Only the selected invitation is retained, not every active call.
        shell.dispatch(Action::Home);QCOMPARE(shell.homeMenuFocus(),0);
        for(const auto& row:shell.homeMenuActions())QVERIFY(!row.toMap()["id"].toString().startsWith("answer-call:"));
        shell.dispatch(Action::Home);
        chat["ringing"]=true;state["chats"]=QVariantList{chat};social->receive(0,state);
        shell.dispatch(Action::Home);commands.clear();
        chat["ringing"]=false;chat["call"]=false;state["chats"]=QVariantList{chat};social->receive(0,state);
        const auto ended=shell.homeMenuActions()[shell.homeMenuFocus()].toMap();
        QVERIFY(!ended["enabled"].toBool());
        shell.dispatch(Action::Confirm);QVERIFY(shell.homeMenuOpen());QVERIFY(commands.isEmpty());
        QCOMPARE(shell.navigationState(),origin);
        shell.dispatch(Action::Back);QVERIFY(!shell.homeMenuOpen());
        shell.dispatch(Action::Home);QCOMPARE(shell.homeMenuFocus(),0);
    }
    void soloNotificationReplyKeepsGameHomeAndRejectsAReplacedProcess() {
        SocialController c;disconnect(c.session_,nullptr,&c,nullptr);disconnect(&c,nullptr,c.session_,nullptr);
        c.receive(0,{{"state","connected"},{"userId","self"},{"channel",remote},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"name","Friend"},{"kind","chats"},{"unread",1}}}}});
        AdventureExitController exit;exit.beginSession(AdventureSavePolicy::ManualConfirm);exit.setAvailable(true);
        AdventureExitPresentation view(exit);TextEntryController keyboard;PlayerOverlayController hud(c,keyboard,view);
        connect(&view,&AdventureExitPresentation::menuCaptureRequested,&view,[&](quint64 token){view.menuCaptureCompleted(token,{});});
        hud.setGame(42,false);QVERIFY(hud.players().isEmpty());QVERIFY(view.requestMenu());
        view.setInputIsolated(true);view.setWindowFocused(true);view.updateInput(view.inputGeneration(),{true,true});
        QVERIFY(view.ready());QVERIFY(hud.openNotification(channel));QVERIFY(hud.chatOpen());
        c.snapshot_["channel"]=channel;emit c.changed();QVERIFY(hud.conversationReady());
        view.updateInput(view.inputGeneration(),{true,true});hud.dispatch(Action::Back);
        QVERIFY(!hud.chatOpen());QVERIFY(view.menuOpen());QVERIFY(!c.notificationChatValid(channel));
        view.updateInput(view.inputGeneration(),{true,true});QVERIFY(hud.openNotification(channel));
        hud.setGame(43,false);QVERIFY(!hud.chatOpen());QVERIFY(!c.notificationChatValid(channel));
    }
    void notificationReplyLeaseBindsKnownChannelAndRestoresPrevious() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        QVariantMap chat{{"id",channel},{"name","Friend"},{"kind","chats"},{"unread",1}};
        QVariantMap state{{"state","connected"},{"userId","self"},{"channel",remote},{"chats",QVariantList{chat}}};
        c.receive(0,state);QVERIFY(!c.acquireNotificationChat("unknown"));QVERIFY(c.acquireNotificationChat(channel));
        QVERIFY(c.notificationChatValid(channel));QVERIFY(!c.acquireNotificationChat(channel));
        commands.clear();c.sendNotificationChat(channel);QVERIFY(commands.isEmpty());
        state["channel"]=channel;c.receive(0,state);c.composeNotificationChat(channel);c.applyText("hello");
        QCOMPARE(commands.last()[0].toString(),QString("send"));QCOMPARE(commands.last()[1].toMap()["channel"].toString(),QString(channel));
        c.releaseNotificationChat();QCOMPARE(commands.last()[0].toString(),QString("conversation"));QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString(remote));
        QVERIFY(!c.notificationChatValid(channel));
        chat["socialHidden"]=true;state["chats"]=QVariantList{chat};c.receive(0,state);QVERIFY(!c.acquireNotificationChat(channel));
    }
    void callNotificationRequiresExplicitAnswerAndDeclineKeepsOrigin() {
        MockLibraryRepository library;MockTrainerRepository trainers;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository hall;MockAchievementProvider achievements;
        ShellController shell(library,trainers,adapter,platform,builtinExperiences(dex,dex),hall,achievements);
        auto* social=shell.social();QSignalSpy commands(social,&SocialController::commandRequested);
        shell.goToPage(3);const auto origin=shell.navigationState();
        social->receive(0,{{"callsSuppressed",true},{"state","connected"},{"userId","self"},{"voice",QVariantMap{{"available",true}}},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"name","Friend"},{"kind","chats"},{"ringing",true}}}}});
        shell.dispatch(Action::Home);shell.dispatch(Action::Right);QVERIFY(shell.notificationsOpen());
        QVERIFY(social->notifications().first().toMap()["answerable"].toBool());
        commands.clear();shell.activateNotification(0);QVERIFY(shell.homeMenuOpen());QVERIFY(!shell.notificationsOpen());
        QCOMPARE(shell.navigationState(),origin);QVERIFY(commands.isEmpty());
        shell.activateHomeAction("answer-call:"+QString(channel));QVERIFY(!shell.homeMenuOpen());
        QCOMPARE(commands.last()[0].toString(),QString("voice-join"));
        shell.dispatch(Action::Home);shell.activateHomeAction("decline-call:"+QString(channel));
        QCOMPARE(commands.last()[0].toString(),QString("voice-decline"));QCOMPARE(shell.navigationState(),origin);
        for(const auto& command:commands)QVERIFY(command[0].toString()!="conversation");
    }
    void incomingCallNeverReplacesExistingCallAndRejectsStaleAnswer() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        QVariantMap chat{{"id",channel},{"name","Friend"},{"kind","chats"},{"ringing",true}};
        QVariantMap state{{"state","connected"},{"userId","self"},{"chats",QVariantList{chat}},
            {"voice",QVariantMap{{"available",true},{"channel",remote}}}};
        c.receive(0,state);QVERIFY(!c.answerCall(channel,true));QVERIFY(commands.isEmpty());
        QCOMPARE(c.incomingCallActions().size(),1);
        QVERIFY(c.incomingCallActions().first().toMap()["id"].toString().startsWith("decline-call:"));
        QVERIFY(c.answerCall(channel,false));QCOMPARE(commands.last()[0].toString(),QString("voice-decline"));
        commands.clear();state["voice"]=QVariantMap{{"available",true}};chat["ringing"]=false;chat["missedCall"]="123";
        state["chats"]=QVariantList{chat};c.receive(0,state);
        QVERIFY(!c.answerCall(channel,true));QVERIFY(!c.answerCall(channel,false));QVERIFY(c.incomingCallActions().isEmpty());
        QVERIFY(commands.isEmpty());
        chat["ringing"]=true;state["chats"]=QVariantList{chat};state["state"]="reconnecting";c.receive(0,state);
        QVERIFY(!c.answerCall(channel,true));QVERIFY(c.incomingCallActions().isEmpty());QVERIFY(commands.isEmpty());
    }
    void homeCallControlsKeepTheOriginAndNeverImplicitlyLeave() {
        MockLibraryRepository library;MockTrainerRepository trainers;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository hall;MockAchievementProvider achievements;
        ShellController shell(library,trainers,adapter,platform,builtinExperiences(dex,dex),hall,achievements);
        auto* social=shell.social();QSignalSpy commands(social,&SocialController::commandRequested);
        social->receive(0,{{"state","connected"},{"userId","self"},{"voice",QVariantMap{{"channel",channel},{"muted",true},{"status","Connected"}}}});
        shell.goToPage(1);const auto origin=shell.navigationState();shell.dispatch(Action::Home);
        QCOMPARE(shell.homeMenuActions().last().toMap()["id"].toString(),QString("call"));
        shell.activateHomeAction("call");QCOMPARE(shell.homeMenuActions().first().toMap()["id"].toString(),QString("voice-mute"));
        commands.clear();shell.dispatch(Action::Confirm);QCOMPARE(commands.last()[0].toString(),QString("voice-mute"));
        shell.dispatch(Action::Back);QVERIFY(shell.homeMenuOpen());shell.dispatch(Action::Home);
        QVERIFY(!shell.homeMenuOpen());QCOMPARE(shell.navigationState(),origin);
        for(const auto& command:commands)QVERIFY(command[0].toString()!="voice-leave");
        shell.dispatch(Action::Home);shell.activateHomeAction("call");
        social->receive(0,{{"state","connected"},{"userId","self"},{"voice",QVariantMap{}}});
        QCOMPARE(shell.homeMenuActions().size(),1);shell.dispatch(Action::Confirm);QVERIFY(shell.homeMenuOpen());
    }
    void chatReconnectKeepsBackgroundCallAndMuteChoice() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,QByteArray){});bind(s);
        s.voiceChannel_=channel;s.voiceConnection_="placement";s.voiceState_="connected";s.voiceMuted_=false;s.voiceDeaf_=true;
        s.disconnected();
        QCOMPARE(s.voiceChannel_,QString(channel));QCOMPARE(s.voiceConnection_,QString("placement"));
        QCOMPARE(s.voiceState_,QString("connected"));QVERIFY(!s.voiceMuted_);QVERIFY(s.voiceDeaf_);
        s.command("voice-mute");QVERIFY(s.voiceMuted_);
        s.setOwner("another",2);QVERIFY(s.voiceChannel_.isEmpty());
    }
    void freshGatewayRechecksCallAccessWithoutRinging() {
        FluxerSession s;Completion result;QString path;
        s.setTransport([&](auto method,auto p,auto,Completion callback,QByteArray){QCOMPARE(method,QByteArray("GET"));path=p;result=callback;});bind(s);
        s.voiceChannel_=channel;s.voiceConnection_="placement";s.voiceState_="connected";
        s.reconcileVoice();QCOMPARE(path,"/v1/channels/"+QString(channel));
        result({200,QJsonDocument(QJsonObject{{"id",channel},{"type",3}})});QCOMPARE(s.voiceChannel_,QString(channel));
        s.reconcileVoice();result({403,{}});QVERIFY(s.voiceChannel_.isEmpty());
    }
    void staleCallAccessReplyDoesNotCloseAnotherCall() {
        FluxerSession s;Completion result;s.setTransport([&](auto,auto,auto,Completion callback,QByteArray){result=callback;});bind(s);
        s.voiceChannel_=channel;s.voiceConnection_="old";s.reconcileVoice();
        s.voiceConnection_="new";result({403,{}});QCOMPARE(s.voiceChannel_,QString(channel));
    }
    void removedGroupMemberLeavesOwnCallButOtherDeviceEventsDoNot() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,QByteArray){});bind(s);
        s.voiceChannel_=channel;s.voiceConnection_="ours";
        s.gatewayEvent({{"op",0},{"t","VOICE_STATE_UPDATE"},{"d",QJsonObject{
            {"user_id",s.self_},{"connection_id","other-device"},{"channel_id",QJsonValue::Null}}}});
        QCOMPARE(s.voiceChannel_,QString(channel));
        s.gatewayEvent({{"op",0},{"t","CHANNEL_RECIPIENT_REMOVE"},{"d",QJsonObject{
            {"channel_id",channel},{"user",QJsonObject{{"id",s.self_}}}}}});
        QVERIFY(s.voiceChannel_.isEmpty());
    }
    void backgroundNavigationNeverCommandsCallTeardown() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.receive(0,{{"state","connected"},{"voice",QVariantMap{{"channel",channel},{"state","connected"}}}});
        c.setSurfaceAvailable(false);c.setGameActive(true);c.setFace("groups");c.setFace("chats");c.setGameActive(false);c.setSurfaceAvailable(true);
        for(const auto& command:commands)QVERIFY(command[0].toString()!="voice-leave");
    }
    void dismissNotificationDoesNotReadAndNewMessageReturns() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        QVariantMap chat{{"id",channel},{"name","Friend"},{"kind","chats"},{"unread",1},{"last","123"},{"mentions",1}};
        QVariantMap state{{"state","connected"},{"userId","self"},{"chats",QVariantList{chat}}};
        c.receive(0,state);QCOMPARE(c.notifications().first().toMap()["detail"].toString(),QString("New messages"));
        c.dismissNotificationAt(0);QVERIFY(c.notifications().isEmpty());QVERIFY(commands.isEmpty());
        c.receive(0,state);QVERIFY(c.notifications().isEmpty());
        chat["last"]="124";state["chats"]=QVariantList{chat};c.receive(0,state);QCOMPARE(c.notifications().size(),1);
        chat["guild"]="community";state["chats"]=QVariantList{chat};c.receive(0,state);
        QCOMPARE(c.notifications().first().toMap()["detail"].toString(),QString("You were mentioned"));
        c.setOwner("different");QVERIFY(c.notifications().isEmpty());
    }
    void mentionAcknowledgementAndDuplicateEventsStayConsistent() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,QByteArray){});bind(s);s.readsReady_=true;
        QJsonObject event{{"op",0},{"t","MESSAGE_CREATE"},{"d",QJsonObject{
            {"id","123"},{"channel_id",channel},{"content","Hello"},{"author",QJsonObject{{"id",remote}}},
            {"mentions",QJsonArray{QJsonObject{{"id",s.self_}}}}}}};
        s.gatewayEvent(event);s.gatewayEvent(event);QCOMPARE(s.mentions_[channel],1);
        s.applyReadState({{"channel_id",channel},{"message_id","123"},{"mention_count",0}},true);
        QCOMPARE(s.mentions_[channel],0);QCOMPARE(s.unread_[channel],0);
    }
    void retryReusesNonceAndCannotDoubleSendWhilePending() {
        FluxerSession s;Completion done;QStringList nonces;
        s.setTransport([&](auto,auto,QJsonObject body,Completion callback,QByteArray){nonces<<body["nonce"].toString();done=callback;});bind(s);
        s.command("send",{{"text","Hello"}});const auto nonce=nonces.first();done({429,{},1});
        QCOMPARE(s.messages_[nonce]["local_delivery"].toString(),QString("Not sent"));
        s.blockedUntil_=0;s.command("retry-message",{{"channel",channel},{"id",nonce}});
        s.command("retry-message",{{"channel",channel},{"id",nonce}});QCOMPARE(nonces.size(),2);QCOMPARE(nonces.last(),nonce);
        done({200,QJsonDocument(QJsonObject{{"id","123"},{"channel_id",channel},{"content","Hello"}})});
        QCOMPARE(s.messageOrder_,QStringList{"123"});QVERIFY(s.pendingNonces_.isEmpty());
    }
    void unknownDeliveryRetriesOnlyWithinProviderNonceWindow() {
        FluxerSession s;int calls=0;
        s.setTransport([&](auto,auto,auto,Completion done,QByteArray){++calls;done({0,{}});});bind(s);
        s.command("send",{{"text","Hello"}});const auto nonce=s.messageOrder_.first();
        s.command("retry-message",{{"channel",channel},{"id",nonce}});QCOMPARE(calls,2);
        s.messages_[nonce]["local_sent_at"]=QString::number(QDateTime::currentMSecsSinceEpoch()-300001);
        s.command("retry-message",{{"channel",channel},{"id",nonce}});QCOMPARE(calls,2);
    }
    void serverEchoWinsOverLateHttpFailure() {
        FluxerSession s;Completion done;
        s.setTransport([&](auto,auto,auto,Completion callback,QByteArray){done=callback;});bind(s);
        QSignalSpy failures(&s,&FluxerSession::sendFailed);
        s.command("send",{{"text","Hello"}});const auto nonce=s.messageOrder_.first();
        s.mergeMessage({{"id","123"},{"channel_id",channel},{"nonce",nonce},{"content","Hello"}});
        done({500,{}});QCOMPARE(s.messageOrder_,QStringList{"123"});QVERIFY(failures.isEmpty());
    }
    void conversationSwitchKeepsHistoryAndAnchor() {
        FluxerSession s;QList<Completion> callbacks;
        s.setTransport([&](auto,auto,auto,Completion done,QByteArray){callbacks<<done;});bind(s);
        s.channels_[remote]={{"id",remote},{"type",1}};
        s.mergeMessage({{"id","123"},{"channel_id",channel},{"content","First"}});
        s.historyAnchor_="123";s.historyPast_=true;s.openConversation(remote);
        s.openConversation(channel);QCOMPARE(s.messageOrder_,QStringList{"123"});QCOMPARE(s.historyAnchor_,QString("123"));
        callbacks.last()({0,{}});QCOMPARE(s.messages_["123"]["content"].toString(),QString("First"));
        s.setOwner("different",2);QVERIFY(s.historyCache_.isEmpty());QVERIFY(s.historyAnchor_.isEmpty());
    }
    void inactiveHistoryReceivesEditsAndDeletions() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,QByteArray){});bind(s);
        s.channels_[remote]={{"id",remote},{"type",1}};
        s.mergeMessage({{"id","123"},{"channel_id",channel},{"content","Before"}});s.openConversation(remote);
        s.gatewayEvent({{"op",0},{"s",1},{"t","MESSAGE_UPDATE"},{"d",QJsonObject{{"id","123"},{"channel_id",channel},{"content","After"}}}});
        QCOMPARE(s.historyCache_[channel]["rows"].toArray().first().toObject()["content"].toString(),QString("After"));
        s.gatewayEvent({{"op",0},{"s",2},{"t","MESSAGE_DELETE"},{"d",QJsonObject{{"id","123"},{"channel_id",channel}}}});
        QVERIFY(s.historyCache_[channel]["rows"].toArray().isEmpty());
    }
    void persistentHistoryStripsMediaCapabilitiesAndUncertainSendsNeverReplay() {
        QTemporaryDir dir;QVERIFY(dir.isValid());FluxerSession s;bind(s);
        s.historyFile_=dir.path()+"/history.json";
        s.mergeMessage({{"id","123"},{"channel_id",channel},{"content","Привет שלום 👩‍👩‍👧‍👦"},
            {"attachments",QJsonArray{QJsonObject{{"url","https://private.invalid/signed-secret"}}}}});
        s.retainHistory();s.saveHistoryCache();QFile file(s.historyFile_);QVERIFY(file.open(QIODevice::ReadOnly));
        const auto bytes=file.readAll();QVERIFY(!bytes.contains("signed-secret"));QVERIFY(!bytes.contains("synthetic-test-session"));
        QVERIFY(bytes.contains(QString("Привет שלום 👩‍👩‍👧‍👦").toUtf8()));
        file.close();s.clearHistoryCache();QVERIFY(!QFileInfo::exists(file.fileName()));
    }
    void deniedHistoryDropsCachedPrivateText() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion done,QByteArray){done({403,{}});});bind(s);
        s.mergeMessage({{"id","123"},{"channel_id",channel},{"content","Private"}});s.retainHistory();s.loadMessages(channel);
        QVERIFY(s.messages_.isEmpty());QVERIFY(s.historyCache_.value(channel)["rows"].toArray().isEmpty());
    }
    void coldHistoryRequiresTheSameProtectedAccountAndRestoresPosition() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion,QByteArray){});bind(s);
        s.owner_="cache-test-"+QUuid::createUuid().toString(QUuid::Id128);
        s.setTransport({});s.loadHistoryCache();
        s.mergeMessage({{"id","123"},{"channel_id",channel},{"content","Retained text"}});
        s.historyAnchor_="123";s.historyPast_=true;s.retainHistory();s.saveHistoryCache();
        const auto owner=s.owner_,self=s.self_,file=s.historyFile_;
        s.historyFile_.clear();s.historyCache_.clear();s.messages_.clear();s.messageOrder_.clear();s.self_.clear();s.channels_.clear();s.channel_.clear();
        s.token_="different-session";s.loadHistoryCache();QVERIFY(s.self_.isEmpty());QVERIFY(s.messages_.isEmpty());
        s.historyFile_.clear();s.token_="synthetic-test-session";s.loadHistoryCache();
        QCOMPARE(s.self_,self);QCOMPARE(s.historyAnchor_,QString("123"));QCOMPARE(s.messages_["123"]["content"].toString(),QString("Retained text"));
        s.clearHistoryCache();QVERIFY(!QFileInfo::exists(file));
        QSettings().remove("social/cached-account/"+QString::fromLatin1(QCryptographicHash::hash(owner.toUtf8(),QCryptographicHash::Sha256).toHex()));
    }
    void retryMenuTargetsTheFailedMessageWithoutReopeningComposer() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested),keyboard(&c,&SocialController::textRequested);
        c.receive(0,{{"state","connected"},{"channel",channel},{"messages",QVariantList{
            QVariantMap{{"id","failed-nonce"},{"mine",true},{"retryable",true},{"delivery","Not sent"},{"text","Try again"}}}}});
        c.dispatch(Action::Right);c.dispatch(Action::Confirm);
        QCOMPARE(c.menu().first(),QString("Retry sending"));c.selectMenu(0);
        QCOMPARE(commands.last()[0].toString(),QString("retry-message"));
        QCOMPARE(commands.last()[1].toMap()["channel"].toString(),QString(channel));
        QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString("failed-nonce"));QVERIFY(keyboard.isEmpty());
    }
    void reviewsKeepExactIdentityAndReadWithoutCompletion() {
        const QString hash="a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af";
        QSettings().remove("reviews");FluxerSession session;int searches=0;
        const QJsonObject message{{"id","1501314428688998300"},{"channel_id",channel},
            {"author",QJsonObject{{"id",remote},{"username","Reader"}}},
            {"content",reviews::encode(hash,"A lovely adventure",true,"emerald-en/champion-v1")}};
        session.setTransport([&](QByteArray method,QString path,QJsonObject body,Completion done,QByteArray){
            QCOMPARE(method,QByteArray("POST"));QCOMPARE(path,QString("/v1/search/messages"));++searches;
            const auto messages=body.contains("author_id")?QJsonArray{}:QJsonArray{message};
            done({200,QJsonDocument(QJsonObject{{"messages",messages},{"total",messages.size()}})});
        });bind(session);
        session.command("reviews-open",{{"identity",hash},{"testChannel",channel}});
        QCOMPARE(searches,2);QCOMPARE(session.reviewRows_.size(),1);QVERIFY(session.reviewFresh_);QVERIFY(session.ownReview_.isEmpty());
        QVERIFY(reviews::decode(message,QString(64,'b')).isEmpty());
        auto webhook=message;webhook["webhook_id"]="123";QVERIFY(reviews::decode(webhook,hash).isEmpty());
        session.command("reviews-save",{{"identity",hash},{"text","No completion"}});QCOMPARE(searches,2);
    }
    void completionIsExactSaveMilestoneNotTimeOrCollection() {
        GameProgress p;p.availability=ProgressAvailability::Available;p.journey=JourneySnapshot{};
        p.contentRevision="a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af";
        p.journey->playtimeMinutes=50000;QVERIFY(!reviewCompletion(p).completed);
        p.journey->milestones={{"champion","Champion",std::nullopt}};QVERIFY(!reviewCompletion(p).completed);
        p.journey->milestones[0].achieved=true;QVERIFY(reviewCompletion(p).completed);
        p.availability=ProgressAvailability::Unreadable;QVERIFY(!reviewCompletion(p).completed);
        p.availability=ProgressAvailability::Available;p.contentRevision=QString(64,'b');QVERIFY(!reviewCompletion(p).completed);
    }
    void singleAttachmentOpensDirectlyWithoutAnOptionsStep() {
        SocialController c;
        c.receive(0,{{"state","connected"},{"channel",channel},{"messages",QVariantList{
            QVariantMap{{"id","123"},{"name","Friend"},{"media",true},{"mine",false},
                {"attachments",QVariantList{QVariantMap{{"url","https://invalid.example/image"},{"content_type","image/png"}}}}}
        }}});
        c.dispatch(Action::Right);c.dispatch(Action::Confirm);
        QVERIFY(c.mediaPreview());QCOMPARE(c.media()->state(),QString("error"));
        QCOMPARE(c.menu(),QStringList{"Close"});
        c.dispatch(Action::Back);QVERIFY(c.menu().isEmpty());QVERIFY(c.conversation());
    }
    void reviewEditAndDeleteTargetOnlyOwnStoredMessage() {
        QSettings().remove("reviews");FluxerSession session;QByteArray method;QString path;Completion pending;
        session.setTransport([&](QByteArray m,QString p,QJsonObject,Completion done,QByteArray){method=m;path=p;pending=std::move(done);});bind(session);
        session.reviewIdentity_="a9dec84dfe7f62ab2220bafaef7479da0929d066ece16a6885f6226db19085af";session.reviewChannel_=channel;session.reviewFresh_=true;
        session.ownReview_={{"id","1501314428688998333"},{"author",session.self_}};
        const QVariantMap args{{"identity",session.reviewIdentity_},{"text","Edited"},{"completed",true},{"policy","emerald-en/champion-v1"},{"id","1501314428688999999"}};
        session.command("reviews-save",args);QCOMPARE(method,QByteArray("PATCH"));QVERIFY(path.endsWith("/1501314428688998333"));QVERIFY(session.reviewBusy_);
        pending({400,{}});QVERIFY(!session.reviewFresh_);
        method.clear();session.command("reviews-delete",args);QVERIFY(method.isEmpty());
        session.reviewFresh_=true;session.command("reviews-delete",args);QCOMPARE(method,QByteArray("DELETE"));QVERIFY(path.endsWith("/1501314428688998333"));
        pending({204,{}});QVERIFY(session.ownReview_.isEmpty());
    }
    void attachmentUsesNativeVoiceContractAndWaitsForMessageAcknowledgement() {
        FluxerSession s;Completion pending;QJsonObject sent;int requests=0;
        s.setTransport([&](QByteArray method,QString path,QJsonObject body,Completion done,QByteArray){
            QCOMPARE(method,QByteArray("POST"));QVERIFY(path.endsWith("/messages"));sent=body;pending=std::move(done);++requests;
        });bind(s);QSignalSpy finished(&s,&FluxerSession::attachmentFinished);
        const QVariantMap upload{{"channel",channel},{"bytes",QByteArray("synthetic encoded fixture")},{"voice",true},{"seconds",4},{"waveform",QByteArray(64,42)}};
        s.command("attachment",upload);QVERIFY(s.attachmentBusy_);QVERIFY(finished.isEmpty());
        QCOMPARE(sent["flags"].toInt(),1<<13);QVERIFY(!sent.contains("content"));
        const auto attachment=sent["attachments"].toArray().first().toObject();
        QCOMPARE(attachment["duration"].toInt(),4);QCOMPARE(QByteArray::fromBase64(attachment["waveform"].toString().toLatin1()),QByteArray(64,42));
        s.command("attachment",upload);QCOMPARE(requests,1);
        pending({200,QJsonDocument(QJsonObject{{"id","1501314428688998290"},{"channel_id",channel},{"content",""}})});
        QVERIFY(!s.attachmentBusy_);QCOMPARE(finished.size(),1);QCOMPARE(finished.first()[2].toInt(),200);
        auto invalid=upload;invalid["waveform"]=QByteArray();s.command("attachment",invalid);QCOMPARE(requests,1);
        s.command("attachment",upload);s.setOwner("other",2);pending({200,{}});QCOMPARE(finished.size(),1);
    }
    void mediaDiscardAndOwnerChangeNeverUpload() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.menuMode_="media-preview";c.menu_={"Discard"};c.menuCommands_={"cancel"};
        c.dispatch(Action::Back);QVERIFY(c.menu().isEmpty());QVERIFY(c.media()->state().isEmpty());
        for(const auto& row:commands)QVERIFY(row[0]!="attachment");
        c.mediaSending_=true;c.menuChannel_=channel;c.closeMenu();
        QCOMPARE(commands.last()[0].toString(),QString("cancel-attachment"));
        c.setOwner("new owner");QVERIFY(c.media()->picture().isEmpty());
    }
    void activeGameNotificationsKeepDestinationWithoutAcknowledging() {
        SocialController c;QSignalSpy background(&c,&SocialController::backgroundNotification);
        QSignalSpy commands(&c,&SocialController::commandRequested);
        c.receive(0,{{"notificationSound",false}});c.setGameActive(true);
        c.session_->incomingMessage(0,channel,"Friend","Hello");
        QCOMPARE(background.size(),1);QCOMPARE(c.toastTitle(),QString("Friend"));
        for(const auto& row:commands)QVERIFY(row[0]!="read");
        c.session_->incomingMessage(99,channel,"Other owner","Hidden");QCOMPARE(background.size(),1);
    }
    void missedNotificationsFollowProviderStateAndNeverAcceptRequests() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        QVariantMap state{{"state","connected"},{"userId","self"},{"friends",QVariantList{
            QVariantMap{{"id","request"},{"name","New friend"},{"type",3}}}},
            {"chats",QVariantList{
            QVariantMap{{"id",channel},{"name","Group"},{"kind","groups"},{"unread",1}},
            QVariantMap{{"id","muted"},{"kind","chats"},{"unread",1},{"muted",true}},
            QVariantMap{{"id","read"},{"kind","chats"},{"unread",0}}}}};
        c.receive(0,state);QCOMPARE(c.notifications().size(),2);
        QCOMPARE(c.notificationFaceAt(1),"communities");c.openNotificationAt(1);
        QCOMPARE(commands.last()[0].toString(),"conversation");
        QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString(channel));
        commands.clear();c.openNotificationAt(0);QVERIFY(!c.contacts());
        QCOMPARE(c.rows().value(c.focusIndex()).toMap()["id"].toString(),"request");
        for(const auto& command:commands)QVERIFY(command[0].toString()!="accept");
        state["chats"]=QVariantList{};c.receive(0,state);QVERIFY(c.notifications().isEmpty());
        c.setOwner("another-trainer");QVERIFY(c.notifications().isEmpty());
    }
    void linkPacketsAreHiddenButKeepTheirReadWatermark() {
        FluxerSession s;bind(s);s.readsReady_=true;
        QSignalSpy snapshots(&s,&FluxerSession::snapshot);
        const auto packet=OnlineLink::encode({{"ns","org.traineros.link"},{"v",1},{"kind","frame"}});
        s.mergeMessage({{"id","100"},{"channel_id",channel},{"content","Hello"}});
        s.mergeMessage({{"id","101"},{"channel_id",channel},{"content",packet}});
        s.publish();auto state=snapshots.last()[1].toMap();
        QCOMPARE(state["messages"].toList().size(),1);QCOMPARE(state["readTail"].toString(),"101");
        QString ack; s.setTransport([&](auto,auto,QJsonObject body,auto,auto){ack=body["read_states"].toArray().first().toObject()["message_id"].toString();});
        s.command("read",{{"channel",channel},{"message","101"}});QCOMPARE(ack,"101");
        s.messages_.remove("100");s.messageOrder_.removeAll("100");s.publish();
        state=snapshots.last()[1].toMap();QVERIFY(state["messages"].toList().isEmpty());QCOMPARE(state["readTail"].toString(),"101");
        SocialController c;c.receive(0,state);c.setSurfaceAvailable(true);c.setConversationVisible(true);
        QSignalSpy commands(&c,&SocialController::commandRequested);
        c.presented(channel,"101");QCOMPARE(commands.size(),1);
        c.presented(channel,"999");QCOMPARE(commands.size(),1);
    }
    void quietTrafficNeverClearsEarlierHumanUnread() {
        FluxerSession s;bind(s);s.setTransport([](auto,auto,auto,auto,auto){});s.readsReady_=true;
        auto event=[&](QString id,QString who,QString content){s.gatewayEvent({{"op",0},{"t","MESSAGE_CREATE"},{"d",QJsonObject{
            {"id",id},{"channel_id",channel},{"author",QJsonObject{{"id",who}}},{"content",content}}}});};
        const auto packet=OnlineLink::encode({{"ns","org.traineros.link"},{"v",1},{"kind","probe"}});
        event("100",remote,packet);QCOMPARE(s.unread_.value(channel),0);
        event("101",s.self_,"My reply");QCOMPARE(s.unread_.value(channel),0);
        event("102",remote,"Hello");QCOMPARE(s.unread_.value(channel),1);
        event("103",remote,packet);QCOMPARE(s.unread_.value(channel),1);
        event("104",s.self_,"Reply while earlier message is unread");QCOMPARE(s.unread_.value(channel),1);
        s.applyReadState({{"id",channel},{"last_message_id","104"}},false);QCOMPARE(s.unread_.value(channel),0);
        s.applyReadState({{"channel_id",channel},{"message_id","99"},{"manual",true}},true);QCOMPARE(s.unread_.value(channel),1);
    }
    void notificationDestinationOpensTheActualGroupAndStaysOwnerScoped() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.receive(0,{{"state","connected"},{"channel",channel},{"chats",QVariantList{QVariantMap{
            {"id",remote},{"kind","groups"},{"unread",1}}}}});
        c.toastChannel_=remote;QCOMPARE(c.notificationFace(),"communities");c.openNotification();
        QCOMPARE(commands.last()[0].toString(),"conversation");QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString(remote));
        QVERIFY(c.notificationFace().isEmpty());
        c.toastChannel_=remote;c.setOwner("different-owner");QVERIFY(c.notificationFace().isEmpty());
        FluxerSession s;bind(s);QString path;s.setTransport([&](auto,auto p,auto,auto,auto){path=p;});
        s.channels_[remote]={{"id",remote},{"type",3}};s.command("conversation",{{"id",remote}});
        QCOMPARE(s.channel_,QString(remote));QCOMPARE(s.face_,"communities");QVERIFY(path.contains(remote));
    }
    void singleMessageAnchorSurvivesPrependingHistory() {
        SocialController c;c.receive(0,{{"channel",channel},{"messages",QVariantList{QVariantMap{{"id","100"}}}}});
        c.dispatch(Action::Right);
        c.receive(0,{{"channel",channel},{"historyPast",true},{"messages",QVariantList{QVariantMap{{"id","99"}},QVariantMap{{"id","100"}},QVariantMap{{"id","101"}}}}});
        QCOMPARE(c.messageIndex(),1);
    }
    void directComposeSendAndTogetherKeepTheirOwnConsent() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        QSignalSpy text(&c,&SocialController::textRequested);
        QVariantMap snapshot{{"state","connected"},{"channel",channel},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"name","Friend"},{"kind","chats"},{"members",QVariantList{QVariantMap{{"id",remote}}}},{"friend",true}}}}};
        c.receive(0,snapshot);
        c.dispatch(Action::Confirm);QVERIFY(c.reading());QVERIFY(text.isEmpty());
        c.dispatch(Action::Secondary);QCOMPARE(text.size(),1);QCOMPARE(c.textSubmitLabel(),"Send");
        c.preserveText(QString::fromUtf8("Привет 😀"));QVERIFY2(commands.isEmpty(),qPrintable(commands.isEmpty()?QString():commands.first()[0].toString()));
        c.applyText(QString::fromUtf8("Привет 😀"));QCOMPARE(commands.size(),1);
        QCOMPARE(commands.last()[0].toString(),"send");
        QCOMPARE(commands.last()[1].toMap()["channel"].toString(),QString(channel));
        QCOMPARE(commands.last()[1].toMap()["text"].toString(),QString::fromUtf8("Привет 😀"));
        QVERIFY(c.draft().isEmpty());commands.clear();
        c.together();QVERIFY2(commands.isEmpty(),qPrintable(commands.isEmpty()?QString():commands.first()[0].toString()));QCOMPARE(c.menuTitle(),"Together");
        c.selectMenu(c.menuCommands_.indexOf("native-activities"));QCOMPARE(commands.size(),1);
        QCOMPARE(commands.last()[0].toString(),"online-probe");QCOMPARE(c.menuTitle(),"Activities");
        c.dispatch(Action::Back);QVERIFY(c.menu().isEmpty());QCOMPARE(commands.size(),1);
        // An ordinary contact must still have chat, without game invitations.
        snapshot["chats"]=QVariantList{QVariantMap{{"id",channel},{"name","Other"},{"kind","chats"}}};
        c.receive(0,snapshot);QVERIFY(!c.togetherAvailable());c.together();QCOMPARE(commands.size(),1);
        // A channel change while editing keeps the original draft, never sends it to the new person.
        c.compose();snapshot["channel"]="another";c.receive(0,snapshot);commands.clear();
        c.applyText("For the original friend");QVERIFY2(commands.isEmpty(),qPrintable(commands.isEmpty()?QString():commands.first()[0].toString()));
        QCOMPARE(c.drafts_.value(channel),"For the original friend");
        snapshot["channel"]=channel;c.receive(0,snapshot);c.compose();
        const auto emojiDraft=QString::fromUtf8("😀").repeated(1001);
        c.preserveText(emojiDraft);QCOMPARE(c.draft(),emojiDraft);
    }
    void sendCannotFollowAnAsynchronousChannelChange() {
        FluxerSession s;bind(s);int posts=0;
        s.setTransport([&](auto,auto,auto,auto,auto){++posts;});
        QSignalSpy failed(&s,&FluxerSession::sendFailed);
        s.command("send",{{"channel","1501314428688999999"},{"text","Original recipient"}});
        QCOMPARE(posts,0);QCOMPARE(failed.size(),1);
        QCOMPARE(failed.first()[1].toString(),"1501314428688999999");
        const auto longEmoji=QString::fromUtf8("😀").repeated(1001);
        s.command("send",{{"channel",channel},{"text",longEmoji}});
        QCOMPARE(posts,0);QCOMPARE(failed.size(),2);
        QCOMPARE(failed.last()[2].toString(),longEmoji);
    }
    void onlineUsesOnlyLiveFriendDmAndStopsQueuedDelivery() {
        FluxerSession s;bind(s);s.bindOnline();s.online_.setAvailable(true);
        s.channels_[channel]["recipients"]=QJsonArray{QJsonObject{{"id",remote}}};
        s.relationships_[remote]={{"type",1}};
        OnlineLink other;other.bind(remote,"11111111-1111-4111-8111-111111111111");other.setAvailable(true);
        QString probe;connect(&other,&OnlineLink::outgoing,this,[&](QString,QString text){probe=text;});other.probe(channel,s.self_);
        QJsonObject m{{"id","1501314428688998200"},{"channel_id",channel},{"type",0},{"author",QJsonObject{{"id",remote}}},{"content",probe}};
        int posts=0;Completion delivery;
        s.setTransport([&](auto method,QString path,QJsonObject body,Completion done,auto){
            if(method=="POST"&&path.endsWith("/messages")){QCOMPARE(body["flags"].toInt(),1<<12);++posts;delivery=done;}else done({200,QJsonDocument(QJsonArray{})});
        });
        s.mergeMessage(m);s.publish();QCOMPARE(posts,0);
        auto event=[&](QString type,QJsonObject value){s.gatewayEvent({{"op",0},{"t",type},{"d",value}});};
        event("MESSAGE_UPDATE",m);QCOMPARE(posts,0);
        auto bad=m;bad["author"]=QJsonObject{{"id","999"}};event("MESSAGE_CREATE",bad);QCOMPARE(posts,0);
        bad=m;bad["webhook_id"]="900";event("MESSAGE_CREATE",bad);QCOMPARE(posts,0);
        event("MESSAGE_CREATE",m);QCOMPARE(posts,1);QCOMPARE(s.online_.state()["stage"].toString(),"offered");
        s.onlineQueue_.append({channel,probe});s.online_.close();QVERIFY(s.onlineQueue_.isEmpty());
        delivery({429,{},10});QVERIFY(s.onlineQueue_.isEmpty());QVERIFY(!s.onlineSendTimer_.isActive());QCOMPARE(posts,1);
        s.relationships_[remote]={{"type",2}};s.command("online-probe",{{"channel",channel}});QCOMPARE(posts,1);
    }
    void communityManifestIsBoundToOwnerGuildAndVersion() {
        QJsonObject m{{"id","199"},{"channel_id",channel},{"type",0},{"author",QJsonObject{{"id",remote}}},
            {"content",communityIdentity::content("200")}};
        QVERIFY(communityIdentity::matches(m,"200",remote,channel));
        QVERIFY(!communityIdentity::matches(m,"201",remote,channel));
        QVERIFY(!communityIdentity::matches(m,"200","202",channel));
        QVERIFY(!communityIdentity::matches(m,"200",remote,"203"));
        auto changed=m;changed["content"]=changed["content"].toString().replace("\"version\":1","\"version\":2");
        QVERIFY(!communityIdentity::matches(changed,"200",remote,channel));
        changed=m;changed["webhook_id"]="204";QVERIFY(!communityIdentity::matches(changed,"200",remote,channel));
        changed=m;changed["content"]="TrainerOS compatible";QVERIFY(!communityIdentity::matches(changed,"200",remote,channel));
    }
    void communityCreationRetainsPartialSuccessAndReusesMessage() {
        FluxerSession s;bind(s);QStringList calls;int creates=0,posts=0,pins=0;bool allowPin=false;
        const QString guild="200";QJsonObject marker;
        const QJsonObject g{{"id",guild},{"name","Our place"},{"owner_id",s.self_},{"system_channel_id",channel}};
        s.setTransport([&](QByteArray method,QString path,QJsonObject body,Completion done,QByteArray){
            calls<<path;
            if(path=="/v1/guilds") {++creates;QCOMPARE(body["name"].toString(),QString("Our place"));
                QCOMPARE(body["template"].toObject()["channels"].toArray().size(),1);done({200,QJsonDocument(g)});}
            else if(path=="/v1/guilds/200")done({200,QJsonDocument(g)});
            else if(path.endsWith("messages?limit=50"))done({200,QJsonDocument(marker.isEmpty()?QJsonArray{}:QJsonArray{marker})});
            else if(method=="POST"&&path.endsWith("/messages")) {++posts;marker={{"id","199"},{"channel_id",channel},{"type",0},{"author",QJsonObject{{"id",s.self_}}},{"content",body["content"]}};done({200,QJsonDocument(marker)});}
            else if(method=="PUT"){++pins;done({allowPin?204:403,{}});}
            else QFAIL("Unexpected community request");
        });
        s.command("create-community",{{"text"," Our place "}});
        QCOMPARE(creates,1);QCOMPARE(posts,1);QCOMPARE(pins,1);QVERIFY(s.guilds_.contains(guild));
        QVERIFY(!s.mutationBusy_);QVERIFY(s.communityStatus_.contains("incomplete"));QVERIFY(!s.communityMarked_.contains(guild));
        allowPin=true;s.command("mark-community",{{"id",guild}});
        QCOMPARE(creates,1);QCOMPARE(posts,1);QCOMPARE(pins,2);QVERIFY(s.communityStatus_.isEmpty());
        // Even a successful write is not a cached positive: read the actual pin.
        QVERIFY(!s.communityMarked_.contains(guild));
    }
    void communityDiscoveryRejectsRevokedAndStalePins() {
        FluxerSession s;bind(s);s.face_="communities";const QString guild="200";
        s.guilds_[guild]={{"id",guild},{"name","TrainerOS"}};
        Completion pins;
        const QJsonObject g{{"id",guild},{"owner_id",remote},{"system_channel_id",channel},
            {"channels",QJsonArray{QJsonObject{{"id",channel},{"type",0}}}}};
        s.setTransport([&](auto,QString path,auto,Completion done,auto){
            if(path=="/v1/guilds/200")done({200,QJsonDocument(g)});else pins=done;
        });
        const QJsonObject m{{"id","199"},{"channel_id",channel},{"type",0},{"pinned",true},
            {"author",QJsonObject{{"id",remote}}},{"content",communityIdentity::content(guild)}};
        const Reply valid{200,QJsonDocument(QJsonObject{{"items",QJsonArray{QJsonObject{{"message",m}}}}})};
        s.checkCommunities();QVERIFY(bool(pins));pins(valid);QVERIFY(s.communityMarked_.contains(guild));
        s.invalidateCommunity(guild);QVERIFY(!s.communityMarked_.contains(guild));s.checkCommunities();
        s.invalidateCommunity(guild);pins(valid);QVERIFY(!s.communityMarked_.contains(guild));
        s.checkCommunities();pins({403,{}});QVERIFY(!s.communityMarked_.contains(guild));
        s.invalidateCommunity(guild);s.checkCommunities();const auto old=pins;
        s.setOwner("other",2);old(valid);QVERIFY(s.communityMarked_.isEmpty());
    }
    void communityCreateIsNotReplayedOnUnknownDeliveryOrOwnerSwitch() {
        FluxerSession s;bind(s);Completion reply;int count=0;
        s.setTransport([&](auto,auto,auto,Completion done,auto){++count;reply=done;});
        s.command("create-community",{{"text","Our place"}});s.command("create-community",{{"text","Our place"}});QCOMPARE(count,1);
        reply({0,{}});QCOMPARE(count,1);QVERIFY(s.guilds_.isEmpty());QVERIFY(!s.mutationBusy_);
        s.command("create-community",{{"text","Our place"}});s.setOwner("other",2);
        reply({200,QJsonDocument(QJsonObject{{"id","200"}})});QVERIFY(s.guilds_.isEmpty());
    }
    void communityControllerUsesOneKeyboardAndNativeTagFilter() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested),text(&c,&SocialController::textRequested);
        c.receive(0,{{"state","connected"}});c.setFace("communities");c.dispatch(Action::Secondary);
        QCOMPARE(text.size(),1);QVERIFY(c.menu().isEmpty());c.applyText("Our place");
        QCOMPARE(commands.last()[0].toString(),QString("create-community"));
        c.dispatch(Action::LocalAction);QVERIFY(!c.menuCommands_.contains("community-filter"));QVERIFY(c.menuCommands_.contains("create-group"));
        FluxerSession s;bind(s);QString path;s.setTransport([&](auto,QString p,auto,Completion done,auto){path=p;done({200,QJsonDocument(QJsonObject{{"guilds",QJsonArray{}},{"total",0}})});});
        s.search("traineros","");QVERIFY(path.contains("tag=traineros-v1"));QVERIFY(!path.contains("query="));
        QVERIFY(s.searchStatus_.contains("No public TrainerOS communities"));
        s.search("communities","");QVERIFY(path.contains("tag=traineros-v1"));
    }
    void nativeChallengeUsesFreshHeaderAndNeverReplaysUncertainMutation() {
        FluxerSession s;QList<Completion> replies;QList<QByteArray> headers;bind(s);
        s.setTransport([&](auto,auto,auto,Completion done,QByteArray token){replies<<done;headers<<token;});
        s.command("create-group",{{"recipients",QStringList{remote}}});
        const QJsonObject challenge{{"parameters",QJsonObject{{"algorithm","PBKDF2/SHA-256"},{"nonce","aabb"},
            {"salt","ccdd"},{"cost",3},{"keyLength",32},{"keyPrefix","280"}}},{"signature","opaque-test-signature"}};
        const Reply rejection{400,QJsonDocument(QJsonObject{{"code","CAPTCHA_REQUIRED"},{"captcha_provider","altcha"},{"altcha_challenge",challenge}})};
        replies.takeFirst()(rejection);QTRY_COMPARE(replies.size(),1);
        QVERIFY(headers.first().isEmpty());
        const auto token=QJsonDocument::fromJson(QByteArray::fromBase64(headers.last())).object();
        QCOMPARE(token["challenge"].toObject(),challenge);QCOMPARE(token["solution"].toObject()["counter"].toInt(),0);
        QCOMPARE(token["solution"].toObject()["derivedKey"].toString(),QString("2800796f5ab30c9490225157a404d97bb89cb5331abdeaa408a3b47c3789a224"));
        replies.takeFirst()({0,{}});QTest::qWait(30);QCOMPARE(headers.size(),2);QVERIFY(!s.mutationBusy_);
        s.command("create-group",{{"recipients",QStringList{remote}}});replies.takeFirst()(rejection);
        s.setOwner("different-trainer",2);QTest::qWait(30);QCOMPARE(headers.size(),3);QVERIFY(!s.proof_);
    }
    void backgroundHistoryDoesNotReadAndGatewayAckWinsOverOlderResponse() {
        FluxerSession s;QList<Completion> replies;QStringList paths;
        s.setTransport([&](auto,auto path,auto,Completion done,QByteArray){paths<<path;replies<<done;});bind(s);
        s.readsReady_=true;s.channels_[channel]["last_message_id"]="105";s.updateUnread(channel);
        s.loadMessages(channel);replies.takeFirst()({200,QJsonDocument(QJsonArray{QJsonObject{{"id","105"},{"channel_id",channel}}})});
        QCOMPARE(s.unread_.value(channel),1);
        s.command("read",{{"channel",channel},{"message","999"}});QCOMPARE(paths.size(),1);
        s.command("read",{{"channel",channel},{"message","105"}});QCOMPARE(paths.last(),QString("/v1/read-states/ack"));
        s.gatewayEvent({{"op",0},{"t","MESSAGE_ACK"},{"d",QJsonObject{{"channel_id",channel},{"message_id","100"},{"manual",true}}}});
        replies.takeFirst()({200,QJsonDocument(QJsonObject{{"read_states",QJsonArray{QJsonObject{{"id",channel},{"last_message_id","105"}}}}})});
        QCOMPARE(s.readThrough_.value(channel),QString("100"));QCOMPARE(s.unread_.value(channel),1);
        s.gatewayEvent({{"op",0},{"t","MESSAGE_ACK"},{"d",QJsonObject{{"channel_id",channel},{"message_id","105"}}}});
        QCOMPARE(s.unread_.value(channel),0);
    }
    void notificationPreferencesSuppressWithoutDiscardingUnread() {
        FluxerSession s;s.setTransport([](auto,auto,auto,auto,auto){});bind(s);s.readsReady_=true;
        QSignalSpy notices(&s,&FluxerSession::incomingMessage);
        auto incoming=[&](QString id,QString who=remote){s.gatewayEvent({{"op",0},{"t","MESSAGE_CREATE"},{"d",QJsonObject{
            {"id",id},{"channel_id",channel},{"content","Private text"},{"author",QJsonObject{{"id",who},{"username","Friend"}}}}}});};
        incoming("101");QCOMPARE(notices.size(),1);QCOMPARE(notices.last()[3].toString(),QString("Open Social to catch up"));
        incoming("101");QCOMPARE(notices.size(),1);
        s.command("mute",{{"channel",channel}});incoming("102");QCOMPARE(notices.size(),1);QCOMPARE(s.unread_.value(channel),1);
        s.command("mute",{{"channel",channel}});s.command("dnd");incoming("103");QCOMPARE(notices.size(),1);
        s.command("dnd");s.command("private");incoming("104");QCOMPARE(notices.size(),2);QCOMPARE(notices.last()[3].toString(),QString("Private text"));
        incoming("105",s.self_);QCOMPARE(notices.size(),2);
        s.relationships_[remote]={{"type",2}};incoming("106");QCOMPARE(notices.size(),2);
    }
    void presentationAcknowledgesOnlyVisibleConversationAndClearsCoveredToast() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.receive(0,{{"channel",channel},{"messages",QVariantList{QVariantMap{{"id","105"}}}}});
        c.presented(channel,"105");QVERIFY(commands.isEmpty());
        c.setSurfaceAvailable(true);c.setConversationVisible(true);c.presented(channel,"105");QCOMPARE(commands.size(),1);
        c.setConversationVisible(false);c.presented(channel,"105");QCOMPARE(commands.size(),1);
        c.toastTitle_="Private";c.setSurfaceAvailable(false);QVERIFY(c.toastTitle().isEmpty());
    }
    void messageMutationsPreserveAttachmentsAndRespectOwnership() {
        FluxerSession s;Completion done;QByteArray method;QJsonObject body;int calls=0;
        s.setTransport([&](auto m,auto,auto b,Completion cb, QByteArray){++calls;method=m;body=b;done=cb;});bind(s);
        const QString id="1501314428688998190";
        QJsonObject message{{"id",id},{"channel_id",channel},{"type",0},{"content","before"},{"author",QJsonObject{{"id",remote}}}};
        s.mergeMessage(message);s.command("edit-message",{{"channel",channel},{"id",id},{"text","after"}});QCOMPARE(calls,0);
        message["author"]=QJsonObject{{"id",s.self_}};s.mergeMessage(message);
        s.command("edit-message",{{"channel",channel},{"id",id},{"text","after"}});
        QCOMPARE(method,QByteArray("PATCH"));QCOMPARE(body,(QJsonObject{{"content","after"}}));
        s.command("delete-message",{{"channel",channel},{"id",id}});QCOMPARE(calls,1);
        s.gatewayEvent({{"op",0},{"s",1},{"t","MESSAGE_DELETE"},{"d",QJsonObject{{"id",id},{"channel_id",channel}}}});
        message["content"]="after";done({200,QJsonDocument(message)});QVERIFY(!s.messages_.contains(id));
        s.mergeMessage(message);s.command("delete-message",{{"channel",channel},{"id",id}});
        QCOMPARE(method,QByteArray("DELETE"));done({204,{}});QVERIFY(!s.messages_.contains(id));
    }
    void organizerLeavesOnlyAfterProviderTransfersToTheChosenPlayer() {
        FluxerSession s;bind(s);const QString group="1501314428688998199";Completion done;QStringList methods;QJsonObject body;
        s.channels_[group]={{"id",group},{"type",3},{"owner_id",s.self_},{"recipients",QJsonArray{QJsonObject{{"id",remote}}}}};
        s.setTransport([&](auto method,auto,auto data,Completion cb,QByteArray){methods.append(QString::fromUtf8(method));body=data;done=cb;});
        s.command("leave-group",{{"channel",group},{"successor",remote}});QCOMPARE(methods,QStringList{"PATCH"});QCOMPARE(body["owner_id"].toString(),QString(remote));
        auto finish=done;finish({403,{}});QCOMPARE(methods.size(),1);QVERIFY(s.channels_.contains(group));
        s.command("leave-group",{{"channel",group},{"successor",remote}});auto updated=s.channels_[group];updated["owner_id"]=remote;
        finish=done;finish({200,QJsonDocument(updated)});QCOMPARE(methods.last(),QString("DELETE"));QVERIFY(s.channels_.contains(group));
        finish=done;finish({204,{}});QVERIFY(!s.channels_.contains(group));
    }
    void leaveWithOnlyViewersClosesInsteadOfTransferringToThem() {
        SocialController s;QObject::disconnect(&s,nullptr,s.session_,nullptr);QObject::disconnect(s.session_,nullptr,&s,nullptr);
        s.receive(0,{{"state","connected"},{"userId","1"},{"name","Mika"}});s.createAnotherLobby();s.submitLobbyDraft();
        LobbyCoordinator viewer;viewer.configure("2","Ren",true);
        connect(&s.lobby_,&LobbyCoordinator::outgoing,&viewer,[&](QString peer,QJsonObject p){if(peer=="2")viewer.receive("1","Mika",p);});
        connect(&viewer,&LobbyCoordinator::outgoing,&s,[&](QString,QJsonObject p){s.lobby_.receive("2","Ren",p);});
        s.lobby_.setAccess("public");const auto room=s.lobbyState()["id"].toString();QVERIFY(viewer.join("1",room,false,true));
        QCOMPARE(s.lobbySpectators().size(),1);s.lobby_.setConversation("300");s.lobbyChatPending_.clear();
        QSignalSpy commands(&s,&SocialController::commandRequested);s.leaveViewedLobby();
        QVERIFY(s.lobbyCloseOnLeave_.contains(room));QVERIFY(commands.last()[1].toMap()["successor"].toString().isEmpty());
        QVERIFY(!s.myLobbies().isEmpty()); // Wait for provider success before closing.
        s.lobby_.leave();QVERIFY(viewer.state().isEmpty());
    }
    void groupCreationAndMembershipUseProviderContract() {
        FluxerSession s;Completion done;QByteArray method;QString path;QJsonObject body;int calls=0;
        s.setTransport([&](auto m,auto p,auto b,Completion cb, QByteArray){if(m=="GET"){cb({200,QJsonDocument(QJsonArray{})});return;}++calls;method=m;path=p;body=b;done=cb;});bind(s);s.face_="communities";
        s.command("create-group",{{"recipients",QStringList{remote,remote}}});QCOMPARE(calls,0);
        s.command("create-group",{{"recipients",QStringList{remote}}});QCOMPARE(calls,1);
        QCOMPARE(method,QByteArray("POST"));QCOMPARE(path,QString("/v1/users/@me/channels"));
        QCOMPARE(body,(QJsonObject{{"recipients",QJsonArray{remote}}}));
        const QString group="1501314428688998199";
        QJsonObject groupData{{"id",group},{"type",3},{"owner_id",remote},{"name","Friends"}};
        done({200,QJsonDocument(groupData)});QVERIFY(s.channels_.contains(group));QCOMPARE(s.channel_,group);
        s.command("remove-member",{{"channel",group},{"id",remote}});QCOMPARE(calls,1);
        s.command("rename-group",{{"channel",group},{"text","Play together"}});QCOMPARE(calls,2);
        QCOMPARE(method,QByteArray("PATCH"));QCOMPARE(body,(QJsonObject{{"name","Play together"}}));
        groupData["name"]="Play together";done({200,QJsonDocument(groupData)});
        s.command("leave-group",{{"channel",group}});QCOMPARE(method,QByteArray("DELETE"));
        QCOMPARE(path,"/v1/channels/"+group);QVERIFY(body.isEmpty());done({204,{}});QVERIFY(!s.channels_.contains(group));
    }
    void deniedGroupCreationNeverInventsConversation() {
        FluxerSession s;s.setTransport([](auto,auto,auto,Completion done, QByteArray){done({403,QJsonDocument(QJsonObject{{"code","CAPTCHA_REQUIRED"}})});});bind(s);
        s.command("create-group",{{"recipients",QStringList{remote}}});
        QCOMPARE(s.channels_.size(),1);QVERIFY(!s.mutationBusy_);QVERIFY(s.status_.contains("verification"));
    }
    void staleChannelRefreshRefetchesInsteadOfErasingNewGroup() {
        FluxerSession s;QList<Completion> replies;s.setTransport([&](auto,auto,auto,Completion done, QByteArray){replies<<done;});bind(s);
        s.refreshChannels();++s.channelRevision_;replies.takeFirst()({200,QJsonDocument(QJsonArray{})});
        QCOMPARE(s.channels_.size(),1);QCOMPARE(replies.size(),1);
        replies.takeFirst()({200,QJsonDocument(QJsonArray{QJsonObject{{"id",channel},{"type",1}}})});QVERIFY(s.channels_.contains(channel));
    }
    void messageDeleteRequiresConfirmationAndEditKeepsCapturedIdentity() {
        SocialController c;c.receive(0,{{"state","connected"},{"channel",channel},{"messages",QVariantList{QVariantMap{{"id","91"},{"text","hello"},{"editable",true}}}}});
        QSignalSpy commands(&c,&SocialController::commandRequested);QSignalSpy text(&c,&SocialController::textRequested);
        c.dispatch(Action::Right);c.dispatch(Action::Confirm);QCOMPARE(c.menu().size(),2);
        c.selectMenu(1);QCOMPARE(c.menuTitle(),QString("Delete this message?"));c.selectMenu(0);QVERIFY(commands.isEmpty());
        c.dispatch(Action::Confirm);c.selectMenu(0);QCOMPARE(text.size(),1);c.preserveText("edited draft");QVERIFY(commands.isEmpty());
        c.applyText("edited");QCOMPARE(commands.last()[0].toString(),QString("edit-message"));
        QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString("91"));QCOMPARE(commands.last()[1].toMap()["channel"].toString(),QString(channel));
    }
    void groupPickerRequiresExplicitCreateAfterSelection() {
        SocialController c;c.face_="groups";c.receive(0,{{"state","connected"},{"userId","self"},{"friends",QVariantList{QVariantMap{{"id",remote},{"name","Friend"},{"type",1}}}}});
        QSignalSpy commands(&c,&SocialController::commandRequested);
        c.dispatch(Action::Secondary);c.dispatch(Action::Confirm);QVERIFY(commands.isEmpty());
        c.dispatch(Action::ToggleContinue);QCOMPARE(commands.size(),1);QCOMPARE(commands.first()[0].toString(),QString("create-group"));
        QCOMPARE(commands.first()[1].toMap()["recipients"].toStringList(),QStringList{remote});QVERIFY(c.menu().isEmpty());
    }
    void credentialPathsCannotEscapeTheirStore() {
        QVERIFY(EncryptedCredentials::path("../token").isEmpty());
        QVERIFY(EncryptedCredentials::path(QString(64,'a')).endsWith("/credentials/fluxer-"+QString(64,'a')+".cred"));
    }
    void draftsSurviveReopenButNotAccountChangeOrLogout() {
        const auto owner="draft-test-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
        QString firstFile;
        {
            SocialController c;c.owner_=owner;
            c.receive(0,{{"state","connected"},{"userId",remote},{"channel",channel}});
            c.compose();c.preserveText("Unsent hello");c.saveDrafts();firstFile=c.draftFile_;
            QVERIFY(QFile::exists(firstFile));
        }
        {
            SocialController c;c.owner_=owner;
            c.receive(0,{{"state","connected"},{"userId",remote},{"channel",channel}});
            QCOMPARE(c.draft(),QString("Unsent hello"));
            c.receive(0,{{"state","connected"},{"userId","2"},{"channel",channel}});
            QVERIFY(c.draft().isEmpty());QVERIFY(c.draftFile_!=firstFile);
            c.receive(0,{{"state","signed-out"}});QVERIFY(c.draftFile_.isEmpty());
            c.receive(0,{{"state","connected"},{"userId",remote},{"channel",channel}});
            QCOMPARE(c.draft(),QString("Unsent hello"));
            c.receive(0,{{"state","signed-out"}});QVERIFY(!QFile::exists(firstFile));
        }
    }
    void olderHistoryMovesABoundedWindowAndKeepsNewEventsOut() {
        FluxerSession s;Completion reply;QString path;
        s.setTransport([&](auto,auto requestPath,auto,Completion done, QByteArray){path=requestPath;reply=done;});bind(s);
        for(int id=100;id<200;++id)s.mergeMessage({{"id",QString::number(id)},{"channel_id",channel},{"content","recent"}});
        s.historyMore_=true;s.loadOlderMessages();QVERIFY(path.endsWith("&before=100"));QVERIFY(s.historyBusy_);
        QJsonArray older;for(int id=59;id>=10;--id)older.append(QJsonObject{{"id",QString::number(id)},{"channel_id",channel},{"content","earlier"}});
        reply({200,QJsonDocument(older)});
        QCOMPARE(s.messageOrder_.size(),100);QCOMPARE(s.messageOrder_.first(),QString("10"));QCOMPARE(s.messageOrder_.last(),QString("149"));
        QVERIFY(s.historyPast_);QVERIFY(!s.historyBusy_);
        s.gatewayEvent({{"op",0},{"s",1},{"t","MESSAGE_CREATE"},{"d",QJsonObject{{"id","201"},{"channel_id",channel},{"content","new"}}}});
        QVERIFY(!s.messages_.contains("201"));QCOMPARE(s.messageOrder_.first(),QString("10"));
        s.command("latest");QVERIFY(!path.contains("before="));
        reply({200,QJsonDocument(QJsonArray{QJsonObject{{"id","201"},{"channel_id",channel},{"content","new"}}})});
        QVERIFY(!s.historyPast_);QCOMPARE(s.messageOrder_,QStringList{"201"});
    }
    void earlierHistoryRetainsControllerReadingPosition() {
        SocialController c;
        c.receive(0,{{"state","connected"},{"channel",channel},{"historyMore",true},{"messages",QVariantList{QVariantMap{{"id","100"},{"text","old"}},QVariantMap{{"id","101"},{"text","new"}}}}});
        c.dispatch(Action::Right);c.dispatch(Action::Up);QCOMPARE(c.messageIndex(),0);
        QSignalSpy commands(&c,&SocialController::commandRequested);c.dispatch(Action::Up);
        QCOMPARE(c.conversationControl(),0);c.dispatch(Action::Up); // Header is reachable; another Up loads older history.
        QCOMPARE(commands.last().first().toString(),QString("older"));
        c.receive(0,{{"state","connected"},{"channel",channel},{"historyPast",true},{"messages",QVariantList{QVariantMap{{"id","99"}},QVariantMap{{"id","100"}},QVariantMap{{"id","101"}}}}});
        QVERIFY(c.reading());QCOMPARE(c.messageIndex(),1);
        c.receive(0,{{"state","connected"},{"channel",channel},{"historyPast",false},{"messages",QVariantList{QVariantMap{{"id","200"}},QVariantMap{{"id","201"}},QVariantMap{{"id","202"}}}}});
        QCOMPARE(c.messageIndex(),2);
    }
    void pagingDoesNotForgetAnUncertainSend() {
        FluxerSession s;Completion reply;
        s.setTransport([&](auto,auto,auto,Completion done, QByteArray){reply=done;});bind(s);
        for(int id=100;id<199;++id)s.mergeMessage({{"id",QString::number(id)},{"channel_id",channel}});
        const QString nonce(32,'a');s.pendingNonces_.insert(nonce,channel);
        s.mergeMessage({{"id",nonce},{"channel_id",channel},{"content","hello"},{"local_delivery","unknown"}});
        s.historyMore_=true;s.loadOlderMessages();
        reply({200,QJsonDocument(QJsonArray{QJsonObject{{"id","99"},{"channel_id",channel}}})});
        QVERIFY(!s.messageOrder_.contains(nonce));QVERIFY(s.messages_.contains(nonce));
        s.gatewayEvent({{"op",0},{"s",1},{"t","MESSAGE_CREATE"},{"d",QJsonObject{{"id","200"},{"channel_id",channel},{"nonce",nonce},{"content","hello"}}}});
        QVERIFY(s.pendingNonces_.isEmpty());QVERIFY(!s.messages_.contains(nonce));QVERIFY(s.messages_.contains("200"));
    }
    void repeatedActionsRespectServerRetryDelay() {
        FluxerSession s;int requests=0;
        s.setTransport([&](auto,auto,auto,Completion done, QByteArray){++requests;done({429,{},60});});bind(s);
        s.request("GET","/test",{},[](Reply){});
        const auto deadline=s.blockedUntil_;
        s.request("GET","/test",{},[](Reply){});
        QCOMPARE(requests,1);QCOMPARE(s.blockedUntil_,deadline);
    }
    void ownerChangeDiscardsLateResponse() {
        FluxerSession s;Completion deferred;
        s.setTransport([&](auto,auto,auto,Completion done, QByteArray){deferred=std::move(done);});bind(s);
        s.loadMessages(channel);s.setOwner("trainer-b",2);
        deferred({200,QJsonDocument(QJsonArray{QJsonObject{{"id","99"},{"channel_id",channel},{"content","private old owner text"}}})});
        QVERIFY(s.messages_.isEmpty());QVERIFY(s.token_.isEmpty());QCOMPARE(s.generation_,quint64(2));
    }
    void canceledLoginNeverInstallsLateToken() {
        FluxerSession s;Completion deferred;
        s.setTransport([&](auto,auto,auto,Completion done, QByteArray){deferred=std::move(done);});s.setOwner("a",1);
        s.code_="test-code";s.pollSecret_="test-poll-secret";s.expires_=QDateTime::currentMSecsSinceEpoch()+10000;
        s.pollLogin();auto late=deferred;s.command("cancel-login");
        late({200,QJsonDocument(QJsonObject{{"status","completed"},{"token","must-not-install"}})});
        QVERIFY(s.token_.isEmpty());QVERIFY(s.code_.isEmpty());QCOMPARE(s.state_,QString("signed-out"));
    }
    void uncertainSendHasOneRequestAndNoAutomaticRetry() {
        FluxerSession s;int requests=0;QString nonce;
        s.setTransport([&](auto method,auto path,auto body,Completion done, QByteArray){
            QCOMPARE(method,QByteArray("POST"));QVERIFY(path.endsWith("/messages"));
            nonce=body["nonce"].toString();++requests;done({0,{}});
        });bind(s);s.command("send",{{"text","Hello :)"}});
        QCOMPARE(requests,1);QCOMPARE(nonce.size(),32);QCOMPARE(s.messageOrder_.size(),1);
        QVERIFY(s.messages_[nonce]["local_delivery"].toString().contains("unknown"));
        s.mergeMessage({{"id","1501314428688998190"},{"channel_id",channel},{"nonce",nonce},{"content","Hello :)"}});
        QCOMPARE(s.messageOrder_.size(),1);QVERIFY(!s.messages_.contains(nonce));QVERIFY(s.pendingNonces_.isEmpty());
    }
    void historyCannotResurrectDeletedMessage() {
        FluxerSession s;Completion deferred;
        s.setTransport([&](auto,auto,auto,Completion done, QByteArray){deferred=std::move(done);});bind(s);
        s.loadMessages(channel);
        s.gatewayEvent({{"op",0},{"s",3},{"t","MESSAGE_DELETE"},{"d",QJsonObject{{"id","1501314428688998190"},{"channel_id",channel}}}});
        deferred({200,QJsonDocument(QJsonArray{QJsonObject{{"id","1501314428688998190"},{"channel_id",channel},{"content","deleted"}}})});
        QVERIFY(s.messages_.isEmpty());
    }
    void logoutRevokesBeforeClearingSession() {
        FluxerSession s;int status=0;
        s.setTransport([&](auto,auto path,auto,Completion done, QByteArray){QCOMPARE(path,QString("/v1/auth/logout"));done({status,{}});});bind(s);
        s.command("logout");QVERIFY(!s.token_.isEmpty());status=204;s.command("logout");
        QVERIFY(s.token_.isEmpty());QVERIFY(s.channels_.isEmpty());QCOMPARE(s.state_,QString("signed-out"));
    }
    void messagesAreBoundedAndIdsStayStrings() {
        FluxerSession s;s.setTransport([](auto,auto,auto,auto,auto){});bind(s);
        for(quint64 id=1501314428688998000ULL;id<1501314428688998200ULL;++id)
            s.mergeMessage({{"id",QString::number(id)},{"channel_id",channel},{"content","hello"},{"author",QJsonObject{{"id",remote}}}});
        QCOMPARE(s.messages_.size(),100);QCOMPARE(s.messageOrder_.last(),QString("1501314428688998199"));
    }
    void presentationPreservesDraftsAndFiltersFaces() {
        SocialController c; // Synthetic snapshots only; no owner means no network/credential access.
        c.receive(0,{{"state","connected"},{"channel",channel},{"friends",QVariantList{QVariantMap{{"id",remote},{"name","Friend"},{"type",1}}}},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"name","Friend"},{"kind","chats"}},QVariantMap{{"id","3"},{"name","Group"},{"kind","groups"}}}}});
        QCOMPARE(c.rows().size(),1);c.compose();c.preserveText("Draft <b>plain text</b>");
        c.setFace("groups");QCOMPARE(c.rows().size(),1);QCOMPARE(c.face_,QString("communities"));QCOMPARE(c.draft(),QString("Draft <b>plain text</b>"));
        c.receive(99,{{"channel","wrong-owner"}});QCOMPARE(c.draft(),QString("Draft <b>plain text</b>"));
        c.setOwner("other");QVERIFY(c.draft().isEmpty());QVERIFY(c.rows().isEmpty());
    }
    void searchUsesDiscoveryAndDoesNotFilterFriends() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.receive(0,{{"state","connected"},{"friends",QVariantList{QVariantMap{{"id",remote},{"name","Odin"},{"type",1}}}}});
        c.setFace("friends");c.editSearch();c.applyText("new community");
        QCOMPARE(commands.last().first().toString(),QString("search"));
        QCOMPARE(commands.last()[1].toMap()["text"].toString(),QString("new community"));
        QVERIFY(c.rows().isEmpty());
        c.showContacts();QCOMPARE(c.rows().size(),1);QVERIFY(!c.contacts());
    }
    void backNeverClosesTheSelectedConversation() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.receive(0,{{"state","connected"},{"channel",channel}});
        c.dispatch(Action::Right);QVERIFY(c.reading());c.dispatch(Action::Back);
        QVERIFY(!c.reading());QVERIFY(c.conversation());QVERIFY2(commands.isEmpty(),qPrintable(commands.isEmpty()?QString():commands.first()[0].toString()));
    }
    void automaticConversationPrefersLastChoiceThenRecent() {
        FluxerSession s;QStringList requests;s.setTransport([&](auto,auto path,auto,Completion done, QByteArray){requests<<path;done({200,QJsonDocument(QJsonArray{})});});bind(s);
        s.channelsLoaded_=true;s.channels_[channel]["last_message_id"]="100";
        s.channels_[remote]={{"id",remote},{"type",1},{"last_message_id","200"}};
        s.channel_.clear();s.ensureConversation();QCOMPARE(s.channel_,QString(remote));
        s.openConversation(channel);s.command("face",{{"face","groups"}});QVERIFY(s.channel_.isEmpty());
        s.command("face",{{"face","chats"}});QCOMPARE(s.channel_,QString(channel));
        auto count=requests.size();s.ensureConversation();QCOMPARE(requests.size(),count);
        s.setOwner("b",2);QVERIFY(s.preferred_.isEmpty());
    }
    void discoveryIgnoresLateQueryAndUsesProviderPaging() {
        FluxerSession s;QList<Completion> responses;QStringList paths;
        s.setTransport([&](auto method,auto path,auto,Completion done, QByteArray){QCOMPARE(method,QByteArray("GET"));paths<<path;responses<<done;});bind(s);
        s.search("communities","old");s.search("communities","new & fun",24);
        QVERIFY(paths.last().contains("offset=24"));QVERIFY(paths.last().contains("%26"));
        QVERIFY(paths.last().contains("tag=traineros-v1"));QVERIFY(paths.last().contains("sort_by=member_count"));
        responses[0]({200,QJsonDocument(QJsonObject{{"total",1},{"guilds",QJsonArray{QJsonObject{{"id",remote},{"name","Old"}}}}})});
        QVERIFY(s.searchResults_.isEmpty());QVERIFY(s.searching_);
        responses[1]({200,QJsonDocument(QJsonObject{{"total",30},{"guilds",QJsonArray{QJsonObject{{"id",remote},{"name","New"},{"member_count",12},{"custom_tags",QJsonArray{"traineros-v1"}}},QJsonObject{{"id","123"},{"name","Unmarked"}}}}})});
        QCOMPARE(s.searchResults_.size(),1);QCOMPARE(s.searchResults_[0].toMap()["name"].toString(),QString("New"));QCOMPARE(s.searchTotal_,30);
        s.search("communities","");QVERIFY(!paths.last().contains("query="));
    }
    void inviteLookupNeverJoinsUntilExplicitAction() {
        FluxerSession s;QStringList methods,paths;
        s.setTransport([&](auto method,auto path,auto,Completion done, QByteArray){methods<<method;paths<<path;if(method=="GET"&&path.startsWith("/v1/invites/"))done({200,QJsonDocument(QJsonObject{{"code","aB1"},{"channel",QJsonObject{{"name","Our group"}}}})});});bind(s);
        s.search("invite","https://fluxer.gg/aB1");QCOMPARE(methods,QStringList{"GET"});QCOMPARE(paths.first(),QString("/v1/invites/aB1"));
        s.command("search-action",{{"id","aB1"}});QCOMPARE(methods.last(),QString("POST"));
    }
    void lateMembershipActionDoesNotReplaceNewSearch() {
        FluxerSession s;Completion join;
        s.setTransport([&](auto method,auto path,auto,Completion done, QByteArray){
            if(method=="POST"&&path=="/v1/invites/aB1")join=done;
        });bind(s);
        s.searchResults_.append(QVariantMap{{"id","aB1"},{"kind","invite"},{"action","Join"}});
        s.command("search-action",{{"id","aB1"}});
        s.search("communities","new search");QVERIFY(s.searching_);
        join({204,{}});QVERIFY(s.searching_);QCOMPARE(s.searchStatus_,QString::fromUtf8("Looking around…"));
        QVERIFY(s.searchResults_.isEmpty());
    }

};
}
QTEST_GUILESS_MAIN(trainer::SocialTests)
#include "SocialTests.moc"
