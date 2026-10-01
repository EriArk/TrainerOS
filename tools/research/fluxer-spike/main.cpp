// Bounded, opt-in public user-client experiment; never linked into TrainerOS.
// Protocol references and evidence: docs/FLUXER_SPIKE.md. No upstream app code.
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>
#include <QWebSocket>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <stdexcept>

void require(bool ok, const char *step) { if (!ok) throw std::runtime_error(step); }
void report(const char *step) { std::printf("PASS %s\n", step); std::fflush(stdout); }
bool waitFor(const std::function<bool()> &predicate, int ms = 15000) {
    QElapsedTimer deadline; deadline.start();
    while (!predicate() && deadline.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    return predicate();
}
bool snowflake(const QString &id) {
    static const QRegularExpression pattern("^[1-9][0-9]{0,19}$");
    return pattern.match(id).hasMatch();
}
struct Response { int status; QJsonDocument json; };
class Http {
    QNetworkAccessManager manager;
public:
    Response request(const QByteArray &method, const QUrl &url,
                     const QString &token = {}, const QJsonObject &body = {}, bool redirected = false) {
        require(url.scheme() == "https" && url.userInfo().isEmpty()
                && (url.host() == "fluxer.app" || url.host() == "api.fluxer.app"), "HTTPS origin");
        require(token.isEmpty() || url.host() == "api.fluxer.app", "credential origin");
        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        request.setTransferTimeout(15000);
        request.setRawHeader("User-Agent", "TrainerOS-Integration-Spike/0.1");
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        if (!token.isEmpty()) request.setRawHeader("Authorization", token.toUtf8());
        const auto bytes = method == "GET" ? QByteArray{} : QJsonDocument(body).toJson(QJsonDocument::Compact);
        auto *reply = manager.sendCustomRequest(request, method, bytes);
        reply->setReadBufferSize(2 * 1024 * 1024 + 1);
        QByteArray received;
        const auto consume = [&] {
            received += reply->readAll();
            if (received.size() > 2 * 1024 * 1024) reply->abort();
        };
        QObject::connect(reply, &QIODevice::readyRead, reply, consume);
        bool finished = waitFor([&] { return reply->isFinished(); }, 18000);
        if (!finished) reply->abort();
        consume();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (!status) std::fprintf(stderr, "HTTP transport error=%d\n", int(reply->error()));
        reply->deleteLater();
        require(finished && received.size() <= 2 * 1024 * 1024, "bounded HTTP response");
        if (!redirected && token.isEmpty() && method == "GET"
            && url == QUrl("https://fluxer.app/.well-known/fluxer")
            && (status == 301 || status == 302 || status == 307 || status == 308)
            && url.resolved(redirect) == QUrl("https://api.fluxer.app/.well-known/fluxer"))
            return this->request(method, url.resolved(redirect), {}, {}, true);
        // Never print URLs, provider payloads, session identifiers or error bodies.
        return {status, QJsonDocument::fromJson(received)};
    }
};
class Gateway {
    QWebSocket socket;
    QTimer heartbeat;
    bool hello = false;
    bool failed = false;
    bool awaitingAck = false;
public:
    int sequence = 0;
    bool revoked = false;
    QString session;
    QList<QJsonObject> events;
    Gateway() {
        socket.setMaxAllowedIncomingMessageSize(2 * 1024 * 1024);
        socket.setMaxAllowedIncomingFrameSize(2 * 1024 * 1024);
        QObject::connect(&heartbeat, &QTimer::timeout, &socket, [this] {
            if (awaitingAck) { failed = true; socket.abort(); return; }
            awaitingAck = true; send(1, sequence);
        });
        QObject::connect(&socket, &QWebSocket::textMessageReceived, &socket, [this](const QString &text) {
            auto event = QJsonDocument::fromJson(text.toUtf8()).object();
            if (!event.contains("op")) { failed = true; return; }
            const int op = event["op"].toInt(-1);
            if (op == 10) {
                const int interval = event["d"].toObject()["heartbeat_interval"].toInt();
                if (interval < 1000 || interval > 120000) { failed = true; return; }
                hello = true; heartbeat.start(interval);
            } else if (op == 11) awaitingAck = false;
            else if (op == 1) send(1, sequence);
            else if (op == 9 || op == 7) { failed = true; revoked = op == 9 && event["d"] == false; }
            else if (op == 0) {
                sequence = event["s"].toInt();
                if (event["t"] == "READY") session = event["d"].toObject()["session_id"].toString();
                if (events.size() == 256) events.removeFirst();
                events.append(event);
            }
        });
    }
    void send(int op, const QJsonValue &data) {
        socket.sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"op", op}, {"d", data}}).toJson(QJsonDocument::Compact)));
    }
    void connect(const QUrl &endpoint, const QString &token, bool resume = false) {
        require(endpoint.scheme() == "wss" && endpoint.host() == "gateway.fluxer.app"
                && endpoint.userInfo().isEmpty(), "gateway origin");
        hello = false; failed = false; awaitingAck = false; events.clear();
        auto url = endpoint; QUrlQuery query; query.addQueryItem("v", "1"); query.addQueryItem("encoding", "json"); url.setQuery(query);
        socket.open(url);
        require(waitFor([&] { return hello || failed; }) && hello && !failed, "gateway HELLO");
        if (resume) send(6, QJsonObject{{"token", token}, {"session_id", session}, {"seq", sequence}});
        else send(2, QJsonObject{{"token", token}, {"properties", QJsonObject{{"os", "Linux/Windows"},
                  {"browser", "TrainerOS integration spike"}, {"device", "desktop"}}}});
        require(event(resume ? "RESUMED" : "READY"), "gateway authentication/resume");
    }
    bool event(const QString &type, const QString &id = {}) {
        return waitFor([&] {
            for (const auto &e : events) if (e["t"] == type && (id.isEmpty() || e["d"].toObject()["id"] == id)) return true;
            return failed;
        }) && !failed && std::any_of(events.begin(), events.end(), [&](const auto &e) {
            return e["t"] == type && (id.isEmpty() || e["d"].toObject()["id"] == id);
        });
    }
    void disconnect() { heartbeat.stop(); socket.abort(); }
    ~Gateway() { disconnect(); }
};

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        const auto args = app.arguments();
        const bool pair = args.contains("--pair-test");
        Http http;
        const auto discovery = http.request("GET", QUrl("https://fluxer.app/.well-known/fluxer"));
        require(discovery.status == 200, "discovery HTTP");
        const auto endpoints = discovery.json.object()["endpoints"].toObject();
        require(endpoints["api_public"] == "https://api.fluxer.app", "public API endpoint");
        report("public discovery over native Qt TLS");
        if (args.contains("--discovery-only")) return 0;
        QFile input; require(input.open(stdin, QIODevice::ReadOnly), "private stdin");
        const auto secret = QJsonDocument::fromJson(input.read(65537)).object();
        const auto accounts = secret["accounts"].toArray();
        require(accounts.size() == 2, "two designated test sessions");
        QString tokens[2], ids[2];
        for (int i = 0; i < 2; ++i) {
            const auto account = accounts[i].toObject();
            tokens[i] = account["token"].toString(); ids[i] = account["user_id"].toString();
            require(!tokens[i].isEmpty() && snowflake(ids[i]), "session input");
            const auto me = http.request("GET", QUrl("https://api.fluxer.app/v1/users/@me"), tokens[i]);
            require(me.status == 200 && me.json.object()["id"] == ids[i] && me.json.object()["verified"].toBool(), "designated verified user");
        }
        require(ids[0] != ids[1], "distinct test accounts");
        report("two verified user sessions");
        Gateway receiver;
        const QUrl gateway(endpoints["gateway"].toString());
        receiver.connect(gateway, tokens[1]); report("native gateway READY");
        receiver.disconnect(); receiver.connect(gateway, tokens[1], true); report("native gateway RESUMED");
        if (args.contains("--logout-test")) {
            auto out = http.request("POST", QUrl("https://api.fluxer.app/v1/auth/logout"), tokens[1]);
            require(out.status == 204, "test-session logout");
            require(waitFor([&] { return receiver.revoked; }), "gateway invalidates revoked session");
            require(http.request("GET", QUrl("https://api.fluxer.app/v1/users/@me"), tokens[1]).status == 401, "revoked token rejected");
            require(http.request("GET", QUrl("https://api.fluxer.app/v1/users/@me"), tokens[0]).status == 200, "other account unaffected");
            require(http.request("POST", QUrl("https://api.fluxer.app/v1/auth/logout"), tokens[0]).status == 204, "remaining test-session logout");
            report("logout revokes HTTP and gateway access; separate account unaffected");
            return 0;
        }
        if (!pair && !args.contains("--voice-contract")) return 0;
        const auto api = [&](int who, const QByteArray &method, const QString &path, const QJsonObject &body = QJsonObject{}) {
            const auto response = http.request(method, QUrl("https://api.fluxer.app/v1" + path), tokens[who], body);
            if (response.status < 200 || response.status >= 300) {
                std::printf("HTTP failure status=%d\n", response.status);
                throw std::runtime_error("bounded pair request; no automatic retry");
            }
            return response.json;
        };
        const auto rel = api(0, "POST", "/users/@me/relationships/" + ids[1]).object();
        if (rel["type"].toInt() != 1) api(1, "PUT", "/users/@me/relationships/" + ids[0], {{"type", 1}});
        for (int i = 0; i < 2; ++i) {
            const auto relations = api(i, "GET", "/users/@me/relationships").array();
            bool found = false;
            for (auto r : relations) if (r.toObject()["id"] == ids[1-i] && r.toObject()["type"].toInt() == 1) found = true;
            require(found, "bilateral friendship");
        }
        report("bilateral friendship");
        const auto channel = api(0, "POST", "/users/@me/channels", {{"recipient_id", ids[1]}}).object()["id"].toString();
        require(snowflake(channel), "DM identifier");
        if (args.contains("--voice-contract")) {
            api(1, "GET", "/channels/" + channel + "/call");
            report("private call eligibility API");
            receiver.send(4, QJsonObject{{"guild_id", QJsonValue::Null}, {"channel_id", channel},
                {"self_mute", true}, {"self_deaf", true}, {"self_video", false}, {"self_stream", false}});
            const bool grantReceived = receiver.event("VOICE_SERVER_UPDATE");
            bool validGrant = false;
            for (const auto &e : receiver.events) if (e["t"] == "VOICE_SERVER_UPDATE") {
                const auto d = e["d"].toObject();
                validGrant = d["channel_id"] == channel && !d["token"].toString().isEmpty()
                    && QUrl(d["endpoint"].toString()).scheme() == "wss";
            }
            receiver.events.clear();
            receiver.send(4, QJsonObject{{"guild_id", QJsonValue::Null}, {"channel_id", QJsonValue::Null}});
            const bool left = waitFor([&] {
                return std::any_of(receiver.events.begin(), receiver.events.end(), [&](const auto &e) {
                    const auto d = e["d"].toObject();
                    return e["t"] == "VOICE_STATE_UPDATE" && d["channel_id"].isNull() && d["user_id"] == ids[1];
                });
            });
            require(grantReceived && validGrant && left, "voice grant and confirmed leave");
            report("muted private voice grant and leave; no media connection or microphone");
            return 0;
        }
        const QString path = "/channels/" + channel + "/messages";
        const auto msg = api(0, "POST", path, {{"content", "TrainerOS consented integration test: native message."}}).object();
        const auto messageId = msg["id"].toString(); require(snowflake(messageId), "message identifier");
        require(receiver.event("MESSAGE_CREATE", messageId), "recipient realtime create"); report("DM send and realtime receive");
        api(0, "PATCH", path + "/" + messageId, {{"content", "TrainerOS consented integration test: edited."}});
        require(receiver.event("MESSAGE_UPDATE", messageId), "recipient realtime edit"); report("realtime edit");
        const auto history = api(1, "GET", path + "?limit=10").array();
        bool found = false; for (auto m : history) if (m.toObject()["id"] == messageId) found = true;
        require(found, "recipient bounded history"); report("recipient history");
        api(0, "DELETE", path + "/" + messageId);
        require(receiver.event("MESSAGE_DELETE", messageId), "recipient realtime delete"); report("realtime delete and own test-message cleanup");
        receiver.disconnect(); receiver.connect(gateway, tokens[1], true); report("resume after messaging");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL %s\n", e.what()); return 1;
    }
}
