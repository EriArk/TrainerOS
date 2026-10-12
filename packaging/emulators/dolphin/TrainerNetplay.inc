// SPDX-License-Identifier: GPL-2.0-or-later
// Included by DolphinQt/MainWindow.cpp. Native NetPlay, no widget automation.
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QMap>
#include <QTimer>
#include <QSocketNotifier>
#include <unistd.h>
#include "Common/TraversalClient.h"
#include "Core/Config/GraphicsSettings.h"
#include <cstdio>
#include "Core/HW/GCPad.h"
#include "Core/HW/Wiimote.h"
#include "DiscIO/Enums.h"
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
  const bool hotseat = settings[QStringLiteral("hotseat")].toBool();
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
  if (hotseat && (seats.size() != 2 || !seats.contains(2)))
    return fail(QStringLiteral("configuration"));
  const auto game = std::make_shared<UICommon::GameFile>(settings[QStringLiteral("game")].toString().toStdString());
  const bool wii = settings[QStringLiteral("controllers")] == QStringLiteral("wiimote");
  if (!game->IsValid() ||
      game->GetPlatform() != (wii ? DiscIO::Platform::WiiDisc : DiscIO::Platform::GameCubeDisc) ||
      (wii && hotseat))
    return fail(QStringLiteral("game"));
  // A Steam-session device name may no longer exist in the native shell.
  // Rebind only the isolated copied SDL map, and only with one unambiguous pad.
  auto* input_config = wii ? Wiimote::GetConfig() : Pad::GetConfig();
  auto* pad = input_config->GetController(0);
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
    input_config->SaveConfig();
  }
  if (!pad->IsDefaultDeviceConnected())
    return fail(QStringLiteral("controller"));
  m_netplay_dialog->SetTrainerGame(game);
  // Native Dolphin limits nicknames to 30 bytes; retain all 128 token bits.
  const auto admission = QString::fromLatin1(QByteArray::fromHex(token.toLatin1()).toBase64(
      QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
  const auto nickname = QString(QStringLiteral("TOS-%1-%2")).arg(admission).arg(slot).toStdString();
  Config::SetBaseOrCurrent(Config::NETPLAY_NICKNAME, nickname);
  if (hotseat)
    Config::SetBaseOrCurrent(Config::NETPLAY_NETWORK_MODE, std::string("golf"));
  if (!(host ? NetPlayHost(*game) : NetPlayJoin()))
    return fail(QStringLiteral("connection"));
  if (hotseat)
  {
    Settings::Instance().GetNetPlayClient()->SetTrainerHotseat(true);
    if (host)
      Settings::Instance().GetNetPlayServer()->SetTrainerHotseat(true);
  }

  auto* timer = new QTimer(this);
  timer->setInterval(200);
  struct State
  {
    bool ready = false, starting = false, running = false;
    qint64 deadline;
    QMap<int, NetPlay::PlayerId> players;
    int owner = 0;
    quint64 sequence = 0;
    QByteArray input;
  };
  auto state = std::make_shared<State>();
  state->deadline = QDateTime::currentMSecsSinceEpoch() + 65000;
  if (hotseat)
  {
    connect(m_netplay_dialog, &NetPlayDialog::TrainerController, this,
            [state, host, fail](int pid) {
      if (!host)
        return fail(QStringLiteral("controller"));
      int owner = 0;
      if (pid != 0)
      {
        owner = state->players.key(static_cast<NetPlay::PlayerId>(pid), 0);
        if (owner == 0)
          return fail(QStringLiteral("controller"));
      }
      state->owner = owner;
      if (!state->running && state->sequence == 0)
        return;  // no turn button before the native game has actually booted
      TrainerNetplayEvent({{QStringLiteral("event"), QStringLiteral("controller")},
                          {QStringLiteral("sequence"), qint64(++state->sequence)},
                          {QStringLiteral("owner"), owner}});
    }, Qt::QueuedConnection);
    auto* input = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
    connect(input, &QSocketNotifier::activated, this, [this, state, input, host, fail] {
      char bytes[256];
      const auto size = ::read(STDIN_FILENO, bytes, sizeof(bytes));
      if (size <= 0)
      {
        input->setEnabled(false);
        return fail(QStringLiteral("controller-channel"));
      }
      state->input.append(bytes, size);
      if (state->input.size() > 512)
        return fail(QStringLiteral("controller-command"));
      while (state->input.contains('\n'))
      {
        const auto end = state->input.indexOf('\n');
        const auto command = QJsonDocument::fromJson(state->input.left(end)).object();
        state->input.remove(0, end + 1);
        if (command[QStringLiteral("action")] == QStringLiteral("quit"))
          return fail(QStringLiteral("ended"));
        if (!host || !state->running || state->owner == 0 ||
            command[QStringLiteral("action")] != QStringLiteral("pass") ||
            command[QStringLiteral("sequence")].toInteger(-1) != qint64(state->sequence) ||
            command[QStringLiteral("target")].toInt() != 3 - state->owner)
          continue;  // stale commands never change a later turn
        const auto client = Settings::Instance().GetNetPlayClient();
        if (!client || !client->IsConnected())
          return fail(QStringLiteral("controller-channel"));
        const auto target = state->players.value(3 - state->owner);
        state->owner = 0;  // one request until native release/acquire confirms it
        client->RequestGolfControl(target);
      }
    });
  }
  connect(timer, &QTimer::timeout, this, [this, timer, state, host, hotseat, wii, admission, seats, fail] {
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
      if (hotseat && host && state->sequence == 0 && state->owner != 0)
        TrainerNetplayEvent({{QStringLiteral("event"), QStringLiteral("controller")},
                            {QStringLiteral("sequence"), qint64(++state->sequence)},
                            {QStringLiteral("owner"), state->owner}});
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
      state->players[seat] = player.pid;
      mapping[seat - 1] = player.pid;
    }
    if (present != seats || !client->DoAllPlayersHaveGame())
      return;
    const NetPlay::PadMappingArray empty{};
    const auto& pads = wii ? empty : mapping;
    const auto& wiimotes = wii ? mapping : empty;
    if (server->GetPadMapping() != pads || server->GetWiimoteMapping() != wiimotes)
    {
      server->SetPadMapping(pads);
      server->SetWiimoteMapping(wiimotes);
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
