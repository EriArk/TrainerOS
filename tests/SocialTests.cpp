#include <QtTest>
#include "integrations/social/FluxerSession.h"
#include "integrations/social/CommunityIdentity.h"
#include "features/social/SocialController.h"
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QUuid>
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
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
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
