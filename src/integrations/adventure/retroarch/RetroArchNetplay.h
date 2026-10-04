#pragma once
#include "RetroArchAdapter.h"
#include <QJsonObject>

namespace trainer::retroarch {
// Runtime multiplayer is independent of semantic Pokemon save adapters.
// The first profile has no persistent game progress; both peers use temporary SRAM.
struct NetplayRequest {
    bool host = false;
    bool relay = false;
    QString address, relaySession, password, nickname;
    QString relayEndpoint;
    quint16 port = 55435;
    quint16 clientPort = 0; // Owned loopback authentication bridge, relay guests only.
    int slot = 0; // Party-assigned controller; never inferred from arrival order.
    QJsonObject expected;
};
QString netplayRelayEndpoint(const QByteArray& directoryResponse);
// Exact supported cartridges only; shared-screen netplay is not a link cable.
QJsonObject netplayProfile(QString platform,QString core,QString contentDigest);
QStringList netplayControllerArguments(const QJsonObject& identity);
QByteArray netplayControllers(const QJsonObject& identity, bool host, int slot);
QJsonObject netplayIdentity(const AdventureRegistration&, const RetroArchInstallation&,
                           const std::atomic_bool& cancelled);
QString prepareNetplay(ProcessCommand&, const AdventureRegistration&,
                       const RetroArchInstallation&, const NetplayRequest&,
                       const std::atomic_bool& cancelled);
}
