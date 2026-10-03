// SPDX-License-Identifier: GPL-2.0-or-later
// Included by DolphinQt/MainWindow.cpp. Native NetPlay, no widget automation.
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include "Common/TraversalClient.h"
#include "Core/Config/GraphicsSettings.h"
#include <cstdio>
#include "Core/HW/GCPad.h"
#include "InputCommon/InputConfig.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace
{
void TrainerNetplayEvent(QJsonObject event)
{
  const auto bytes = QJsonDocument(event).toJson(QJsonDocument::Compact);
  std::fprintf(stdout, "[TrainerOSNetplay]%s\n", bytes.constData());
  std::fflush(stdout);
}
}

void MainWindow::InitTrainerNetplay()
{
  const auto path = qEnvironmentVariable("TRAINEROS_DOLPHIN_SESSION");
  if (path.isEmpty())
    return;
  auto fail = [this](const QString& reason) {
    TrainerNetplayEvent({{QStringLiteral("event"), QStringLiteral("error")}, {QStringLiteral("reason"), reason}});
    // Queue teardown outside native networking callbacks/constructor stacks.
    QTimer::singleShot(0, this, [this] {
      if (Core::IsUninitialized(m_system))
        QApplication::exit(2);
      else
        ForceStop();
    });
  };
  connect(m_netplay_dialog, &NetPlayDialog::TrainerError, this, fail, Qt::QueuedConnection);
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > 8192)
    return fail(QStringLiteral("configuration"));
  const auto settings = QJsonDocument::fromJson(file.readAll()).object();
  const auto token = settings[QStringLiteral("token")].toString();
  const int slot = settings[QStringLiteral("slot")].toInt();
  const bool host = settings[QStringLiteral("host")].toBool();
  QSet<int> seats;
  for (const auto& value : settings[QStringLiteral("slots")].toArray())
    seats.insert(value.toInt());
  if (settings[QStringLiteral("protocol")] != 1 || !QRegularExpression(QStringLiteral("^[a-f0-9]{32}$")).match(token).hasMatch() ||
      seats.size() < 2 || seats.size() > 4 || !seats.contains(1) || !seats.contains(slot) ||
      (host != (slot == 1)))
    return fail(QStringLiteral("configuration"));
  for (int seat : seats)
    if (seat < 1 || seat > 4)
      return fail(QStringLiteral("configuration"));
  const auto game = std::make_shared<UICommon::GameFile>(settings[QStringLiteral("game")].toString().toStdString());
  if (!game->IsValid())
    return fail(QStringLiteral("game"));
  // A Steam-session device name may no longer exist in the native shell.
  // Rebind only the isolated copied SDL map, and only with one unambiguous pad.
  auto* pad = Pad::GetConfig()->GetController(0);
  if (!pad->IsDefaultDeviceConnected() && pad->GetDefaultDevice().source == "SDL")
  {
    std::vector<std::string> candidates;
    for (const auto& device : g_controller_interface.GetAllDevices())
      if (device->GetSource() == "SDL")
        candidates.push_back(device->GetQualifiedName());
    if (candidates.size() != 1)
      return fail(QStringLiteral("controller"));
    const auto lock = pad->GetStateLock();
    pad->SetDefaultDevice(candidates.front());
    pad->UpdateReferences(g_controller_interface);
    Pad::GetConfig()->SaveConfig();
  }
  if (!pad->IsDefaultDeviceConnected())
    return fail(QStringLiteral("controller"));
  m_netplay_dialog->SetTrainerGame(game);
  // Native Dolphin limits nicknames to 30 bytes; retain all 128 token bits.
  const auto admission = QString::fromLatin1(QByteArray::fromHex(token.toLatin1()).toBase64(
      QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
  const auto nickname = QString(QStringLiteral("TOS-%1-%2")).arg(admission).arg(slot).toStdString();
  Config::SetBaseOrCurrent(Config::NETPLAY_NICKNAME, nickname);
  if (!(host ? NetPlayHost(*game) : NetPlayJoin()))
    return fail(QStringLiteral("connection"));

  auto* timer = new QTimer(this);
  timer->setInterval(200);
  struct State { bool ready = false, starting = false, running = false; qint64 deadline; };
  auto state = std::make_shared<State>();
  state->deadline = QDateTime::currentMSecsSinceEpoch() + 65000;
  connect(timer, &QTimer::timeout, this, [this, timer, state, host, admission, seats, fail] {
    const auto client = Settings::Instance().GetNetPlayClient();
    const auto server = Settings::Instance().GetNetPlayServer();
    if (!client || !client->IsConnected())
    {
      timer->stop();
      return fail(QStringLiteral("disconnected"));
    }
    if (!state->running && Core::IsRunning(m_system))
    {
      state->running = true;
      TrainerNetplayEvent({{QStringLiteral("event"), QStringLiteral("running")}, {QStringLiteral("players"), int(seats.size())}});
    }
    if (!state->running && QDateTime::currentMSecsSinceEpoch() > state->deadline)
    {
      timer->stop();
      return fail(QStringLiteral("timeout"));
    }
    if (!host || !server)
      return;
    if (!state->ready)
    {
      QJsonObject event{{QStringLiteral("event"), QStringLiteral("ready")}, {QStringLiteral("port"), server->GetPort()}};
      if (Config::Get(Config::NETPLAY_TRAVERSAL_CHOICE) == "traversal")
      {
        if (!Common::g_TraversalClient ||
            Common::g_TraversalClient->GetState() != Common::TraversalClient::State::Connected)
          return;
        const auto id = Common::g_TraversalClient->GetHostID();
        event[QStringLiteral("code")] = QString::fromLatin1(id.data(), int(id.size()));
      }
      state->ready = true;
      TrainerNetplayEvent(event);
    }
    if (state->starting)
      return;
    NetPlay::PadMappingArray mapping{};
    QSet<int> present;
    // Snapshot under the native players mutex: no dangling Player pointers.
    const auto players = client->GetTrainerPlayers();
    for (const auto& player : players)
    {
      int seat = 0;
      for (int wanted : seats)
        if (player.name == QString(QStringLiteral("TOS-%1-%2")).arg(admission).arg(wanted).toStdString())
          seat = wanted;
      if (!seat || present.contains(seat))
      {
        if (!player.IsHost())
          server->KickPlayer(player.pid);
        return;
      }
      present.insert(seat);
      mapping[seat - 1] = player.pid;
    }
    if (present != seats || !client->DoAllPlayersHaveGame())
      return;
    if (server->GetPadMapping() != mapping)
    {
      server->SetPadMapping(mapping);
      return;  // let the native reliable channel deliver mapping before Start
    }
    state->starting = m_netplay_dialog->StartTrainerGame();
    if (!state->starting)
    {
      timer->stop();
      fail(QStringLiteral("start"));
    }
  });
  timer->start();
}
