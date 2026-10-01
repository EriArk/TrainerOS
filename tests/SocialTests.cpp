#include <QtTest>
#include "integrations/social/FluxerSession.h"
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
        s.setTransport([&](auto,auto requestPath,auto,Completion done){path=requestPath;reply=done;});bind(s);
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
        s.setTransport([&](auto,auto,auto,Completion done){reply=done;});bind(s);
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
        s.setTransport([&](auto,auto,auto,Completion done){++requests;done({429,{},60});});bind(s);
        s.request("GET","/test",{},[](Reply){});
        const auto deadline=s.blockedUntil_;
        s.request("GET","/test",{},[](Reply){});
        QCOMPARE(requests,1);QCOMPARE(s.blockedUntil_,deadline);
    }
    void ownerChangeDiscardsLateResponse() {
        FluxerSession s;Completion deferred;
        s.setTransport([&](auto,auto,auto,Completion done){deferred=std::move(done);});bind(s);
        s.loadMessages(channel);s.setOwner("trainer-b",2);
        deferred({200,QJsonDocument(QJsonArray{QJsonObject{{"id","99"},{"channel_id",channel},{"content","private old owner text"}}})});
        QVERIFY(s.messages_.isEmpty());QVERIFY(s.token_.isEmpty());QCOMPARE(s.generation_,quint64(2));
    }
    void canceledLoginNeverInstallsLateToken() {
        FluxerSession s;Completion deferred;
        s.setTransport([&](auto,auto,auto,Completion done){deferred=std::move(done);});s.setOwner("a",1);
        s.code_="test-code";s.pollSecret_="test-poll-secret";s.expires_=QDateTime::currentMSecsSinceEpoch()+10000;
        s.pollLogin();auto late=deferred;s.command("cancel-login");
        late({200,QJsonDocument(QJsonObject{{"status","completed"},{"token","must-not-install"}})});
        QVERIFY(s.token_.isEmpty());QVERIFY(s.code_.isEmpty());QCOMPARE(s.state_,QString("signed-out"));
    }
    void uncertainSendHasOneRequestAndNoAutomaticRetry() {
        FluxerSession s;int requests=0;QString nonce;
        s.setTransport([&](auto method,auto path,auto body,Completion done){
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
        s.setTransport([&](auto,auto,auto,Completion done){deferred=std::move(done);});bind(s);
        s.loadMessages(channel);
        s.gatewayEvent({{"op",0},{"s",3},{"t","MESSAGE_DELETE"},{"d",QJsonObject{{"id","1501314428688998190"},{"channel_id",channel}}}});
        deferred({200,QJsonDocument(QJsonArray{QJsonObject{{"id","1501314428688998190"},{"channel_id",channel},{"content","deleted"}}})});
        QVERIFY(s.messages_.isEmpty());
    }
    void logoutRevokesBeforeClearingSession() {
        FluxerSession s;int status=0;
        s.setTransport([&](auto,auto path,auto,Completion done){QCOMPARE(path,QString("/v1/auth/logout"));done({status,{}});});bind(s);
        s.command("logout");QVERIFY(!s.token_.isEmpty());status=204;s.command("logout");
        QVERIFY(s.token_.isEmpty());QVERIFY(s.channels_.isEmpty());QCOMPARE(s.state_,QString("signed-out"));
    }
    void messagesAreBoundedAndIdsStayStrings() {
        FluxerSession s;s.setTransport([](auto,auto,auto,auto){});bind(s);
        for(quint64 id=1501314428688998000ULL;id<1501314428688998200ULL;++id)
            s.mergeMessage({{"id",QString::number(id)},{"channel_id",channel},{"content","hello"},{"author",QJsonObject{{"id",remote}}}});
        QCOMPARE(s.messages_.size(),100);QCOMPARE(s.messageOrder_.last(),QString("1501314428688998199"));
    }
    void presentationPreservesDraftsAndFiltersFaces() {
        SocialController c; // Synthetic snapshots only; no owner means no network/credential access.
        c.receive(0,{{"state","connected"},{"channel",channel},{"friends",QVariantList{QVariantMap{{"id",remote},{"name","Friend"},{"type",1}}}},
            {"chats",QVariantList{QVariantMap{{"id",channel},{"name","Friend"},{"kind","chats"}},QVariantMap{{"id","3"},{"name","Group"},{"kind","groups"}}}}});
        QCOMPARE(c.rows().size(),1);c.compose();c.applyText("Draft <b>plain text</b>");
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
        FluxerSession s;QStringList requests;s.setTransport([&](auto,auto path,auto,Completion done){requests<<path;done({200,QJsonDocument(QJsonArray{})});});bind(s);
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
        s.setTransport([&](auto method,auto path,auto,Completion done){QCOMPARE(method,QByteArray("GET"));paths<<path;responses<<done;});bind(s);
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
        s.setTransport([&](auto method,auto path,auto,Completion done){methods<<method;paths<<path;if(method=="GET"&&path.startsWith("/v1/invites/"))done({200,QJsonDocument(QJsonObject{{"code","aB1"},{"channel",QJsonObject{{"name","Our group"}}}})});});bind(s);
        s.search("invite","https://fluxer.gg/aB1");QCOMPARE(methods,QStringList{"GET"});QCOMPARE(paths.first(),QString("/v1/invites/aB1"));
        s.command("search-action",{{"id","aB1"}});QCOMPARE(methods.last(),QString("POST"));
    }
    void lateMembershipActionDoesNotReplaceNewSearch() {
        FluxerSession s;Completion join;
        s.setTransport([&](auto method,auto path,auto,Completion done){
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
