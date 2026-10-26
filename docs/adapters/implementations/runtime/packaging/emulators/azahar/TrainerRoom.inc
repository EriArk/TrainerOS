// SPDX-License-Identifier: GPL-3.0-or-later
// Private launch admission only. Native Azahar Room/RoomMember own all wireless
// traffic, console identity, ENet reliability and game emulation.
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <csignal>
#include <QTimer>
#include <QElapsedTimer>
#include "core/hle/service/cfg/cfg.h"
#include "network/room.h"
#include "network/room_member.h"

namespace TrainerRoom {
static QJsonObject config;
static bool enabled = false;
static volatile std::sig_atomic_t stop_requested = 0;
static void RequestStop(int) { stop_requested = 1; }
static bool Prepare() {
    const auto filename = qEnvironmentVariable("TRAINEROS_AZAHAR_SESSION");
    if (filename.isEmpty()) return true;
    enabled = true;
    QFile file(filename);
    const QFileInfo info(file);
    if (!info.isFile() || info.isSymLink() || info.size() > 8192 ||
        !file.open(QIODevice::ReadOnly)) return false;
    config = QJsonDocument::fromJson(file.readAll()).object();
    const auto user = config[QStringLiteral("user")].toString();
    const auto root = QFileInfo(user).canonicalFilePath();
    const int port = config[QStringLiteral("port")].toInt();
    const int players = config[QStringLiteral("players")].toInt();
    if (players < 2 || players > 4 || config[QStringLiteral("players")] != QJsonValue(players) ||
        config[QStringLiteral("nickname")].toString().right(1).toInt() > players ||
        config[QStringLiteral("host")].toBool() != (config[QStringLiteral("nickname")].toString() == QStringLiteral("Player1")) ||
        config.size() != 7 || !config[QStringLiteral("host")].isBool() ||
        !QRegularExpression(QStringLiteral("^[a-f0-9]{64}$")).match(config[QStringLiteral("password")].toString()).hasMatch() ||
        !QRegularExpression(QStringLiteral("^Player[1-4]$")).match(config[QStringLiteral("nickname")].toString()).hasMatch() ||
        root.isEmpty() || root != user || !QFileInfo(root).isDir() ||
        !root.startsWith(info.canonicalPath() + QLatin1Char('/')) || port < 1 || port > 65535 ||
        config[QStringLiteral("port")] != QJsonValue(port)) return false;
    // Must run before any QtConfig/GetUserPath call. A failed override must not
    // silently fall back to the user's normal NAND or configuration.
    const auto sysdata = QFileInfo(config[QStringLiteral("sysdata")].toString()).canonicalFilePath();
    if (sysdata.isEmpty() || !QFileInfo(sysdata).isDir()) return false;
    FileUtil::SetUserPath(root.toStdString() + "/");
    FileUtil::UpdateUserPath(FileUtil::UserPath::SysDataDir, sysdata.toStdString() + "/");
    return FileUtil::GetUserPath(FileUtil::UserPath::UserDir) == root.toStdString() + "/";
}

static void Start(GMainWindow& window, Core::System& system) {
    if (!enabled) return;
    std::signal(SIGTERM, RequestStop);
    std::signal(SIGINT, RequestStop);
    UISettings::values.confirm_before_closing = false;
    const auto room = Network::GetRoom().lock();
    const auto member = Network::GetRoomMember().lock();
    const auto password = config[QStringLiteral("password")].toString().toStdString();
    const auto port = static_cast<u16>(config[QStringLiteral("port")].toInt());
    const bool host = config[QStringLiteral("host")].toBool();
    if (!room || !member || !system.IsPoweredOn() ||
        (host && !room->Create("TrainerOS", "", "127.0.0.1", port, password, config[QStringLiteral("players")].toInt(),
                              "Player1", "", 0,
                              std::make_unique<Network::VerifyUser::NullBackend>()))) {
        std::cout << "[TrainerOS3DS] error" << std::endl;
        QTimer::singleShot(0, &window, [&window] { window.close(); });
        return;
    }
    if (host) std::cout << "[TrainerOS3DS] room-ready" << std::endl;
    // Join performs a bounded blocking ENet handshake, just like the upstream
    // direct-connect dialog. Keep it off the GUI thread; join it before teardown.
    const auto nick = config[QStringLiteral("nickname")].toString().toStdString();
    const auto console = Service::CFG::GetConsoleIdHash(system);
    const auto mac = Service::CFG::GetConsoleMacAddress(system);
    auto future = std::make_shared<QFuture<void>>(QtConcurrent::run([=] {
        member->Join(nick, console, "127.0.0.1", port, 0, mac, password);
    }));
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, &window, [future, member, room, host] {
        future->waitForFinished();
        member->Leave();
        if (host) room->Destroy();
    });
    auto timer = new QTimer(&window);
    auto elapsed = std::make_shared<QElapsedTimer>(); elapsed->start();
    QObject::connect(timer, &QTimer::timeout, &window,
        [timer, elapsed, member, future, &window, joined = false]() mutable {
        if (stop_requested) { timer->stop(); window.close(); return; }
        if (member->IsConnected()) {
            if (!joined) { joined = true; std::cout << "[TrainerOS3DS] running" << std::endl; }
        } else if ((joined || elapsed->elapsed() > 20000) && future->isFinished()) {
            timer->stop();
            std::cout << "[TrainerOS3DS] error" << std::endl;
            window.close();
        }
    });
    timer->start(100);
}
} // namespace TrainerRoom
