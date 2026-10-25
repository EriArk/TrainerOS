#include "PlatformHost.h"
#include "SdkNetwork.h"
#include "features/trainer/AvatarAssets.h"
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QTemporaryDir>
#include "features/device/ControlsDashboard.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>
#include <algorithm>

namespace trainer {
namespace {
constexpr int MaxFrame = 64 * 1024, MaxQueued = 256 * 1024;
bool identifier(const QString& value) {
    static const QRegularExpression valid("^[a-z][a-z0-9_.-]{0,63}$");
    return valid.match(value).hasMatch();
}
QString widgetKey(const QString& app, const QString& id) { return "sdk:" + app + ":" + id; }
QJsonObject capabilities() {
    return {{"context", 1}, {"lifecycle", 1}, {"ui.start.widget", 1}};
}
}
PlatformHost::PlatformHost(ControlsDashboard& dashboard, QObject* parent)
    : QObject(parent), dashboard_(dashboard) {
    connect(&server_, &QTcpServer::newConnection, this, &PlatformHost::accept);
    connect(&dashboard_, &ControlsDashboard::contributionActivated, this, &PlatformHost::activate);
}
PlatformHost::~PlatformHost() { stop(); }
void PlatformHost::setAvatar(const QJsonObject& descriptor,bool available) {
    avatarAvailable_=available;
    if(descriptor==avatar_)return;
    avatar_=descriptor;
    for(auto* socket:peers_.keys())if(peers_.value(socket).authenticated&&peers_.value(socket).avatarSubscribed)
        send(socket,{{"event","avatar.changed"},{"avatar",avatar_}});
}
void PlatformHost::setNetwork(SdkNetwork* service) {
    if(network_)disconnect(network_,nullptr,this,nullptr);
    network_=service;
    if(service)connect(service,&SdkNetwork::event,this,[this](const QString& app,const QJsonObject& packet){
        for(auto* socket:peers_.keys())if(peers_.value(socket).authenticated&&peers_.value(socket).app==app)send(socket,packet);
    });
}
QString PlatformHost::defaultEndpoint() {
    const auto explicitPath = qEnvironmentVariable("TRAINEROS_SDK_ENDPOINT");
    if (!explicitPath.isEmpty()) return explicitPath;
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath("traineros/platform-v1/endpoint.json");
}
bool PlatformHost::start(const QString& endpointFile, QString* error) {
    if (server_.isListening()) { if (error) *error = "SDK host is already running"; return false; }
    const QFileInfo info(endpointFile);
    const auto directory = info.absolutePath();
    auto fail = [&](const QString& message) { if (error) *error = message; stop(); return false; };
    if (info.isSymLink() || QFileInfo(directory).isSymLink() || !QDir().mkpath(directory))
        return fail("Cannot create the private SDK endpoint directory");
#ifdef Q_OS_UNIX
    if (QFileInfo(directory).ownerId() != QFileInfo(QDir::homePath()).ownerId()
        || !QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner))
        return fail("SDK endpoint directory must belong to the current user");
#endif
    lock_ = std::make_unique<QLockFile>(info.absoluteFilePath() + ".lock");
    lock_->setStaleLockTime(0);
    if (!lock_->tryLock()) return fail("Another TrainerOS instance owns this SDK endpoint");
    if (!server_.listen(QHostAddress::LocalHost, 0)) return fail(server_.errorString());
    instance_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    token_ = QUuid::createUuid().toString(QUuid::Id128) + QUuid::createUuid().toString(QUuid::Id128);
    QSaveFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly)) return fail(file.errorString());
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    const auto bytes = QJsonDocument(QJsonObject{{"protocol", 1}, {"host", "127.0.0.1"},
        {"port", server_.serverPort()}, {"token", token_}, {"instance", instance_}}).toJson(QJsonDocument::Compact);
    if (file.write(bytes) != bytes.size() || !file.commit()) return fail(file.errorString());
    endpoint_ = info.absoluteFilePath();
    return true;
}
void PlatformHost::stop() {
    server_.close();
    const auto sockets = peers_.keys();
    for (auto* socket : sockets) {
        send(socket, {{"event", "host.stopping"}});
        release(socket);
    }
    // Never remove the discovery file of a different host.
    if (!endpoint_.isEmpty()) {
        QFile file(endpoint_);
        if (file.open(QIODevice::ReadOnly)
            && QJsonDocument::fromJson(file.read(MaxFrame)).object()["instance"] == instance_) {
            file.close(); QFile::remove(endpoint_);
        }
    }
    endpoint_.clear(); token_.clear(); lock_.reset();
}
QJsonObject PlatformHost::snapshot() const {
    auto value = context_;
    value["revision"] = QString::number(revision_);
    value["instance"] = instance_;
    return value;
}
void PlatformHost::setContext(const QJsonObject& value) {
    if (value == context_) return;
    const bool ownerChanged = value["profile"] != context_["profile"];
    context_ = value; ++revision_;
    if (ownerChanged) {
        avatar_={};
        // An old process must not automatically adopt the next player's context.
        // Clear all contributions together: releasing peers one by one must not
        // republish another old owner's widget into the new profile's layout.
        for (auto& peer : peers_) peer.widgets = {};
        const auto sockets = peers_.keys();
        for (auto* socket : sockets) {
            send(socket, {{"event", "context.revoked"}, {"reason", "profile_changed"}});
            release(socket);
        }
    } else {
        for (auto* socket : peers_.keys()) if (peers_.value(socket).subscribed)
            send(socket, {{"event", "context.changed"}, {"context", snapshot()}});
    }
}
void PlatformHost::notifyLifecycle(const QString& event) {
    for (auto* socket : peers_.keys()) if (peers_.value(socket).subscribed)
        send(socket, {{"event", "lifecycle"}, {"name", event}, {"context", snapshot()}});
}
void PlatformHost::accept() {
    while (auto* socket = server_.nextPendingConnection()) {
        if (peers_.size() >= 16) { socket->abort(); socket->deleteLater(); continue; }
        peers_.insert(socket, {});
        socket->setReadBufferSize(MaxFrame + 1);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { read(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] { release(socket); });
        QTimer::singleShot(5000, socket, [this, socket] {
            if (peers_.contains(socket) && !peers_.value(socket).authenticated) release(socket);
        });
    }
}
void PlatformHost::read(QTcpSocket* socket) {
    if (!peers_.contains(socket)) return;
    peers_[socket].input += socket->readAll();
    // Bound each read turn as well as individual frames; do not let a local
    // stalled/malformed client occupy the GUI event loop with unbounded input.
    if (peers_[socket].input.size() > MaxQueued) { release(socket); return; }
    int count = 0;
    while (peers_.contains(socket)) {
        auto& bytes = peers_[socket].input;
        const auto end = bytes.indexOf('\n');
        if (end < 0) {
            if (bytes.size() >= MaxFrame) release(socket);
            return;
        }
        if (end + 1 > MaxFrame) { release(socket); return; }
        const auto line = bytes.left(end); bytes.remove(0, end + 1);
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) { release(socket); return; }
        request(socket, document.object());
        if (++count == 32 && peers_.contains(socket) && !peers_[socket].input.isEmpty()) {
            QTimer::singleShot(0, socket, [this, socket] { read(socket); }); return;
        }
    }
}
void PlatformHost::send(QTcpSocket* socket, const QJsonObject& packet) {
    if (!peers_.contains(socket)) return;
    if (socket->bytesToWrite() > MaxQueued) { release(socket); return; }
    socket->write(QJsonDocument(packet).toJson(QJsonDocument::Compact) + '\n');
}
void PlatformHost::release(QTcpSocket* socket) {
    if (!peers_.contains(socket)) return;
    const auto peer=peers_.take(socket);
    if(network_&&peer.authenticated)network_->disconnected(peer.app);
    disconnect(socket, nullptr, this, nullptr);
    publishWidgets();
    socket->disconnectFromHost();
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    if (socket->state() == QAbstractSocket::UnconnectedState) socket->deleteLater();
    else QTimer::singleShot(1000, socket, [socket] { socket->abort(); socket->deleteLater(); });
}
void PlatformHost::request(QTcpSocket* socket, const QJsonObject& packet) {
    if (!peers_.contains(socket)) return;
    const auto id = packet["id"].toString();
    if (id.isEmpty() || id.size() > 64) { release(socket); return; }
    const auto method = packet["method"].toString();
    const auto args = packet["params"].toObject();
    auto ok = [&](const QJsonObject& result) { send(socket, {{"id", id}, {"result", result}}); };
    auto fail = [&](const QString& code) { send(socket, {{"id", id}, {"error", QJsonObject{{"code", code}}}}); };
    auto& peer = peers_[socket];
    if (!peer.authenticated) {
        if (method != "hello" || args["token"].toString() != token_) { fail("unauthorized"); release(socket); return; }
        if (args["protocol"].toInt() != 1) { fail("unsupported_version"); release(socket); return; }
        const auto app = args["app"].toString();
        if (!identifier(app)) { fail("invalid_app"); return; }
        for (const auto& other : peers_) if (other.authenticated && other.app == app) { fail("app_in_use"); return; }
        peer.authenticated = true; peer.app = app;
        auto caps=capabilities();
        if(avatarAvailable_){caps["avatar.local"]=1;caps["avatar.recipe"]=1;}
        if(network_&&network_->available()){caps["lobby.activity"]=1;caps["network.records"]=1;caps["network.group"]=1;}
        ok({{"protocol", 1}, {"capabilities", caps}, {"context", snapshot()}});
        return;
    }
    if (method == "context.get") { ok({{"context", snapshot()}}); return; }
    if (method == "context.subscribe") { peer.subscribed = true; ok({{"context", snapshot()}}); return; }
    if (method == "context.unsubscribe") { peer.subscribed = false; ok({}); return; }
    if(method=="avatar.get"||method=="avatar.subscribe"||method=="avatar.unsubscribe") {
        if(!avatarAvailable_){fail("unsupported_method");return;}
        if(context_["profile"].toString().isEmpty()){fail("no_profile");return;}
        if(method=="avatar.unsubscribe"){peer.avatarSubscribed=false;ok({});return;}
        if(method=="avatar.subscribe")peer.avatarSubscribed=true;
        if(avatar_.isEmpty()){fail("avatar_not_ready");return;}
        auto descriptor=avatar_;descriptor["profile"]=context_["profile"];
        ok({{"avatar",descriptor}});return;
    }
    if(method=="avatar.resolve") {
        if(!avatarAvailable_){fail("unsupported_method");return;}
        if(context_["profile"].toString().isEmpty()){fail("no_profile");return;}
        if(peer.avatarResolving||avatarJobs_>=4){fail("avatar_busy");return;}
        if(args.size()!=1||!args["recipe"].isObject()){fail("invalid_avatar_recipe");return;}
        auto cache=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/trainer-avatar-peer-XXXXXX");
        if(!cache->isValid()){fail("avatar_cache_unavailable");return;}
        peer.avatarResolving=true;++avatarJobs_;
        auto* watcher=new QFutureWatcher<QJsonObject>(this);
        const QPointer<QTcpSocket> target(socket);
        connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher,target,id,cache]{
            --avatarJobs_;
            const auto result=watcher->result();watcher->deleteLater();
            if(!target||!peers_.contains(target))return;
            auto& current=peers_[target];current.avatarResolving=false;
            if(result.contains("error"))send(target,{{"id",id},{"error",QJsonObject{{"code",result["error"]}}}});
            else {
                current.avatarCache=cache;
                // A renderable recipe is not an authenticated account/profile.
                send(target,{{"id",id},{"result",QJsonObject{{"avatar",result}}}});
            }
        });
        const auto recipe=args["recipe"].toObject();
        watcher->setFuture(QtConcurrent::run([recipe,cache]{return AvatarAssets::resolveRecipe(recipe,cache->path());}));
        return;
    }
    if(network_&&(method.startsWith("activity.")||method.startsWith("network."))) {
        if(context_["profile"].toString().isEmpty()){fail("no_profile");return;}
        QJsonObject result;
        const auto error=network_->request(peer.app,method,args,result);
        if(error.isEmpty())ok(result);else fail(error);
        return;
    }
    if (method == "widget.put") {
        if (context_["profile"].toString().isEmpty()) { fail("no_profile"); return; }
        if (args["revision"].toString() != QString::number(revision_)) { fail("stale_context"); return; }
        const auto widget = args["id"].toString();
        if (!identifier(widget) || args["title"].toString().trimmed().isEmpty()
            || args["title"].toString().size() > 64 || args["text"].toString().size() > 240) { fail("invalid_widget"); return; }
        if (!peer.widgets.contains(widget) && peer.widgets.size() >= 8) { fail("limit_reached"); return; }
        // Only declarative text + one activation, rendered by existing host UI.
        peer.widgets[widget] = QJsonObject{{"id", widgetKey(peer.app, widget)},
            {"title", args["title"].toString()}, {"text", args["text"].toString()}, {"external", true}};
        publishWidgets(); ok({{"id", widget}}); return;
    }
    if (method == "widget.remove") {
        peer.widgets.remove(args["id"].toString()); publishWidgets(); ok({}); return;
    }
    if (method == "goodbye") { ok({}); release(socket); return; }
    // Unbound services never return fixture success or launch a second stack.
    fail("unsupported_method");
}
void PlatformHost::publishWidgets() {
    QVariantList widgets;
    for (const auto& peer : peers_) for (auto i = peer.widgets.begin(); i != peer.widgets.end(); ++i)
        widgets.append(i.value().toObject().toVariantMap());
    std::sort(widgets.begin(), widgets.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap()["id"].toString() < b.toMap()["id"].toString();
    });
    dashboard_.setExternalWidgets(widgets);
}
void PlatformHost::activate(const QString& key) {
    for (auto* socket : peers_.keys()) {
        const auto peer = peers_.value(socket);
        for (auto i = peer.widgets.begin(); i != peer.widgets.end(); ++i)
            if (widgetKey(peer.app, i.key()) == key) {
                send(socket, {{"event", "widget.activated"}, {"id", i.key()}, {"context", snapshot()}});
                return;
            }
    }
}
}
