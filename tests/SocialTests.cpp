#include <QtTest>
#include "integrations/social/FluxerSession.h"
#include "features/social/SocialController.h"
#include <QJsonArray>

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
    void searchAndAddFriendHaveDistinctControllerActions() {
        SocialController c;
        c.receive(0,{{"state","connected"},{"friends",QVariantList{
            QVariantMap{{"id",remote},{"name","Odin"},{"type",1}},
            QVariantMap{{"id","4"},{"name","Flip"},{"type",1}}}}});
        c.setFace("friends");QSignalSpy input(&c,&SocialController::textRequested);
        c.dispatch(Action::Secondary);QCOMPARE(input.last().first().toString(),QString("Find friends"));
        c.applyText("odin");QCOMPARE(c.rows().size(),1);QCOMPARE(c.rows().first().toMap()["name"].toString(),QString("Odin"));
        c.dispatch(Action::ToggleContinue);QVERIFY(input.last().first().toString().startsWith("Add friend"));
    }
};
}
QTEST_GUILESS_MAIN(trainer::SocialTests)
#include "SocialTests.moc"
