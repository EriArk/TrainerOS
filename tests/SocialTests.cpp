#include <QtTest>
#include "integrations/social/AdventureReviews.h"
#include "core/model/AdventureCompletion.h"
#include "core/navigation/ShellController.h"
#include "integrations/adventure/mock/MockAdventureAdapter.h"
#include <QSettings>
#include "integrations/social/FluxerSession.h"
#include "integrations/social/CommunityIdentity.h"
#include "features/social/SocialController.h"
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include "platform/storage/EncryptedCredentials.h"

namespace trainer {
class SocialTests : public QObject {
    Q_OBJECT
    using Reply=FluxerSession::Reply;
    using Completion=FluxerSession::Completion;
    static constexpr auto channel="1501314428688998182";
    static constexpr auto remote="1501314428688998183";
    static void bind(FluxerSession& s) {
        s.setOwner("trainer-a",1);s.self_="1501314428688998181";s.token_="synthetic-test-session";s.state_="connected";
        s.channel_=channel;s.channels_[channel]={{"id",channel},{"type",1}};
    }
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true);QCoreApplication::setOrganizationName("TrainerOSTests");QCoreApplication::setApplicationName("SocialTests"); }
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
    void profileAvatarAcceptsShortProviderHashesButNotPaths() {
        FluxerSession s;bind(s);
        s.updateProfile({{"id",s.self_},{"avatar","9038e4f6"}});
        QCOMPARE(s.profile_["avatar"].toString(),"https://fluxerusercontent.com/avatars/"+s.self_+"/9038e4f6.png?size=64");
        s.updateProfile({{"id",s.self_},{"avatar","../other"}});
        QVERIFY(s.profile_["avatar"].toString().isEmpty());
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
        QCOMPARE(c.notificationFace(),QString("chats"));c.openNotification();QVERIFY(c.contacts());
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
        ShellController shell(library,trainers,adapter,platform,dex,dex,hall,achievements);
        auto* social=shell.social();QSignalSpy commands(social,&SocialController::commandRequested);
        shell.goToPage(1);const auto origin=shell.navigationState();commands.clear();
        social->receive(0,{{"state","connected"},{"userId","self"},{"voice",QVariantMap{{"available",true}}},
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
        ShellController shell(library,trainers,adapter,platform,dex,dex,hall,achievements);
        auto* social=shell.social();QSignalSpy commands(social,&SocialController::commandRequested);
        QVariantMap chat{{"id",channel},{"name","Friend"},{"kind","chats"},{"ringing",true}};
        QVariantMap state{{"state","connected"},{"userId","self"},{"voice",QVariantMap{{"available",true}}},{"chats",QVariantList{chat}}};
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
        // Explicit navigation, rather than a provider update, selects Home.
        for(int i=0;i<5;++i)shell.dispatch(Action::Up);
        QCOMPARE(shell.homeMenuActions()[shell.homeMenuFocus()].toMap()["id"].toString(),QString("home"));
        // A later ring must not resurrect the old selection or steal focus.
        chat["ringing"]=true;state["chats"]=QVariantList{chat};social->receive(0,state);
        QCOMPARE(shell.homeMenuFocus(),0);QVERIFY(commands.isEmpty());
    }
    void expiredRingKeepsJoinUntilCallEndsWithoutRedirectingConfirm() {
        MockLibraryRepository library;MockTrainerRepository trainers;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository hall;MockAchievementProvider achievements;
        ShellController shell(library,trainers,adapter,platform,dex,dex,hall,achievements);
        auto* social=shell.social();QSignalSpy commands(social,&SocialController::commandRequested);
        shell.goToPage(3);const auto origin=shell.navigationState();
        QVariantMap chat{{"id",channel},{"name","Friend"},{"kind","chats"},{"ringing",true},{"call",true}};
        QVariantMap state{{"state","connected"},{"userId","self"},{"voice",QVariantMap{{"available",true}}},{"chats",QVariantList{chat}}};
        social->receive(0,state);shell.dispatch(Action::Home);commands.clear();
        chat["ringing"]=false;state["chats"]=QVariantList{chat};social->receive(0,state);
        QCOMPARE(shell.homeMenuActions()[shell.homeMenuFocus()].toMap()["label"].toString(),QString("Join call"));
        shell.dispatch(Action::Confirm);
        QVERIFY(!shell.homeMenuOpen());QCOMPARE(shell.navigationState(),origin);
        QCOMPARE(commands.size(),1);QCOMPARE(commands.last()[0].toString(),QString("voice-join"));
        // Only the selected invitation is retained, not every active call.
        shell.dispatch(Action::Home);QCOMPARE(shell.homeMenuFocus(),0);
        QCOMPARE(shell.homeMenuActions().size(),4);
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
    void callNotificationAnswersDirectlyAndDeclineKeepsOrigin() {
        MockLibraryRepository library;MockTrainerRepository trainers;MockAdventureAdapter adapter;
        DevelopmentPlatformService platform;MockPokedexRepository dex;MockHallOfFameRepository hall;MockAchievementProvider achievements;
        ShellController shell(library,trainers,adapter,platform,dex,dex,hall,achievements);
        auto* social=shell.social();QSignalSpy commands(social,&SocialController::commandRequested);
        shell.goToPage(3);const auto origin=shell.navigationState();
        social->receive(0,{{"state","connected"},{"userId","self"},{"voice",QVariantMap{{"available",true}}},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"name","Friend"},{"kind","chats"},{"ringing",true}}}}});
        shell.dispatch(Action::Home);shell.activateHomeMenu(3);QVERIFY(shell.notificationsOpen());
        QVERIFY(social->notifications().first().toMap()["answerable"].toBool());
        shell.activateNotification(0);QVERIFY(!shell.homeMenuOpen());QCOMPARE(shell.navigationState(),origin);
        QCOMPARE(commands.last()[0].toString(),QString("voice-join"));
        shell.dispatch(Action::Home);shell.activateHomeMenu(5);
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
        ShellController shell(library,trainers,adapter,platform,dex,dex,hall,achievements);
        auto* social=shell.social();QSignalSpy commands(social,&SocialController::commandRequested);
        social->receive(0,{{"state","connected"},{"userId","self"},{"voice",QVariantMap{{"channel",channel},{"muted",true},{"status","Connected"}}}});
        shell.goToPage(1);const auto origin=shell.navigationState();shell.dispatch(Action::Home);
        QCOMPARE(shell.homeMenuActions().last().toMap()["id"].toString(),QString("call"));
        shell.activateHomeMenu(4);QCOMPARE(shell.homeMenuActions().first().toMap()["id"].toString(),QString("voice-mute"));
        commands.clear();shell.dispatch(Action::Confirm);QCOMPARE(commands.last()[0].toString(),QString("voice-mute"));
        shell.dispatch(Action::Back);QVERIFY(shell.homeMenuOpen());shell.dispatch(Action::Home);
        QVERIFY(!shell.homeMenuOpen());QCOMPARE(shell.navigationState(),origin);
        for(const auto& command:commands)QVERIFY(command[0].toString()!="voice-leave");
        shell.dispatch(Action::Home);shell.activateHomeMenu(4);
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
        QCOMPARE(c.notificationFaceAt(1),"groups");c.openNotificationAt(1);
        QCOMPARE(commands.last()[0].toString(),"conversation");
        QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString(channel));
        commands.clear();c.openNotificationAt(0);QVERIFY(c.contacts());
        QCOMPARE(c.rows().value(c.focusIndex()).toMap()["id"].toString(),"request");
        for(const auto& command:commands)QVERIFY(command[0].toString()!="accept");
        state["chats"]=QVariantList{};c.receive(0,state);QCOMPARE(c.notifications().size(),1);
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
        c.toastChannel_=remote;QCOMPARE(c.notificationFace(),"groups");c.openNotification();
        QCOMPARE(commands.last()[0].toString(),"conversation");QCOMPARE(commands.last()[1].toMap()["id"].toString(),QString(remote));
        QVERIFY(c.notificationFace().isEmpty());
        c.toastChannel_=remote;c.setOwner("different-owner");QVERIFY(c.notificationFace().isEmpty());
        FluxerSession s;bind(s);QString path;s.setTransport([&](auto,auto p,auto,auto,auto){path=p;});
        s.channels_[remote]={{"id",remote},{"type",3}};s.command("conversation",{{"id",remote}});
        QCOMPARE(s.channel_,QString(remote));QCOMPARE(s.face_,"groups");QVERIFY(path.contains(remote));
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
            {"chats",QVariantList{QVariantMap{{"id",channel},{"name","Friend"},{"kind","chats"},{"friend",true}}}}};
        c.receive(0,snapshot);
        c.dispatch(Action::Confirm);QCOMPARE(text.size(),1);QCOMPARE(c.textSubmitLabel(),"Send");
        c.preserveText(QString::fromUtf8("Привет 😀"));QVERIFY(commands.isEmpty());
        c.applyText(QString::fromUtf8("Привет 😀"));QCOMPARE(commands.size(),1);
        QCOMPARE(commands.last()[0].toString(),"send");
        QCOMPARE(commands.last()[1].toMap()["channel"].toString(),QString(channel));
        QCOMPARE(commands.last()[1].toMap()["text"].toString(),QString::fromUtf8("Привет 😀"));
        QVERIFY(c.draft().isEmpty());commands.clear();
        c.dispatch(Action::ToggleContinue);QCOMPARE(commands.size(),1);
        QCOMPARE(commands.last()[0].toString(),"online-probe");QCOMPARE(c.menuTitle(),"Play together");
        c.dispatch(Action::Back);QVERIFY(c.menu().isEmpty());QCOMPARE(commands.size(),1);
        // An ordinary contact must still have chat, without game invitations.
        snapshot["chats"]=QVariantList{QVariantMap{{"id",channel},{"name","Other"},{"kind","chats"}}};
        c.receive(0,snapshot);QVERIFY(!c.togetherAvailable());c.together();QCOMPARE(commands.size(),1);
        // A channel change while editing keeps the original draft, never sends it to the new person.
        c.compose();snapshot["channel"]="another";c.receive(0,snapshot);commands.clear();
        c.applyText("For the original friend");QVERIFY(commands.isEmpty());
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
        c.dispatch(Action::ToggleContinue);QCOMPARE(commands.last()[0].toString(),QString("community-filter"));
        FluxerSession s;bind(s);QString path;s.setTransport([&](auto,QString p,auto,Completion done,auto){path=p;done({200,QJsonDocument(QJsonObject{{"guilds",QJsonArray{}},{"total",0}})});});
        s.search("traineros","");QVERIFY(path.contains("tag=traineros-v1"));QVERIFY(!path.contains("query="));
        QVERIFY(s.searchStatus_.contains("Private communities"));
        s.search("communities","");QVERIFY(!path.contains("tag="));
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
    void groupCreationAndMembershipUseProviderContract() {
        FluxerSession s;Completion done;QByteArray method;QString path;QJsonObject body;int calls=0;
        s.setTransport([&](auto m,auto p,auto b,Completion cb, QByteArray){++calls;method=m;path=p;body=b;done=cb;});bind(s);
        s.command("create-group",{{"recipients",QStringList{remote,remote}}});QCOMPARE(calls,0);
        s.command("create-group",{{"recipients",QStringList{remote}}});QCOMPARE(calls,1);
        QCOMPARE(method,QByteArray("POST"));QCOMPARE(path,QString("/v1/users/@me/channels"));
        QCOMPARE(body,(QJsonObject{{"recipients",QJsonArray{remote}}}));
        const QString group="1501314428688998199";
        QJsonObject groupData{{"id",group},{"type",3},{"owner_id",remote},{"name","Friends"}};
        done({200,QJsonDocument(groupData)});QVERIFY(s.channels_.contains(group));
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
        c.setFace("groups");QCOMPARE(c.rows().first().toMap()["name"].toString(),QString("Group"));QCOMPARE(c.draft(),QString("Draft <b>plain text</b>"));
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
        c.showContacts();QCOMPARE(c.rows().size(),1);QVERIFY(c.contacts());
    }
    void backNeverClosesTheSelectedConversation() {
        SocialController c;QSignalSpy commands(&c,&SocialController::commandRequested);
        c.receive(0,{{"state","connected"},{"channel",channel}});
        c.dispatch(Action::Right);QVERIFY(c.reading());c.dispatch(Action::Back);
        QVERIFY(!c.reading());QVERIFY(c.conversation());QVERIFY(commands.isEmpty());
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
        responses[0]({200,QJsonDocument(QJsonObject{{"total",1},{"guilds",QJsonArray{QJsonObject{{"id",remote},{"name","Old"}}}}})});
        QVERIFY(s.searchResults_.isEmpty());QVERIFY(s.searching_);
        responses[1]({200,QJsonDocument(QJsonObject{{"total",30},{"guilds",QJsonArray{QJsonObject{{"id",remote},{"name","New"},{"member_count",12}}}}})});
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
        join({204,{}});QVERIFY(s.searching_);QCOMPARE(s.searchStatus_,QString("Searching..."));
        QVERIFY(s.searchResults_.isEmpty());
    }

};
}
QTEST_GUILESS_MAIN(trainer::SocialTests)
#include "SocialTests.moc"
